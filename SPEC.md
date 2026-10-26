# Specification

[日本語](#日本語) / English

This document describes observable behavior, implementation limits and validation
status. Installation and everyday operation are in [README.md](README.md).

## Timing and accuracy

For encoded recordings with audio, packet CTS/PTS pairs calibrate a mapping from
the current rendered-video clock to recording presentation time. Packet delivery
count is not used as the current recording time. Stamping waits for the first
timed packet when calibration is required.

- Time is stored as a frame index and milliseconds derived using rational FPS.
- SMPTE NDF and 29.97/59.94 DF formatting are supported.
- Pause freezes the marker clock; resume excludes the paused interval. Markers
  stamped while paused retain the paused flag.
- Each split segment has its own document and relative origin. If the split
  notification arrives after stamping, markers at or after the boundary move to
  the new segment. IDs, labels, comments, colors and creation times are retained;
  relative frames, milliseconds and timecodes are recalculated.
- The split boundary is estimated from the latest observed video keyframe PTS.
  The muxer's `file_changed` event does not expose the exact boundary PTS.
- Raw/video-only output uses an estimated recording-start origin and emits a
  warning, because the required timed packet callbacks are unavailable.

This is presentation-time estimation, not a frame-accuracy guarantee. Preview
latency, pause cutoff, skipped frames, audio buffering, encoder settings and
muxer split boundaries require comparison with an actual recorded file/NLE.

## Documents and editing

A document contains session identity, recording path, start time, resolution,
frame rate and the final marker list. Marker fields include a document-local ID,
frame/millisecond position, type, label, color, comment, creation time and paused
state. JSON is not an archive of the complete editing history.

Changing type updates the color. A label matching the old slot's current default
follows the new default; a custom label remains unchanged. The table supports
single-row selection. Delete/Backspace deletes its selected marker without a
confirmation; Clear and unsaved-document Discard require confirmation.

Undo/Redo covers additions, deletion, clear-all and label/comment/type changes.
History resets on load, recovery, recording start and split. Markers carried into
a new split segment form its initial state; later edits are undoable normally.
Text controls retain their own editing Undo while focused.

Opening JSON/recovery is explicit and allowed only while recording is stopped.
Failed validation retains the current document and reports the field/marker or
journal line. The previous document is not opened automatically at startup;
retained recovery copies are offered in Unsaved.

## Resource and performance limits

| Item | Behavior |
|---|---|
| UI polling | Recording-state polling every 250 ms; stamping also drains pending recording events |
| Marker history | Up to 128 commands and approximately 8 MiB per document |
| Oversized history edit | The edit remains applied; previous history is cleared with a warning |
| Memory-only pending documents | Ordinary rollover stops at eight retained snapshots rather than evicting data |
| Delayed split at that limit | Existing markers may partition into additional documents; retain both parts and suspend new stamping until enough documents are rescued |
| Marker IDs | Unsigned 32-bit IDs; new insertion fails with a warning if exhausted |
| JSON integers | Validated up to 2^53 − 1 for interoperable frame/millisecond values |
| Imported FPS | Numerator/denominator up to 1,000,000; effective FPS from 1 to 1000 |

Disk-backed pending entries do not retain a full in-memory snapshot. Current
markers, table data, export snapshots and history still use memory proportional
to their contents. Journal writes and file exports are synchronous; storage
latency and very large documents can affect UI responsiveness. No numerical
CPU/memory benchmark or maximum recording-duration performance guarantee is
published. The numeric bounds above are not measured performance targets.

## Saving and recovery

### Automatic JSON on

Stop/split and later edits save the canonical JSON document. Saving uses atomic
replacement. An existing canonical file is updated only when its session ID and
start time match; a foreign, corrupt or unreadable file is retained. Failed
segments remain separate from the next recording.

### Automatic JSON off

Stop/split and subsequent edits do not create a canonical JSON. Recovery journals
still record changes, and the document remains Unsaved until explicitly saved or
discarded. **Unsaved → Save JSON as** saves a permanent copy and acknowledges it.
Ordinary Export, including JSON Export, does not acknowledge the live document
or remove its recovery data.

During normal shutdown, retained journals are converted to discoverable recovery
copies when storage is writable. They reappear in Unsaved after restart. Journals
that cannot be converted remain on disk and can be opened explicitly. Data that
exists only in memory cannot survive process termination.

### Recovery files and failures

Journals are newline-delimited JSON (`*.tmp.jsonl`) containing a header and edit
operations. Recovery tolerates an incomplete final line after interruption; other
invalid operations are reported transactionally. Journals are flushed, but
power-loss durability and frame-exact muxer recovery are not guaranteed.

The Unsaved menu offers per-document Save JSON as, Export snapshot and Discard.
A stale menu or file dialog cannot operate on a replaced current document.
Discard removes associated recovery files after confirmation; if removal fails,
the remaining authoritative recovery data is retained.

Automatic non-JSON exports never overwrite existing files. A collision uses
`basename.timestamp-memo.<session-id>.<extension>`, followed by a numeric suffix
if needed. Explicit Export follows the save dialog's overwrite decision.

## Export formats

| Format | Content and limitations |
|---|---|
| JSON | Session metadata and final marker state; can be opened again |
| CSV | UTF-8 with BOM; spreadsheet/marker list; NLE import unverified |
| EDL | CMX-style marker output; Resolve/Premiere import unverified |
| XML | FCP-style sequence markers; FCP7/Premiere import unverified |
| SRT | Subtitle cues, normally up to 2.5 seconds and limited by the next marker; same-time cues get a fallback duration |
| VTT | Subtitle cues with WebVTT escaping |
| Markdown | Report using the video basename, escaped text and preserved line breaks |
| Chapters text | Timestamp list usable in a video description; same-second markers are grouped |

CSV's `Index` is 1..N in output order; `Marker ID` is the persistent identity.
User text beginning with `=`, `+`, `-` or `@` after whitespace, or already starting
with an apostrophe, gets one leading apostrophe in CSV only. This prevents common
spreadsheet formula interpretation without changing the JSON/domain value. To
reverse this representation, remove one added apostrophe from affected fields;
spreadsheet import/display behavior can vary.

Chapters text includes every marker type, including Cut/Edit, and adds a Start
entry when needed. It does not enforce YouTube's recognition rules or delete
markers to meet them. YouTube requires an initial 00:00, at least three ascending
entries and chapters of at least ten seconds; see
[YouTube's chapter requirements](https://support.google.com/youtube/answer/9884579).

Export/Settings windows are modeless and reused. Reopening Export captures the
current document; one file-export operation retains the snapshot it started with
while the save dialog is open. Copy success does not open a modal notification.

## Configuration and storage

Settings and hotkeys are plugin-global, independent of profile and scene
collection. All paths are relative to OBS's configuration root, including a
portable installation's root. The plugin asks OBS for the module configuration
path instead of assuming a platform-specific absolute location.

| File/directory | Purpose |
|---|---|
| `plugin_config/obs-timestamp-memo/settings.ini` | Marker labels/colors, automatic exports and ordinary notifications |
| `plugin_config/obs-timestamp-memo/hotkeys.json` | Five OBS hotkey bindings |
| `plugin_config/obs-timestamp-memo/cache/` | Journals whose recording path is unresolved |
| `plugin_config/obs-timestamp-memo/cache/unsaved/` | Retained recovery JSON copies |
| Beside the recording | Journals and canonical JSON when a recording path is available |

On first use, the old platform QSettings configuration is imported if the new
INI is absent; legacy data is not deleted. Hotkeys can import bindings from the
first loaded legacy scene collection. Afterwards plugin-global bindings take
precedence. Collections not loaded during migration are not imported.

A corrupt global hotkey file is retained and protected from overwrite. Close OBS,
back it up and repair or move it aside before saving new bindings. Back up the
configuration and cache together when moving an OBS installation.

## Compatibility and verification

- Windows/macOS build dependencies pin OBS sources to 31.1.1 and obs-deps/Qt to
  the versions in `buildspec.json`. The Ubuntu workflow installs native OBS from
  the OBS PPA and uses its available development libraries; that version is not
  fixed by the Windows/macOS source pin.
- Windows x64, macOS universal and Ubuntu 24.04 x86_64 are CI build targets.
- The three automated suites cover domain logic, application behavior and Qt/OBS
  stubs. Encoder/muxer timing, native clipboard/keyboard behavior, real dock
  placement and NLE interoperability still require actual application checks.
- macOS Developer ID signing/notarization requires maintainer credentials. An
  archive or successful CI run alone does not imply that it has been notarized.
- Linux distribution archives target a native OBS installation, not a verified
  Flatpak/Snap runtime. ABI/library compatibility must match the installed OBS.
- Streaming-only stamping is not implemented.

See [tests/README.md](tests/README.md) for coverage and
[docs/BUILDING.md](docs/BUILDING.md) for developer commands.

---

## 日本語

### 時刻・精度

音声付きのエンコード録画では、映像パケットのCTS/PTSで描画時刻と録画内時刻を対応させます。
現在時刻にエンコーダー送出済みパケット数は使いません。校正が必要な出力では最初の時刻付きパケットを待ちます。

- 有理数FPSを使い、フレーム番号とミリ秒を保持します。SMPTE NDF・29.97/59.94 DF表示に対応します。
- 一時停止中は時刻を固定し、再開後は停止期間を除外します。停止中のマーカーには停止フラグを保持します。
- 分割後は文書・相対時刻を分けます。通知より先に打刻された境界以降のマーカーも新しい文書へ移し、
  ID・ラベル・メモ・色・作成日時を保持して相対フレーム・ms・timecodeを再計算します。
- 分割境界は直近の映像キーフレームPTSから推定します。muxerの`file_changed`通知に厳密な境界PTSは含まれません。
- raw出力・映像のみの出力は必要なパケット通知がないため、開始時刻からの推定と警告表示を使います。

すべての環境でのフレーム一致は保証しません。プレビュー遅延、一時停止境界、欠落フレーム、音声バッファ、
エンコーダー設定、muxerの分割境界は実ファイルやNLEとの比較が必要です。

### 文書・編集

文書はセッションID、動画パス、開始日時、解像度、FPS、最終マーカー一覧を持ちます。
JSONはUndoを含む全編集履歴ではありません。種別変更では色を更新し、旧既定名と一致するラベルだけを
新しい既定名へ追従させます。カスタムラベルは保持します。

表は単一行選択です。表にフォーカスがあるとDelete/Backspaceで確認なしに削除します。
全削除と未保存文書の破棄には確認があります。Undo/Redoは追加・削除・全削除・ラベル・メモ・種別の変更が対象で、
読込・復旧・録画開始・分割時に履歴を初期化します。分割で移動したマーカーは新文書の初期状態となり、
その後の編集は通常どおりUndo/Redoできます。

JSON・ジャーナルは録画停止中に明示的に開きます。不正な文書はエラー箇所を示し、現在の文書を保持します。
起動時に前回文書は自動では開きません。保持済みの復旧コピーは未保存一覧へ表示します。

### 性能・上限

| 項目 | 動作 |
|---|---|
| 状態更新 | 250 msごとにポーリング。打刻時も保留中イベントを処理 |
| マーカー履歴 | 文書ごとに128操作・約8 MiB |
| 上限を越える巨大編集 | 編集は適用し、警告とともに旧履歴を消去 |
| メモリのみの未保存文書 | 通常の切替では8件のスナップショットを上限にし、暗黙に捨てず新たな打刻を停止 |
| 上限時の遅延分割 | 打刻済みマーカーの分割で文書が増えても両方を保持し、十分な救済が済むまで打刻を停止 |
| マーカーID | 符号なし32 bit。枯渇時は警告して追加を拒否 |
| JSONのフレーム・ms値 | 他のJSON利用環境との精度互換性のため2^53 − 1まで検証 |
| 読込FPS | 分子・分母は各1,000,000まで、実効FPSは1〜1000 |

ディスク上に保持できた未保存文書は全体スナップショットをメモリに残しません。
現在のマーカー、表、Exportのスナップショット、履歴には内容量に応じたメモリを使います。
ジャーナル書込・Exportは同期処理のため、遅い保存先や大きな文書ではUI応答へ影響する場合があります。
CPU・メモリ使用量の測定値や、特定の録画時間までの性能保証は公開していません。

### 保存・復旧

**JSON自動保存ON**では停止・分割・停止後の編集で最終状態を保存します。原子的に置換し、既存の保存先は
セッションID・開始日時が一致する場合だけ更新します。他の文書、不正JSON、読めないファイルを無断上書きしません。

**OFF**では恒久JSONを生成せず、復旧ジャーナルを更新して未保存に残します。
**未保存 → JSONを別名保存**で救済を確定します。通常のExportはJSON形式でも未保存状態を解除しません。
通常終了時、保存先へ書き込めるジャーナルは復旧コピーへ変換し、再起動後も未保存一覧へ戻します。
変換できないジャーナルは明示的に開けます。メモリにしかない内容はプロセス終了を越えて保持できません。

`*.tmp.jsonl`はヘッダーと編集操作を記録する行単位JSONです。復旧時は中断による最終行の欠損を許容し、
それ以外の不正操作は部分適用せずエラーにします。書込はflushしますが、停電時の永続化保証ではありません。
未保存メニュー・ファイルダイアログ中に現在の文書が置き換わった場合は操作を中断します。
破棄で復旧ファイルを削除し、削除失敗時は残る正本を保持します。

自動の非JSON出力は既存ファイルを上書きせず、衝突時は
`basename.timestamp-memo.<session-id>.<extension>`と連番を使います。
明示的なExportでは保存ダイアログの上書き判断に従います。

### 出力形式

JSONは再読込できる文書です。CSVはBOM付きUTF-8、EDLはCMX風、XMLはFCP風の出力ですが、
CSV・EDL・XMLのNLE取込互換性は未検証です。SRT/VTTは字幕、Markdownはレポート、
chapters textは動画説明欄など向けのタイムスタンプ一覧です。

CSVの`Index`は出力順1..N、`Marker ID`は永続IDです。空白除去後に`= + - @`で始まる文字列や、
先頭がapostropheの文字列には、出力時だけapostropheを1つ加えます。元文書は変更しません。
戻す場合は対象フィールドの追加prefixを1つ取り除きます。表計算ソフトの取込・表示は環境ごとに確認してください。

Markdownには動画のbasenameだけを表示し、ラベル・メモ・改行をescapeします。
SRT/VTTの通常字幕長は最大2.5秒で次マーカーまでに制限し、同時刻では代替の長さを使います。
chapters textはCut/Editを含む全種別と同一秒のgrouping、Start補完を維持します。
YouTubeの認識条件に合わせた削除・統合はせず、00:00開始・昇順3件以上・各章10秒以上を自動で保証しません。
[YouTubeの条件](https://support.google.com/youtube/answer/9884579)も参照してください。

Export/Settingsはmodelessで再利用します。Exportを開き直すと現在の文書を取得し、1回の保存操作では
開始時点のスナップショットを維持します。Clipboard成功時に確認ダイアログは出しません。

### 設定・保存先

設定・ホットキーはプロファイルやシーンコレクションから独立したプラグイン共通設定です。
絶対パスを固定せず、OBSから取得した設定ルートを使うためポータブル版にも追従します。

| OBS設定ルートからの相対パス | 用途 |
|---|---|
| `plugin_config/obs-timestamp-memo/settings.ini` | ラベル・色、自動出力、通常通知 |
| `plugin_config/obs-timestamp-memo/hotkeys.json` | 5つのホットキー割当 |
| `plugin_config/obs-timestamp-memo/cache/` | 動画パス未確定時のジャーナル |
| `plugin_config/obs-timestamp-memo/cache/unsaved/` | 保持する復旧JSONコピー |
| 動画ファイルの隣 | パス確定時のジャーナル・恒久JSON |

新INIがなければ旧QSettingsを初回移行し、旧データは削除しません。旧ホットキーは最初に読み込んだ
シーンコレクションから移行でき、その後はプラグイン共通割当を優先します。未読込コレクションは移行できません。
不正なホットキーファイルは上書きせず保持します。OBS終了後にバックアップし、修復・退避してから再設定してください。
OBS環境を移す場合は設定とcacheを一緒にバックアップしてください。

### 互換性・検証状況

- Windows/macOSはOBS 31.1.1のソースと`buildspec.json`のobs-deps/Qtを使います。
  UbuntuではOBS PPAのネイティブパッケージを導入するため、このソース固定はUbuntuの依存バージョンには適用されません。
- CI対象はWindows x64、macOS universal、Ubuntu 24.04 x86_64です。
- 自動テストはドメイン・アプリケーション・Qt/OBSスタブの3層です。エンコーダー・muxerの実際の時刻、
  ネイティブClipboard・キー操作、ドック配置、NLE互換性は実機での確認が必要です。
- macOSのDeveloper ID署名・公証には保守者の認証情報が必要です。アーカイブ生成・CI成功だけでは公証済みを意味しません。
- Linux配布物はネイティブ版OBS向けです。Flatpak/Snapや異なるライブラリ構成での動作は未検証です。
- 配信だけのタイムスタンプ記録は未実装です。

テスト範囲は[tests/README.md](tests/README.md)、開発手順は[docs/BUILDING.md](docs/BUILDING.md)を参照してください。
