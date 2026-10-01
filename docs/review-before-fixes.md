# 修正前レビュー（2026-10-02）

## 対象・Git・調査方法

- 開始ブランチ: `master`、HEAD: `dde92b01480f2096167417aba9cacaba677adf18`。
- `git status --short --branch`: `## master...origin/master`。追跡ファイルの未コミット変更なし。
- GitHub connectorと `git ls-remote origin refs/heads/master` でGitHubのmasterも同一SHAと確認。最新コミットのcombined statusにstatus登録なし（CI成功の証拠ではない）。
- 最近の履歴: README改訂、VTT/YouTube/Markdown追加、Controller分離、再帰mutex導入、Unicodeパス/原子的保存、Dock解除/出力パス修正。過去の対策が現行コードでも十分か再点検した。
- 未追跡のローカルビルド補助 `build-local.ps1` / `CMakeUserPresets.json`、`.deps`、`build_x64`、`compile_commands.json` はignore対象。既存ファイルを破棄しない。
- 全 `src` の実装/ヘッダー、README、locale、CMake・各OS補助・GitHub Actionsを確認。OBS 31.1.1のローカルソースを照合。
- 修正前: 既存 `build_x64` をMSVC 2022でRelWithDebInfoビルド成功。これは増分ビルドであり、全警告の検査は修正後に行う。

## 構造・処理の流れ

言語はC++20、UIはQt6 Core/Gui/Widgets（AUTOMOC）、OBS libobs/obs-frontend-apiを使用。依存はbuildspecのOBS 31.1.1とobs-deps/Qt6 2025-07-11。独自source/filterの登録、シーン/ソース参照の保持、native window埋込みはない。

`obs_module_load` → Controller singleton初期化 → ObsBridgeがfrontendイベント/save callback/5個のhotkeyを登録 → UIをDockとして登録。録画イベントはController → RecordingSession → UIのsignal/slot。打刻はUIまたはhotkey → Controller → OBS出力フレーム数 → MemoMarker → JSONL journal → 表/通知。停止はSession JSON確定 → 任意の自動エクスポート。履歴/復旧はJSON/JSONLをSessionへ読み、表をreset。各エクスポーターはQSaveFileで原子的に保存する。

PluginConfigはQSettings（`oz-coden` / `obs-timestamp-memo`）で4スロット/自動export/通知設定を保存。Windowsではレジストリ、他OSではQt標準設定領域。hotkeyはOBSシーンコレクションのsave callbackで保存/復元。UI状態のDock配置はOBSが管理。停止後の表は次の録画でリセットされ、次回OBS起動時に最後のセッションを自動ロードする仕様はない。

CMakePresetsはWindows/macOS/Linux、Actionsはビルド/パッケージ/format。READMEからビルド説明が消えている。ローカルは `build-local.ps1`（存在する環境）またはCMake presetでビルドする。ENABLE_QT/ENABLE_FRONTEND_APIはOFFを指定できるが全実装が両方に依存する。

## UI階層（確認済み）

`src/plugin-main.cpp::obs_module_load` が `new DockWidget(main_win)` を `obs_frontend_add_dock_by_id` に渡す。`DockWidget : QDockWidget`、`setup_ui` は `new QWidget(this)` を `setWidget(container)` する。OBS 31.1.1 `frontend/OBSStudioAPI.cpp::obs_frontend_add_dock_by_id` はさらに `new OBSDock(main)` → `dock->setWidget(widget)` → `main->AddDockWidget` を実行。

```text
OBSBasic (QMainWindow)
└─ OBSDock (OBSが生成、floating可)
   └─ DockWidget (プラグインのQDockWidget、内側のタイトル/操作枠)
      └─ QWidget container / QVBoxLayout
         ├─ QFrame status / QLabel
         ├─ quick buttons / memo input
         ├─ QTableView + MarkerTableModel
         └─ open/export/clear/settings buttons
```

SettingsDialog/ExportDialogはDockWidgetをparentとしたスタック上のQDialogで `exec()` する通常の独立したモーダルダイアログ。QMainWindow生成、setCentralWidget、setParent、native handle埋込み、手動window flagsはプラグインにない。二重Dockは不要。Dock内容をQWidgetにし、外側のDock所有・floating・close・OBSメニュー/配置復元をOBSに任せるのが自然。

## 問題一覧（この文書作成時点でソース未修正）

| ID | 重要度 / 確度 | ファイル・関数 / 問題となるコード | 現在の動作・条件・理由 / 推奨修正 |
|---|---|---|---|
| R01 | High / 潜在的障害 | `ui/dock-widget.cpp::onContextMenuRequested`: `const auto &marker = markers[row]`; `QMenu::exec`後も参照を使用 | メニューのnested event loop中に打刻でvector再配置、clear/録画開始/分割で削除されるとdangling reference。markerを値で保持。 |
| R02 | High / 確認済みAPI契約違反 | `plugin-main.cpp::obs_module_unload`: frontend remove、`ObsBridge::shutdown` | OBSのEXIT後はfrontend API呼出し禁止。現在EXITを扱わずunloadで呼ぶ。EXIT内で解除/保存/Dock破棄し、unloadは冪等cleanup。Dock追加の戻り値/親nullも確認する。 |
| R03 | High / 確認済み | `core/recording-session.cpp::load_from_json`, `recover_from_cache`: `num <= 1000`, `den <= 1000` | 30000/1001・60000/1001・24000/1001が60/1へ変わり、再exportや表示のFPSが壊れる。有効な有理FPSを同じ検証で保持。 |
| R04 | High / 確認済み | `obs/obs-bridge.cpp::get_current_record_file_path`, `core/session-controller.cpp::check_recording_file_changed` | OBS muxerは内部pathを分割時に変更して `file_changed(next_file)` を発火するがoutput settingsのpathは更新しない。分割検出が動かない。さらにtotal_framesは分割でresetされず、分割後も全録画の時刻。output signalを登録/解除し、境界のフレーム数を保存して区間の時刻を計算。 |
| R05 | High / 確認済み | `recording-session.cpp::start_session`, `set_video_path`: journal `Truncate`; path空でcacheなし | 同じ動画stemの残存復旧cacheを上書き。path遅延中は打刻が永続化されず、journal失敗もtrue。session固有cache、path未確定時のfallback、I/O失敗通知、path更新journalを導入。flushは停電時のfsync保証ではないためREADMEも修正。 |
| R06 | High / 確認済み | `recording-session.cpp::load_from_json` / `save_to_json` | 読み込んだJSONのファイル名を保持せず、編集時に動画metadataから再計算したJSONへ保存。別名export/移動済みJSONの編集で別ファイルを上書きし得る。読み込み元を保持し、構造不正のJSONを状態変更前に拒否。 |
| R07 | Medium / 確認済み | `SessionController::update_marker_type`; `RecordingSession::update_marker` | type_indexをUI用copyだけで変更。Sessionとjournalは変更せず、JSON再読込で元のtypeに戻る。typeも本体更新/操作ログ/復旧へ含める。 |
| R08 | Medium / 確認済み | `SessionController::shutdown` / `initialize` | bridge callback解除のみでQt接続を解除しない。再初期化でslot/hotkey lambdaが増え、1操作が複数回処理される。disconnectとpending queued eventの除去、稼働中セッションの確定を追加。 |
| R09 | Medium / 確認済み | `RecordingSession::stop_session`, `SessionController::perform_auto_export` | 設定 `auto_export.json` は読み書きするが停止時に無条件JSON保存。OFFが無効。OFFならJSONLを保持し、ON時のみJSON確定/成功後cache削除。編集保存失敗/QSettings失敗も利用者へ通知。 |
| R10 | Medium / 潜在的障害 | `ui/export-dialog.hpp`: `const RecordingSession &session_` | export/file dialogのnested loop中に新録画/分割が起きると、ユーザーが開いたセッションとexport対象が入れ替わる。ダイアログ開始時にimmutableなcopyを保持。 |
| R11 | Medium / 確認済み | `DockWidget::DockWidget`, `onMarkerAdded`, `onContextMenuRequested` | controllerはDock生成前にセッション開始できるが表の初期snapshotがない。履歴copy timecodeも現OBS FPSを使う。表初期同期/セッションFPS使用。親全体のDelete/Backspace処理は意図しない削除を招くため表focusへ限定。 |
| R12 | Medium / 確認済み | `DockWidget : QDockWidget`, `obs_module_load` | 二重Dock、内側close/float枠。上記UI階層参照。QWidgetの内容に変更。 |
| R13 | Low / 確認済み | `timecode-helper.cpp::smpte_to_frame_index` | 5項目のscanfを `< 4` で判定、分/秒/frame範囲未検証、29.97fpsでもcolon NDFをDF解釈。現在通常UI経路からは未使用。separator/全項目/DF禁止番号を検証しround-trip修正。 |
| R14 | Low / 潜在的形式不正 | `srt-exporter.cpp`, `vtt-exporter.cpp`, `youtube-exporter.cpp` | 読み込んだmarker順が非時刻順で字幕/章順も乱れる。VTT label/commentの`<`, `&`, 改行でcueが壊れる。安定ソートとVTT payload escape/空行正規化、章の同秒重複集約。 |
| R15 | Improvement / 確認済み | `CMakeLists.txt`, README | 実行不能な依存OFF設定を早期エラーにする。ビルド/検証手順、回帰テストを追加。format無関係変更は避ける。 |

## 機能別追跡とその他の結果

- 4スロット/メモ: UIクリック/EnterまたはOBS hotkey → Controller → recording有効性確認 → FPS/フレーム → Session add → journal → signal/table/ステータス通知。停止中は拒否、一時停止中は同時刻でpaused markerを作る。R04/R05/R08に影響される。
- ラベル/コメント編集: Qt model setData → Controller update → Session/journal、停止後はJSON自動保存。R06/R07および保存失敗無視に影響。modelからemit中にslotがモデルを更新するのでemit後のmarker参照使用も避ける。
- 削除/clear/種別変更: Controller経由、本体とjournal操作。typeだけ欠落（R07）。modelはrowとidを両方検証し範囲外を防いでいる。
- 設定: QDialogからQSettingsへsync → configChanged → ボタン更新。4設定数はdefaults/loadで維持され、通常操作で範囲外になる証拠はない。色文字列の外部設定値を検証し、設定保存失敗を確認すべき。
- 8形式export: JSON全情報、CSV引用符escape、EDL1frameマーカー、SRT/VTT cue、YouTube章、Markdown表、FCP7 XML marker。QSaveFile commit/書込status確認、Qt所有権は基本的に適切。CSV/NLEの具体的取込互換、XML schema/制御文字、EDL event数制限はNLEで追加検証が必要。
- 録画開始/停止/分割: R03–R09参照。停止前の未解決pathは再取得する。APIのreturn failureを通知すべき。
- シーン/ソース変更: プラグインは直接source/sceneを保持しないため削除によるdangling OBS objectの証拠なし。hotkeyはシーンコレクション保存/復元の実機確認が必要。
- OBS output/settings/hotkey arrayはrelease対応がある。Qt子widgets/models/timersはparent所有。recursive mutexの既存stop→save→to_json再入は現在deadlockを回避している。
- Qt AutoConnectionは通常hotkey別スレッドからUIスレッドへqueueする。ただし初回StatusNotifier作成が呼出し元thread依存なのでUI側で明示初期化する。file_changedもUIへのqueueとcallback解除が必要。
- 翻訳はlocaleファイルがあるがUI文字列は英語固定。翻訳全体の追加は実際の障害修正から分離する。
- 新OBSとの互換、実際のframe精度（encoder遅延/出力FPS divisor/高負荷drop）、一時停止、split境界、NLE importはOBS実機/別OS検証が必要。コードだけでframe完全一致とは断定しない。

## 修正方針・検証予定

R01/R02/R05/R06のライフタイムとデータ保護を優先し、FPS/分割/種別/設定の明確なバグとUIを修正。独立したQt/OBS stub回帰テストで保存・復旧・分割・再初期化・exportと二重Dockを検証する。MSVC warning-as-errorビルド、clang analyzer（Qtに不適切なwebkitルールを除く）、git diffを確認。OBS実機手順を別文書へ保存する。

参考: [OBS Frontend API](https://docs.obsproject.com/reference-frontend-api)（特にEXIT契約/Dock ownership）、ローカル `.deps/obs-studio-31.1.1/frontend/OBSStudioAPI.cpp` / `widgets/OBSBasic_Docks.cpp` / `utility/BasicOutputHandler.cpp` / `plugins/obs-outputs/mp4-output.c` / `plugins/obs-ffmpeg/obs-ffmpeg-mux.c` / `libobs/obs-output.c`。
