# OBS Timestamp Memo Plugin

Japanese section is [available](#目次) in below this section.

## Table of Contents

1. [Summary](#summary)
2. [Background of Development](#background-of-development)
3. [Features](#features)
4. [Usage](#usage)
5. [Requirements, Dependencies](#requirements-dependencies)
6. [Installation](#installation)
7. [Setting](#setting)
8. [LICENSE](#license)
9. [CREDIT](#credit)

## Summary

**OBS Timestamp Memo** is a high-precision timestamping and marker annotation dock plugin for OBS Studio. It enables creators, streamers, and video editors to stamp chapter markers, edit points, highlights, and comments in real-time during recording.  
Recorded markers can be exported to multiple formats including YouTube Chapters, Markdown documents, DaVinci Resolve CSV, CMX 3600 EDL, WebVTT / SRT subtitles, and Premiere Pro XML.

## Background of Development

During long live streams, gaming sessions, or recording workflows, locating highlights, memorable moments, or cut points in post-production is extremely tedious and time-consuming. Scrubbing through hours of footage to find a single moment wastes significant editing time.  
Existing external note-taking tools are detached from OBS, leading to timecode discrepancies caused by recording pauses or fractional frame rate drops (e.g., 59.94 fps).  
This plugin was developed to bridge OBS Studio and video editing software (NLEs), providing exact, pause-compensated, frame-accurate marker logging with a single keystroke or click.

## Features

- **Frame-Accurate Timecode & Frame Calculation**  
  Synchronizes strictly with the recorded video timeline by automatically excluding pause durations. Supports fractional frame rates (59.94, 29.97, 23.976 fps) and automatic SMPTE Drop Frame (DF: `HH:MM:SS;FF`) / Non-Drop Frame (NDF: `HH:MM:SS:FF`) formatting.
- **Quick Marker Slots & Hotkeys**  
  4 customizable quick marker buttons with distinct labels and colors, assignable to global OBS hotkeys for instant stamping during gameplay or full-screen activities.
- **Inline Memo & Comment Input**  
  Add custom memo notes on the fly via the text input box (press `Enter`), or double-click table cells in the dock to modify labels and comments at any time.
- **Diverse Export Formats & Clipboard Integration**  
  - **YouTube Chapters (`.chapters.txt` / One-click Copy)**: Auto-formatted as `00:00 Chapter Title` (with automatic `00:00 Start` fallback and `hh:mm:ss` scaling).
  - **Markdown Document (`.md` / One-click Copy)**: Complete session report with metadata, bulleted chapter list, and detailed table for Obsidian, Notion, or GitHub.
  - **WebVTT (`.vtt`) & SRT (`.srt`)**: Video subtitle cues with millisecond accuracy.
  - **CSV (`.csv`)**: Compatible with DaVinci Resolve marker import and spreadsheet workflows (UTF-8 with BOM).
  - **CMX 3600 EDL (`.edl`)**: Industry-standard edit decision list for NLE timeline markers.
  - **Premiere Pro XML (`.xml`)**: Sequence markers for Adobe Premiere Pro and Final Cut Pro 7 XML workflows.
  - **Plugin JSON (`.json`)**: Complete session archive preserving all metadata and marker history.
- **Crash Resilience & Auto-Split Support**  
  - **Real-time Journaling**: Flushes every marker to `.tmp.jsonl` immediately upon stamping, preventing data loss on unexpected power cuts or crashes.
  - **OBS File Split Integration**: Seamlessly saves the finished segment and starts a fresh session when OBS auto-splits recordings.
- **Session History & Reloading**  
  Load and review past session JSON files or recover interrupted crash caches directly from the dock UI.

## Usage

1. **Show the Dock**: In OBS Studio, open `Docks` menu -> check `Timestamp Memo`.
2. **Stamping Markers**:
   - Click one of the numbered Quick Marker buttons (1 to 4) or press your configured hotkey.
   - Or type a memo into the text input box at the bottom and press `Enter`.
3. **Editing Markers**:
   - Double-click any cell in the dock table to edit label or comment inline.
   - Right-click any row to open the context menu: copy timecodes, change marker type, copy YouTube chapters to clipboard, or delete the marker.
4. **Exporting**:
   - Click `Export...` to choose any target format, or click `Copy to Clipboard` to directly paste chapters or Markdown into YouTube description or Notion.
   - Configure automatic export upon recording stop in settings.

## Requirements, Dependencies

- OBS Studio 31.0 or later
- Supported OS:
  - Windows 10 / 11 (x64)
  - macOS 13+ (Apple Silicon & Intel)
  - Linux (Ubuntu 22.04 / 24.04, etc.)
- Qt 6 (bundled with OBS Studio 31+)

## Installation

### Using Pre-built Release

1. Download the latest release archive (`.zip`, `.pkg`, or `.tar.gz`) for your OS from GitHub Releases.
2. Extract or install it into your OBS Studio plugins folder:
   - **Windows**: `C:\Program Files\obs-studio\obs-plugins\64bit\` and `data\obs-plugins\`
   - **macOS**: `~/Library/Application Support/obs-studio/plugins/`
   - **Linux**: `~/.config/obs-studio/plugins/`
3. Restart OBS Studio.

## Setting

Access settings via the `Settings` button in the Timestamp Memo dock:
- **Marker Types**: Customize labels and display colors for the 4 quick marker slots.
- **Auto-Export on Recording Stop**: Select which format(s) (`.json`, `.csv`, `.edl`, `.srt`, `.vtt`, `.chapters.txt`, `.md`, `.xml`) should be automatically saved alongside the video file when recording stops.
- **Hotkeys**: Configure global hotkeys in OBS Studio's main menu `Settings` -> `Hotkeys` (search for `Timestamp Memo`).

## LICENSE

This project is released under the GNU General Public License v2.0 (GPL-2.0). See [LICENSE](LICENSE) for details.

## CREDIT

Developed by oz-coden. Built on OBS Studio API and Qt 6.

---

# OBS Timestamp Memo Plugin

## 目次

1. [概要](#概要)
2. [開発背景](#開発背景)
3. [機能](#機能)
4. [使い方](#使い方)
5. [前提・依存](#前提依存)
6. [導入方法](#導入方法)
7. [設定](#設定)
8. [ライセンス](#ライセンス)
9. [クレジット](#クレジット)

## 概要

**OBS Timestamp Memo** は、OBS Studio での録画中に高精度なタイムスタンプ付きでチャプター・編集点・ハイライト・メモをリアルタイムに記録できるドックプラグインです。  
記録したマーカーは、YouTubeチャプター、Markdownドキュメント、DaVinci Resolve CSV、CMX 3600 EDL、WebVTT / SRT 字幕、Premiere Pro XML など、多彩な形式へエクスポートできます。

## 開発背景

長時間の配信・ゲーム実況・動画撮影において、動画編集時に「見どころ」や「カットしたい場所」を探し出す作業は非常に時間と労力を要します。何時間もある動画素材を最初から見返して目当てのシーンを探すのは、編集効率を大きく低下させる要因です。  
一般的なメモ帳や外部ツールでは、OBSの一時停止（ポーズ）時間や 59.94 fps 等の端数フレームレートによるズレが発生し、動画編集ソフトのタイムラインと正確に一致させることが困難でした。  
本プラグインは、OBS Studio と動画編集ソフト（NLE）を直接結びつけ、ポーズ時間を正確に除外したフレーム精度のタイムコードマーカーをワンキー／ワンクリックで手軽に記録できるようにするために開発されました。

## 機能

- **高精度タイムコード & フレーム計算**  
  ポーズ時間を完全に除外した動画タイムライン基準のミリ秒・フレーム番号を算出。59.94 / 29.97 / 23.976 fps などの端数フレームレートや、SMPTE ドロップフレーム（DF: `HH:MM:SS;FF`）／ノンドロップフレーム（NDF: `HH:MM:SS:FF`）の自動判定に対応。
- **4つのクイックマーカー & ホットキー**  
  カラーとラベルを自由に設定できる4つの打刻ボタンを搭載。OBSのグローバルホットキーに対応し、ゲーム中や全画面表示中でもキー一発で即座に打刻可能。
- **インライン編集 & テキストメモ入力**  
  テキスト入力欄からの Enter キー送信で現在時刻に素早くメモを残せるほか、ドック内のテーブル上でラベルやコメントを直接編集可能。
- **多彩なエクスポート & クリップボード直接コピー**  
  - **YouTube チャプター (`.chapters.txt` / ワンクリックコピー)**: `00:00 チャプター名` 形式（先頭 00:00 自動補完、1時間以上の `hh:mm:ss` 拡大に対応）。
  - **Markdown ドキュメント (`.md` / ワンクリックコピー)**: メタ情報、箇条書きタイムライン、詳細テーブルを含む綺麗な文書形式。Notion、Obsidian、GitHub 等に最適。
  - **WebVTT (`.vtt`) & SRT (`.srt`)**: Webプレイヤーや各種動画編集ソフトでそのまま使える字幕ファイル。
  - **CSV (`.csv`)**: DaVinci Resolve のマーカー取り込みや表計算ソフトに対応（UTF-8 BOM付き）。
  - **CMX 3600 EDL (`.edl`)**: タイムコードマーカーとして各NLEにインポート可能な業界標準フォーマット。
  - **Premiere Pro XML (`.xml`)**: Adobe Premiere Pro や Final Cut Pro 7 形式のシーケンスマーカー。
  - **プラグイン専用 JSON (`.json`)**: すべてのセッションメタデータとマーカーを保持する保存形式。
- **耐障害性（クラッシュ対策）& 自動分割連携**  
  - **リアルタイムジャーナリング**: 打刻のたびに 1 行ずつテンポラリ JSON (`.tmp.jsonl`) に即座に追記フラッシュし、不意のクラッシュや停電によるデータ消失を防止。
  - **録画ファイル自動分割（Split Recording）連携**: OBS のファイル自動分割を検知し、分割ごとに前ファイルを確定保存して新ファイル用セッションを自動生成。
- **セッション履歴の読み込み・再編集**  
  過去の録画 JSON ファイルや中断されたクラッシュキャッシュをドックUIから開き、メモの追記・修正や別形式への再エクスポートが可能。

## 使い方

1. **ドックの表示**: OBS Studio のメニューバーから「ドック」->「Timestamp Memo」にチェックを入れてドックを表示します。
2. **マーカーの打刻**:
   - 画面上のクイックマーカーボタン（1〜4）をクリックするか、割り当てたホットキーを押します。
   - または、下部の入力欄にテキストを入力して `Enter` キーを押します。
3. **マーカーの編集**:
   - テーブル内のセルをダブルクリックして、ラベルやコメントを直接編集できます。
   - テーブルの行を右クリックすると、タイムコードのコピー、マーカータイプの変更、YouTubeチャプターの全コピー、マーカー削除を行えます。
4. **エクスポート**:
   - 「Export...」ボタンから好きな形式を選んで保存できます。「Copy to Clipboard」を押せば、YouTube動画説明欄やNotionへ即座に貼り付け可能です。
   - 設定画面から録画停止時の自動エクスポートを有効にしておくこともできます。

## 前提・依存

- OBS Studio 31.0 以上
- 対応OS:
  - Windows 10 / 11 (x64)
  - macOS 13 以上 (Apple Silicon / Intel)
  - Linux (Ubuntu 22.04 / 24.04 等)
- Qt 6 (OBS Studio 31+ に標準同梱)

## 導入方法

### 配布バイナリを利用する場合

1. GitHub の「Releases」ページからお使いの OS に合わせた最新のアーカイブ（`.zip`, `.pkg`, `.tar.gz` 等）をダウンロードします。
2. OBS Studio のプラグインディレクトリに配置・インストールします：
   - **Windows**: `C:\Program Files\obs-studio\obs-plugins\64bit\` および `data\obs-plugins\`
   - **macOS**: `~/Library/Application Support/obs-studio/plugins/`
   - **Linux**: `~/.config/obs-studio/plugins/`
3. OBS Studio を再起動します。

## 設定

ドック内の「Settings」ボタンをクリックして設定ダイアログを開きます：
- **マーカー設定**: 4つのクイックスロットのラベルとカラーをカスタマイズできます。
- **録画終了時の自動エクスポート**: 録画が停止した際、動画ファイルと同じフォルダへ自動保存する形式（`.json`, `.csv`, `.edl`, `.srt`, `.vtt`, `.chapters.txt`, `.md`, `.xml`）を選択できます。
- **ホットキー設定**: OBS Studio 本体の「設定」->「ホットキー」から `Timestamp Memo` を検索し、お好みのキーを割り当ててください。

## ライセンス

このプロジェクトは GNU General Public License v2.0 (GPL-2.0) の下で公開されています。詳細は [LICENSE](LICENSE) ファイルをご確認ください。

## クレジット

Developed by oz-coden. Built on OBS Studio API and Qt 6.
