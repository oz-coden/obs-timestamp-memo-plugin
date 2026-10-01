# 設計レビューとリファクタリング（2026-10-02）

対象は codex/review-and-fix-obs-plugin の d973fca 以降。前回のバグ修正と単一ドック化を維持し、責務・状態の所有・検証方法を見直した。
ユーザーからは、変更前の実OBSでドック表示、録画中の打刻、任意文字列の入力・保存、Export関連が確認済みと報告された。今回の変更後の実OBS操作は未実施であり、以下のローカル検証と区別する。

## 旧構造の問題と対応

|分類|旧構造の問題|変更と理由|
|---|---|---|
|設計|RecordingSessionがマーカー、録画状態、時刻、JSON、ファイル、ジャーナルを所有|純粋な文書モデル、録画状態機械、codec、storeへ分離。ファイル失敗とドメイン操作を独立してテストできる|
|設計|Controller、OBS bridge、設定、通知、export登録がsingleton経由で暗黙的につながる|PluginRuntimeがサービスを所有しコンストラクターで渡す。寿命と依存関係を構築箇所で確認できる|
|設計|UIのタイマーが録画ファイルの確定処理を進める|Controllerがタイマーを所有。ドック非表示でもセッション処理が進む|
|状態|表モデルが先に編集し、Controllerの状態更新とは別に成功を表示|表は編集要求だけ送る。文書更新後のController通知で表示を更新する|
|状態|読み込み元や録画状態、表示タイトルが別々の場所で保持される|Controllerが文書、Timeline、履歴元を所有しタイトルを導出する|
|UX|コピーとファイルexport成功で毎回OKが必要|成功モーダルを削除。コピーは無通知、ファイル保存はExport画面を閉じる。失敗表示は維持|
|UX|Export/SettingsのexecがOBS操作を阻む|それぞれ一つのモデルレス画面。連打で増殖しない。Exportは対象文書のsnapshotとタイトルを保持|
|安全性|設定書込み結果に関係なくライブ値を更新できる|sync成功後だけ設定を公開・通知。失敗時は以前の値を維持して警告|
|安全性|UI破棄がホストのドック削除方式に依存する|Controller停止、ドック解除、残るビュー破棄、サービス破棄の順を明示。遅延削除するホストも回帰テスト|

全面書き直しは行わず、export形式ごとの既存生成処理とOBSイベントの境界処理を残した。インターフェースはOBSを置き換えるRecordingGatewayに限定し、保存・設定・exportは具体クラスの注入で十分と判断した。

## 主要コンポーネントと依存方向

```text
plugin-main → PluginRuntime（構築・寿命）
  ├─ UI → SessionController → coreの文書・Timeline
  │                         → SessionStore → SessionCodec → core
  │                         → PluginConfig → PluginSettings
  │                         → ExporterRegistry → 各exporter → core
  │                         → RecordingGateway ← ObsBridge → OBS API
  └─ StatusNotifier ← Controllerの通知（OBSログ / Qt status bar）
```

|場所|責務・所有する状態|
|---|---|
|src/core|Qt/OBS非依存。RecordingSessionは文書とID発行、MemoMarkerは値、RecordingTimelineはIdle/Recording/Pausedと区間原点、TimecodeHelperはFPS・時刻変換、PluginSettingsは設定値|
|src/application/session-controller.*|操作の入口。唯一の編集対象文書、Timeline、履歴元、ファイル確定タイマーを所有。イベント→状態遷移→保存→表示通知の順を決める|
|src/application/recording-gateway.hpp|録画snapshot、イベント、ホットキー要求の契約。Qt信号は使用するがOBSヘッダーには依存しない|
|src/application/plugin-runtime.*|OBS adapter、設定repository、export登録、store、controller、通知adapterを明示的な寿命で構築するcomposition root|
|src/persistence|SessionCodecはJSON変換、SessionStoreはファイル・ジャーナル・保存元と復旧。PluginConfigはQSettingsによる読書きと公開済み設定値|
|src/exporters|文書snapshotから各形式の内容を生成して保存。Clipboard用テキストも同じ生成器を利用|
|src/obs|ObsBridgeだけが録画・output参照・ホットキー・frontend callbackにアクセス。StatusNotifierは通知先のログとstatus barを担当|
|src/ui|表示用の表projection、入力、選択、ダイアログの寿命とexport snapshot。OBS APIを呼ばずControllerに操作を依頼する|
|src/plugin-main.cpp|OBS moduleエントリポイントと一つのruntime/root widgetだけを保持する|

UIの表は編集対象の第二の所有者ではなくprojection。Exportのコピーは、録画開始・分割で元文書が切り替わっても表示中の出力対象を固定する意図的なsnapshotである。
SessionStoreの保存元は物理ファイルの責務、Controllerの履歴元は表示・操作文脈の責務であり、録画開始や履歴読み込み時にまとめて切り替える。

## データフローとスレッド

1. OBS frontend callback / outputのfile_changed / hotkeyをObsBridgeが受ける。output参照と世代番号で解除後のイベントを無効化する。
2. worker threadの通知はQtキューでUI threadへ渡す。打刻と停止前にpending splitを排出し、新しい区間に正しく所属させる。
3. ControllerがRecordingTimelineを遷移させ、必要なら旧文書を保存・exportして新しい文書を作る。逆方向の分割原点は拒否する。FPSは文書metadataを使用する。
4. UI編集はControllerのコマンドに渡る。文書の変更後にジャーナルを追記、停止中の履歴は保存し、表へ結果を通知する。
5. JSONロード・復旧は候補文書の検証が成功した後に置き換える。失敗で現在の表を消さない。復旧は最後の未完了行のみ許容し、完結した不正行や不正操作は拒否する。

Controller / Store / Config / UIはUI threadに閉じ込め、共有mutexで各状態を別々に保護する設計を避けた。純粋モデルは値として使い、同一の可変文書を複数threadから扱う契約にはしない。
Coreが日時やOBSを取得することはなく、作成日時・録画frame・metadataは呼び出し側が渡す。
終了時はイベントを排出して保存し、callbackを解除してqueued呼出しを除去した後、UI、サービスの順で破棄する。二重終了も安全に扱う。

## 保存互換性とUI

JSONのキー・schema 1.0.0、既存の設定organization/application名、OBSホットキー名を維持した。整数・分数FPSを保持し、timecodeはロード時に再計算する。
QSettingsは形式を明示的に渡す。通常は既存のnative形式、テストでは一時ディレクトリのINIを使う。Qtのorganization/applicationのみのconstructorはdefaultFormatを使用しないため、明示形式がテストのregistry隔離に必要だった（[Qt公式資料](https://doc.qt.io/qt-6/qsettings.html#defaultFormat)）。
保存はQSaveFileで確定し、失敗時には復旧cacheを保持する。JSON自動保存OFFでもジャーナルを保持する。最後のセッションを起動時に自動ロードする機能は追加していない。

ウィンドウ階層は OBS QMainWindow → OBS所有の外側Dock → プラグインQWidget → 操作部品。前回取り除いた内側QDockWidgetを再導入しない。
Export/Settingsは必要時に作る独立したモデルレスQDialogで、DockのQObject子として寿命を管理する。閉じたら削除し、開いている間は再利用する。
Exportを再度押した時に対象sessionが変わっていれば新しいsnapshotへ切り替える。開いているExportの内容は分割中にも勝手に切り替えない。
停止中の打刻、空文書のExport/全削除は無効にする。全削除・設定リセット・ファイル上書き・失敗の判断は残す。
確認画面やcontext menu中のセッション切替・アンロードはQPointerとsession IDで保護し、旧操作が新しい録画へ作用しないようにする。

## 検証結果

- 実OBS 31.1.1ヘッダーとQt 6によるWindows x64プラグインビルド成功。MSVCの警告をエラーとして検証。
- CTest 3/3成功。domain_testsはQt/OBS非依存、application_testsはQt Core/GuiとFakeGateway（OBS/Widgets不要）、regressionsはQt WidgetsとOBS stub。
- ENABLE_INTEGRATION_TESTS=OFFの別ビルドでQtなしのdomain_tests 1/1成功。
- 全25実装ファイルにclang-tidyのcore/cplusplus/deadcode analyzerを実施。export-dialogのQt所有layoutに既知のpotential leak誤検出が一件残る。親による破棄とビュー寿命は回帰テストで検証した。実リークと判断する根拠はない。
- configure時のOBS側FindDetours版表記とVirtualCamera GUID未設定の警告は残る。プラグインのコンパイル失敗ではない。
- git diff --check成功。実OBSにDLLを設置した後の確認は別途必要（[手順](obs-manual-test.md)）。

追加した振る舞いテスト: 同一frameの保持、分数FPS、録画状態遷移、遅延path、逆分割拒否、履歴の保存先、codec/recovery、設定失敗時の状態保持、編集要求から文書・表への反映、重複初期化、成功Clipboardにmodalがないこと、モデルレス画面の一意性とアンロード時破棄。
既存の8形式export、UTF-8 BOM、context menu再入、worker分割・停止、callback解除、登録失敗rollbackも維持した。

## Gitと残る負債

- 7ce6c3d: 文書・保存・録画状態の分離と純粋ロジックのテスト。
- 8f835b1: サービスの明示所有、gateway、UI操作と設定処理、アプリケーションテスト。
- 文書コミット: 本設計記録、テスト説明、実機確認手順。

変更前の作業treeはclean。未追跡・ignoredのローカルビルド設定や依存物を変更対象に含めず、履歴書換えはしない。master統合前・push前にorigin/masterが開始時のdde92b01480f2096167417aba9cacaba677adf18から未知の変更を受けていないことを確認する。

残る技術的負債: exporterはテキスト共通処理以外にQtファイル/JSON依存があり、全形式を純粋C++へ移す価値は今回限定的と判断した。Controllerは保存・autoexportの順序調整も担当し、今後別の利用者が増えれば専用service抽出を検討できる。表示文言のローカライズは不完全。実encoderのdrop/分割境界、録画途中の再ロード、別OS、新OBS、NLE互換はstubで保証できない。
