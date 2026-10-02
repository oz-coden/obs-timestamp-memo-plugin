# 作業引き継ぎ（2026-10-02）

## 再開後の更新（2026-10-02）

- 再開時のmasterは`2f5a81b`、origin/masterは`0c1b59b`。fetchで未知のremote変更がないこと、working treeがcleanであることを確認した。
- Windows x64 plugin build、test build、CTestのdomain/application/regressionsを再実行し、**3/3成功**。
- 実OBS 32.2.1に入っていたDLLは旧版（SHA256 `7B11E06D9516838C8DBF9DB401EFBDA7FC10D6811027E86C74C63921B0C9C4C7`）だったため、OBS停止中に検証済みDLL/PDBへ更新した。
- 更新後DLLのSHA256は`371273D4350B61065E35C65D7EDA44A90E15350AF33FD68DD278CEE0B052EC43`。build側とinstall先のDLL/PDBのhash一致を確認済み。コードは`0c1b59b`と同一で、本体コードの追加変更はない。
- install先は`C:\Program Files\obs-studio\obs-plugins\64bit`。旧DLL/PDBのbackupは`.cache/obs-validation/previous-plugin-9fa8b38e18844e798dd096dbe99c9dc6`。`.cache/obs-validation/install-result.json`に結果を記録した。
- Program Filesへの通常の書き込みはWindows権限で拒否されたため、UAC承認後、対象をDLL/PDBに限定した`.cache/obs-validation/install-reviewed-plugin.ps1`で更新した。OBS設定・録画・cacheは変更していない。
- **実OBSの確認は一部完了**。ユーザー提供の`2026-10-02 23-00-55.json`とMarkdown Clipboard結果を照合し、35件の全詳細行、4種別、pause中の固定frame、再開後のframe進行を確認した。続く同一文書の編集で日本語を含む4件のcommentがJSON保存と再コピーの両方に反映されたことも確認した。[実機データの記録](docs/obs-manual-test.md)を参照。次はfile Export操作中のsnapshot保持、split、再起動、保存失敗救済、cache衝突を確認する。
- 文書更新とHANDOFFを`7050bef`まで通常push済み。[再開後のCI](https://github.com/oz-coden/obs-timestamp-memo-plugin/actions/runs/37016306206)は全3OS build/testとformat成功。実機データの確認記録は次のローカル文書commitで保存し、確認作業がまとまってからpushする。
- 以下の「現在地」以降は中断時の記録。文書commitのpush状態は再開時に`git status`と`git log origin/master..master`で確認する。

ユーザーの外出に合わせ、コード修正・master統合・push・CI検証が完了した時点で中断する。途中のコード変更はない。再開時はまずGitの実際の状態を確認する。

## 現在地

- リポジトリ: `C:\Users\white\Documents\Workspace\obs-timestamp-memo-plugin`
- 現在branch: `master`
- 修正branch: `codex/fix-final-review`（ローカル・originに保持）
- 修正前master/origin/master: `aa3518be55b8bb10980605b86b69b9e604ade154`
- 修正完了・通常push済みmaster/origin/master: `0c1b59b7965434fb67ab3447ee1f41cdc60382dc`
- このHANDOFFは上記commitの後に**ローカルcommitのみ**追加する。中断時のmasterはorigin/masterより文書commit 1つだけahead、working treeはcleanとなる。HANDOFF追加に伴う新しいCIは起動しない。
- 履歴のrebase/squash/amend/force push、未コミット変更のreset/stash/破棄は行っていない。

## 完了した作業

指定順序でR7 → R1 → R2 → R3 → R4 → I1/I2 → I3/I4を修正した。全差分レビューで見つけたR1/R3の読み込み境界を最後に補強した。

| ID | 完了内容 |
|---|---|
| R7 | Windows/macOS/Ubuntuのbuild workflowへdomain/application/regressionsのCTestを接続。失敗でworkflowを失敗させる。format gateを固定versionで厳格化。macOSのheadless native-style crashをテスト専用Fusion styleで回避。 |
| R1 | 保存失敗文書を分割/次回録画でも保持。Unsaved一覧から再保存・Exportを提供。healthy journal、設定側recovery JSON、最大8件のmemory snapshotを順に使用。上限時は古い文書を捨てず、OBS録画を続けて打刻だけ停止し、救済後再開。 |
| R2 | 自動保存前に既存JSONのdecode、session_id、started_atを照合。別session/不正JSONは無断上書きせずcache保持。明示的なSave JSON as...成功後にcacheを整理。 |
| R3 | JSON/journalの意味的検証、schema/legacy/future区別、transactional read、partial updateを実装。10日の上限を撤去。整数換算のoverflow保護、ID上限のwrap防止。 |
| R4 | DockのExport操作ごとに最新snapshot取得。1回のfile Exportは開始時のimmutable snapshotを保持。nested file dialog中の編集/分割、同UUID再ロード、Clipboardを検証。 |
| I1 | readonly marker vectorを参照で読む。更新候補だけコピー。sort/snapshot/lifetime保護コピーは維持。 |
| I2 | controllerの250ms監視へ集約。1打刻1snapshot。FPS/寸法をoutput/encoder単位にcacheし、変更/detachで無効化、video-info取得失敗は再試行。 |
| I3 | SRT/VTTのcue計算・sort・payload生成だけpure helperへ共通化。format固有処理は各exporterへ残した。 |
| I4 | 内部の未使用API/設定フィールド、plugin側の到達不能Qt5分岐を削除。公開OBS entrypoint、設定key、hotkey識別子は維持。 |

主要修正は新しいテストが修正前に失敗し、修正後に成功することを確認した。**R5/R6の意味上の挙動は変更していない。**

修正branchの最終CI成功後、origin/masterが基準のままであることとcleanを確認し、masterへfast-forward統合した。masterでWindows build/全CTestを再実行して成功を確認後、通常pushした。push後のmaster CIも成功した。

## 主な変更ファイル

- `.github/scripts/run-tests.py`, `.github/workflows/{build-project,check-format,push}.yaml`: 3OSテスト、format失敗条件、修正branchのCI。clang-format **19.1.1** / gersemi **0.21.0**を固定。
- `src/application/session-controller.{cpp,hpp}`: 未保存文書/dirty/journal freshness/revisionの所有、救済、読み込みcommit境界、単一poll。
- `src/persistence/session-store.{cpp,hpp}`: 保存衝突検証、atomic recovery copy、検証済みreaderの採用、成功後のcache整理。
- `src/persistence/session-codec.{cpp,hpp}`: 厳密な入力検証、transactional decode、partial update。
- `src/core/{recording-session,timecode-helper}.*`: ID overflow防止、整数による時間換算。
- `src/obs/obs-bridge.{cpp,hpp}`: stable metadata cache。output取得・reference・callbackの既存lifecycleは維持。
- `src/ui/{dock-widget,export-dialog}.*`: Unsaved救済UI、最新snapshotへの更新、操作中snapshot保持、controller駆動の時刻表示。
- `src/exporters/*`: readonlyコピー削減、`TextFormats::subtitle_cues`によるSRT/VTT小規模共通化、Qt5分岐/未使用extension lookup削除。
- `src/core/plugin-settings.hpp`, `src/ui/settings-dialog.*`, `src/core/recording-timeline.hpp`: 未使用設定値・signal・getter削除。
- `tests/{application-tests,domain-tests,regressions,obs-stubs}.*`: 保存失敗、衝突、codec、Export nested loop、cache無効化等の振る舞いテスト。
- `CMakeLists.txt`, `tests/CMakeLists.txt`: 既存のgersemi不一致を固定versionで整形したもの。version差による大規模format変更ではない。
- [修正結果とcommit一覧](docs/final-review-fixes.md)、[実OBS手順](docs/obs-manual-test.md)、[修正前レビュー](docs/final-master-review.md): 原因・リスク・互換・制約・検証記録。

## 現在のビルド・テスト結果

- Windows x64実plugin build（RelWithDebInfo）: 修正branch・masterとも成功。
- CTest `domain`, `application`, `regressions`: master上で**3/3成功**。
- Qt/OBSを使わない独立domain build: **1/1成功**。
- clang-format 19.1.1 / gersemi 0.21.0: 全追跡対象check成功。
- `git diff --check`、修正前masterからの全差分レビュー: 完了。
- clang-tidy: 全25 translation unitのcore/cplusplus/deadcode解析完了、エラーなし。Qt親layoutの所有を理解できない既知のPotential leak 1件だけ（ExportDialog::setup_uiのvbox）。layout破棄テストが成功しているため手動deleteを追加しない。
- OBS依存のconfigureでDetours version/Virtual Camera GUIDの警告は残る。今回のplugin compile/test failureではない。
- [修正branch最終CI](https://github.com/oz-coden/obs-timestamp-memo-plugin/actions/runs/36974850748): 全3OSの実build・全3suite・format成功。
- [push後のmaster CI](https://github.com/oz-coden/obs-timestamp-memo-plugin/actions/runs/36975392831): Windows/macOS/Ubuntu build・全3suite・format成功。Release jobはtagでないためskip。
- 新DLL: `build_x64/rundir/RelWithDebInfo/obs-timestamp-memo.dll`。今回のDLLを実OBSへ入れて操作する確認は未実施。

## 未完了・次に行うこと（優先順）

1. **実OBSでの人間による再確認**: 更新済み[確認手順](docs/obs-manual-test.md)を実施する。起動/Dock、録画/打刻、pause/resume/split、Export/Clipboard、Settings、停止/終了/再起動、history/recovery、保存失敗救済、cache衝突を含む。既存データのコピーと専用録画フォルダを使用する。
2. **R5/R6は未解決の調査事項として維持**: R5のactive/null outputの成立条件、R6の実muxer分割境界を実測する。今回の依頼ではどちらの挙動変更も禁止。新しい測定結果とユーザー指示なしに推測で修正しない。
3. HANDOFFをGitHubにも残す場合は、fetch/statusで未知のremote変更がないことを確認し、この**文書だけのローカルcommitを通常push**する。現状、修正本体は既にorigin/masterへ反映済み。
4. 実機で新しい再現可能な問題が見つかった場合のみ、現在のmasterから新しい`codex/`作業branchを作り、再現テスト→修正→関連検証を行う。今すぐ追加のリファクタリングをする必要はない。

## 再開時のコマンド（PowerShell）

```powershell
Set-Location C:\Users\white\Documents\Workspace\obs-timestamp-memo-plugin
git branch --show-current
git status --short --branch
git fetch origin
git log -5 --oneline --decorate
git rev-parse HEAD master origin/master
git log --oneline origin/master..master
git diff --check
# 全修正の比較は、統合後のmaster...HEAD（空）ではなく旧基準を使う。
git diff --stat aa3518be55b8bb10980605b86b69b9e604ade154...0c1b59b

$reviewCMakeBin = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$reviewCMakeBin\cmake.exe" --build build_x64 --config RelWithDebInfo --parallel 4
& "$reviewCMakeBin\cmake.exe" --build build_tests --config RelWithDebInfo --parallel 4
& "$reviewCMakeBin\ctest.exe" --test-dir build_tests -C RelWithDebInfo --output-on-failure
# 純粋domainを単独検証する場合:
& "$reviewCMakeBin\cmake.exe" --build .cache/domain_build --config RelWithDebInfo --parallel 4
& "$reviewCMakeBin\ctest.exe" --test-dir .cache/domain_build -C RelWithDebInfo --output-on-failure

# 固定版formatter（このPCでは既に導入済み）:
$reviewSources = git ls-files -- '*.c' '*.h' '*.cpp' '*.hpp' '*.m' '*.mm'
& .cache/review-tools/clang_format/data/bin/clang-format.exe --dry-run --Werror $reviewSources
$env:PYTHONPATH = "$PWD/.cache/review-tools"
$reviewCMake = git ls-files -- '*.cmake' '*CMakeLists.txt'
& .cache/review-tools/bin/gersemi.exe --check $reviewCMake
# 全C++静的解析（このcheckout用のcompile database）:
& 'C:\Users\white\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' .cache/final-review-analysis.py
```

失敗したコマンドの後に成功したとして次の工程へ進まない。新しいcheckoutや依存変更時はbuild directory/compile databaseの再configureが必要。
Codex sandboxからGit/buildや.cache内のtoolへアクセス拒否された場合は通常の承認付き実行を使い、権限や保護を回避しない。

## 注意点・設計方針

- 状態所有者はSessionController。現在文書、timeline、未保存一覧、revisionを所有する。UIはprojectionで、OBS APIはObsBridge境界に留まる。Runtimeのサービス寿命/外側Dock所有は維持した。
- **全storage故障時の永続化保証はない**。memoryのみの文書はOBS終了/停電前に救済が必要。最大8文書の上限でデータをevictせず打刻を停止する方針。OBS録画は止めない。file-backed項目は文書内容全体をRAMに保持しない。
- Save JSON as...だけが救済成功をacknowledgeしてcache/項目を整理する。Export snapshotはコピーで、cacheを消さない。
- 自動JSON保存は正常decode＋同session ID＋同started_atだけ更新可能。basename一致だけでは更新しない。明示的上書きはfile dialogのユーザー判断を通す。
- schema `1.0.0`とschemaなしlegacyを受け入れ、futureを拒否する。旧writer（初期commitのtype:header/markerと後期のop形式）も確認した。strict validationで以前default補正されていた不正fileは拒否されるが、元fileは壊さない。
- timestamp/frameはJSONで正確な`0..2^53-1`整数。durationの10日上限はない。partial updateで省略項目は維持する。
- Export再呼び出しは最新snapshot、1回の保存操作は固定snapshot。shared_ptrはnested dialog中の寿命保護のためで、mutable共有状態ではない。
- output取得は録画activeになってからのみ。R5対策のつもりで起動時output取得を再導入しない。R6のframe origin/file_changed採取は変更していない。
- QJsonArray走査でtemporary proxyへconst referenceをbindしない。initializer_listの参照は安全だが両者を混同しない。
- テストexeをGUIとして直接起動しない。CTestがminimal platformとQt DLL PATHを設定する。テスト専用Fusion style/非native file dialogをproductionへ適用しない。
- `.cache/final-review-probes`は旧不具合を確認したscratchで、BADな旧動作を期待する。修正後の合否に使わない。過去の`.cache/refactor*.py`など非idempotentな編集scriptも再実行しない。正式なtests/と上記コマンドを使用する。
- native file dialog/OS clipboard、外部NLE互換、encoder負荷やdrop、既に分割済みの録画途中でのplugin reloadは実機確認が残る。CI/stubだけで証明済みとは扱わない。

詳細な原因・修正risk・追加テスト・13件の今回commitと既存レビューcommitについては[修正記録](docs/final-review-fixes.md)と`git log aa3518b..0c1b59b`を参照する。
