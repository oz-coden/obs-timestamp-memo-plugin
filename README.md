# OBS Timestamp Memo Plugin (`obs-timestamp-memo`)

OBS Studio 録画中に高精度タイムスタンプ付きでメモやマーカーをリアルタイム打刻・編集し、動画編集ソフト（Premiere Pro, DaVinci Resolve, Final Cut Pro等）向けの各種フォーマットへエクスポートできるOBSプラグインです。

---

## 主な機能・特徴

- ⏱ **高精度タイムコード & フレーム計算**
  - ポーズ（一時停止）時間を完全除外した動画タイムライン基準のミリ秒を取得
  - 端数フレームレート（59.94 fps, 29.97 fps, 23.976 fps 等）の完全一致フレーム計算
  - ドロップフレーム（DF: `HH:MM:SS;FF`）/ ノンドロップフレーム（NDF: `HH:MM:SS:FF`）の自動判別

- 🎯 **柔軟な入力 UX**
  - **4つのクイックマーカースロット**: ホットキーおよびドックUIボタンからワンクリックで打刻（カラー・ラベル設定可能）
  - **テキストメモ入力**: テキストを入力して Enter で現在タイムスタンプにメモを追加
  - **インライン編集**: 録画中・録画後問わずドック内テーブルからコメントを直接加筆・修正
  - **ステータス通知**: 打刻時にOBSステータスバーへ即座にフィードバック表示

- 🛡 **耐障害性 & 自動分割連携**
  - **クラッシュ対策**: マーカー打刻のたびに 1 行ずつテンポラリ JSON (`.tmp.jsonl`) に即時フラッシュ
  - **OBSファイル自動分割（Split Recording）連携**: `OBS_FRONTEND_EVENT_RECORDING_FILE_CHANGED` を検知し、分割ごとに前ファイルを確定保存・新ファイル用のセッションを生成

- 📤 **多彩なエクスポートフォーマット**
  - **JSON (`.json`)**: プラグイン専用フォーマット（全メタデータ + マーカー配列）
  - **CSV (`.csv`)**: DaVinci Resolve / Premiere Pro 互換マーカーCSV（UTF-8 BOM付き）
  - **CMX 3600 EDL (`.edl`)**: 各種NLEにインポート可能なタイムコードマーカーリスト（DF/NDF対応）
  - **SRT 字幕 (`.srt`)**: タイムライン上にテキストとして表示可能な字幕ファイル
  - **Premiere Pro XML (`.xml`)**: Final Cut Pro 7 XML / Premiere Pro シーケンスマーカー
  - **自動書き出し設定**: 録画終了時に指定フォーマットを動画と同じフォルダに自動出力

- 📂 **過去の録画の読み込み・再エクスポート**
  - ドックUIから過去の録画 JSON を開き、メモの追記・修正や別形式への再エクスポートが可能

---

## ビルド方法

### 必要要件
- CMake 3.28 以上
- C++20 対応コンパイラ (MSVC 2022 / Clang / GCC)
- Qt 6 (Core, Gui, Widgets)
- OBS Studio 31.x 以上 (libobs, obs-frontend-api)

### ビルド手順 (Windows)

```powershell
# CMake Configure (Preset利用)
cmake --preset windows-x64

# ビルド
cmake --build --preset windows-x64 --config RelWithDebInfo
```

---

## ライセンス

GPL-2.0-or-later
