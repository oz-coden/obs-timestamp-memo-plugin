# Manual releases

[日本語](#日本語) / English

Releases are created only by manually running the **Release** workflow.
Normal branch pushes and tag pushes do not publish releases. Version numbers are
chosen by the maintainer and are never incremented automatically.

## Before running

1. Set the intended version in `buildspec.json`, for example `1.0.0`, `1.0.1`,
   `1.0.0-beta1` or `1.0.0-rc.1`. Do not include a `v` prefix. Numeric components
   must be 0..65535 without leading zeroes. Build metadata (`+...`) is unsupported.
2. Update `CHANGELOG.md` and ensure installation/specification changes are
   documented. Commit the changes and push the selected branch.
3. Check CI and, before claiming runtime compatibility, test a real OBS recording
   on the intended environment. CI uses OBS stubs for automated behavior tests.

The workflow file must exist on the default branch for GitHub to show the
**Run workflow** button. After it is available there, select the branch/tag to
build in the workflow's branch selector. The run uses that selected commit even
if the branch advances while builds are running.

## Run the workflow

1. Open the repository's **Actions** tab and select **Release**.
2. Click **Run workflow** and choose the branch containing the intended source
   and `buildspec.json` version (normally `master`).
3. Enter that exact version in **version**.
4. Choose the options below, then click **Run workflow**.

| Input | Default | Meaning |
|---|---|---|
| `version` | Required | Exact version already committed in `buildspec.json` |
| `publish` | On | Create/reuse the matching tag and create a GitHub Release after successful validation |
| `draft` | Off | Keep the created Release unpublished for later inspection |

For a build-only trial, turn **publish off**. It builds, tests and prepares the
same distribution files, but creates neither a tag nor a Release. Download the
`release-files-<commit>` artifact from the completed run. Turning **draft on**
instead creates a tag and an unpublished draft Release; the two options are not
the same.

The equivalent CLI invocation (after authenticating GitHub CLI) is:

```sh
gh workflow run release.yaml --ref master -f version=1.0.0 -F publish=true -F draft=false
```

Replace the version and ref explicitly. To make a build-only trial, use
`-F publish=false`.

## What the workflow does

1. Rejects invalid versions or a mismatch with `buildspec.json`. A version tag
   pointing at another commit is rejected before building when publishing.
2. Builds Release binaries and runs the domain/application/Qt-OBS-stub suites on
   Windows, macOS and Ubuntu.
3. Verifies archive names, binaries, both locale files and bundled license/docs,
   and verifies the source archive. Missing, duplicate or empty required payloads
   fail the run.
4. Prepares the four archives below and `SHA256SUMS.txt` as an Actions artifact.
5. If `publish` is enabled, creates a lightweight version tag at the selected
   commit if needed. Existing tags must identify the same commit.
6. Creates a draft Release with all five assets, then publishes it unless
   `draft` was requested. A version with a suffix is marked as a prerelease.

| Asset | Contents / target |
|---|---|
| `obs-timestamp-memo-<version>-windows-x64.zip` | DLL and locale data in the OBS installation layout |
| `obs-timestamp-memo-<version>-macos-universal.tar.xz` | Complete Apple Silicon/Intel `.plugin` bundle |
| `obs-timestamp-memo-<version>-ubuntu-24.04-x86_64.tar.xz` | Native Linux user-plugin directory |
| `obs-timestamp-memo-<version>-source.tar.xz` | Source archive for building |
| `SHA256SUMS.txt` | SHA-256 hashes of the four archives |

The ready-to-install archives also include `LICENSE`, README, SPEC, changelog and
developer documentation. No external OBS/Qt runtime is bundled. macOS signing
credentials are optional. The archive workflow does not perform notarization;
without signing credentials the bundle is not Developer ID signed either.

On Linux/macOS, validate downloaded archives with:

```sh
sha256sum --check SHA256SUMS.txt
```

On a macOS installation without `sha256sum`, use `shasum -a 256 -c SHA256SUMS.txt`.
On Windows, use `Get-FileHash <archive.zip> -Algorithm SHA256` and compare the
result with the corresponding line in `SHA256SUMS.txt`.

## Failed or repeated runs

A failed build/validation creates no tag or Release. If publication fails after
creating the tag, rerun at the same source commit; a matching tag can be reused.
Existing Releases are not overwritten by a rerun. If a failed asset upload leaves
a draft, inspect it and either finish it manually or delete that draft before
retrying the same version. Published versions should receive a new version
rather than replacing their binaries.

Only the final publish job needs `contents: write`. The workflow uses the
repository's `GITHUB_TOKEN`; a separate personal access token is not required.
Repository rules can still restrict tag/Release creation.

---

## 日本語

**Releaseワークフローを手動実行したときだけ**リリースします。通常のpush・タグのpushでは
公開しません。バージョンは保守者が決め、勝手に採番・更新しません。

### 準備

1. `buildspec.json`の`version`を`1.0.0`、`1.0.1`、`1.0.0-beta1`、`1.0.0-rc.1`などへ変更します。
   `v`は付けません。数値部分は先頭ゼロなしの0〜65535で、`+...`のビルドメタデータは対象外です。
2. `CHANGELOG.md`と必要な導入・仕様文書を更新し、コミットしてpushします。
3. CIを確認します。実機での動作確認とOBSスタブの自動テストは別なので、互換性を案内する前に
   対象環境で実際の録画も確認してください。

GitHubにRun workflowを表示させるには、ワークフローがデフォルトブランチへ反映されている必要があります。
実行時のブランチ選択で、配布したいソースとバージョンを含むブランチ・タグを選びます。
ビルド中にブランチが進んでも、実行開始時のコミットを配布します。

### 実行

1. **Actions → Release → Run workflow**を開きます。
2. 対象ブランチ（通常`master`）を選びます。
3. **version**へ、コミット済み`buildspec.json`と同じバージョンを入力します。
4. **publish**と**draft**を選んで実行します。

| 入力 | 初期値 | 意味 |
|---|---|---|
| `version` | 必須 | コミット済みのバージョンと完全一致させる |
| `publish` | ON | 成功後にタグ・GitHub Releaseを作成する |
| `draft` | OFF | 作成したReleaseを未公開の下書きで止める |

**公開せず動作確認する場合はpublishをOFF**にします。同じビルド・テスト・配布物検証を行いますが、
タグ・Releaseは作りません。完了した実行のArtifactsから`release-files-<commit>`を取得できます。
**draftをON**にするとタグと下書きReleaseを作るので、publish OFFとは異なります。

CLIの場合は、GitHub CLIで認証したうえで次を実行します。

```sh
gh workflow run release.yaml --ref master -f version=1.0.0 -F publish=true -F draft=false
```

バージョン・ブランチを明示的に置き換え、公開しない確認では`-F publish=false`を使います。

### 生成物と失敗時

3 OSのReleaseビルド・全テスト、アーカイブの中身とソースの検証、チェックサム計算を行います。
バージョン不一致・別コミットを指す同名タグ・必要ファイルの不足や重複は失敗扱いです。

配布するのはWindows ZIP、macOS universal tar.xz、Ubuntu 24.04 x86_64 tar.xz、ソースtar.xzと
`SHA256SUMS.txt`の5つです。導入用アーカイブにはライセンスと各文書も同梱し、OBS/Qt本体は同梱しません。
macOSの署名用認証情報は任意です。このアーカイブ用ワークフローは公証を行わず、
署名情報がない場合はDeveloper ID署名も行いません。

publish ONでは選択コミットへタグを作り、添付物をそろえた下書きReleaseを作成してから公開します。
バージョンにsuffixがあればプレリリースとして扱い、draft ONなら下書きのまま止めます。
ビルド・検証の失敗時はタグ・Releaseを作りません。

タグ作成後に公開処理が失敗した場合は、同じコミットのタグを再利用できます。
既存Releaseは上書きしません。添付途中の下書きが残った場合は内容を確認し、手動で完成させるか、
その下書きを削除して再実行します。公開済みバイナリの差し替えには新しいバージョンを使います。

通常はリポジトリの`GITHUB_TOKEN`を使い、個人アクセストークンは不要です。
書き込み権限を使うのは最後の公開ジョブだけですが、リポジトリ側のタグ・Release制限は適用されます。
