# coroutine配置変更と初期化分離 引継ぎ書

作成日: 2026-10-14
対象リポジトリ: `C:\usr\src\MCUFW`
作成時ブランチ: `feature/mrunsv`
作成時HEAD: `558466c Complete generic sys migration docs`

## 1. 目的

Windows coroutine実装を`esh`層からWindows依存層へ移し、file/inode tableの
初期化とbuffer cacheの初期化を分離する。

最終状態は次のとおり。

- `src/core/esh/cortn.c`を
  `src/core/sys/dev/windows/cortn.c`へ移動
- `src/core/esh/include/cortn.h`を
  `src/core/sys/dev/windows/cortn.h`へ移動
- `file_system_init()`を`finit()`へ改名
- `finit()`は`file[]`と`inode[]`だけを初期化
- `finit()`から`binit()`への依存を除去
- 完全初期化が必要な呼出し側は`binit()`、`finit()`の順に明示的に呼出し
- coroutine/thread両backendの既存動作を維持

本作業はStep 0からStep 7までを別タスクで順番に実施する。各タスクは担当
Stepの範囲だけを扱い、後続Stepを先取りしない。各Step完了時に本書の状態、
検証結果、引継事項を更新する。

## 2. Step状態

| Step | 内容 | 状態 | 完了コミット |
|---|---|---|---|
| 0 | 変更前状態と基準試験結果の記録 | done | uncommitted |
| 1 | coroutine source/headerの物理移動 | done | uncommitted |
| 2 | build定義とinclude pathの新配置対応 | done | uncommitted |
| 3 | `file_system_init()`から`binit()`を分離し`finit()`へ改名 | done | uncommitted |
| 4 | production codeと試験の初期化呼出し更新 | done | uncommitted |
| 5 | 現行設計文書の更新 | done | uncommitted |
| 6 | 旧参照と依存関係の静的確認 | done | uncommitted |
| 7 | 全回帰試験とWindows target確認 | pending | - |

状態は`pending`、`active`、`done`、`blocked`を使用する。各Step開始時に
`active`、完了時に`done`へ更新する。完了コミットを作らない運用の場合は、
完了コミット欄に`uncommitted`と記録する。

## 3. Step間の依存関係

実行順序を変更しない。

```text
Step 0
  -> Step 1 -> Step 2
  -> Step 3 -> Step 4
  -> Step 5 -> Step 6 -> Step 7
```

Step 1とStep 3は、それぞれ後続Stepと一組の変更である。

- Step 1完了直後は、旧pathを直接指定する一部Makefileが失敗する
- Step 3完了直後は、旧関数名を使う呼出し元がcompileできない
- Step 2とStep 4が対応を完了するまで、この一時的な失敗を別不具合として
  修正しない
- Step 1とStep 2の間、Step 3とStep 4の間に無関係な変更を入れない

各Stepを常にbuild可能なcommitとして残す必要がある運用では、Step 1と
Step 2、Step 3とStep 4をそれぞれ一つのcommitへまとめる。ただしタスクの
担当範囲と結果記録はStepごとに分ける。

## 4. 共通作業規則

各タスクは最初に次を実施する。

1. ルートの`AGENTS.md`を読む
2. `docs/coding_style_summary_linux_kernel.md`を読む
3. 本書の共通方針と担当Stepを読む
4. `git status --short`で既存変更と未追跡ファイルを確認
5. 直前Stepが`done`で、結果と引継事項が記録済みか確認
6. 担当Stepを`active`へ変更
7. 作業前の差分を確認し、既存変更を上書きしない

各タスクは終了時に次を実施する。

1. 担当Stepの完了条件を確認
2. 実行した検証と結果を担当Stepの結果欄へ記録
3. 未解決事項と次Stepへの注意点を記録
4. 担当Stepを`done`へ変更
5. `git diff --check`を実行
6. `git status --short`で意図しない生成物や変更がないことを確認

作成時点では`.agents/`、試験生成物、cscope生成物、STM32 tree、Windows実行物
など多数の未追跡ファイルが存在する。これらを削除、移動、commitしない。
clean処理は対象試験directory内に限定する。

GCCはMSYS2 MINGW64 shell経由でのみ実行する。PowerShellから
`C:\msys64\mingw64\bin\gcc.exe`を直接実行しない。

## 5. 固定方針

### 5.1 coroutine配置

`cortn.c`と`cortn.h`はWindows固有のcontext実装と同じdirectoryへ移す。
移動先は次で固定する。

```text
src/core/sys/dev/windows/
  cortn.c
  cortn.h
  cortn_context.c
  cortn_context.h
  cortn_switch.S
  sched_cortn.c
  sched_thread.c
  sched_backend.mk
```

ファイル内容、公開関数名、構造体、stack size、guard処理、context switchの
動作は変更しない。配置変更に便乗したrefactoringは行わない。

### 5.2 初期化責務

最終的な責務は次で固定する。

```text
binit()
  -> buffer cache初期化

finit()
  -> file[]初期化
  -> inode[]初期化
```

完全初期化が必要なproduction codeと既存試験は次の順で呼ぶ。

```c
binit();
finit();
```

`finit()`から`binit()`を呼ばない。`fio.c`から`buf.h`への依存も除去する。
旧名`file_system_init()`のalias、macro、互換wrapperは残さない。

既存初期化関数と同様、所有中のfile、inode、bufferがない状態で呼ぶ。live
resourceを保持したまま再初期化できるAPIには変更しない。

### 5.3 UNIX V7との関係

bundled UNIX V7の`usr/sys/sys/main.c`では、`binit()`は起動処理から独立して
呼ばれる。V7の`fio.c`にはMCUFWの`file_system_init()`に相当する初期化関数は
なく、file tableなどの静的領域は起動時のzero初期化に依存する。

したがって次を区別する。

- `binit()`の独立呼出しはV7の責務分割と整合
- `finit()`はMCUFWの固定tableを試験や再初期化でzeroへ戻す独自API
- V7のdisk、root filesystem、trap、MMU、process起動処理は導入しない

参照資料:

- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/main.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/fio.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/h/file.h`

## 6. 現在確認済みの影響範囲

### 6.1 coroutine旧pathの直接参照

次のMakefileは旧`cortn.c`を直接指定している。

- `src/core/esh/test/cortn/Makefile`
- `src/core/esh/test/sched/Makefile`
- `src/core/esh/test/pipe/Makefile`
- `src/core/sys/test/sched/Makefile`

次は`SCHED_CSOURCES = cortn.c ...`と`vpath`を使用している。物理移動後も新しい
`sys/dev/windows`から解決されることを確認する。

- `src/core/sys/dev/windows/sched_backend.mk`
- `src/core/esh/test/exe/Makefile`
- `src/core/esh/test/pipeline_exec/Makefile`
- `src/target/windows/Makefile`
- `src/target/windows/TDM-GCC/Makefile`

### 6.2 `file_system_init()`の呼出し元

作成時点で次に呼出しがある。

- `src/core/esh/target/windows/esh_adapt.c`
- `src/core/esh/test/pipe/test_pipe.c`
- `src/core/esh/test/pipeline_exec/test_pipeline_exec.c`
- `src/core/sys/test/file/test_file.c`
- `src/core/sys/test/pipe/test_pipe.c`
- `src/core/sys/test/sched/test_sched.c`

定義と宣言は次にある。

- `src/core/sys/sys/fio.c`
- `src/core/sys/h/file.h`

## 7. Step 0 変更前状態と基準試験結果の記録

### 目的

後続Stepの失敗が既存問題か今回の変更によるものか判断できる基準を残す。

### 作業範囲

source、header、Makefileは変更しない。本書のStep状態と結果欄だけを更新する。

### 作業手順

1. `git status --short`を記録
2. branch、HEAD、最新commit subjectを記録
3. 次の旧symbolと旧pathの検索結果を記録
   - `file_system_init`
   - `binit(`
   - `src/core/esh/cortn.c`
   - `src/core/esh/include/cortn.h`
4. MSYS2 MINGW64 shellで次を実行
   - `src/core/esh/test/cortn/run_test.sh`
   - `src/core/sys/test/file/run_test.sh`
   - `src/core/sys/test/bio/run_test.sh`
   - `src/core/sys/test/pipe/run_test.sh`
   - `src/core/sys/test/sched/run_test.sh`
   - `src/core/esh/test/pipe/run_test.sh`
   - `src/core/esh/test/pipeline_exec/run_test.sh`
5. 各suiteの成否、backend、最適化条件を結果欄へ記録

### 完了条件

- 変更前HEADとworking tree状態が記録済み
- 各基準試験の結果が記録済み
- 失敗がある場合、既存失敗として再現条件を記録済み
- source変更なし

### 結果

- 実施日: 2026-10-14
- branch: `feature/mrunsv`
- HEAD: `558466c Complete generic sys migration docs`
- 追跡対象のsource、header、Makefileに変更なし
- 作業開始時から存在する未追跡fileを削除、移動、commitしていない
- 基準試験で新規作成した`core/sys/test/sched`生成物は確認後に削除
- `file_system_init()`は宣言、定義、呼出しを含め34箇所
- `binit()`は`bio.c`で定義、`buf.h`で宣言
- `binit()`の既存呼出しは`fio.c`、bdev試験、bio試験に存在
- `src/core/esh/cortn.c`と`src/core/esh/include/cortn.h`は追跡対象として存在
- 旧`cortn.c`を直接指定するMakefileは次の4個
  - `src/core/esh/test/cortn/Makefile`
  - `src/core/esh/test/sched/Makefile`
  - `src/core/esh/test/pipe/Makefile`
  - `src/core/sys/test/sched/Makefile`

基準試験結果:

| Suite | 条件 | 結果 |
|---|---|---|
| `core/esh/test/cortn` | `-O0`, `-O2` | pass |
| `core/sys/test/file` | `-O0`, `-O2` | pass |
| `core/sys/test/bio` | `-O0`, `-O2` | pass |
| `core/sys/test/pipe` | `-O0`, `-O2` | pass |
| `core/sys/test/sched` | cortn/thread × `-O0`/`-O2` | pass |
| `core/esh/test/pipe` | cortn/thread × `-O0`/`-O2` | pass |
| `core/esh/test/pipeline_exec` | cortn/thread × `-O0`/`-O2` | pass |

sandbox内の初回起動ではMSYS2 runtimeのnamed object作成が
`0xC0000022`で拒否され、試験開始前に終了した。sandbox外のMSYS2 MINGW64
shellで再実行し、全suiteが成功した。これはcodeの基準失敗には含めない。

`pipeline_exec`では次の既存warningを確認した。

- `help.c`: unused parameter
- `lex.c`: unused function、format/string truncation
- `par.c`: char subscript
- `test.c`: unused function

異常系試験によるlexer/parserのerror表示も出るが、全組合せの最終判定は
`all tests passed`である。

### 次Stepへの引継事項

- Step 1は2ファイルの物理移動だけを行い、内容を変更しない
- Step 1完了直後に上記4個のMakefileが失敗することは計画済み
- Step 2でbuild pathとinclude pathを更新する
- `pipeline_exec`の既存warningを今回の配置変更に便乗して修正しない
- MSYS2試験で同じsandbox拒否が出た場合は、code失敗と区別して実行環境を
  確認する

## 8. Step 1 coroutine sourceとheaderの物理移動

### 目的

coroutine engineをWindows backendのdirectoryへ配置する。

### 前提条件

- Step 0が`done`
- Step 0の基準結果を確認済み

### 作業範囲

次の2ファイルの物理移動だけを行う。

```text
src/core/esh/cortn.c
  -> src/core/sys/dev/windows/cortn.c

src/core/esh/include/cortn.h
  -> src/core/sys/dev/windows/cortn.h
```

### 作業手順

1. 移動先に同名ファイルがないことを確認
2. Gitがrenameとして追跡できる形で2ファイルを移動
3. 移動前後の内容が同一であることを確認
4. `cortn.c`内の`#include "cortn.h"`は変更しない
5. coroutine API、定数、コメント、formatを変更しない
6. `git diff --summary`で2件のrenameとして認識されるか確認

### このStepで行わないこと

- Makefile変更
- include path変更
- coroutine実装のrefactoring
- `finit()`関連変更
- 文書更新

### 検証

- 新しい2ファイルが存在
- 古い2ファイルが存在しない
- rename以外の内容差分がない

旧pathを直接指定するMakefileはStep 2まで失敗してよい。このStep単独では
全build成功を完了条件にしない。

### 完了条件

- 指定された2ファイルの移動完了
- 意図しない内容変更なし
- 一時的に失敗するbuild経路を結果欄へ記録

### 結果

- 実施日: 2026-10-14
- 開始時HEAD: `e55990d Record coroutine relocation baseline`
- 次の2ファイルを指定先へ移動
  - `src/core/esh/cortn.c`
    -> `src/core/sys/dev/windows/cortn.c`
  - `src/core/esh/include/cortn.h`
    -> `src/core/sys/dev/windows/cortn.h`
- 移動先に同名fileが存在しないことを移動前に確認
- 移動後、旧pathにfileが存在しないことを確認
- file sizeは移動前後で一致
  - `cortn.c`: 4247 bytes
  - `cortn.h`: 1524 bytes
- SHA-256は移動前後で一致
  - `cortn.c`:
    `C984FA75A5566637786C0C50CDD51BC35B98A611BF08BA747DED1F6BF3FE0265`
  - `cortn.h`:
    `C393FC69A5DAA53EE8990528B25350154E56E35415154A1D710A7944C6BD7A0F`
- HEAD上の旧fileと移動先fileのGit blob IDも一致
  - `cortn.c`: `2159a09913a5e34982c95aa404c4d059fbf2db1c`
  - `cortn.h`: `5fa35219e6c9262792633b6eb7409fe31bd5300d`
- source/header内容、API、定数、comment、formatに変更なし
- Makefile、include path、`finit()`関連fileに変更なし
- Step 1単独のbuild/testは計画どおり未実施

### 次Stepへの引継事項

- 旧pathを直接指定する次の4個のMakefileはStep 2まで一時的に失敗する
  - `src/core/esh/test/cortn/Makefile`
  - `src/core/esh/test/sched/Makefile`
  - `src/core/esh/test/pipe/Makefile`
  - `src/core/sys/test/sched/Makefile`
- Step 2で上記4個のsource pathを新配置へ変更する
- `sys/dev/windows`を既に`vpath`へ含むbuild経路も新しい`cortn.c`を選ぶことを
  clean buildで確認する
- coroutine headerは`src/core/sys/dev/windows/cortn.h`から解決する
- 配置変更に便乗したcoroutine実装変更を行わない

## 9. Step 2 build定義とinclude pathの新配置対応

### 目的

全build経路が移動後の`cortn.c`と`cortn.h`を参照する状態へ戻す。

### 前提条件

- Step 1が`done`
- 新配置に2ファイルが存在

### 作業手順

1. 旧`cortn.c`を直接指定する4個のMakefileを新pathへ変更
2. `cortn.h`が`sys/dev/windows`のinclude pathから解決されることを確認
3. `src/core/sys/test/sched/Makefile`の
   `-I../../../esh/include`がcoroutine headerのためだけなら削除
4. esh試験で必要な`esh/include`は削除しない
5. `sched_backend.mk`のbare filenameは維持
6. `vpath`使用buildが`sys/dev/windows/cortn.c`を選ぶことを依存fileまたは
   build出力で確認
7. 古い`.d`を使わないよう対象試験をclean build

### 検証

MSYS2 MINGW64 shellで次を実行する。

- `src/core/esh/test/cortn/run_test.sh`
- `src/core/esh/test/sched/run_test.sh`
- `src/core/esh/test/pipe/run_test.sh`
- `src/core/esh/test/pipeline_exec/run_test.sh`
- `src/core/sys/test/sched/run_test.sh`

coroutine/thread両backendを持つsuiteは両方、`-O0`と`-O2`の両方を確認する。

### 完了条件

- 全buildが新配置を参照
- 旧source/header pathのbuild参照なし
- 指定試験成功
- thread backendに不要なcoroutine依存を追加していない

### 結果

- 実施日: 2026-10-14
- 開始時HEAD: `1773369 Move coroutine engine to Windows backend`
- 旧`cortn.c`を直接指定していた次のMakefileを新pathへ変更
  - `src/core/esh/test/cortn/Makefile`
  - `src/core/esh/test/sched/Makefile`
  - `src/core/esh/test/pipe/Makefile`
  - `src/core/sys/test/sched/Makefile`
- `src/core/sys/test/sched/Makefile`から不要な
  `-I../../../esh/include`を削除
- `sched_backend.mk`のbare filename定義は変更なし
- `pipeline_exec/run_test.sh`へ構成別cleanを追加
  - `-B`でも古い`.d`内の消滅済みpathが解決されるため、build前cleanが必要
  - clean対象は各構成の`OUTDIR`と`TARGET`だけに限定
- tracked build定義から旧source/header path参照が消えたことを確認
- `pipeline_exec`生成dependencyで次を確認
  - `../../../sys/dev/windows/cortn.c`
  - `../../../sys/dev/windows/cortn.h`
- MSYS2 MINGW64 shellで次の試験が成功
  - `src/core/esh/test/cortn/run_test.sh`: O0/O2
  - `src/core/esh/test/sched/run_test.sh`: cortn/thread x O0/O2
  - `src/core/esh/test/pipe/run_test.sh`: cortn/thread x O0/O2
  - `src/core/esh/test/pipeline_exec/run_test.sh`: cortn/thread x O0/O2
  - `src/core/sys/test/sched/run_test.sh`: cortn/thread x O0/O2
- sandbox内の初回実行はMSYS2 named object作成拒否で失敗
  - 通常のMINGW64環境で再実行し、全試験成功
- `pipeline_exec`の既存warningと異常系試験のerror表示は基準状態と同じ
- 試験前に存在しなかった`src/core/sys/test/sched`生成物だけを削除

### 次Stepへの引継事項

- Step 3では`fio.c`と`file.h`だけを変更する
- `file_system_init()`を`finit()`へ改名し、内部の`binit()`呼出しを削除する
- `file[]`と`inode[]`の初期化は維持する
- 呼出し元はStep 4で更新するため、Step 3単独の一時的なcompile失敗は計画済み
- coroutine配置とbuild定義をStep 3で変更しない

## 10. Step 3 `finit()`実装と`binit()`依存除去

### 目的

file/inode table初期化をbuffer cache初期化から分離する。

### 前提条件

- Step 2が`done`
- coroutine配置変更後の関係試験が成功

### 作業手順

1. `src/core/sys/sys/fio.c`の`file_system_init()`を`finit()`へ改名
2. `finit()`内の`binit()`呼出しを削除
3. `file[]`と`inode[]`の`memset()`は維持
4. 不要になった`#include "buf.h"`を`fio.c`から削除
5. `src/core/sys/h/file.h`の宣言を`void finit(void);`へ変更
6. alias、macro、互換wrapperを追加しない
7. `finit()`のscopeや戻り値を変更しない

最終形は次の責務とする。

```c
void finit(void)
{
	memset(file, 0, NFILE * sizeof(file[0]));
	memset(inode, 0, NINODE * sizeof(inode[0]));
}
```

### このStepで行わないこと

- 呼出し元の変更
- 試験の期待値変更
- file/inode構造体の変更
- `binit()`実装の変更

### 検証

- `fio.c`に`binit`参照がない
- `fio.c`に`buf.h`includeがない
- `file.h`と`fio.c`の`finit()`宣言・定義が一致
- `finit()`がfile/inode tableだけをzero初期化

呼出し元はStep 4まで旧名のため、このStep単独ではcompile失敗してよい。

### 完了条件

- `finit()`の実装と公開宣言が完成
- `fio.c`からbuffer cache依存を除去
- 呼出し元以外に`file_system_init`を残していない

### 結果

- 実施日: 2026-10-14
- 開始時HEAD: `32e2bde Update coroutine build paths`
- `src/core/sys/sys/fio.c`を次のとおり変更
  - `file_system_init()`を`finit()`へ改名
  - `binit()`呼出しを削除
  - 不要になった`buf.h`includeを削除
  - `file[]`と`inode[]`のzero初期化を維持
- `src/core/sys/h/file.h`の公開宣言を`void finit(void);`へ変更
- alias、macro、互換wrapperは追加なし
- 静的確認結果
  - `fio.c`に`binit`参照なし
  - `fio.c`に`buf.h`includeなし
  - `file.h`と`fio.c`の`finit()`宣言・定義が一致
  - 旧名はStep 4対象の呼出し元だけに残存
- MINGW64 GCCで`fio.c`単体のsyntax check成功
  - `-Wall -Wextra -Werror -pedantic -std=c11`
- sandbox内の初回実行はMSYS2 named object作成拒否で失敗
  - 通常のMINGW64環境で再実行し、syntax check成功
- 呼出し元は旧名のままなので、計画どおり全体buildは未実施

### 次Stepへの引継事項

- Step 4で旧名が残る次の6 sourceを更新する
  - `src/core/esh/target/windows/esh_adapt.c`
  - `src/core/esh/test/pipe/test_pipe.c`
  - `src/core/esh/test/pipeline_exec/test_pipeline_exec.c`
  - `src/core/sys/test/file/test_file.c`
  - `src/core/sys/test/pipe/test_pipe.c`
  - `src/core/sys/test/sched/test_sched.c`
- 各`file_system_init()`を`binit()`、`finit()`の順へ置換する
- `binit()`宣言用の`buf.h`を必要なsourceだけに追加する
- Step 4完了後に関係suiteをclean buildする
- `fio.c`へ`binit()`呼出しや`buf.h`includeを戻さない

## 11. Step 4 初期化呼出し元の更新

### 目的

旧API利用箇所を`binit()`と`finit()`の明示的呼出しへ移行し、build可能な状態へ
戻す。

### 前提条件

- Step 3が`done`
- `finit()`の定義と宣言が存在

### 作業手順

1. 全`file_system_init()`呼出しを調査
2. 既存動作と同じ完全初期化が必要な箇所を次へ置換

   ```c
	binit();
	finit();
   ```

3. `binit()`宣言が必要なsourceへ`buf.h`を追加
4. 既に`buf.h`をincludeするsourceには重複追加しない
5. include順とsource構成を`AGENTS.md`へ合わせる
6. 各試験でlive resourceがない時点の初期化であることを確認
7. 初期化順を`binit()`、`finit()`へ統一

対象候補は次のとおり。

- `src/core/esh/target/windows/esh_adapt.c`
- `src/core/esh/test/pipe/test_pipe.c`
- `src/core/esh/test/pipeline_exec/test_pipeline_exec.c`
- `src/core/sys/test/file/test_file.c`
- `src/core/sys/test/pipe/test_pipe.c`
- `src/core/sys/test/sched/test_sched.c`

### 検証

MSYS2 MINGW64 shellで次を実行する。

- `src/core/sys/test/file/run_test.sh`
- `src/core/sys/test/bio/run_test.sh`
- `src/core/sys/test/pipe/run_test.sh`
- `src/core/sys/test/sched/run_test.sh`
- `src/core/esh/test/pipe/run_test.sh`
- `src/core/esh/test/pipeline_exec/run_test.sh`

### 完了条件

- sourceとheaderに`file_system_init`が残っていない
- 完全初期化箇所が`binit()`、`finit()`の順
- `fio.c`は`binit()`に依存しない
- 指定試験成功

### 結果

- 実施日: 2026-10-15
- 開始時HEAD: `9decfaf Separate file initialization from buffer cache`
- 旧APIを使用していた6 source、32箇所を次の順へ変更

  ```c
	binit();
	finit();
  ```

- `buf.h`を次のsourceへ追加
  - `src/core/esh/target/windows/esh_adapt.c`
  - `src/core/esh/test/pipeline_exec/test_pipeline_exec.c`
- `esh_adapt.c`の混在していたCRLF/LFをLFへ統一
- 残り4 sourceは既存の`buf.h`includeを使用
- sourceとheaderに`file_system_init`が残っていないことを確認
- `src/core/sys/sys/fio.c`に`binit`参照と`buf.h`includeがないことを確認
- MSYS2 MINGW64 shellで次の試験が成功
  - `src/core/sys/test/file/run_test.sh`: O0/O2
  - `src/core/sys/test/bio/run_test.sh`: O0/O2
  - `src/core/sys/test/pipe/run_test.sh`: O0/O2
  - `src/core/sys/test/sched/run_test.sh`: cortn/thread x O0/O2
  - `src/core/esh/test/pipe/run_test.sh`: cortn/thread x O0/O2
  - `src/core/esh/test/pipeline_exec/run_test.sh`: cortn/thread x O0/O2
- sandbox内の初回実行はMSYS2 named object作成拒否で失敗
  - 通常のMINGW64環境で再実行し、全試験成功
- `pipeline_exec`の既存warningと異常系試験のerror表示は基準状態と同じ
- 試験前に存在しなかった`src/core/sys/test/sched`生成物だけを削除

### 次Stepへの引継事項

- Step 5で`docs/cortn_windows.md`と`docs/esh_pipe_core.md`を更新する
- coroutine engineの配置を`src/core/sys/dev/windows`として説明する
- scheduler backend固有のsource構成を現在のbuild定義へ合わせる
- buffer cache初期化とfile/inode table初期化の責務分離を説明する
- 完全初期化順を`binit()`、`finit()`として記録する
- Step 4で確認した試験結果を設計文書へ必要な範囲で反映する

## 12. Step 5 現行設計文書の更新

### 目的

現在の配置と初期化責務を説明する文書を実装へ合わせる。

### 前提条件

- Step 4が`done`
- source、header、呼出し元の最終形が確定

### 更新対象

- `docs/cortn_windows.md`
- `docs/esh_pipe_core.md`

### 作業手順

1. `cortn_windows.md`のcoroutine engine配置を新pathへ更新
2. `esh_pipe_core.md`の初期化説明を次の責務へ更新
   - `binit()`はbuffer cache
   - `finit()`はfile/inode table
   - 完全初期化は`binit()`、`finit()`の順
3. 現行pathを説明する他の文書を検索
4. 過去のreview結果、計画、handoff文書は歴史的記録として原則変更しない
5. 過去文書が現行手順と誤認される場合だけ、現行文書への短い参照を追加

### 完了条件

- 現行設計文書が実装と一致
- `file_system_init()`を現行APIとして説明していない
- coroutineの現行配置が正しい
- 過去記録を現在形へ無差別に書き換えていない

### 結果

- 実施日: 2026-10-15
- 開始時HEAD: `1854fbd Update file initialization callers`
- `docs/cortn_windows.md`を次の現行実装へ更新
  - coroutine engineとheaderの配置を`src/core/sys/dev/windows`へ変更
  - `sched_backend.mk`によるbackend選択を追加
  - cortn backendのC sourceとassembly sourceを記録
  - thread backendが`sched_thread.c`だけを使用することを記録
  - 更新日を2026-10-15へ変更
- `docs/esh_pipe_core.md`を次の初期化責務へ更新
  - `binit()`はbuffer cacheを初期化
  - `finit()`はfile tableとinode tableを初期化
  - 完全初期化順は`binit()`、`finit()`
  - `finit()`は`binit()`を内部で呼ばない
  - 更新日を2026-10-15へ変更
- 対象2文書に旧coroutine pathと`file_system_init()`がないことを確認
- sourceと`sched_backend.mk`を現行文書の記述と照合
- review結果、計画、過去handoff内の旧記述は歴史的記録として変更なし
- 文書だけの変更なのでbuildと実行試験は未実施

### 次Stepへの引継事項

- Step 6でsource、header、Makefileの旧APIと旧pathを再検索する
- `finit()`の定義と公開宣言が各1個であることを確認する
- 全初期化箇所が`binit()`、`finit()`の順であることを確認する
- `fio.c`に`binit`参照と`buf.h`includeがないことを確認する
- cortn/thread backendのobject構成を静的に確認する
- `vpath`が移動後のcoroutine sourceを一意に解決することを確認する
- 歴史的記録内の旧名と旧pathを現行codeの残存と誤認しない

## 13. Step 6 旧参照と依存関係の静的確認

### 目的

移動漏れ、改名漏れ、意図しない層間依存を検出する。

### 前提条件

- Step 5が`done`

### 確認項目

1. 現行source、header、Makefileに`file_system_init`がない
2. build定義に次の旧pathがない
   - `src/core/esh/cortn.c`
   - `src/core/esh/include/cortn.h`
   - 相対pathの`../../cortn.c`
   - 相対pathの`../../../esh/cortn.c`
3. `fio.c`に`binit`と`buf.h`がない
4. `finit()`の定義が1個、公開宣言が1個
5. 初期化呼出しは`binit()`、`finit()`の順
6. coroutine headerはWindows backendを使用するbuildだけが参照
7. thread-only buildがcoroutine objectをlinkしない
8. 移動先に同名sourceの曖昧な`vpath`解決がない
9. header guard、typedef禁止、comment規則を維持
10. `git diff --check`が成功

生成済み`.d`、`.o`、`.exe`、`cscope.out`の検索結果はsource漏れとして扱わない。
追跡対象ファイルを優先して確認する。

### 完了条件

- 旧APIと旧build pathの現行参照なし
- `fio.c`とbuffer cache初期化の依存なし
- build backend間の依存混入なし
- 修正が必要な場合は担当範囲内の参照漏れだけを修正

### 結果

- 実施日: 2026-10-15
- 開始時HEAD: `c896067 Update coroutine and file initialization docs`
- 追跡対象の現行source、header、Makefileに`file_system_init`なし
- build定義に次の旧path参照なし
  - `src/core/esh/cortn.c`
  - `src/core/esh/include/cortn.h`
  - 相対pathの`../../cortn.c`
  - 相対pathの`../../../esh/cortn.c`
- `src/core/sys/sys/fio.c`に`binit`と`buf.h`なし
- `finit()`は定義1個、公開宣言1個
  - 定義: `src/core/sys/sys/fio.c`
  - 公開宣言: `src/core/sys/h/file.h`
- 全32個の`finit()`呼出しで直前に`binit()`を実行
- `cortn.h`をincludeするproduction sourceはWindows backend内だけ
  - `cortn.c`
  - `cortn_context.c`
  - `sched_cortn.c`
- coroutine単体試験の`test_cortn.c`も意図どおり`cortn.h`をinclude
- `sched_backend.mk`を使用する全6個のbuildについて、Make展開結果を確認
  - `SCHED_BACKEND=thread`では`SCHED_CSOURCES=sched_thread.c`
  - `SCHED_ASOURCES`は空
  - coroutine objectはthread構成へ混入しない
- 追跡対象に`cortn.c`と`cortn.h`の同名fileなし
- `sys/dev/windows`を含む`vpath`で同名sourceの曖昧な解決なし
- 移動したsource/headerで次を確認
  - header guardは`__CORTN_H__`を維持
  - `typedef`なし
  - 80文字を超えるcomment行なし
- 修正が必要な参照漏れと依存混入なし
- Step 6は静的確認だけのため、build/test未実施
- sandbox内のMSYS2起動はnamed object作成拒否で失敗
  - 通常のMINGW64環境でMake展開を再実行し、確認成功

### 次Stepへの引継事項

- Step 7の全回帰試験とWindows target確認へ進める
- 全試験はMSYS2 MINGW64 shell内でclean buildする
- backend選択を持つsuiteはcoroutine/thread、`-O0`/`-O2`を確認する
- Windows targetは両backendをbuildし、各sample commandを実行する
- 既存の未追跡生成物を削除、移動、commitしない

## 14. Step 7 全回帰試験とWindows target確認

### 目的

最終状態がcoroutine/thread backend、最適化条件、Windows targetで従来動作を
維持することを確認する。

### 前提条件

- Step 6が`done`
- 静的確認で未解決項目なし

### 試験手順

MSYS2 MINGW64 shell内でclean buildして実行する。

#### coroutine単体

```sh
cd /c/usr/src/MCUFW/src/core/esh/test/cortn
bash ./run_test.sh
```

`-O0`と`-O2`の両方が成功すること。

#### sys層

```sh
cd /c/usr/src/MCUFW/src/core/sys/test/bio
bash ./run_test.sh
cd ../bdev
bash ./run_test.sh
cd ../file
bash ./run_test.sh
cd ../pipe
bash ./run_test.sh
cd ../sched
bash ./run_test.sh
cd ../header
bash ./run_test.sh
cd ../table
bash ./run_test.sh
```

#### esh結合

```sh
cd /c/usr/src/MCUFW/src/core/esh/test/pipe
bash ./run_test.sh
cd ../sched
bash ./run_test.sh
cd ../pipeline_exec
bash ./run_test.sh
cd ../par
bash ./run_test1.sh
bash ./run_test2.sh
cd ../exe
bash ./run_test.sh
```

backend選択を持つsuiteではcoroutine/thread、`-O0`/`-O2`の全組合せを確認する。

#### Windows target

`src/target/windows`で次を確認する。

1. `SCHED_BACKEND=cortn`のclean build
2. `SCHED_BACKEND=thread`のclean build
3. 各実行物でpromptが`#`へ変わること
4. 次のcommandが正常に動作すること

```text
sam1
sam2 1 10 100
sam3 -a -b string
```

対話試験が自動化できない場合は、build結果と未実施理由を記録し、実施済みと
記載しない。

### 最終確認

- `git diff --check`
- `git status --short`
- `git diff --stat`
- 追跡対象に意図しない生成物がない
- Step 0の基準失敗から新しい失敗が増えていない

### 完了条件

- 対象試験がすべて成功、または既存失敗との差分を説明済み
- Windows targetが両backendでbuild成功
- sample commandの結果を記録
- 本書の全Stepが`done`
- 最終変更file一覧と残課題を記録

### 結果

`pending`

### 最終引継事項

`pending`

## 15. 最終Definition of Done

- `cortn.c`と`cortn.h`が`src/core/sys/dev/windows`にだけ存在
- 全buildが新配置を参照
- `file_system_init()`が現行codeから消えている
- `finit()`がfile/inode tableだけを初期化
- `binit()`が呼出し側から独立して呼ばれる
- `fio.c`にbuffer cache headerやsymbolへの依存がない
- production codeと試験の完全初期化順が`binit()`、`finit()`
- coroutine/thread両backendの既存挙動を維持
- `-O0`/`-O2`の対象試験が成功
- 現行設計文書が新配置と新APIへ追従
- 既存の未追跡fileを削除、移動、commitしていない

## 16. Task開始用prompt

別タスクへ渡すときは、次のpromptを使用し、`N`を担当Step番号へ置換する。

```text
C:\usr\src\MCUFW\docs\cortn_finit_refactoring_handoff.mdを読み、
Step Nだけを実行してください。ルートのAGENTS.mdとcoding styleに従い、
既存の未追跡fileや担当外の変更には触れないでください。開始時にStep状態を
activeへ変更し、完了条件と指定試験を満たしたら結果、次Stepへの引継事項、
状態を文書へ記録してください。後続Stepは先取りしないでください。
```
