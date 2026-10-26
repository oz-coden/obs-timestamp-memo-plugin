# OBS Timestamp Memo Plugin

[日本語はこちら](#日本語) / English

Record timestamps and editing notes without leaving OBS Studio.

## Table of Contents

1. [Summary](#summary)
2. [Features](#features)
3. [Requirements](#requirements)
4. [Installation](#installation)
5. [Usage](#usage)
6. [Settings](#settings)
7. [Troubleshooting](#troubleshooting)
8. [Documentation](#documentation)
9. [License](#license)
10. [Credits](#credits)

## Summary

OBS Timestamp Memo adds a dock for marking moments to revisit during video editing.
Stamp a marker with a button or hotkey, attach a note, and export the marker list
when recording ends. Markers and notes can also be edited after recording.

The plugin works with **recording**. Streaming-only timestamps are not supported.

## Features

- Four marker slots with configurable labels, colors and OBS hotkeys.
- Memo input, editable marker labels/comments, and Undo/Redo.
- Recording pause/resume and split-file support.
- Automatic JSON saving, recovery journals and an Unsaved document menu.
- JSON, CSV, EDL, XML, SRT, VTT, Markdown and chapters-text exports.
- English and Japanese UI through OBS's language setting.

See [SPEC.md](SPEC.md) for timing accuracy, export limitations, recovery behavior
and resource limits.

## Requirements

| Item | Requirement |
|---|---|
| Host | OBS Studio 31.x or later with its Qt6 frontend |
| Windows package | Windows x64; use the matching x64 OBS installation |
| macOS package | Universal bundle for Apple Silicon and Intel; an OS supported by OBS |
| Linux package | Ubuntu 24.04 x86_64 build; compatible native OBS/Qt6 libraries |

OBS supplies the frontend and Qt runtime; installing a separate Qt SDK is not
part of the normal plugin installation. The CI builds and automated tests do not
replace testing the plugin in an actual OBS recording. See
[compatibility and verification](SPEC.md#compatibility-and-verification).

## Installation

Download a build from [Releases](https://github.com/oz-coden/obs-timestamp-memo-plugin/releases).
If no release is listed yet, a distribution has not been published. Select the
asset for your OS, rather than GitHub's **Source code (zip)** or **Source code
(tar.gz)** downloads. Those contain source files, not an installable plugin.

Close OBS before installing or updating the plugin.

### Windows

1. Download `obs-timestamp-memo-<version>-windows-x64.zip` and extract it to a
   temporary folder.
2. Locate the OBS installation folder. A standard installation is
   `C:\Program Files\obs-studio`; it contains `bin`, `data` and `obs-plugins`.
   For portable OBS, use that installation's root folder.
3. Copy the archive's **`obs-plugins`** and **`data`** folders into the OBS root.
   Merge the folders and replace this plugin's old files when updating; retain
   other plugins and OBS files.
4. Check that these files exist under the OBS root:

   ```text
   obs-plugins/64bit/obs-timestamp-memo.dll
   data/obs-plugins/obs-timestamp-memo/locale/en-US.ini
   data/obs-plugins/obs-timestamp-memo/locale/ja-JP.ini
   ```

5. Start OBS and enable **Timestamp Memo & Markers** from the **Docks** menu.

The installation folder may require administrator permission. Avoid installing a
second copy into another plugin directory when updating an existing installation.

### macOS

1. Download `obs-timestamp-memo-<version>-macos-universal.tar.xz` and extract it.
2. In Finder, choose **Go → Go to Folder** and open:

   ```text
   ~/Library/Application Support/obs-studio/plugins/
   ```

3. Create the `plugins` folder if it does not exist. Copy the complete
   **`obs-timestamp-memo.plugin`** bundle into it. Keep the bundle intact.
4. Start OBS and enable **Timestamp Memo & Markers** from **Docks**.

The archive contains a universal bundle; separate Intel/Apple Silicon archives
are not needed. Downloaded builds are not necessarily Developer ID signed or
notarized; see [SPEC.md](SPEC.md#compatibility-and-verification).

### Linux: native OBS installation

1. Download `obs-timestamp-memo-<version>-ubuntu-24.04-x86_64.tar.xz` and extract it.
2. From the extracted directory, copy the plugin folder to your OBS plugin root:

   ```sh
   plugin_root="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins"
   mkdir -p "$plugin_root"
   cp -a obs-timestamp-memo "$plugin_root/"
   ```

3. Check the resulting layout (with the default configuration root):

   ```text
   ~/.config/obs-studio/plugins/obs-timestamp-memo/bin/64bit/obs-timestamp-memo.so
   ~/.config/obs-studio/plugins/obs-timestamp-memo/data/locale/en-US.ini
   ~/.config/obs-studio/plugins/obs-timestamp-memo/data/locale/ja-JP.ini
   ```

4. Start OBS and enable the dock from **Docks**.

This build targets Ubuntu 24.04 and a native OBS installation. Flatpak, Snap and
other distributions can require different paths and compatible builds; the above
archive is not verified for those environments.

### Update or uninstall

Back up plugin settings and recovery data before updating; their locations are
listed in [SPEC.md](SPEC.md#configuration-and-storage). Close OBS, replace the
plugin files using the same installation method, then reopen OBS.

To uninstall, close OBS and remove only this plugin's DLL/data directories or
`.plugin` bundle/plugin folder. Settings and saved JSON documents remain separate
from the binaries. Do not delete recovery files before saving needed documents.

## Usage

1. Open **Docks → Timestamp Memo & Markers** and start an OBS recording.
2. Click one of the four marker buttons to stamp a moment. To attach a memo, type
   into the memo field and press **Enter** or **Stamp Memo**.
3. Double-click a label or comment in the table to edit it. The context menu
   changes marker type, copies text or deletes a marker.
4. Use **Undo / Redo** or the platform-standard shortcuts to undo marker edits.
   Text inputs retain their own text-editing shortcuts. Delete/Backspace deletes
   the selected marker when the table has focus.
5. Stop recording. Automatic JSON saving is enabled by default and writes beside
   the video. **Export** lets you choose another format or destination.
6. If **Unsaved (N)** appears, choose a document and use **Save JSON as** to keep
   it permanently. **Discard** deletes that document's recovery files after
   confirmation.

Use **Open JSON** to open a previous JSON document or `.tmp.jsonl` recovery
journal while recording is stopped. Opening an arbitrary export such as CSV is
not supported. The last document is not loaded automatically at startup.

To assign shortcuts, open **OBS Settings → Hotkeys**, search for **Timestamp
Memo**, and assign **Quick Marker 1–4** and/or **Focus Memo Input**.

## Settings

Open **Settings** in the plugin dock. Save changes with **Save Settings**.

| Setting | Purpose | Default |
|---|---|---|
| Marker slots | Label and color for each of the four buttons | Marker 1 / Chapter / Highlight / Cut / Edit |
| Automatic JSON | Save the final marker document on recording stop/split and subsequent edits | On |
| Other automatic exports | Also write selected formats on stop/split | Off |
| Ordinary status notifications | Show success and recording-state notifications | On |

With automatic JSON off, recovery journals remain enabled and documents stay in
**Unsaved** until explicitly saved or discarded. Ordinary Export preserves a
snapshot, but does not clear the Unsaved state; use **Unsaved → Save JSON as**
for that. Warnings remain visible even when ordinary notifications are off.

## Troubleshooting

- **The dock is missing:** check that the package matches your OS/architecture,
  both the binary and locale files are installed, and OBS was restarted. Open
  **Help → Log Files → View Current Log** and search for `obs-timestamp-memo` or
  `Timestamp Memo` to check loading errors.
- **A marker cannot be stamped:** start a recording. Streaming alone is not
  enough. Some outputs also need their first timed video packet before stamping.
- **Saving failed or Unsaved appears:** use **Save JSON as** to choose a writable
  destination, and keep recovery files until the needed document is rescued.
- **Timestamps differ from the video:** timing is an estimate. Pause, encoder and
  muxer behavior can affect accuracy; see [timing](SPEC.md#timing-and-accuracy).

## Documentation

- [Specification, limits and compatibility](SPEC.md)
- [Changelog](CHANGELOG.md)
- [Building and testing](docs/BUILDING.md)
- [Manual release procedure](docs/RELEASING.md)
- [Report an issue](https://github.com/oz-coden/obs-timestamp-memo-plugin/issues)

## License

This project is released under the [GNU General Public License v2.0](LICENSE).

## Credits

Developed by [oz-coden](https://github.com/oz-coden), using the OBS Studio API and
Qt6. The build and packaging structure is based on
[obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate).

---

## 日本語

### 目次

1. [概要](#概要)
2. [機能](#機能)
3. [動作環境](#動作環境)
4. [導入方法](#導入方法)
5. [使い方](#使い方)
6. [設定](#設定)
7. [トラブル対応](#トラブル対応)
8. [関連文書](#関連文書)
9. [ライセンス](#ライセンス)
10. [クレジット](#クレジット)

### 概要

録画中に「編集で見返したい瞬間」とメモを残すためのOBSドックです。ボタンや
ホットキーでマーカーを記録し、録画後に一覧を編集・出力できます。

対象は**録画**です。配信だけを行っている場合のタイムスタンプ記録には対応していません。

### 機能

- ラベル・色・OBSホットキーを設定できる4つのマーカースロット。
- メモ入力、ラベル・コメントの編集、Undo/Redo。
- 録画の一時停止・再開、ファイル分割への対応。
- JSON自動保存、復旧用ジャーナル、未保存文書の管理。
- JSON・CSV・EDL・XML・SRT・VTT・Markdown・チャプターテキストへの出力。
- OBSの言語設定に合わせた英語・日本語UI。

時刻精度、出力形式の制約、保存・復旧、履歴上限などの詳細は[SPEC.md](SPEC.md)を参照してください。

### 動作環境

| 項目 | 条件 |
|---|---|
| OBS | Qt6フロントエンドを使用するOBS Studio 31.x以降 |
| Windows配布物 | Windows x64とx64版OBS |
| macOS配布物 | Apple Silicon・Intel共通。OBSが対応するmacOS |
| Linux配布物 | Ubuntu 24.04 x86_64向け。互換性のあるネイティブ版OBS・Qt6が必要 |

通常の導入では、Qt SDKを別途インストールする必要はありません。CIでのビルド・自動テストは、
実際のOBSでの録画確認とは別です。[互換性・検証状況](SPEC.md#互換性検証状況)も参照してください。

### 導入方法

[Releases](https://github.com/oz-coden/obs-timestamp-memo-plugin/releases)から、OSに合った配布物を取得します。
リリースが表示されない場合は、まだ配布物が公開されていません。GitHubが自動表示する
**Source code (zip)**・**Source code (tar.gz)**は導入用バイナリではありません。

導入・更新の前にOBSを終了してください。

#### Windows

1. `obs-timestamp-memo-<version>-windows-x64.zip`をダウンロードし、一時フォルダへ展開します。
2. OBSのインストール先を開きます。通常は`C:\Program Files\obs-studio`です。
   `bin`・`data`・`obs-plugins`が並ぶ階層を使います。ポータブル版では、そのOBSのルートを使います。
3. 展開した**`obs-plugins`と`data`**をOBSのルートへコピーします。
   フォルダを結合し、更新時はこのプラグインの旧ファイルを上書きします。
   OBS本体や他のプラグインのファイルは残してください。
4. 次の配置になっていることを確認します。

   ```text
   obs-plugins/64bit/obs-timestamp-memo.dll
   data/obs-plugins/obs-timestamp-memo/locale/en-US.ini
   data/obs-plugins/obs-timestamp-memo/locale/ja-JP.ini
   ```

5. OBSを起動し、**ドック → Timestamp Memo / マーカー**を表示します。

コピー先によっては管理者権限が必要です。更新時は既存の導入先を使い、別のプラグインフォルダへ
同じプラグインを重複配置しないようにしてください。

#### macOS

1. `obs-timestamp-memo-<version>-macos-universal.tar.xz`をダウンロードして展開します。
2. Finderの**移動 → フォルダへ移動**から、次の場所を開きます。

   ```text
   ~/Library/Application Support/obs-studio/plugins/
   ```

3. `plugins`がなければ作成し、**`obs-timestamp-memo.plugin`全体**をコピーします。
   バンドル内部のファイルは分けず、そのまま配置してください。
4. OBSを起動し、ドックメニューから表示します。

Apple Silicon・Intel共通のバンドルです。配布物はDeveloper ID署名・公証済みとは限りません。
詳細は[SPEC.md](SPEC.md#互換性検証状況)を参照してください。

#### Linux：ネイティブ版OBS

1. `obs-timestamp-memo-<version>-ubuntu-24.04-x86_64.tar.xz`を取得して展開します。
2. 展開先で次を実行し、プラグインフォルダをOBSの設定ディレクトリへコピーします。

   ```sh
   plugin_root="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins"
   mkdir -p "$plugin_root"
   cp -a obs-timestamp-memo "$plugin_root/"
   ```

3. 標準設定では、次の配置になります。

   ```text
   ~/.config/obs-studio/plugins/obs-timestamp-memo/bin/64bit/obs-timestamp-memo.so
   ~/.config/obs-studio/plugins/obs-timestamp-memo/data/locale/en-US.ini
   ~/.config/obs-studio/plugins/obs-timestamp-memo/data/locale/ja-JP.ini
   ```

4. OBSを起動し、ドックメニューから表示します。

この配布物はUbuntu 24.04のネイティブ版OBS向けです。Flatpak・Snap・他ディストリビューションでは
配置先や依存ライブラリが異なる場合があり、この配布物での動作は未検証です。

#### 更新・アンインストール

更新前に設定・復旧データをバックアップしてください。保存先は[SPEC.md](SPEC.md#設定保存先)に記載しています。
OBS終了後、導入時と同じ場所のプラグインファイルを差し替えます。

削除する場合もOBSを終了し、このプラグインのDLL・データフォルダ、`.plugin`バンドル、
またはプラグインフォルダだけを取り除きます。設定・保存済みJSONはバイナリとは別に残ります。
必要な文書を保存するまでは、復旧ファイルを削除しないでください。

### 使い方

1. **ドック → Timestamp Memo / マーカー**を表示し、OBSで録画を開始します。
2. 4つのマーカーボタンのいずれかを押して打刻します。メモ付きで残す場合は、入力欄に書いて
   **Enter**または**メモを打刻**を押します。
3. 表のラベル・コメントをダブルクリックして編集します。右クリックメニューでは種別変更・
   コピー・削除ができます。
4. **Undo/Redo**またはOS標準のショートカットで編集を戻せます。文字入力中は入力欄自身のUndoが優先します。
   表にフォーカスがある状態でDelete/Backspaceを押すと、選択マーカーを削除します。
5. 録画を停止します。初期設定では動画の隣にJSONを自動保存します。**Export**から形式や保存先を選ぶこともできます。
6. **未保存 (N)**が表示された場合は、対象文書から**JSONを別名保存**して恒久保存します。
   **破棄**は確認後にその文書の復旧ファイルを削除します。

録画停止中の**JSONを開く**で、保存済みJSONや`.tmp.jsonl`復旧ジャーナルを読み込めます。
CSVなどの任意の出力形式を読み込む機能ではありません。前回の文書は起動時に自動では開きません。

ショートカットは**OBSの設定 → ホットキー**で「Timestamp Memo」を検索し、
**クイックマーカー1〜4**や**メモ入力欄へフォーカス**へ割り当てます。

### 設定

ドック内の**設定**を開き、変更後に**設定を保存**を押します。

| 設定 | 内容 | 初期値 |
|---|---|---|
| マーカースロット | 4ボタンのラベル・色 | Marker 1 / Chapter / Highlight / Cut / Edit |
| JSON自動保存 | 停止・分割時と、その後の編集で最終状態を保存 | ON |
| その他の自動出力 | 停止・分割時に選んだ形式も出力 | OFF |
| 通常のステータス通知 | 成功・録画状態の通知 | ON |

JSON自動保存がOFFでも復旧ジャーナルは更新され、明示的に保存・破棄するまで**未保存**に残ります。
通常のExportはスナップショットの出力であり、未保存状態を解除しません。
救済を確定する場合は**未保存 → JSONを別名保存**を使います。通常通知をOFFにしても警告は表示されます。

### トラブル対応

- **ドックが見つからない：** OS・CPU構成に合った配布物か、バイナリと翻訳ファイルの両方を配置したか、
  OBSを再起動したかを確認します。**ヘルプ → ログファイル → 現在のログを表示**で
  `obs-timestamp-memo`や`Timestamp Memo`を検索すると、読み込みエラーを確認できます。
- **打刻できない：** 録画を開始してください。配信だけでは打刻できません。
  出力方式によっては最初の時刻付き映像パケットを待つ必要もあります。
- **保存失敗・未保存表示：** **JSONを別名保存**から書き込み可能な場所へ救済します。
  保存が済むまでは復旧ファイルを残してください。
- **動画と時刻がずれる：** 時刻は推定値です。一時停止・エンコーダー・muxerによる影響は
  [時刻・精度](SPEC.md#時刻精度)を参照してください。

### 関連文書

- [詳細仕様・性能・制約・互換性](SPEC.md)
- [変更履歴](CHANGELOG.md)
- [ビルド・テスト](docs/BUILDING.md)
- [手動リリース手順](docs/RELEASING.md)
- [不具合報告](https://github.com/oz-coden/obs-timestamp-memo-plugin/issues)

### ライセンス

[GNU General Public License v2.0](LICENSE)で公開しています。

### クレジット

開発：[oz-coden](https://github.com/oz-coden)。OBS Studio API・Qt6を利用しています。
ビルド・パッケージ構成は[obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)をベースにしています。
