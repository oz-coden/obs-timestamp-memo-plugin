# OBS Timestamp Memo Plugin

[日本語](#日本語) / English

A dock for recording markers and memos in OBS Studio. Four configurable marker
slots, inline label/comment editing, hotkeys, Undo/Redo, recovery and eight export
formats are provided. Streaming-only stamping is not implemented.

## Timing and accuracy

For normal encoded recordings with audio, the plugin maps the current rendered
video clock to recording presentation time using OBS packet CTS/PTS pairs. It
avoids using the count of packets already delivered by the encoder. Pause freezes
the marker clock; resume excludes the paused interval. Rational FPS and SMPTE
DF/NDF formatting are supported.

This is presentation-time estimation, not a guarantee that every marker matches
an exact player/NLE frame. Preview/display latency, pause cutoff, skipped video,
audio buffering and muxer split boundaries need actual-file verification. Raw or
video-only outputs use a recording-start clock estimate and display a warning.
Stamping waits for the first timed packet when calibration is required. See
[time-source rationale and measurement procedure](docs/recording-time-source.md).

## Usage

1. Enable **Timestamp Memo & Markers** in OBS's Docks menu. Float/re-dock the
   single dock as usual.
2. Start recording, then click a marker slot or use its OBS hotkey. Enter a memo
   and press Enter to stamp it. The **Focus Memo Input** hotkey focuses the input.
3. Double-click label/comment cells to edit. Right-click to change type or delete.
   A custom label survives type changes; a label still matching its old default
   follows the new default. Delete/Backspace deletes a selected marker when the
   table has focus, without confirmation. Text inputs keep normal text deletion.
4. Use **Undo / Redo**, or platform-standard shortcuts (Ctrl+Z and Ctrl+Y /
   Ctrl+Shift+Z on Windows; Qt's standard keys on macOS). Input controls retain
   their own text Undo while editing. Marker history is per document, limited to
   128 commands / approximately 8 MiB, and resets on load, recovery, start or
   split. Very large edits clear the history with a warning. Clear-all remains
   confirmed and is undoable within this limit.
5. **Export** and **Settings** are modeless and reused. Pressing the dock's Export
   button again captures the current document. A file export keeps its starting
   snapshot through the file dialog. Clipboard success has no modal notification.
6. Open earlier JSON or recovery JSONL explicitly; the last document is not loaded
   automatically at startup. Failed reads show the marker/field or journal line
   involved and retain current markers.

## Data protection and settings

- **Automatic JSON ON**: stop/split and later edits save the final marker state.
  JSON is a document, not the complete editing/Undo history. Atomic replacement
  preserves the previous file if saving fails. An existing JSON is updated only
  when its session identity matches; conflicting or corrupt files are retained.
- **Automatic JSON OFF**: no canonical JSON is created by stop or subsequent
  edits. Recovery journals still record changes. Use **Unsaved → Save JSON as**
  or an explicit JSON Export when you want a permanent document. Exporting an
  arbitrary format alone does not mark a document saved or delete its recovery
  data; use Save JSON as for that.
- Failed segments remain separate from the next recording. **Unsaved (N)** offers
  Save JSON as, Export snapshot, and **Discard** for each named document. Discard
  requires confirmation and removes its recovery files. No old document is
  silently evicted: if all storage fails and eight pending documents exist only
  in memory, recording continues but new stamping pauses until one is rescued or
  discarded. Data existing only in memory cannot survive closing OBS.
- Ordinary status notifications can be disabled. Failures and data-protection
  warnings remain visible. Journals are flushed, but power-loss durability is not
  guaranteed.
- Settings use `obs_module_config_path("settings.ini")`, normally under the OBS
  configuration directory's `plugin_config/obs-timestamp-memo/`. They are plugin
  global, independent of profile/scene collection, and follow portable OBS's
  configuration root. First use imports the old platform QSettings configuration
  if the new file is absent; old data is not deleted.
- The five hotkey bindings use `hotkeys.json` in the same plugin directory. The
  first legacy scene-collection load can import old bindings; global bindings then
  take precedence across collection changes. Only the loaded legacy collection
  can be migrated; inspect assignments after upgrade. A corrupt global hotkey
  file is retained and protected from overwrite; back it up and repair/remove it
  while OBS is closed before saving new assignments.
- Journals normally sit beside the recording. Unresolved-path journals and
  retained unsaved copies use the module's `cache` directory. Back up that folder
  together with plugin settings when moving an OBS installation.

## Export formats

| Format | Purpose / limitations |
|---|---|
| JSON | Final session metadata and marker state; can be opened again |
| CSV | UTF-8 with BOM; spreadsheet / marker list, **NLE import unverified** |
| EDL | CMX-style marker output; **Resolve/Premiere interoperability unverified** |
| XML | FCP-style sequence marker output; **FCP7/Premiere interoperability unverified** |
| SRT / VTT | Subtitle cues; format-specific escaping and newlines |
| Markdown | Report / clipboard; video basename only, escaped labels/comments |
| Chapters text | General timestamp list / YouTube description / clipboard |

Automatic non-JSON exports never overwrite an existing file. They use the video
basename if available, then `basename.timestamp-memo.<session-id>.<extension>`
(and a numeric suffix for further collisions). Explicit file export keeps the
file dialog's overwrite decision.

CSV **Index** is 1..N in exported order; **Marker ID** is the persistent identity.
For user text beginning with `=`, `+`, `-`, `@` after whitespace, or already
beginning with an apostrophe, CSV adds one leading apostrophe to prevent automatic
spreadsheet formula interpretation. Only export text is changed; the JSON/domain
value stays intact. To reverse it, remove one prefix apostrophe from such exported
fields. Spreadsheet display/import behavior can vary; inspect the imported data.

Chapters output keeps all marker types, including Cut/Edit. Its existing
same-second grouping and Start fallback remain. It does not filter, enforce
10-second gaps, or delete markers to meet YouTube rules. YouTube recognition needs
00:00 first, at least three ascending timestamps, and chapters at least ten seconds
long; see [YouTube's conditions](https://support.google.com/youtube/answer/9884579).

## Requirements, installation and build

OBS Studio 31.x or later, Qt6, Windows x64 / macOS / Linux. The build dependencies
are pinned to OBS 31.1.1 in `buildspec.json`; runtime timing verification on other
OBS versions remains necessary. Close OBS before replacing plugin binaries.

- Windows: preserve the release's DLL/data layout in the OBS installation
  (`obs-plugins/64bit` and `data/obs-plugins/obs-timestamp-memo`).
- macOS: place the `.plugin` bundle under
  `~/Library/Application Support/obs-studio/plugins/`.
- Linux user install (OBS 31.1.1's 64-bit frontend search layout):
  `~/.config/obs-studio/plugins/obs-timestamp-memo/bin/64bit/obs-timestamp-memo.so`
  and `~/.config/obs-studio/plugins/obs-timestamp-memo/data/locale/`.
  Respect `XDG_CONFIG_HOME` and the paths supplied by distro/Flatpak packaging.

CMake 3.28+, C++20 and Visual Studio 2022 on Windows:

```sh
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --parallel
```

Use the macOS / Ubuntu presets in `CMakePresets.json` on those systems.
[Tests](tests/README.md), [architecture](docs/architecture-and-refactoring.md),
[manual OBS verification](docs/obs-manual-test.md) and
[latest fixes](docs/opus-review-fixes.md) describe validation and remaining limits.

## 日本語

録画中のタイムスタンプとメモを記録するOBSドックです。4つのスロット、ラベル・
コメント編集、ホットキー、Undo/Redo、復旧、8形式のExportを提供します。
**配信のみでの打刻は未実装**です。

### 時刻と精度

通常の音声付きエンコード録画では、packetのPTSと描画時刻CTSを対応させ、現在の
映像時刻を録画ファイルの時間へ換算します。encoderから到着済みのpacket数には
依存しません。一時停止中は固定時刻、再開後は停止時間を除外します。有理数FPSと
DF/NDF表示に対応します。

すべての環境で「フレーム精度」を保証するものではありません。プレビュー表示遅延、
pause境界、欠落frame、音声buffer、muxerの分割境界は実ファイルの測定が必要です。
raw / 映像のみのoutputは開始時刻からの推定となり、警告を表示します。校正が必要な
outputでは最初のpacketを待って打刻します。[根拠と測定手順](docs/recording-time-source.md)。

### 操作

1. ドックメニューからTimestamp Memoを表示します。外側のドックは1つです。
2. 録画開始後、4ボタンまたはホットキーで打刻します。入力欄のEnterでメモを打刻し、
   Focus Memo Inputのホットキーで入力欄へ移動できます。
3. 表のラベル・コメントをダブルクリックして編集します。右クリックで種別変更や削除。
   手動編集したラベルは種別変更でも保持し、旧既定ラベルのままなら新既定名に追従します。
   表にフォーカスがあるとDelete/Backspaceで確認なしにマーカー削除、入力欄では文字削除です。
4. Undo/Redoボタン、WindowsのCtrl+Z / Ctrl+Y / Ctrl+Shift+Z、macOSのQt標準キーで
   編集を戻せます。文字入力中は入力欄自身のUndoが優先します。履歴は文書ごとに
   128操作・約8MiBまでで、読込・復旧・録画開始・分割時に初期化します。巨大な編集は
   警告とともに履歴を消去します。全削除は確認があり、上限内ならUndoできます。
5. Export / Settingsはmodelessで再利用します。ドックのExportを再度押すと最新文書を
   取得し、1回のファイルExport中は開始時のsnapshotを固定します。コピー成功のOK画面はありません。
6. 履歴JSON / 復旧JSONLは明示的に開きます。起動時の最終文書自動読込はありません。
   読込失敗時はmarker・field・journal行を示し、現在の表を保持します。

### 保存・復旧・設定

- 自動JSON ON: 停止・分割・停止後の編集で最終marker状態を保存します。JSONは
  Undoを含む編集履歴ではありません。atomic置換で保存し、同一sessionと確認できない
  既存JSONや不正JSONは無断上書きしません。
- 自動JSON OFF: 停止・その後の編集で恒久JSONを生成しません。復旧journalは更新します。
  「未保存 → JSONを別名保存」または明示的なJSON Exportを利用してください。
  任意形式へのExportだけでは保存済みにせず、復旧ファイルも削除しません。救済を確定
  する場合はJSONを別名保存します。
- 保存失敗したsegmentは次の録画と分けて保持します。「未保存 (N)」の各文書から
  保存 / Export / 確認付き破棄ができます。破棄で対象復旧ファイルを削除します。
  古い文書を暗黙に捨てません。すべての保存が失敗しpending文書8件がメモリのみの場合、
  OBS録画を続けて新たな打刻だけ停止し、救済・破棄後に再開します。メモリのみの
  データはOBS終了を越えて保持できません。
- 通常通知はOFFにできますが、失敗・データ保護の警告は常に通知します。
  journalはflushしますが停電時の永続化保証ではありません。
- 設定はOBSの `obs_module_config_path("settings.ini")`、通常はOBS設定フォルダ内の
  `plugin_config/obs-timestamp-memo/` に保存します。profile / scene collectionから
  独立したplugin global設定で、portable OBSではその設定ルートを使います。新ファイルが
  なければ旧QSettingsから初回移行し、旧データは残します。
- ホットキー5件は同じ場所の `hotkeys.json` へ保存します。初回の旧scene collection
  読込で移行し、その後はglobal割り当てを優先します。読み込まれていない旧collectionの
  割り当ては移行できないため、更新後に確認してください。不正hotkeyファイルは保持し、
  自動上書きしません。OBS停止中にバックアップして修復・退避した後、再設定してください。
- 通常のjournalは動画の隣、path未確定時のjournalと未保存copyはpluginの `cache` 内です。
  OBS環境を移す際は設定とcacheを一緒にバックアップしてください。

### Export仕様

JSON、CSV、EDL、XML、SRT、VTT、Markdown、chapters textに対応します。
**Resolve / Premiere / FCP7の実機import互換性は未検証**です。EDL/XMLの構造は
推測による変更をしていません。CSVは表計算・マーカー一覧として扱ってください。
Markdownは動画basenameのみ表示し、ラベル・メモの構文文字をescapeします。

自動JSON以外のExportは既存ファイルを上書きせず、衝突時は
`basename.timestamp-memo.<session-id>.<extension>`、さらに連番を使います。
明示的なファイルExportでは保存dialogの上書き判断を利用します。

CSVのIndexは1..N、Marker IDは永続IDです。ユーザー入力が空白除去後に `= + - @`
で始まる場合や、先頭がapostropheの場合、CSVにだけapostropheを1つ付加して数式評価を
避けます。元文書は変えず、該当するExport値のprefixを1つ取り除くことで戻せます。
表計算ソフトごとの表示・importは確認してください。

chapters textはCut/Editを含む全種別を出力し、既存の同一秒groupingとStart補完を維持します。
10秒未満の統合・自動削除・種別filterは追加していません。YouTubeでの章認識には
00:00開始・昇順3件以上・各章10秒以上が必要です。[YouTubeの条件](https://support.google.com/youtube/answer/9884579)。

### 導入・検証

OBS Studio 31.x以上 / Qt6、Windows x64・macOS・Linux。ビルド依存はOBS 31.1.1です。
他versionでの時刻精度は実機確認が必要です。OBS終了後に配布物の構成を維持して配置します。
Windowsは `obs-plugins/64bit` と `data/obs-plugins/obs-timestamp-memo`、macOSは
`~/Library/Application Support/obs-studio/plugins/` の `.plugin` bundleです。
Linuxのユーザー配置は `~/.config/obs-studio/plugins/obs-timestamp-memo/bin/64bit/obs-timestamp-memo.so`
と同moduleの `data/locale/`。`XDG_CONFIG_HOME`やFlatpak / distributionの配置を尊重してください。
CMake 3.28+ / C++20で上記presetを使います。[テスト](tests/README.md)、
[実OBS手順](docs/obs-manual-test.md)、[今回の修正](docs/opus-review-fixes.md)を参照してください。

## License / ライセンス・クレジット

[GPL-2.0](LICENSE). Developed by oz-coden. Built on OBS Studio API and Qt6.
