# Building and testing

[日本語](#日本語) / English

For installation of published binaries, use [README.md](../README.md#installation).

## Tools and dependencies

Use Git, CMake 3.28+, a C++20 compiler and Python 3.11+ for release tooling.
The plugin requires OBS's frontend API and Qt6 Core/Gui/Widgets. Windows uses
Visual Studio 2022 with the C++ workload and Windows SDK 10.0.22621. macOS builds
require Xcode 16+ and a macOS 15 SDK; CI selects Xcode 16.1. Linux builds require
Ninja and OBS/Qt development packages compatible with the target installation.

Windows/macOS CMake presets download pinned OBS sources and obs-deps/Qt from
`buildspec.json`. Linux uses installed native OBS/Qt libraries. The Ubuntu CI
setup adds the OBS PPA and installs its packages; it does not pin OBS to the
Windows/macOS source version.

## Build from a checkout

Clone the repository and run the commands for the host platform from its root:

```sh
git clone https://github.com/oz-coden/obs-timestamp-memo-plugin.git
cd obs-timestamp-memo-plugin
```

Windows (Developer PowerShell with the Visual Studio tools available):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release --parallel
cmake --install build_x64 --config Release --prefix release/Release
```

macOS (Xcode and its command-line tools selected with `xcode-select`):

```sh
cmake --preset macos
cmake --build --preset macos --config Release --parallel
cmake --install build_macos --config Release --prefix release/Release
```

Linux, after installing a compatible OBS development environment and Qt6:

```sh
cmake --preset ubuntu-x86_64 -DCMAKE_BUILD_TYPE=Release
cmake --build --preset ubuntu-x86_64 --config Release --parallel
cmake --install build_x86_64 --config Release --prefix release/Release
```

Installation here stages files in the checkout; it does not install into a running
OBS instance. To generate an installable archive, select the corresponding host:

```sh
python .github/scripts/release.py archive --platform windows-x64 --config Release
python3 .github/scripts/release.py archive --platform macos-universal --config Release
python3 .github/scripts/release.py archive --platform ubuntu-24.04-x86_64 --config Release
```

Run only the command for the platform you built. Published archives include the
binary, locale data, license and documentation, not OBS or a separate Qt runtime.
Do not copy build-only staging folders directly according to the archive's
installation instructions; the archive generator arranges the final layout.

## Tests

Standalone domain/application/Qt-OBS-stub commands are in
[tests/README.md](../tests/README.md). The release tooling has a separate stdlib
Python test suite:

```sh
python3 .github/scripts/test-release.py
```

CI runs all three plugin suites on Windows/macOS/Ubuntu and checks C++/CMake
formatting. Pull requests additionally build in Release configuration and validate
the complete distribution set without creating tags or GitHub Releases.

To perform a build through GitHub instead, open **Actions → Dispatch → Run
workflow**, choose `build`, and select a branch. This produces Actions artifacts;
it does not publish a release. Use [Release](RELEASING.md) when publishing.

## Versions

Edit `buildspec.json` manually. Stable versions use `MAJOR.MINOR.PATCH`;
prereleases add a suffix such as `-beta1` or `-rc.1`. CMake/native numeric metadata
uses the three numeric components, while the plugin version string and archive
names retain the full version. Nothing increments the version automatically.

---

## 日本語

配布済みバイナリの導入は[README](../README.md#導入方法)を参照してください。

Git・CMake 3.28以降・C++20コンパイラー・OBSフロントエンドAPI・Qt6が必要です。
配布処理にはPython 3.11以降を使います。WindowsはVisual Studio 2022のC++環境と
Windows SDK 10.0.22621、macOSはXcode 16以降とmacOS 15 SDKを使います。
Linuxでは対象OBSと互換性のある開発ライブラリ・Qt6・Ninjaを準備します。

上のcloneコマンドとOS別のconfigure/build/installを実行すると、`release/Release`へ
ステージングします。これはOBS本体へのインストールではありません。
対応OSの`release.py archive`を実行すると、READMEの導入手順に合ったアーカイブを作成します。
Windows/macOSの固定依存と、Ubuntuのネイティブ依存は区別してください。

プラグインの自動テスト手順は[tests/README.md](../tests/README.md)にあります。
配布処理は`python3 .github/scripts/test-release.py`でも検証できます。
PRではReleaseビルドと配布物全体の検証まで行い、タグ・Releaseは作りません。

手元でビルドせず確認する場合は**Actions → Dispatch → Run workflow**で`build`を選びます。
生成先はActionsのArtifactsです。公開は[手動リリース手順](RELEASING.md)で行います。

バージョンは`buildspec.json`を手動で変更します。`1.0.0`や`1.0.0-beta1`などを使えます。
ネイティブの数値メタデータは3つの数値部分を使い、プラグインの文字列・配布名には完全な
バージョンを保持します。自動採番・自動バージョン更新は行いません。
