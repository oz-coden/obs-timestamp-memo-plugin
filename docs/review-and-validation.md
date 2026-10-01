# 全体レビュー・修正・検証結果（2026-10-02）

## 結論と対象

ローカルとGitHub masterの開始SHAが一致していること、追跡ファイルに既存変更がないことを確認し、
全ソース・UI・保存/復旧・OBS登録解除・依存/ビルド・ドキュメント・履歴を調査した。
[修正前レビュー](review-before-fixes.md)を先にコミットし、その後で修正した。
この報告の「確認済み」はコード/APIまたは回帰テストによる確認であり、実OBS上での再現確認とは区別する。
Criticalと断定できる問題は見つからなかった。Highのデータ保護・ライフタイム問題から優先して修正した。

開始: master / dde92b01480f2096167417aba9cacaba677adf18、作業ブランチ: codex/review-and-fix-obs-plugin。
GitHub https://github.com/oz-coden/obs-timestamp-memo-plugin のmasterも開始時点で同じSHA。
combined statusに登録がなく、GitHub CI成功とは判断していない。push/PR作成は行っていない。
既存のignore対象ビルド補助、依存、ビルドディレクトリは維持した。

構造はC++20 / Qt6 Widgets / OBS libobs・frontend API。
obs_module_load → Controller/Bridge → frontend録画イベント・5ホットキー →
RecordingSession・操作ジャーナル → Qt model/view → 8エクスポーター。
buildspecはOBS 31.1.1、obs-deps/Qt6 2025-07-11を指定。
QSettingsでプラグイン設定、OBS save callbackでホットキー、OBS本体でDock配置を管理する。
プラグインはscene/sourceを保持せず、native windowを埋め込まない。

## 発見した問題と修正

詳細な発生条件、修正前のコードと推奨方法は修正前レビューのR01–R15を参照。
次の表のファイルはsrc配下。R16–R18は修正後の検証・追加レビューで見つかった。

| ID | 重要度・確度 | 原因（ファイル・関数） | 修正と検証 |
|---|---|---|---|
| R01 | High・潜在的UAF | ui/dock-widget.cpp / onContextMenuRequested。marker参照をQMenu::execをまたいで使用 | 値をcopyし、session IDで古い変更操作を拒否。メニュー中にclear+40件追加する試験で元のTCを安全にcopy。 |
| R02 | High・確認済みAPI契約違反 | plugin-main.cpp / obs_module_unload、obs/obs-bridge.cpp / shutdown。EXIT後のfrontend呼出し、登録失敗未処理 | EXIT内でcallback/hotkey/output/Dockを解除し冪等化。main/null/UI thread、Dock登録失敗を確認。EXIT後frontend呼出し0、参照/登録残り0、失敗rollbackをstub試験。 |
| R03 | High・確認済み | core/recording-session.cpp / load_from_json、recover_from_cache。num/den<=1000 | 有理FPSの値と比率を検証し30000/1001等を保持。保存/読込/復旧とTCを試験。 |
| R04 | High・確認済み | obs/obs-bridge.cpp、core/session-controller.cpp / check_recording_file_changed。古いsettings pathをpoll、分割後total_frames未補正 | muxerのfile_changed(next_file)を監視しUIへqueue。境界フレームを引いて区間時刻を算出。stop直前のpending splitを先に処理。worker split/連続区間/直後stopを試験。 |
| R05 | High・確認済み | core/recording-session.cpp / start_session、set_video_path。Truncate、path空、I/O無視 | UUID付きNewOnly cache、path未確定時のOBS設定領域cache、path更新操作ログ、書込失敗通知。cache衝突/遅延path/書込不能を試験。停電保証というREADME表現を修正。 |
| R06 | High・確認済み | core/recording-session.cpp / load_from_json、save_to_json。読込元を保持せず動画名から保存先を再計算 | source JSON pathを保持し、そのファイルへ編集保存。最低限の構造を状態変更前に検証。別名JSONだけが更新され、不正JSONで現セッションが変わらないことを試験。 |
| R07 | Medium・確認済み | core/session-controller.cpp / update_marker_type、RecordingSession::update_marker。本体type未更新 | Session/JSONL/復旧へtype_indexを含める。再読込とcache復旧後の種別を試験。 |
| R08 | Medium・確認済み | core/session-controller.cpp / initialize、shutdown。Qt接続とqueued処理が再初期化後に残る | disconnect、pending処理の排出/削除、停止保存、通知の明示初期化/停止。再loadでも1 hotkey=1 markerを試験。 |
| R09 | Medium・確認済み | RecordingSession::stop_session、SessionController::perform_auto_export、PluginConfig::save | 自動JSON OFFを尊重し健康なjournalを保持。保存失敗を通知し成功と誤表示しない。色を検証、QSettings statusを返す。OFF/cache/失敗exportを試験。 |
| R10 | Medium・潜在的対象取り違え | ui/export-dialog.hpp / session_。live sessionへの参照 | ダイアログ開始時のRecordingSession snapshotを保持。元sessionをclearしても元の章をcopyできることを試験。 |
| R11 | Medium・確認済み | ui/dock-widget.cpp / constructor、onMarkerAdded、context menu、keyPressEvent | 初期表同期、session FPSでTC表示/copy、Delete/Backspaceを表focusに限定。履歴FPSと表行数を試験、実際のキー伝播は実OBS手順に含めた。 |
| R12 | Medium・確認済み | ui/dock-widget.hpp / DockWidget : QDockWidget、plugin-main.cpp / dock登録 | 内容をQWidgetに変更、OBSが作る外側Dockを利用。focus hotkeyは外側を表示。stub UIで内側QDockWidgetが0個であることを試験。 |
| R13 | Low・確認済み | core/timecode-helper.cpp / smpte_to_frame_index。scanf項目不足、範囲/DF解釈 | from_charsで全項目/範囲/区切り/DFの禁止番号を検証。DF/NDF境界、round-trip、不正入力を試験。通常UIからの呼出しはない。 |
| R14 | Low・形式不正の可能性 | exporters/srt、vtt、youtube、xml | 時刻順に安定ソート、字幕改行/特殊文字処理、同秒の章を集約、XML禁止制御文字を除去。8形式と特殊文字を試験。NLE互換は実機確認が必要。 |
| R15 | Improvement・確認済み | CMakeLists.txt、README | 必須のQt/frontend OFFをconfigure時に拒否。ビルド/テスト/実OBS手順を追加。無関係な大規模format/renameは実施しない。 |
| R16 | Low・確認済み | exporters/csv-exporter.cpp、srt-exporter.cpp。QTextStreamへBOMのraw char列を流す | Qtによる文字変換で正しいEF BB BFにならない。setGenerateByteOrderMark(true)に変更し先頭3byteを試験。 |
| R17 | High・確認済み設計不整合 | obs/obs-bridge.cpp / get_current_frame_rate、get_video_dimension。global videoだけ参照 | 録画encoderのFPS divisorを有理FPSに反映し、encoderの出力寸法を使用。divisor=2で30000/1001、30frames=1001ms、1280x720を試験。 |
| R18 | High・確認済み | core/recording-session.cpp / recover_from_cache。不正JSON行を無条件skip | 完結行のJSON破損を拒否して現状態を保持。クラッシュ由来の改行なし最終不完了行だけ許容。両ケースを試験。 |

marker-table-model.cppもdataChanged等のemit前にmarkerをcopyし、直接接続slotでmodelがresetされても
参照を使い続けないようにした。再入slotでclearする回帰試験を追加した。

## 「ウィンドウの中にウィンドウ」の原因

OBS 31.1.1 frontend/OBSStudioAPI.cppはobs_frontend_add_dock_by_idに渡されたQWidgetを、
新しく生成したOBSDockのsetWidgetへ渡す。プラグイン自身もQDockWidgetを生成したため、
タイトル/close/float枠が二段になっていた。native windowの埋込みではなく不要なDockの入れ子だった。

修正前:

    OBSBasic QMainWindow
      └─ OBS所有のOBSDock
          └─ プラグインDockWidget : QDockWidget
              └─ container QWidget → QVBoxLayout → 各操作部品

修正後:

    OBSBasic QMainWindow
      └─ OBS所有のOBSDock
          └─ プラグインDockWidget : QWidget
              └─ QVBoxLayout → status/ボタン/メモ/表/設定・export操作

SettingsDialog/ExportDialogは通常のQDialogとして維持した。外側Dockの所有、浮動、配置保存、
ドックメニューはOBSが担当する。プラグイン側でsetCentralWidget/addDockWidget/setParent/
window flags/native handleを追加する必要はない。
[OBS Frontend API](https://docs.obsproject.com/reference-frontend-api)のDockとEXITの契約を、
固定依存のOBS 31.1.1ソースでも照合した。

## 機能レビュー（修正後）

| 機能 | 操作 → 内部/OBS → 保存 → 次回利用 | 結果・制限 |
|---|---|---|
| 4クイックマーカー/Enter/5 hotkey | Qt/OBS hotkey → Controller → output frame数とencoder FPS → Session → journal → 表/通知 | 停止時は拒否、pause時は同時刻。繰り返し/reloadはstub試験済み。実hotkey割当/復元は未確認。 |
| 編集/種別/削除/全消去 | model/menu → Controller → Session操作ログ → 停止後は読込元JSONへ原子的保存 | 種別・label/comment・削除を保持。メニュー中のsession変更はguard。失敗時は警告。 |
| 設定 | SettingsDialog → QSettings sync/status → configChanged → 操作部品更新 | ラベル/色/自動export/通知を保存。実OBS再起動時の確認手順あり。 |
| 録画開始/停止/pause/split | frontend/output callback → UI queue → Session開始/区間offset/確定 → JSON・任意export | 分割settings pollingを廃止。JSON OFFはjournal保持。実muxerの境界精度は未確認。 |
| Open/Recover | ファイルdialog → 検証/一時Session → model reset → 編集/再export | 正常なfractional FPSを保持。不正JSON/完結cache破損時は現在の状態を保持。次回起動は手動Open/Recover。 |
| 8形式export/clipboard | dialog時点のsnapshot → Registry → exporter → QSaveFile commit/clipboard | 形式生成と失敗returnを試験。CSV/SRT BOM、字幕escape等を修正。NLE側の取り込みは未確認。 |
| シーン/ソース変更 | scene/sourceを直接保持しない。hotkey saveはOBS collection callback | source削除によるdangling objectは見つからず。collection変更時のhotkey復元は実機確認。 |
| unload/OBS終了 | EXIT内でbridge解除 → controller保存/停止 → OBS Dock解除 → 冪等unload | stubでは登録/参照残りとEXIT後frontend呼出し0。実OBS終了ログは未確認。 |

## 変更ファイルと理由

| ファイル | 理由 |
|---|---|
| src/core/recording-session.hpp・cpp | snapshot、固有journal、保存先、JSON選択、FPS、type/復旧検証 |
| src/core/session-controller.hpp・cpp | 分割offset、保存エラー、Qt接続解除、pending split排出、JSON設定 |
| src/core/plugin-config.hpp・cpp、src/ui/settings-dialog.cpp | QSettings失敗と不正色 |
| src/core/timecode-helper.cpp | DF/NDF parseの検証 |
| src/obs/obs-bridge.hpp・cpp | output参照/signal、UI queue、EXIT、encoder FPS/寸法 |
| src/obs/status-notifier.hpp・cpp | UI側初期化と終了後通知の抑制 |
| src/plugin-main.cpp | 登録失敗、null/thread検証、EXIT内cleanup、冪等load/unload |
| src/ui/dock-widget.hpp・cpp | 一段Dock、初期同期、menu参照、session guard、focus/キー |
| src/ui/export-dialog.hpp、src/ui/marker-table-model.cpp | snapshotとemit再入安全性 |
| src/exporters/csv・srt・vtt・xml-exporter.cpp、youtube-exporter.hpp・cpp | BOM、順序、改行/escape、同秒章、拡張子 |
| CMakeLists.txt、.gitignore | 必須依存の検証、docs/tests追跡 |
| tests/CMakeLists.txt、regressions.cpp、obs-stubs.hpp・cpp、stubs/*.h、analyzer-compat.hpp、README.md | 全production実装を使う独立した回帰テスト、解析補助と再現手順 |
| README.md、docs/review-before-fixes.md、review-and-validation.md、obs-manual-test.md | ビルド、理由/確度/検証、実OBSチェック手順、停電保証表現の訂正 |

srcは23ファイルを修正。locale、外部依存、既存ローカルビルド補助は変更していない。

## 検証結果と再現方法

- 本体: Windows x64 / MSVC 2022 / RelWithDebInfo、実OBS 31.1.1・Qt6ヘッダーで成功。
  修正途中でclean rebuildを実施し、最後の変更も再ビルド成功。警告をエラーとして扱った。
- 回帰: /W4 /WXで全production C++実装をOBS stubとQt6でコンパイル成功。
  CTest 1/1成功（6グループ、最終0.22秒）。tests/README.mdに再現手順。
- 静的解析: 全21個のsrc/*.cppをclang-tidyのcore/cplusplus/deadcode analyzerで確認。
  最後の変更があった6ソースも再解析成功。残ったlayout所有権の警告1件はQt parent所有の
  QVBoxLayoutで、QPointerによるダイアログ破棄試験で解放を確認したためfalse positiveと判断。
  Windows clang/OBS intrinsic用に解析専用宣言を使用し、本体へは組み込んでいない。
- git diff --check、ステージ済み差分のレビューを実施。不要な生成物やユーザー設定は追跡しない。
- テスト初期実行で存在しないoffscreenを選び、ユーザー画面にQt platformエラーが出た。
  同梱されているminimalへ変更し、その後のCTestは正常終了した。

本体ビルド（CMakeがPATHにないこの環境ではVisual Studio同梱cmake.exeを使用）:

    cmake --build build_x64 --config RelWithDebInfo --parallel -- /p:TreatWarningAsError=true
    cmake --build build_tests --config RelWithDebInfo --parallel
    ctest --test-dir build_tests -C RelWithDebInfo --output-on-failure

成果物: build_x64/rundir/RelWithDebInfo/obs-timestamp-memo.dll。
OBSへのDLL置換や既存OBS設定への変更は実施していない。

## 未確認・今回の範囲外

実OBSの描画/キー伝播/浮動Dock・再起動/終了、hotkey collection復元、実encoderの遅延/drop、
正確なsplit境界との一致、既に分割済みの録画途中でのreload、Mac/Linux、
最新OBS互換、NLE importは未確認。[実OBS確認手順](obs-manual-test.md)で検証できる。
stub試験を実OBS動作確認の代わりとしない。

flushはfsync/停電時保証ではない。cacheと最終保存先がともに書込不能なら、メモリ上の
マーカーを次の録画前に別の書込可能先へExportする必要がある。
翻訳全体の追加、未使用のkeep_tmp_cache_on_crash設定整理、NLE特有の形式拡張は範囲外。
好みによる大規模rename/formatやmutex設計全面変更は行っていない。

## Gitによる追跡

1. bb8138c — docs: record full plugin review before fixes（ソース変更前の調査記録）
2. 1305bc2 — fix: preserve marker data and recording lifecycle（データ・録画・終了・形式）
3. 8085d53 — fix: use a single OBS dock and stable UI snapshots（UI構造・参照）
4. test: add plugin regression coverage and review documentation（検証コード・ビルド検証・本報告）

最終状態: git status --short --branchは作業ブランチ名のみで、追跡対象の未コミット変更なし。
全差分はgit diff dde92b0..HEAD --stat、コミット番号はgit log -4 --onelineで再確認できる。
履歴の書換え、reset/stash、強制pushを行っていない。
