# Changelog

[日本語](#日本語) / English

## Unreleased

### Added

- Undo/Redo for marker insertion, deletion, label, memo and type changes, with
  standard platform shortcuts and bounded, per-document history.
- Per-document Unsaved management: save a JSON copy, export, or explicitly
  discard recovery data after confirmation.
- English and Japanese UI, notifications, diagnostics and hotkey descriptions
  through OBS localization.

### Changed

- Recording timestamps use the rendered video clock calibrated to recording
  packet CTS/PTS, rather than the count of packets already delivered by the
  encoder. Pause freezes the clock; resume excludes the paused interval.
- Settings and hotkeys use OBS's plugin-global configuration location, including
  portable installations. Existing QSettings values and hotkeys from a loaded
  legacy scene collection are migrated where available.
- Automatic non-JSON exports use an alternate filename when the target already
  exists, preserving files from other sessions or tools.
- Markdown exports show the video basename instead of its absolute path and
  escape labels, comments and line breaks.
- CSV separates sequential `Index` from persistent `Marker ID`. Potential
  spreadsheet formulas and leading apostrophes receive an export-only apostrophe
  prefix; the stored document is unchanged.

### Fixed

- Important warnings remain visible when ordinary success notifications are off.
- Custom labels survive marker type changes.
- Automatic JSON OFF no longer creates permanent JSON during later edits;
  recovery journals remain enabled, and explicit Save/Export is still available.
- Invalid JSON reports the affected marker, field and expected/actual values
  without partially replacing the current document.
- Failed saves retain recoverable documents across stop, split and later
  recordings. Conflicting recovery documents do not silently overwrite another
  session's JSON.
- Reopening Export captures the latest document, while a single file-export
  operation retains its starting snapshot through the file dialog.
- Export and Settings are reusable modeless windows. Successful clipboard copies
  no longer require dismissing a dialog.
- Startup, output/callback lifetime and cross-platform build regressions are
  covered by domain, application and Qt/OBS-stub tests in CI.

### Compatibility and timing notes

- Presentation-time estimation does not guarantee exact player/NLE frame
  alignment. Encoder settings, preview latency, pause cutoff and muxer split
  boundaries require measurement against actual recordings. Raw/video-only
  outputs use an estimated recording-start origin and show a warning.
- EDL/XML NLE compatibility remains unverified. CSV's explicit column and text
  protection changes may affect existing import scripts.
- YouTube/timestamp-list output keeps existing marker behavior; it does not
  enforce YouTube chapter eligibility rules or filter marker types.
- Streaming-only timestamps are not implemented.

## 日本語

### 未リリース

#### 追加

- markerの追加・削除・label・memo・type変更にUndo/Redoを追加。OS標準の
  ショートカットに対応し、文書ごとの履歴量に上限を設けました。
- Unsaved文書ごとにJSON別名保存・Export・確認付きDiscardを追加しました。
- OBSの言語設定に合わせた英語・日本語のUI、通知、診断、hotkey説明を追加しました。

#### 変更

- 打刻時刻をencoder送出packet数ではなく、packet CTS/PTSで校正した描画video clock
  から求める方式へ変更。pause中は固定し、resume後は停止期間を除外します。
- 設定とhotkeyをOBSのplugin共通configへ保存。portable版にも対応し、旧QSettingsと
  読込済みscene collectionの既存hotkeyを可能な範囲で移行します。
- 自動非JSON Exportは既存ファイルがあれば別名を使い、他session・他ツールの出力を保護します。
- Markdownの動画情報は絶対pathからbasenameへ変更し、label・comment・改行をescapeします。
- CSVの連番`Index`と永続`Marker ID`を分離。数式として解釈され得る文字列と先頭の
  apostropheにExport時だけapostropheを追加し、保存文書自体は変更しません。

#### 修正

- 通常通知OFFでも重要警告を表示。type変更時にはcustom labelを保持します。
- 自動JSON OFFでは停止後の編集で恒久JSONを生成せず、復旧journalだけを更新します。
  明示的なSave/Exportは引き続き利用できます。
- 不正JSONはmarker・field・expected/actualを表示し、現在の正常文書を保持します。
- 保存失敗した文書を停止・分割・次回録画で失わず、復旧時の別session JSON上書きを防ぎます。
- Exportの再操作は最新文書を取得し、file dialog中は操作開始時のsnapshotを保持します。
- Export/Settingsを再利用可能なmodeless画面にし、Clipboard成功時のOK画面をなくしました。
- 起動・output/callbackの寿命・各OSビルドの回帰を3層テストとCIで検証します。

#### 互換性・精度の注意

- 時刻はpresentation timeの推定であり、player/NLEでの完全なframe一致を保証しません。
  encoder設定、preview遅延、pause cutoff、実muxerの分割境界は録画ファイルでの実測が必要です。
  raw/video-only outputは録画開始点の推定を利用し、警告を表示します。
- EDL/XMLのNLE互換性は未検証です。CSVの列・文字列保護の変更は既存import処理に影響し得ます。
- YouTube用を兼ねたtimestamp listは従来のmarker出力を維持し、章の認識条件の強制や
  marker typeによるfilterを行いません。
- 配信だけを対象とするtimestampは未実装です。
