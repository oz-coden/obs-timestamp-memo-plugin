## 今回の新版で優先する手順（2026-10-03）

今回のH1/M2〜M6/L1〜L8修正はローカル・stub・CIで検証するが、実OBS確認はまだ。
以下を最新手順とし、後半の旧版検証記録を新版の確認済みとは扱わない。
設定と既存録画をバックアップし、テスト録画用folderで実施する。
[時刻sourceの測定手順](recording-time-source.md)、[修正と制約](opus-review-fixes.md)も参照。

1. OBS停止中にDLLとdata/localeを同じ新版へ更新する。起動を2回行い、起動直後に
   crashしないこと。Dockは1つ、float/re-dock/close/再表示が正常であること。
2. OBSの英語/日本語を切り替えて再起動し、UI、設定、警告、hotkey説明の表示を確認。
   既存markerのlabel/commentは言語変更で書き換わらないこと。
3. 初回移行前後の4slot/color/auto export設定を比較。settings.iniとhotkeys.jsonがOBSの
   plugin_config内に生成されること。portable版ではportable configへ保存されること。
   5hotkeysを設定し、scene collection/profile変更・再起動後も保持すること。
4. 停止中の打刻は失敗通知、memoは消えないこと。録画開始直後の校正待ちwarningを確認し、
   続いて4button、hotkey、memo Enterで打刻。Focus Memo hotkeyが入力欄を表示・focusすること。
5. 通常通知OFFでも保存失敗・打刻失敗がstatus barへ表示されること。成功のOK画面は不要。
6. label/comment編集、custom labelでtype変更、削除/全削除、Undo/Redoを順に実施。
   操作ごとの表、JSON、Export、再読み込みの内容を比較。表での標準shortcutと
   入力欄の文字Undo/Delete/Backspaceを別々に確認。次録画/別文書で古いUndoが作用しないこと。
7. pause中に打刻し時刻が固定されること。resume後にgapが含まれないこと。
   B-frame/lookahead有無、60/1と60000/1001、encoder負荷を変えて焼き込みcounterと
   保存動画のPTSを照合する。H1は推定精度の実測、R6のsplit frame誤差は未解決として記録する。
8. 短時間でsplitし、その直前/直後に打刻する。各segment内の位置とfirst PTSを比較。
   split直後stop、dock非表示、メニュー/Exportを開いた状態でも混ざらないこと。
9. modeless Export/Settingsを繰り返し開き、重複しないこと。Exportを開いた後に同一文書を
   編集してDockのExportを再度押し、Clipboardに最新内容が出ること。file dialog中の
   編集/splitではその操作開始時のsnapshotを保存すること。Copy成功にモーダルはないこと。
10. 全8形式を保存。Markdownの絶対path非表示、pipe/backtick/改行の表示、CSVのIndex/IDと
    formula文字の文字列化を確認。EDL/XMLのNLE importは将来実機検証として別記録し、互換済みとしない。
11. 自動JSON OFFでstop後にcomment編集・Undoし、恒久JSONができずjournalが更新されること。
    UnsavedのJSON別名保存を行い、救済先とcache整理を確認。明示Exportだけではcacheを消さないこと。
12. テストfolderに動画同名のforeign CSV/EDL/XML/SRT/VTT/MD/chaptersを事前に置き、
    auto export後もbytesが同じでUUID suffixの別名ができること。既存JSONはsession ID一致時のみ更新。
13. 書込不能のsidecar/設定cache先で録画・打刻・split・stop・次録画を試す。
    警告とUnsaved一覧で古い文書が残ること。保存可能な場所へのSave JSON as / Exportで救済。
    対象名を確認してDiscard、取消、再起動後に破棄文書が戻らないこと。削除失敗では最新copyを保持。
    全storage不能のデータはmemory-onlyなので、そのテスト中に救済せずOBSを閉じない。
14. 古いcache、異sessionの同名JSON、不正JSONを使い、無断上書きせず別名保存できること。
    JSONの1markerを不正にして、marker/field/expected/actualが表示され、表を保持すること。
    正常legacy cache、途中の壊れたjournal、切断末尾をそれぞれ確認。
15. 録画停止、OBS終了、再起動、履歴/復旧を確認。終了時crash、終了後OBS API呼出し、
    多重callbackの兆候がないこと。通常hotkeyのlegacy importは読込collectionのみである点を確認。

以下は旧版で行った検証・旧手順の履歴。

# OBS実機確認手順

OBS 32.2.1起動時のクラッシュに対する修正後は、まず通常起動を2回行い、録画していない起動直後に落ちないことを確認する。原因と回帰テストは[startup-and-ci-fixes.md](startup-and-ci-fixes.md)を参照。

前回の修正後、ユーザーが実OBSでドック表示、録画中の打刻、任意文字列の入力・保存、Export関連を確認した。
2026-10-02の設計変更後はOBS本体で再確認していない。以下は新しいDLLを実OBSへ入れた後に人間が確認する手順。
既存のプラグイン、設定、録画をバックアップし、テスト録画用ディレクトリで実施する。
DLLは build_x64/rundir/RelWithDebInfo/obs-timestamp-memo.dll。
2026-10-02の作業再開時に、Windows build/CTest 3/3を再確認し、実OBS 32.2.1のDLL/PDBを修正版へ更新した。install先とbuild側のhash一致は確認済み。実操作の結果は確認待ち。旧DLL/PDBのbackup場所とhashは[HANDOFF](../HANDOFF.md)を参照。
OBSを終了してから置き換え、同じプラグインが二重にインストールされていないことを確認する。

1. OBSを起動し、ドックメニューからTimestamp Memoを表示する。
   タイトルバーとclose/float操作が外側に一組だけあることを確認する。
   ドラッグしてフローティング、再ドック、閉じる、メニューから再表示を行う。
   ウィンドウ階層は OBS QMainWindow → OBS所有のOBSDock → プラグインQWidget → layout/操作部品。
2. Settingsで4スロットのラベル・色、自動保存形式を変更する。
   OBSで5つのホットキーを設定し、OBSを再起動する。
   設定・ホットキー・ドック配置の復元を確認する。最後の録画セッションの自動復元は実装されていない。
3. 録画停止中にボタン/Enter/ホットキーを操作する。マーカーが増えず、入力メモが勝手に消えないこと。
   録画を開始し、4ボタン・各ホットキー・メモEnterを操作する。1操作につき1行であること。
4. ラベル/コメントを編集し、右クリックで種別変更・削除、全削除を行う。
   停止後のJSONを開き直して変更が残ること。入力中のDelete/Backspaceは文字を消し、
   表にフォーカスがある場合だけ選択マーカーを削除すること。
5. 一時停止中に複数回打刻し、paused表示と同じ時刻、再開後に停止時間が含まれないことを確認する。
   60/1、30000/1001、60000/1001、24000/1001で保存/再読込後のFPSとDF/NDFを比較する。
   対応する録画エンコーダーでFPS divisor=2、出力1280x720も試し、JSON metadataを確認する。
6. 自動分割を短い間隔（例30秒）にする。各分割の前後で打刻し、各動画に対応する保存ファイルと、
   分割後の時刻が区間の先頭から始まることを確認する。ドック非表示でも試す。
   分割直後に録画停止するケースも試し、最後の2区間が混ざらないことを確認する。
7. 右クリックメニューを開いたままホットキーで多数の打刻、分割、停止/次の録画開始を行う。
   クラッシュせず、古いメニューの種別変更/削除が新しいセッションへ作用しないこと。
8. Exportを開いたまま録画を開始/分割する。ダイアログを開いた時点のセッションを保存すること。
   JSON/CSV/EDL/SRT/VTT/YouTube/Markdown/XML全8形式とクリップボードを試す。
   日本語、絵文字、引用符、&、<、>、改行を含める。CSV/SRTのUTF-8 BOM、
   VTT字幕表示、YouTube章順を確認し、Resolve/Premiere等にも実際に取り込む。
9. 保存JSONを別名にコピーしてOpenで開き、編集する。編集保存先がその別名ファイルで、
   元のJSONを変更しないこと。空のオブジェクト等の不正JSONを開いても現在の表が失われないこと。
10. 自動JSON保存をOFFにし、停止時にJSONが生成されず、固有名のJSONLが残ること。
    同じ動画名でも以前のcacheを上書きしないこと。通常cacheは動画の隣、
    録画path未確定時はOBSプラグイン設定ディレクトリのcacheサブディレクトリに保存される。
    書込不可の保存先を選び、保存成功と誤表示せず警告されること。
11. テスト用JSONLをコピーし、最後の未完了行だけを切断したファイルはRecoverで復旧できること。
    改行で完結した途中行が壊れたファイルは拒否され、現在の表を変えないこと。
    復旧後のJSON確定成功で対象cacheが削除されること。flushは停電時の永続化保証ではない。
12. シーン/ソースの追加・削除、シーンコレクション変更後にホットキーを再確認する。
    録画中/停止後/ドックを閉じた状態でOBSを終了・再起動し、保存結果とOBSログの
    エラー、終了時クラッシュ、callback多重処理を確認する。

## 設計変更後に優先して確認する操作

2026-10-02の最終レビュー修正後は、以下の「最終レビュー修正後の確認」も実施する。

1. 空の状態ではExport/全削除、停止中では打刻が無効であること。履歴を開くと文書名と表が一致し、次の録画開始で履歴表示が新しい録画へ切り替わること。
2. ExportとSettingsを開いたままOBSを操作できること。それぞれ再度ボタンを押しても画面が増殖しないこと。閉じた後に再び開けること。
3. Copy to Clipboardを繰り返し押し、毎回OKを求める成功ダイアログが出ず、貼り付け内容は正しいこと。ファイルexport成功で画面が閉じ、失敗ではエラーが表示されること。
4. Exportを開いて対象ファイル名を確認し、録画開始・分割後も開いた画面は元の文書を出力すること。新しい文書のExportボタンを押すと新しい対象名の画面になること。
5. 表のラベル・コメントを編集し、表示とJSONが同じ内容になること。録画中の種別変更・削除も復旧cacheに反映されること。
6. Settingsの保存・キャンセル・リセットを試す。保存後にボタンと種別変更へ反映され、キャンセル時は変更されず、OBS再起動後も保存値が復元されること。
7. Export/Settingsが開いた状態、右クリックメニュー中、全削除の確認中でも録画開始・分割を試し、古い操作が新しい文書へ作用しないこと。
8. Export/Settingsを開いたままOBS終了を試し、終了時の保存・ログと次回起動を確認する。プラグインの再ロードが可能な環境では同条件で再ロードし、画面・callback・ホットキーが重複しないこと。

追加検証が必要な点: 実エンコーダーの遅延/負荷によるdropと動画上の正確な分割境界、
既に分割済みの録画途中でのプラグイン再ロード（過去の区間境界は取得できない）、
別OS、新しいOBSバージョン、各NLEのインポート互換。

## 最終レビュー修正後の確認

レビューIDの対応・自動テストは[final-review-fixes.md](final-review-fixes.md)に記録する。
実OBSでの再確認は未実施。既存データのコピーと専用録画フォルダだけを使う。

1. **起動/Dock/終了**: 録画していない状態で2回起動する。Dock表示、float、再Dock、閉じる/再表示を試す。内側のDockがないことを確認する。Settings/Exportを開いたまま終了し、クラッシュ・終了後のOBS APIエラー・callback多重登録がないことをログで確認する。
2. **録画と時刻表示**: 開始直後に4種の打刻、任意文字列のメモ保存、ラベル/コメント編集、種別変更、削除を行う。250ms監視による表示更新が続き、pauseで時刻が止まり、resumeで再開することを確認する。短い自動分割と手動分割の前後で打刻し、分割後の表示が新しい区間になること、停止直前の分割も保存されることを確認する。
3. **Export鮮度（R4）**: Exportを開き、同じ文書を編集してDockのExportを再度押す。現在の内容がClipboard/全8形式へ出ることを確認する。同じsession UUIDを保った別revisionのJSONをOpenで読み込み、再びExportする。対象名と件数も確認する。Copy成功でOKダイアログが出ないこと、閉じて再度開けること、ウィンドウが増殖しないことを確認する。
4. **進行中のExport（R4）**: 保存先選択ダイアログを開いたまま録画分割を待ち、保存する。開始時の文書が出力されることを確認する。次のExport操作で新しい文書が出ることも確認する。OSのネイティブ保存ダイアログ中の編集・OBS終了は各OSで確認が必要。headless回帰テストは非nativeダイアログで編集/分割を検証している。
5. **Settings/再起動/履歴**: ラベル、色、自動保存形式、hotkeyを保存・キャンセル・リセットして再起動する。設定が復元され、保存JSONをOpenで読み込んだ後の編集がその読み込み元へ保存されることを確認する。不正JSONをOpen/Recoverしても文書・未保存表示が変わらないことを確認する。
6. **JSON保存失敗からの救済（R1）**: JSON自動保存をONにする。専用フォルダで録画を開始し、その動画basenameと同じ名前の「.jsonディレクトリ」を作る（例: test.mkvに対するtest.json）。打刻後に分割/停止し、Unsavedに元の文書が残ることを確認する。別名の次回録画を開始しても残ること、UnsavedのExport snapshotで救済できること、Save JSON as...成功後に対象項目が消えることを確認する。保存先をキャンセル/書込不可にした場合は項目/cacheが消えないことを確認する。
7. **全保存先の失敗（R1）**: journal/JSON/fallbackの同時失敗とメモリ上限8文書は自動テストで注入済み。通常環境全体を容量不足や書込禁止にしない。実機で行うなら隔離したportable OBS・コピーfixtureのみを使う。上限では古い文書を保持し、OBS録画を止めず打刻を一時停止する。1文書のSave JSON as...で打刻が再開することを確認する。全保存先が故障したままOBSを終了するとメモリだけの文書を永続化できないため、終了前に救済する。
8. **古いcache衝突（R2）**: JSON自動保存をOFFにした短い録画を停止し、固有名の.tmp.jsonlをコピーする。別録画の正常JSONを、cacheが参照する動画basenameの.jsonとしてテストフォルダへ置く（session_idが異なることを確認する）。古いcacheをRecoverして編集し、既存JSONのbytesが変わらず、cacheとUnsavedが残ることを確認する。Save JSON as...で別名へ保存でき、成功して初めてcacheが削除されることを確認する。不正JSON、JSONなし、同じIDのJSONでも試す。正当な同ID更新は自動保存してよい。
9. **cacheのみ/復旧（R2/R3）**: video_pathが空の旧cache、JSONなしのcache、最後だけ途切れたjournalをコピーfixtureでRecoverする。path未確定の文書もSave JSON as...で救済できることを確認する。途中の破損、型不正、future schemaは拒否され、現在の正常文書やcacheを変更しないことを確認する。fallbackのcache/unsaved/*.jsonは再起動後Unsavedへ表示される。動画の隣のjournalはOpen/Recoverから選ぶ。
10. **FPS/字幕/外部アプリ**: 60/1、30000/1001、60000/1001、24000/1001、encoder FPS divisorを確認する。録画停止中にFPS/寸法/encoder設定を変更して再録画し、新しいmetadataが使われることを確認する。SRT/VTTの同時刻cue、日本語/改行/記号、BOM、各NLEのEDL/XMLインポートを確認する。

**R5は未変更**: recording activeでもoutputを取得できない条件は未立証。録画前のoutput取得、推測による再試行/打刻拒否を追加していない。
**R6は未変更**: OBS cumulative frameと実muxerの分割境界が一致する保証はない。frame番号を焼き込んだ動画で、自動/手動分割・負荷・音声・muxer別に最初のframeを測定し、markerとの誤差を別途記録する。今回のテストはcontrollerのoffset計算を検証し、この実境界精度を証明していない。

## 実機データの確認記録（2026-10-02）

修正版のインストール後、ユーザーから実録画の`2026-10-02 23-00-55.json`と、MarkdownのCopy to Clipboard結果が提供された。元ファイルを変更せずに照合した。

- source JSON SHA256: `D7E364FD29BEA6DE8070CEE206E0EC846528DD3B4429228F92396CC648D41457`。
- session ID: `f3465b77-ebeb-401c-a09b-0fcbb49ddc8f`、schema `1.0.0`、1920x1080、60/1 fps。
- 35件のIDは一意で1〜35。Markdownの件数・全詳細行のtimestamp/SMPTE/label/comment/colorが元JSONと一致し、frameとtimestampの往復換算も全件整合した。
- ID 10〜19（5700ms）、25〜28（8833ms）、35（12033ms）は元JSONで`is_paused: true`。異なるcreated_atを持つ同一frameでの打刻が残り、再開後の行ではframeが進んでいる。Markdownが重複生成したものではない。
- 4種別のlabel/colorは保存・Clipboard結果で一致。開始UTC表記と動画名の日本時間も整合する。
- この試行で実録画データのJSON保存とMarkdown Clipboardの生成を確認できた。全commentは空のため、任意文字列のメモ保存、編集後のsnapshot鮮度、ファイルExport、split、再起動、保存失敗救済、cache衝突の合否はこのデータだけでは確認できない。
- 続くコメント編集後のJSON（SHA256 `2DDDE0BE1E7A623A474D2B54B0A42140FB821CF4F6AFE81337D83754E0101852`）と再コピー結果も照合した。同じsession ID・35件のまま、ID 1=`確認用メモ`、2=`a`、20=`Yo`、33=`Hi`がJSONとMarkdownに一致し、他の31件のcommentは空。日本語メモの保存と編集後のClipboard出力への反映を確認した。その他の未確認操作は引き続き残る。
- 編集後のJSON保存は`SessionStore::save()`の`QSaveFile`による文書全体の再生成・置換。既存JSONを先に削除する処理はなく、一時ファイルへの全量書き込みが成功した後にcommitする。保存成功後に削除するのは復旧cacheである。
- 再開後の文書commit `7050bef`に対する[CI](https://github.com/oz-coden/obs-timestamp-memo-plugin/actions/runs/37016306206)も全3OS build/testとformatに成功した。
