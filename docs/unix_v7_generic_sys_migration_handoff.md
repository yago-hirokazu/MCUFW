# UNIX V7風汎用file pipe scheduler移行 引継ぎ書

作成日: 2026-10-12
対象リポジトリ: `C:\usr\src\MCUFW`
作成時ブランチ: `feature/mrunsv`
作成時HEAD: `887fbd8 Add block device request queue`

## 1. 目的

`src/core/esh`にあるfile、pipe、schedulerをesh専用実装から汎用の
`src/core/sys`層へ移し、可能な範囲でUNIX V7と同じ構造、名前、責務分割へ
揃える。

この作業はStep 0からStep 8までを別タスクで順番に実施する。各タスクは
担当Stepだけを実装し、完了条件と試験を満たした後、この引継ぎ書へ結果を
記録する。後続Stepを同じタスクで先取りしない。

最終的に次を達成する。

- 汎用sys層のファイル名、型名、変数名、関数名、マクロ名から`esh_`と
  `ESH_`を除去
- `struct file`、`struct inode`、file table、inode tableをsys層へ配置
- V7の`fio.c`、`pipe.c`、`rdwri.c`等に対応する責務分割
- 物理ストレージ、MMU、trap命令なしでpipeを実現
- coroutine backendとthread backendで同じ外部動作を維持
- 指定されたWindows固有ファイルを`src/core/sys/dev/windows`へ移動

## 2. Step状態

| Step | 内容 | 状態 | 完了コミット |
|---|---|---|---|
| 0 | API名、V7対応範囲、配置の確定 | pending | - |
| 1 | 汎用header追加 | pending | - |
| 2 | file table、inode table追加 | pending | - |
| 3 | `file.c`のV7名sourceへの分割 | pending | - |
| 4 | V7風`pipe.c`とinode I/O | pending | - |
| 5 | scheduler汎用化 | pending | - |
| 6 | Windows backend移動 | pending | - |
| 7 | esh呼出し側の切替と旧実装削除 | pending | - |
| 8 | 全回帰試験と文書更新 | pending | - |

状態は`pending`、`active`、`done`、`blocked`のいずれかを使用する。
各Step開始時に`active`、完了時に`done`へ更新する。仕様判断が必要で安全に
継続できない場合だけ`blocked`とし、選択肢と推奨案を記録する。

## 3. 作業開始時の共通確認

各タスクは最初に次を実施する。

1. ルートの`AGENTS.md`を読む
2. `docs/coding_style_summary_linux_kernel.md`を読む
3. 本引継ぎ書の共通方針と担当Stepを読む
4. `git status --short`で既存変更と未追跡ファイルを確認する
5. 直前Stepが`done`で、完了条件と引継事項が記録済みか確認する
6. 担当Step開始前の関連試験を実行する

作成時点では`.agents/`、cscope生成物、試験生成物、STM32 tree、Windows実行物
など多数の未追跡ファイルがある。これらを削除、移動、commitしない。自分が
作成した生成物だけを識別し、clean操作の対象範囲を限定する。

GCCは必ずMSYS2 MINGW64 shell経由で実行する。PowerShellから
`C:\msys64\mingw64\bin\gcc.exe`を直接起動しない。

## 4. 現在の構造と影響範囲

直接の移行対象は次のファイルである。

- `src/core/esh/include/esh_file.h`
- `src/core/esh/include/esh_pipe.h`
- `src/core/esh/include/esh_sched.h`
- `src/core/esh/include/esh_sched_backend.h`
- `src/core/esh/file.c`
- `src/core/esh/pipe.c`

ただし、接頭辞除去と配置変更は次にも波及する。

- `src/core/esh/sched.c`
- `src/core/esh/exe.c`
- `src/core/esh/include/exe.h`
- `src/core/esh/target/windows/stdio_adapt.c`
- `src/core/esh/test/pipe/`
- `src/core/esh/test/sched/`
- `src/core/esh/test/pipeline_exec/`
- `src/core/esh/test/exe/`
- `src/target/windows/Makefile`
- `src/target/windows/TDM-GCC/Makefile`

現行の所有関係は次のとおり。

```text
struct esh_proc
  -> struct esh_fd_table
       -> struct esh_file
            -> struct esh_pipe_inode
                 -> struct buf
                      -> buffers[NBUF][BSIZE]
```

現行値は`BSIZE=256`、`NBUF=3`、pipe最大3本、pipeline最大4段、
fd最大12/process、file最大12である。

## 5. UNIX V7で確認済みの責務分割

一次資料はbundled UNIX V7 sourceを使用する。

- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/h/file.h`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/h/inode.h`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/fio.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/pipe.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/rdwri.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/sys2.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/alloc.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/iget.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/conf/c.c`

V7には独立した`pipe.h`はない。file状態は`file.h`、inode状態は`inode.h`、
pipe処理は`pipe.c`に置かれている。MCUFWの`pipe.h`は、固定容量、移植用状態、
試験用interfaceなどV7にない最小定義だけを持つ。

V7の主要な対応は次のとおり。

| MCUFW現行 | V7対応 |
|---|---|
| `struct esh_file` | `struct file` |
| `f_refs` | `f_count` |
| direction enum | `f_flag`と`FREAD/FWRITE/FPIPE` |
| `struct esh_pipe_inode` | `struct inode` |
| `pi_count` | `i_size` |
| reader offset | `struct file.f_offset` |
| file pool | `file[NFILE]` |
| pipe inode pool | `inode[NINODE]` |
| `esh_fd_get` | `getf` |
| `esh_file_alloc` | `falloc` |
| `esh_file_put` | `closef` |
| pipe read/write | `readp`、`writep` |
| inode I/O | `readi`、`writei` |
| pipe lock | `plock`、`prele` |

## 6. 最終配置案

Step 0で確定するが、開始時の推奨配置は次のとおり。

```text
src/core/sys/
├── conf/
│   └── conf.c
├── dev/
│   ├── bio.c
│   └── windows/
│       ├── cortn_context.c
│       ├── cortn_context.h
│       ├── cortn_switch.S
│       ├── sched_backend.mk
│       ├── sched_cortn.c
│       └── sched_thread.c
├── h/
│   ├── file.h
│   ├── inode.h
│   ├── pipe.h
│   ├── sched.h
│   └── sched_backend.h
└── sys/
    ├── alloc.c
    ├── fio.c
    ├── iget.c
    ├── pipe.c
    ├── rdwri.c
    └── sched.c
```

`sys2.c`を追加するかは、host CRTの`read`、`write`、`close`、`pipe`との
名前衝突をStep 0で確認して決める。

## 7. 全Step共通の設計条件

### 7.1 維持する動作

- zero-length read/writeはfdやpeerを検査せず0
- 空pipeでwriterが残っていればreadは待機
- 空pipeでwriterがなければreadはEOF
- 満杯pipeでreaderが残っていればwriteは待機
- readerがなければwriteはEPIPE相当
- 最終writer close後も残存dataを読み切ってからEOF
- closeは待機中のpeerを起床する
- wakeup後は条件を再検査する
- coroutine/threadで同じ結果
- 固定長poolを使用し、必須の動的確保を追加しない
- stderrとdebug出力をpipeへ流さない

### 7.2 導入しないもの

- trap命令によるsystem call
- kernel/user address space分離
- MMU依存処理
- 物理disk I/O
- directory、pathname、mount、permission
- V7 filesystem全体
- signal機構。broken pipeはerrorとして通知
- 無制限queueまたは必須のheap allocation

### 7.3 コーディング規則

- `typedef`を追加しない
- source構成は`src/core/esh/tty.c`をモデルにする
- header guardは`__NAME_H__`形式
- コメントは冠詞を可能な限り省略した電報style
- 一文コメントは末尾periodなし
- コメントを含め80 column以内
- 既存未追跡ファイルを変更対象へ混ぜない

### 7.4 build-green原則

各Step終了時点で既存のbuildと試験を壊さない。Step 1からStep 4では新しい
sys層を旧esh実装と並行して追加してよい。Step 7で切替が完了するまで旧実装を
本番buildから外さない。同名symbolを同じbinaryへ二重linkしない。

一時的なcompatibility定義が必要な場合は次を守る。

- privateまたは移行専用と明記
- 新しいsys層からesh headerをincludeしない
- Step 7の削除対象として本書へ記録
- public APIとして固定しない

## 8. Step 0 API名 V7対応範囲 配置の確定

### 目的

実装前に、V7名をそのまま使用できる範囲と、Windows CRT衝突を避ける名前を
確定する。以後のStepが別々の命名を採用しないよう、対応表を本書へ残す。

### 前提

- 依存Stepなし
- source変更は原則行わない
- 名前衝突確認用の一時的な最小buildは可

### 調査項目

1. `pipe`、`read`、`write`、`close`、`sleep`のMinGW declarationとlink衝突
2. `struct proc`とesh側`struct proc`のtag衝突
3. `EBADF`等のhost `errno.h`との衝突
4. `off_t`等の型を導入せず表現する方法
5. `NFILE`、`NINODE`、`NOFILE`、`PIPSIZ`の配置
6. `sys2.c`を作るか、`sys_read`等の明示的APIを使うか
7. `cortn.c`と`cortn.h`を今回移動するか、後続作業へ残すか

### 推奨判断

- 内部関数はV7名の`getf`、`falloc`、`closef`、`readp`、`writep`、
  `readi`、`writei`、`plock`、`prele`を使用
- CRTと衝突するpublic入口は`sys_pipe`、`sys_read`、`sys_write`、
  `sys_close`を使用
- scheduler側が`struct proc`を所有し、esh executor側の現行`struct proc`は
  `struct exe_proc`へ変更
- error値はhostから独立した汎用名を使用し、host `errno`を暗黙利用しない
- `PIPSIZ`は第一段階では`BSIZE`とする

### 完了条件

- 新旧ファイル、型、関数、マクロの対応表が本書に追加されている
- 衝突確認結果と採用理由が記録されている
- Step 1以降が判断なしで命名できる
- source動作を変更していない

### 次Stepへの引継事項

- 確定したsymbol表
- headerごとの公開/private区分
- compatibility定義の有無と削除予定Step

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 0を実行してください。
API名、UNIX V7対応範囲、配置を確定し、結果を引継ぎ書へ記録してください。
実装は名前衝突確認に必要な最小範囲に留め、Step 1以降を開始しないでください。
```

## 9. Step 1 汎用header追加

### 目的

汎用sys層の公開契約を定義する。旧esh headerと並行して追加し、本番呼出し側の
切替は行わない。

### 前提

- Step 0が`done`
- Step 0のsymbol表が確定済み

### 対象

- `src/core/sys/h/file.h`
- `src/core/sys/h/inode.h`
- `src/core/sys/h/pipe.h`
- `src/core/sys/h/sched.h`
- `src/core/sys/h/sched_backend.h`

### 実装内容

1. `file.h`へ`struct file`、file flag、`extern struct file file[]`を定義
2. `inode.h`へpipeに必要な最小`struct inode`とflagを定義
3. `pipe.h`へMCUFW固有の容量、I/O状態、internal prototypeを定義
4. `sched.h`へprocess、wait channel、scheduler APIを定義
5. `sched_backend.h`へtarget backend契約を定義
6. include依存を循環させず、forward declarationを優先
7. header単体compile testを追加

`inode.h`へV7のfilesystem全体をコピーしない。pipe動作に必要なfieldと、将来の
拡張に必要な明確な最小subsetだけを定義する。

### 完了条件

- 新規header内に`esh_`または`ESH_`がない
- 新しい`typedef`がない
- include順に依存しない
- Cおよび必要ならC++ include testが成功
- 既存esh buildが変更前と同じく成功

### 最小試験

- 新規header compile test
- buffer cache O0/O2
- pipe coroutine O0

### 次Stepへの引継事項

- 各structのfieldと所有権
- 各定数の値と配置理由
- 未確定fieldがあれば理由を記録

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 1を実行してください。
Step 0の確定事項に従って汎用headerを追加し、header単体試験と既存最小試験を
実行してください。esh呼出し側の切替とStep 2以降は行わないでください。
```

## 10. Step 2 file table inode table追加

### 目的

V7の`conf/c.c`と同様に、汎用file tableとinode tableの所有場所を確定する。

### 前提

- Step 1が`done`
- `file.h`と`inode.h`の構造が確定済み

### 対象

- `src/core/sys/conf/conf.c`
- 必要な定数header
- 新規sys table単体試験

### 実装内容

1. `struct file file[NFILE]`を`conf.c`へ定義
2. `struct inode inode[NINODE]`を`conf.c`へ定義
3. `NFILE`、`NINODE`、`NOFILE`の固定容量を定義
4. 初期化後の全entry未使用条件を試験
5. 既存`buf[]`、`bfreelist`、`bdevsw[]`との配置順を規約に合わせる

旧`esh_file_table`と`esh_pipe_table`はこのStepでは削除しない。新旧tableを同じ
binaryへlinkする場合も、symbol名と利用経路を分離する。

### 完了条件

- file tableとinode tableの定義が一箇所だけにある
- header側は`extern`宣言のみ
- 固定容量がcompile時に確認可能
- 初期化、全entry未使用、境界走査の試験が成功
- 既存buildが成功

### 最小試験

- 新規table単体試験 O0/O2、`-Werror`
- buffer cache O0/O2
- pipe coroutine O0

### 次Stepへの引継事項

- table初期化API
- 未使用entryの判定条件
- 容量枯渇時のerror方針

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 2を実行してください。
汎用file tableとinode tableをconf.cへ追加し、旧esh tableはまだ削除しないで
ください。Step 2の試験結果と所有権を引継ぎ書へ記録してください。
```

## 11. Step 3 file.cのV7名sourceへの分割

### 目的

現行`src/core/esh/file.c`の責務を、V7と同じ名前のsourceへ実装する。
このStepでは新sys実装を追加するが、esh本番経路の最終切替は行わない。

### 前提

- Step 2が`done`
- file/inode tableと公開headerが使用可能

### 対象

- `src/core/sys/sys/fio.c`
- `src/core/sys/sys/alloc.c`
- `src/core/sys/sys/iget.c`
- 専用のfile/inode単体試験

### 実装内容

1. `getf`でfd範囲とentryを検査
2. `ufalloc`でprocess fd tableの空きentryを確保
3. `falloc`でfdとfile objectを確保し、途中失敗を巻き戻す
4. `closef`でfile参照数を減らし、最終参照でinodeを解放
5. pipe用最小`ialloc`で空きinodeを確保
6. `iput`でinode参照数を減らし、最終参照でbufferを返却
7. process fd table初期化、明示位置install、全closeなどMCUFW補助を分離

V7の大域`u`は導入しない。process contextを明示引数で渡すか、Step 0で決めた
current process interfaceを使用する。

### 重要invariant

- fd entryは`struct file *`を所有する
- fileの`f_count`はfd tableからの参照数
- inodeの`i_count`はlive file objectからの参照数
- fd複製では`f_count`だけが増える
- file最終closeで初めて`i_count`が減る
- 途中失敗後に使用数が開始前へ戻る

### 完了条件

- 新sys実装に`esh_`または`ESH_`がない
- file枯渇、fd枯渇、inode枯渇を区別
- allocation各段の失敗注入試験が成功
- closeと全close後に全resourceを再利用可能
- 既存esh実装と既存試験が成功

### 最小試験

- file/inode単体 O0/O2、`-Werror`
- buffer cache O0/O2
- 既存pipe coroutine/thread O0

### 次Stepへの引継事項

- file、inode、fdの参照数遷移
- allocation失敗時のcleanup順序
- pipe作成側が使用する確定API

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 3を実行してください。
現行file.cの責務をV7名のfio.c、alloc.c、iget.cへ実装し、専用単体試験を
追加してください。旧file.cの削除とesh本番経路の切替は行わないでください。
```

## 12. Step 4 V7風pipe.cとinode I/O

### 目的

V7の`pipe.c`、`rdwri.c`を基準に、buffer cacheを使う汎用pipe data pathを
実装する。

### 前提

- Step 3が`done`
- file/inode lifecycleが単体試験で固定済み

### 対象

- `src/core/sys/sys/pipe.c`
- `src/core/sys/sys/rdwri.c`
- 必要ならpipe専用のscheduler stub
- 新規sys pipe単体試験

### 実装内容

1. `pipe`相当処理でinodeとread/write fileを組み立てる
2. file flagを`FREAD|FPIPE`と`FWRITE|FPIPE`へ設定
3. `readp`、`writep`、`plock`、`prele`を実装
4. `readi`、`writei`をbuffer cache上のpipe inode向けに実装
5. reader offsetを`f_offset`、writer終端を`i_size`で管理
6. readerが`i_size`へ追いついた時だけoffsetとsizeを0へ戻す
7. blocking処理から一回分の判定・転送をprivate helperへ分離
8. close時のEOF/EPIPE wakeup条件を実装

第一段階では`PIPSIZ=BSIZE`を推奨する。V7の4096 byteを採用するには複数block
mappingと十分な`NBUF`が必要であり、別仕様として扱う。

### V7との差分として明記する事項

- signalを送らずerrorを返す
- 物理block allocationを行わない
- `bmap`の完全実装を行わない
- trap/system call entryを持たない
- coroutineではsleepをpending/resumeへ変換
- 固定bufferを越えるV7のwrite overshootを許可しない

### 完了条件

- empty、full、EOF、EPIPEの状態遷移が試験化されている
- offset/size resetがV7方式
- wakeup後に条件を再検査
- closeでpeerが永久待機しない
- buffer、file、inodeのleakなし
- 旧esh pipe試験も成功

### 最小試験

- 新規sys pipe単体 O0/O2、`-Werror`
- resource exhaustionとrollback
- 既存pipe coroutine/thread O0/O2
- buffer cache O0/O2

### 次Stepへの引継事項

- schedulerに要求するsleep/wakeup契約
- wait channelの所有場所
- V7との差分一覧
- 旧ring試験のうちStep 7で変更が必要な項目

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 4を実行してください。
V7のpipe.cとrdwri.cを基準に汎用pipe data pathを追加してください。
V7との差分を明記し、esh本番経路の切替とStep 5以降は行わないでください。
```

## 13. Step 5 scheduler汎用化

### 目的

scheduler本体と公開型をeshからsys層へ移し、file/pipeから利用できる汎用待機
機構にする。

### 前提

- Step 4が`done`
- sys pipeが必要とするwait契約が記録済み

### 対象

- `src/core/esh/sched.c`から`src/core/sys/sys/sched.c`への移行
- `sched.h`、`sched_backend.h`の確定
- `exe.h`、`exe.c`、旧file/pipeの必要な追従
- scheduler test

### 実装内容

1. `struct esh_proc`を汎用process型へ変更
2. `struct esh_wait_chan`を汎用wait channel型へ変更
3. `ESH_PROC_*`、`ESH_WAIT_*`を汎用名へ変更
4. `esh_kernel_*`、`esh_sleep`、`esh_wakeup`、`esh_sched_*`を変更
5. scheduler側が`struct proc`を使う場合、esh executor側を
   `struct exe_proc`へ変更
6. wait list登録、yield、wake、releaseの順序を維持
7. test-only failure hookも汎用名へ変更

Windows backendの物理移動はStep 6で行う。このStepでは現配置のbackendを
必要最小限だけ新scheduler interfaceへ追従させてよい。

### 重要invariant

- kernel lockは非再帰
- condition検査とwait登録は同じlock区間
- backend yieldはlock保持条件を破らない
- wakeupはwait listから外してrunnableへ遷移
- releaseはwait listを切り離してからbackendを解放
- fd close後、lock外でthread join

### 完了条件

- portable scheduler source/headerに`esh_`または`ESH_`がない
- coroutine/thread × O0/O2でscheduler試験成功
- 複数waiter、release、yield失敗、wake直後releaseが成功
- pipeとpipelineの既存試験が成功

### 最小試験

- scheduler coroutine/thread × O0/O2、`-Werror`
- pipe coroutine/thread O0/O2
- pipeline executor coroutine/thread O0

### 次Stepへの引継事項

- backend APIの確定symbol一覧
- backendが守るlock/current process契約
- 一時compatibility定義とStep 7削除予定

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 5を実行してください。
scheduler本体、型、APIをsys層へ汎用化し、既存callerを必要範囲で追従させて
ください。Windows backendの物理移動はStep 6へ残してください。
```

## 14. Step 6 Windows backend移動

### 目的

指定されたWindows固有scheduler/coroutine backendをsys device層へ移す。

### 前提

- Step 5が`done`
- 汎用scheduler backend APIが確定済み

### 移動対象

```text
src/core/esh/target/windows/cortn_context.c
src/core/esh/target/windows/cortn_context.h
src/core/esh/target/windows/cortn_switch.S
src/core/esh/target/windows/sched_backend.mk
src/core/esh/target/windows/sched_cortn.c
src/core/esh/target/windows/sched_thread.c
```

移動先はすべて`src/core/sys/dev/windows/`とする。

### 実装内容

1. fileを移動し、履歴を追いやすい差分にする
2. backend内の`esh_`、`ESH_`symbolを汎用名へ変更
3. build変数を`SCHED_BACKEND`、`SCHED_CSOURCES`、
   `SCHED_ASOURCES`、`SCHED_CPPFLAGS`等へ変更
4. Windows targetと全関連test Makefileのinclude、vpath、依存先を更新
5. `cortn_context`とassemblyのoffset契約を維持
6. coroutine/thread双方でbuildと実行を確認

`cortn.c`と`cortn.h`は直接指定された移動対象ではない。Step 0で移動すると
決定していない限り、このStepで追加移動しない。

### 完了条件

- 指定6ファイルが新配置に存在
- 旧配置に指定6ファイルが残っていない
- Makefileが旧pathや`ESH_SCHED_*`を参照しない
- coroutine/thread × O0/O2の関連試験が成功
- assembly offset `_Static_assert`が成功

### 最小試験

- coroutine単体 O0/O2、`-Werror`
- scheduler coroutine/thread × O0/O2、`-Werror`
- pipe coroutine/thread O0/O2
- pipeline executor coroutine/thread O0
- Windows target両backend build

### 次Stepへの引継事項

- 全build fileの変更一覧
- 旧path検索結果
- `cortn.c`、`cortn.h`の残置理由または移動結果

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 6を実行してください。
指定されたWindows backend 6ファイルをsrc/core/sys/dev/windowsへ移動し、
build pathとbackend symbolを更新してください。Step 7のfile/pipe切替は行わないで
ください。
```

## 15. Step 7 esh呼出し側切替と旧実装削除

### 目的

esh executorとstdioを新しいsys file/pipe/schedulerへ接続し、旧esh専用実装を
削除する。

### 前提

- Step 6が`done`
- 新sys file、inode、pipe、scheduler、Windows backendが個別に検証済み

### 対象

- `src/core/esh/exe.c`
- `src/core/esh/include/exe.h`
- `src/core/esh/target/windows/stdio_adapt.c`
- Windows target Makefile
- 関連testとMakefile
- 旧esh file/pipe/scheduler headerとsource

### 実装内容

1. executor processのfd tableを新しい型へ変更
2. pipeline作成を新しいsys pipe APIへ変更
3. stdin/stdout fd installを新しいfile APIへ変更
4. stdio adapterのread/writeを新APIへ変更
5. cleanup順序をfd close後、backend release/joinのまま維持
6. 旧compatibility定義を削除
7. 次の旧ファイルを削除

```text
src/core/esh/include/esh_file.h
src/core/esh/include/esh_pipe.h
src/core/esh/include/esh_sched.h
src/core/esh/include/esh_sched_backend.h
src/core/esh/file.c
src/core/esh/pipe.c
src/core/esh/sched.c
```

8. source list、include path、dependencyを新配置へ変更
9. cscope listや関連文書の旧pathを更新

### 完了条件

- `src/core/sys`に`esh_`または`ESH_`がない
- 削除対象の旧header/sourceが存在しない
- 旧pathを参照するMakefileがない
- pipeline成功・失敗時のresource回収が成功
- EOF、EPIPE、部分転送、close wakeupが新実装で成功
- parser/executorの外部動作が退行しない

### 最小試験

- file/inode/sys pipe単体 O0/O2、`-Werror`
- pipe coroutine/thread × O0/O2、`-Werror`
- scheduler coroutine/thread × O0/O2、`-Werror`
- pipeline executor coroutine/thread × O0/O2
- executor test一式
- parser test1/test2
- Windows target両backend build

### 次Stepへの引継事項

- 削除済み旧API一覧
- 変更したpipe意味、特にringからV7 offset方式への差分
- 残っている既存warning
- 未実行の対話試験

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 7を実行してください。
esh呼出し側を新sys file/pipe/schedulerへ切り替え、指定された旧esh専用headerと
sourceを削除してください。全関連試験を実行し、Step 8の文書完了作業は残して
ください。
```

## 16. Step 8 全回帰試験と文書更新

### 目的

移行結果を全体として検証し、実装と文書を最終状態へ揃える。

### 前提

- Step 7が`done`
- 旧esh file/pipe/scheduler実装が削除済み

### 静的確認

1. `src/core/sys`の`esh_`、`ESH_`検索が0件
2. 旧header名と旧pathの参照が0件
3. source/headerの新規`typedef`が0件
4. trailing whitespaceが0件
5. コメントを含む変更行が80 column以内
6. `git diff --check`成功
7. 意図しない未追跡ファイルがdiffへ入っていない

### 回帰試験

- buffer cache O0/O2
- file/inode単体 O0/O2、`-Werror`
- sys pipe単体 O0/O2、`-Werror`
- coroutine単体 O0/O2、`-Werror`
- pipe coroutine/thread × O0/O2、`-Werror`
- scheduler coroutine/thread × O0/O2、`-Werror`
- pipeline executor coroutine/thread × O0/O2
- parser test1/test2
- executor test一式
- cquote
- Windows target coroutine/thread build
- Windows対話確認
  - `sam1`
  - `sam2 1 10 100`
  - `sam3 -a -b string`
  - 2段pipeline
  - 4段pipeline
  - 引用変数を含むpipeline

### 文書更新

最低限、次を現行の型名、path、pipe意味へ更新する。

- `docs/esh_pipe_api.md`
- `docs/esh_pipe_core.md`
- `docs/esh_pipe_sched.md`
- `docs/esh_pipe_thread.md`
- `docs/esh_pipe_regression.md`
- `docs/cortn_windows.md`
- 本引継ぎ書の全Step状態

古い設計経緯を残す文書では、過去形で旧`esh_*`名を記載してよい。現行APIとして
誤読される記述は更新する。

### 完了条件

- 全必須試験が成功
- 新規warningなし
- 既存warningは種類と対象を記録
- 全Stepが`done`または理由付き`blocked`
- V7から採用した要素と意図的差分が文書化済み
- Windows-firstの移行が完了し、STM32移植は未着手のまま

### 新規タスク開始文

```text
docs/unix_v7_generic_sys_migration_handoff.mdのStep 8を実行してください。
全回帰試験、静的確認、Windows対話確認、関連文書更新を行い、各Stepの最終状態と
UNIX V7との差分を記録してください。新機能追加やSTM32移植は行わないでください。
```

## 17. 試験実行方法

代表的なsuiteはMSYS2 MINGW64 shell内で実行する。

```sh
cd /c/usr/src/MCUFW/src/core/sys/test/bio
bash ./run_test.sh

cd /c/usr/src/MCUFW/src/core/esh/test/pipe
bash ./run_test.sh

cd /c/usr/src/MCUFW/src/core/esh/test/sched
bash ./run_test.sh

cd /c/usr/src/MCUFW/src/core/esh/test/pipeline_exec
bash ./run_test.sh

cd /c/usr/src/MCUFW/src/core/esh/test/par
bash ./run_test1.sh
bash ./run_test2.sh

cd /c/usr/src/MCUFW/src/core/esh/test/exe
bash ./run_test.sh
```

新規sys試験は既存の`src/core/sys/test/bio`と同じ形式で、O0/O2、`-Werror`を
基本とする。threadを使う試験はtimeoutを設定し、hangを成功扱いしない。

## 18. 作成時ベースライン

2026-10-12に次を実行し、すべて成功した。

- buffer cache O0/O2
- pipe coroutine/thread × O0/O2
- scheduler coroutine/thread × O0/O2
- pipeline executor coroutine/thread × O0/O2

pipeとschedulerは`-Wall -Wextra -Werror -pedantic`で成功した。pipeline全体には
help、lex、par、test由来の既存warningがある。移行による新規warningと混同しない。

## 19. 各Step完了時の記録形式

各担当タスクは、該当Step末尾またはこの節へ次の形式で追記する。

```text
状態: done
実施日: YYYY-MM-DD
開始HEAD: <commit>
完了HEAD: <commit または uncommitted>

変更:
- ...

設計判断:
- ...

試験:
- <suite>: pass/fail

残課題:
- ...

次Stepへの注意:
- ...
```

commit、push、PR作成は、各新規タスクでユーザーから明示的に依頼された場合だけ
行う。実装依頼だけの場合は変更と試験まで行い、commitを自動で作成しない。
