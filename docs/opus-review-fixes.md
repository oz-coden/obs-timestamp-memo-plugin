# Opusレビュー指摘への修正（2026-10-03）

基準: `master` @ `5cde664`、開始時 `origin/master` @ `7050bef`。
作業branch: `codex/fix-opus-review`。H2 / M1はユーザー指定どおり本体仕様を維持し、
M7は設計検討だけとした。既存データの破棄、履歴書換え、force pushは行っていない。

## 項目別結果

| ID | 結果・原因と修正 | テスト・リスク・制約 |
|---|---|---|
| H1 | 修正。packet送出数ではなく、描画video clockをpacket CTS/PTSで録画presentation timeへ校正。pause固定・resumeのgap除外・古いpacketの拒否・有理数FPS換算。 | pure clock、video/outputの差、遅延packet、pause/resume、split、fractional FPS、startup/EXITを検証。実encoder/muxerのframe精度は未測定。raw/video-onlyは開始時刻からの推定。 |
| H2 | EDL/XML等の構造は変更せず。NLE確認済みと誤認するUI/README表現を修正。 | Resolve / Premiere / FCP7の実機importは未検証。CSVの列とformula保護は別途L6の明示指示で変更。 |
| M1 | chaptersの既存出力を維持。種別filter、短い章の削除/統合等を追加しない。 | 元からある同一秒groupingとStart補完も維持。YouTube条件は説明だけ。 |
| M2 | 修正。Info / Warning / Errorを分離し、設定で消せるのはInfoのみ。 | 通知OFFでもWarning/Errorを表示、終了後は通知しない。モーダルを追加していない。 |
| M3 | 修正。controllerの文書に差分commandによるUndo/Redoを導入。追加、削除、全削除、label/comment/type編集。 | ID/順序、Redo分岐、journal復旧、JSON、revision、OFF時保存、上限を検証。128操作・約8MiB、文書境界で初期化。Undo操作1回はtransactional copyで安全性を優先。文字入力自身のUndoも維持。 |
| M4 | 修正。Unsaved各文書に確認付きDiscard。対応cache/copyを削除し、memory上限も解放。 | 未保存別文書と現録画の分離、再起動後の消失、削除失敗時に最新copyが残ることを検証。全storage不能のmemory-onlyデータは終了で失われる。 |
| M5 | 修正。自動非JSON ExportはNewOnlyで名前を予約し、衝突時session UUID suffix/連番を使用。 | 全7形式でforeignファイルのbytes維持・再実行の別名を検証。停止イベント中に確認dialogを出さない。異常終了時に空の予約ファイルが残る可能性はあるが次回上書きしない。 |
| M6 | 修正。OBS module config pathのsettings.iniへ移行。旧QSettingsは初回read-only import、atomic sync成功後にactive設定へ反映。 | 日本語path、各設定、初回/再読込、失敗、別portable rootを検証。plugin globalでprofile/collection非依存。 |
| M7 | 今回未実装。独立timebaseを持つ配信captureを将来追加する案を下記に記載。 | 先行抽象化やstreaming callbackは追加していない。 |
| L1 | 修正。Markdownの動画pathをbasenameに限定。構文文字、pipe/backtick、HTML、LF/CR/CRLFをescape。 | privacy、label/commentの構文文字と各改行をpure testで検証。memo本文にユーザーが直接書いたpathは自動削除しない。 |
| L2 | 修正。OBS localeをQtへ接続するObsTranslatorをruntime所有。UI、通知、hotkey、診断を英語/日本語化。 | 実localeファイルで日本語UI・通知・hotkey・エラー、英語fallback、他pluginのcontext非介入、解除を検証。保存label/commentは翻訳しない。 |
| L3 | 修正。hotkeys.jsonへ5件をglobal保存。旧collection初回loadからmigration、以後global優先。 | collection変更・再初期化・Focus割当・破損ファイル保護・startupのoutput未取得を検証。未読込の旧collectionは移行できない。 |
| L4 | 修正。旧defaultに一致するlabelだけ新defaultへ追従し、custom labelを保持。 | custom/default label、色、Undo/Redoを検証。 |
| L5 | 修正。strict/transactional validationを保ち、marker番号/ID・field・expected/actual、journal行、JSON構文位置を返す。UIで表示。 | 型不正・欠落・schema・frame不一致・破損時の正常文書保持、詳細を検証。切断された末尾journal行の従来復旧仕様は維持。 |
| L6 | 修正。CSV Indexを1..Nへ、永続IDをMarker ID列へ分離。formula起点文字/元のapostropheにはprefixを1つ追加。 | =/+/-/@、空白、apostrophe、欠番ID、元文書非変更を検証。Exportだけの可逆変換。表計算/NLE固有のimport挙動は未検証。 |
| L7 | 修正。自動JSON OFFの停止後編集/Undoは復旧snapshotのみ保存し、恒久JSONを生成しない。明示Save/Exportと区別。 | OFFで停止/編集/Undo/復旧、JSONなし・journal保持、明示Save、ON切替を検証。設定keyは維持し説明を更新。 |
| L8 | コメントのみ修正。FPS要素の1e6上限とms換算係数の1e9上限を区別。 | timecodeの数値挙動は変更していない。H1のns換算は別修正。 |

## H1の選定理由

| 候補 | 評価 |
|---|---|
| output total frames | encoder/interleave後のpacket数。遅延・欠落時に現在の映像時刻にならないため不採用。 |
| 単独のvideo frame数/clock | encoder遅延を避けられるが、録画開始originとpauseの正確なPTS対応が不足。校正後の現在時刻として採用。 |
| 単独のpacket PTS | ファイル時間に対応するが、到着を待つとencoder遅延分だけ過去になる。校正anchorとして採用。 |
| 単独のwall clock | 描画FPS・pause・output開始点の関係が曖昧。通常録画の基準には使わない。 |
| OBS output pause offset | OBS 31.1.1のencoded pauseはencoder側で管理され、output側offsetだけでは不足。eventで固定/再開し、post-resume PTSで再校正。 |

API、CTS/PTSの意味、muxerのrescale/split処理をOBS 31.1.1の公開sourceで確認した。
[詳細と一次資料](recording-time-source.md)。分割通知に境界PTSがないため最新keyframeを
推定境界とする。R6の実frame境界測定は引き続き未解決。録画前のoutput取得は追加していない。
activeなのにoutputを取得できないR5の従来意味も変更していない。

## 状態と永続化

SessionControllerが文書、revision、Undo履歴、未保存文書と録画lifecycleを所有する。
RecordingGatewayはOBS snapshot/eventを渡し、ObsBridgeがOBS参照とpacket clockを所有する。
PresentationClock / MarkerEditHistoryはQt/OBS非依存の値ロジック。
SessionStoreはUI threadに閉じたjournal/atomic JSON/復旧を担当し、PluginConfigは
active設定と移行を担当する。UIは操作をcontrollerへ伝えて表示を更新する。
ExportDialogは操作時のimmutable snapshotを保持し、Exporterは文書を変えない。
ObsTranslatorの寿命はservices/viewsを包み、OBS APIをUI/applicationへ漏らさない。

UndoはUIだけを巻き戻さず、controller文書へ差分を適用してrevisionを増加する。
録画中は逆操作をjournalに追記（挿入row付きで順序を復元）、停止後はJSON ON/OFFの
保存方針へ従う。失敗時はdirty/Unsavedを維持し、Exportの再操作は最新revisionを取得する。
Undo履歴自体をJSONへ保存せず、再起動後は最終marker状態を読み込む。

## 将来の配信対応（M7）

文書モデルやExportは録画ファイルpath以外にはOBSへ依存していない。
将来はrecording / streamingそれぞれのclock anchorとpause/lifecycleをadapterで管理し、
applicationへ「対象capture」とpresentation timeを渡す。両者同時実行や開始時刻差を
単一offsetへ押し込まない。配信にはURL/service metadataと切断/reconnect境界も必要。
その段階でRecordingGateway/SnapshotをCaptureGateway等へ一般化する価値がある。
RecordingSessionは保存文書として独立させたまま、任意のcapture metadataを追加する案が自然。
今回はrecording以外の状態・API・interfaceを先行追加していない。

## 検証と残る課題

- 各論理commitでWindows plugin/test buildとCTest 3/3を実施。
- Qt/OBSなしdomain build 1/1、全追跡C++ clang-format 19.1.1 / CMake gersemi 0.21.0成功。
- clang-tidy全26実装fileのcore/cplusplus/deadcode解析はエラーなし。Qt親layoutの所有を
  認識できない既知のPotential leak 1件のみ。親所有とlifetimeテストがあるため手動deleteを追加しない。
- `git diff --check`、基準masterからの全差分レビューを実施。レビュー後にDiscard失敗時の
  最新copy保持とMarkdownのCR改行を補強し、回帰テストを追加。
- [主要修正CI](https://github.com/oz-coden/obs-timestamp-memo-plugin/actions/runs/37123109540)
  はWindows / macOS / Ubuntuのbuild、3層CTest、formatがすべて成功。
  最終補強・文書commitとmasterは別途CI結果を確認して統合する（Git履歴と最終報告参照）。
- 新DLLはまだ実OBSへ入れていない。旧版の35件の実機検証結果を新版の確認済みとは扱わない。
  [今回の実OBS手順](obs-manual-test.md)を実施する。
- NLE import、raw/video-only origin、pause cutoff、B-frame/lookahead/overload時の照合、
  split実frame境界、native shortcut/clipboard、portable OBSでの移行は実機検証が残る。
- stopped editでの復旧snapshotは全markerを保存する設計。差分journal最適化を先行せず
  安全性と単純さを優先。save失敗が継続する環境は警告と救済UIを利用する。

## コミット

- `b30086e`: 通知severity、custom label、timecodeコメント。
- `1d624a8`: bounded Undo/Redo、Unsaved Discard、JSON OFFの編集保存。
- `7d372af`: 自動Export衝突保護、Markdown privacy/escaping、CSV列/formula。
- `44ac2e0`: OBS configへの設定/global hotkey移行。
- `1bab740`: rendered video / packet PTS校正clock、H1テストと根拠。
- `8fcf96e`: 詳細validationエラー、正常文書保持。
- `ea85d18`: OBS locale / Qt接続、英語・日本語UI。
- `c70ed4e`: Discard失敗保護・Markdown改行・callback内の重複getter削減。

READMEは英語/日本語双方で今回の機能と制約を揃え、Linuxのbin/64bit/data配置、
JSONの最終状態、modeless、Focus Memo、Undo、Unsaved、config/hotkey方式を更新した。
