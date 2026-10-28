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
| 0 | API名、V7対応範囲、配置の確定 | done | `f5f6925` |
| 1 | 汎用header追加 | done | `2fe2b53` |
| 2 | file table、inode table追加 | done | `4f8dd3c` |
| 3 | `file.c`のV7名sourceへの分割 | done | `5858392` |
| 4 | V7風`pipe.c`とinode I/O | done | `17e4824` |
| 5 | scheduler汎用化 | done | `2f46a2b` |
| 6 | Windows backend移動 | done | `e8516c4` |
| 7 | esh呼出し側の切替と旧実装削除 | done | `7c6a1f9` |
| 8 | 全回帰試験と文書更新 | done | `uncommitted` |

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

## 6. 最終配置（Step 0確定）

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
│       ├── sched_adapt.mk
│       ├── sched_cortn.c
│       └── sched_thread.c
├── h/
│   ├── cortn.h
│   ├── file.h
│   ├── inode.h
│   ├── param.h
│   ├── pipe.h
│   ├── sched.h
│   ├── sched_adapt.h
│   └── sys_error.h
└── sys/
    ├── alloc.c
    ├── cortn.c
    ├── fio.c
    ├── iget.c
    ├── pipe.c
    ├── rdwri.c
    ├── sched.c
    └── sys2.c
```

Step 0で`sys2.c`、`param.h`、`sys_error.h`、汎用coroutine実装の配置を
上記のとおり確定した。Windows固有backend 6ファイルは
`src/core/sys/dev/windows/`へ置く。

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

### Step 0実施結果（2026-10-12）

状態: `done`

source動作は変更していない。確認用sourceは一時作成後に削除した。以下を
Step 1以降の確定事項とする。

#### V7から採用する範囲

| V7要素 | MCUFWでの採用範囲 | 意図的差分 |
|---|---|---|
| `struct file` | `f_flag`、`f_count`、`f_inode`、`f_offset` | `off_t`でなく`size_t` |
| `struct inode` | pipe用flag、参照数、size、buffer、wait channel | device、mode、owner、block addressは持たない |
| `file[NFILE]`、`inode[NINODE]` | `conf/conf.c`が固定長tableを所有 | 動的確保なし |
| `getf`、`ufalloc`、`falloc`、`closef` | V7と同じ責務名を使用 | 大域`u`でなく`struct proc *`を明示 |
| `pipe`、`readp`、`writep` | `pipe.c`へ配置 | public入口だけ`sys_pipe` |
| `readi`、`writei` | `rdwri.c`へ配置 | buffer cache上のpipeだけを対象 |
| `plock`、`prele` | `pipe.c`へ配置 | explicit wait channelを使用 |
| `read`、`write`、`close` | `sys2.c`へ配置 | symbol名は`sys_read`等 |
| `sleep`、`wakeup` | `sched.c`へ配置 | `sched_sleep`、`sched_wakeup` |

directory、pathname、mount、permission、special device、signal、trap、MMU、
disk block mappingは対象外とする。V7 `u.u_*`に相当する暗黙状態は導入しない。
current processの取得だけをbackend interfaceへ残し、file allocation helperは
`struct proc *`を明示引数で受け取る。

#### 型と所有関係

`struct proc`をscheduler所有の汎用process型とし、次を保持する。

- `struct file *p_ofile[NOFILE]`
- process state、wait channel link、backend pointer、終了status

現行`struct esh_fd_table`は独立型として移行せず、V7の`u.u_ofile[]`に相当する
配列を`struct proc`へ埋め込む。現行executorの`struct proc`はStep 5で
`struct exe_proc`へ変更する。これにより`struct proc` tagの二重定義を避ける。

`struct file`は一つのopen pipe endpointを表す。`f_count`はfdからfileへの参照、
`struct inode`の`i_count`はlive file objectからinodeへの参照を数える。fd複製では
`f_count`だけを増やす。最後の`closef`でのみ`i_count`を減らす。

offsetとsizeは非負で`PIPSIZ`以下に制限されるため、host依存の`off_t`を
導入せず`size_t`を使う。転送byte数または負errorを返す入口は従来どおり
`ptrdiff_t`を使う。新しい`typedef`は追加しない。

#### 確定symbol表

| 現行symbol | 汎用sys symbol | 公開区分、配置 |
|---|---|---|
| `struct esh_file` | `struct file` | 公開定義、`h/file.h` |
| `struct esh_pipe_inode` | `struct inode` | sys内部型、`h/inode.h` |
| `struct esh_proc` | `struct proc` | 公開型、`h/sched.h` |
| executor `struct proc` | `struct exe_proc` | esh内部型、`esh/include/exe.h` |
| `struct esh_wait_chan` | `struct wait_channel` | sys内部型、`h/sched.h` |
| `ESH_FD_STDIN/STDOUT/STDERR` | `FD_STDIN/STDOUT/STDERR` | `h/file.h` |
| `ESH_OPEN_MAX` | `NOFILE` | `h/param.h`、値12 |
| `ESH_FILE_MAX` | `NFILE` | `h/param.h`、値12 |
| `ESH_PIPE_MAX` | `NINODE` | `h/param.h`、値3 |
| `ESH_PIPE_BUFFER_SIZE` | `PIPSIZ` | `h/pipe.h`、値`BSIZE` |
| `ESH_PIPELINE_MAX` | `PIPELINE_MAX` | `h/param.h`、値4 |
| `ESH_FILE_READ/WRITE` | `FREAD/FWRITE` | `h/file.h`、bit flag |
| なし | `FPIPE` | `h/file.h`、bit flag |
| `ESH_E*` | `SYS_E*` | `h/sys_error.h` |
| `esh_file_system_init` | `file_system_init` | 公開、`fio.c` |
| `esh_fd_get` | `getf` | sys内部、`fio.c` |
| fd空き検索 | `ufalloc` | sys内部、`fio.c` |
| `esh_file_alloc`とfd確保 | `falloc` | sys内部、`fio.c` |
| `esh_file_put` | `closef` | sys内部、`fio.c` |
| `esh_fd_install_at` | `fd_install_at` | 移行補助、`fio.c` |
| `esh_close_all` | `close_all` | 公開、`fio.c` |
| `esh_pipe_inode_alloc` | `ialloc` | sys内部、`alloc.c` |
| `esh_pipe_inode_discard` | `iput` | sys内部、`iget.c` |
| `esh_pipe_for_proc` | `pipe_for_proc` | sys内部、`pipe.c` |
| `esh_pipe` | `sys_pipe` | 公開、`pipe.c` |
| pipe一回read/write helper | `pipe_read_once`/`pipe_write_once` | `pipe.c`の`static` |
| pipe read/write | `readp`/`writep` | sys内部、`pipe.c` |
| inode read/write | `readi`/`writei` | sys内部、`rdwri.c` |
| pipe lock/unlock | `plock`/`prele` | sys内部、`pipe.c` |
| `esh_read` | `sys_read` | 公開、`sys2.c` |
| `esh_write` | `sys_write` | 公開、`sys2.c` |
| `esh_close` | `sys_close` | 公開、`sys2.c` |
| `esh_kernel_*` | `kernel_*` | 公開、`sched.c` |
| `esh_sleep`/`esh_wakeup` | `sched_sleep`/`sched_wakeup` | sys内部、`sched.c` |
| `esh_current_proc` | `current_proc` | 公開、`sched.c` |
| `esh_proc_*` | `proc_*` | 公開、`sched.c` |
| `esh_sched_*` | `sched_*` | 公開、`sched.c` |
| `esh_sched_backend_*` | `sched_adapt_*` | private backend契約 |
| `ESH_PROC_*` | `PROC_*` | `h/sched.h` |
| `ESH_WAIT_*` | `WAIT_*` | `h/sched.h` |

`SYS_EBADF`、`SYS_EMFILE`、`SYS_ENFILE`、`SYS_ENOMEM`、`SYS_EINVAL`、
`SYS_EPIPE`、`SYS_EAGAIN`、`SYS_ENOSYS`は正値の汎用enumとし、関数は失敗時に
その負値を返す。host `errno`へ値を合わせず、sys coreから`<errno.h>`を
includeしない。

#### header公開範囲

| header | 区分 | 内容 |
|---|---|---|
| `h/param.h` | 公開 | `NOFILE`、`NFILE`、`NINODE`、`PIPELINE_MAX` |
| `h/sys_error.h` | 公開 | host非依存の`SYS_E*` |
| `h/file.h` | 公開 | `struct file`、flag、`sys_*`、file lifecycle API |
| `h/sched.h` | 公開 | `struct proc`、process state、scheduler API |
| `h/inode.h` | sys private | pipe用最小`struct inode`、inode lifecycle |
| `h/pipe.h` | sys private | `PIPSIZ`、`readp`、`writep`、lock契約 |
| `h/sched_adapt.h` | backend private | target backend契約 |
| `h/cortn.h` | backend private | coroutine engineと単体試験用契約 |

「公開」はesh等のsys外callerが使用可能という意味であり、安定ABIは意味しない。
`file[]`、`inode[]`は各headerで不完全配列の`extern`宣言だけを置き、実体は
`conf/conf.c`だけに置く。sys private headerをeshから直接includeしない。

#### source配置と移行時期

- `sys/sys2.c`を追加し、`sys_read`、`sys_write`、`sys_close`を置く
- `sys_pipe`はV7と同じく`sys/pipe.c`へ置く
- `h/param.h`と`h/sys_error.h`はStep 1で他headerと同時に追加する
- `cortn.c`は`sys/sys/cortn.c`、`cortn.h`は`sys/h/cortn.h`へStep 6で移す
- Windows固有6ファイルは予定どおりStep 6で`sys/dev/windows`へ移す

`cortn.c`と`cortn.h`にはesh依存がなく、sys schedulerのcoroutine backendが
使用する。これらをeshへ残すとsys層からesh層への逆依存になるため、Step 6の
backend移動と同時に移す。

#### MinGW衝突確認

MSYS2 MINGW64 GCCでheaderをincludeした最小programをcompile/linkした。

| 名前 | 結果 | 判断 |
|---|---|---|
| `read` | `<io.h>`の`int read(..., unsigned int)`と型衝突 | `sys_read` |
| `write` | `<io.h>`の`int write(..., unsigned int)`と型衝突 | `sys_write` |
| `close` | 同じ型で定義でき、実行物が自前`close`をexport | silent overrideを避け`sys_close` |
| `pipe` | MinGWは`_pipe`だけを宣言し、現状はlink可能 | 他hostとの一貫性から`sys_pipe` |
| `sleep` | `<unistd.h>`の`unsigned int sleep(unsigned int)`と型衝突 | `sched_sleep` |
| `EBADF`等 | `<errno.h>`のmacro展開でenum名が構文error | `SYS_E*` |
| `struct proc` | 同一翻訳単位でtag再定義error | executor側を`struct exe_proc`へ変更 |

MinGWの`off_t`はbuild optionにより`off32_t`または`off64_t`となるため採用しない。
`pipe`が現在衝突しないことはWindows固有の結果であり、bare-metalを含む共通API名を
hostごとに変える理由にはしない。

#### compatibility方針

汎用sys headerへ`esh_*` aliasやmacro wrapperは追加しない。Step 5では旧esh
file/pipe callerを新scheduler名へ必要範囲で追従させ、Step 7でfile/pipe入口を
一括切替する。一時的compatibility定義は現時点で不要であり、追加する場合は
Step 7で必ず削除する。

#### 開始時試験

次の既存試験が成功した。

- buffer cache O0/O2
- pipe coroutine/thread O0/O2
- scheduler coroutine/thread O0/O2
- pipeline executor coroutine/thread O0/O2
- parser `run_test1.sh`、`run_test2.sh`
- executor `run_test.sh`

#### Step 1への引継事項

- 上記7 headerを追加し、`param.h`と`sys_error.h`もStep 1対象に含める
- `struct proc.p_ofile[NOFILE]`をfd tableとし、別の`struct fd_table`を作らない
- file/inode offsetには`size_t`を使用し、`off_t`を追加しない
- sys headerにhost `errno`、Windows header、esh headerをincludeしない
- `cortn.c`、`cortn.h`の物理移動はStep 6まで行わない
- compatibility aliasは作らない

## 9. Step 1 汎用header追加

### 目的

汎用sys層の公開契約を定義する。旧esh headerと並行して追加し、本番呼出し側の
切替は行わない。

### 前提

- Step 0が`done`
- Step 0のsymbol表が確定済み

### 対象

- `src/core/sys/h/param.h`
- `src/core/sys/h/sys_error.h`
- `src/core/sys/h/file.h`
- `src/core/sys/h/inode.h`
- `src/core/sys/h/pipe.h`
- `src/core/sys/h/sched.h`
- `src/core/sys/h/sched_adapt.h`

### 実装内容

1. `param.h`へ固定容量、`sys_error.h`へhost非依存errorを定義
2. `file.h`へ`struct file`、file flag、`extern struct file file[]`を定義
3. `inode.h`へpipeに必要な最小`struct inode`とflagを定義
4. `pipe.h`へMCUFW固有の容量、I/O状態、internal prototypeを定義
5. `sched.h`へprocess、wait channel、scheduler APIを定義
6. `sched_adapt.h`へtarget backend契約を定義
7. include依存を循環させず、forward declarationを優先
8. header単体compile testを追加

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

### Step 1実施結果（2026-10-12）

状態: `done`

Step 0で確定した次のheaderを追加した。

- `src/core/sys/h/param.h`
- `src/core/sys/h/sys_error.h`
- `src/core/sys/h/file.h`
- `src/core/sys/h/inode.h`
- `src/core/sys/h/pipe.h`
- `src/core/sys/h/sched.h`
- `src/core/sys/h/sched_adapt.h`

旧esh headerとcallerは変更していない。新sys headerからesh header、host
`errno.h`、Windows headerはincludeしていない。compatibility aliasもない。

#### structと所有権

| struct | 主なfield | 所有権、未使用条件 |
|---|---|---|
| `struct file` | `f_flag`、`f_count`、`f_inode`、`f_offset` | `file[]`が所有、`f_count == 0`で未使用 |
| `struct inode` | `i_flag`、`i_count`、`i_buf`、`i_size`、3 wait channel | `inode[]`が所有、`i_count == 0`で未使用 |
| `struct proc` | `p_state`、`p_ofile[]`、wait link、backend、status | scheduler callerがstorageを所有 |
| `struct wait_channel` | wait list head、tail | inodeまたはcallerがstorageを所有 |
| `struct pipe_io_status` | result、転送数、error | 一回分pipe I/Oの値object |

`struct proc.p_ofile[NOFILE]`がprocessごとのfd tableである。fd entryは
`struct file *`を所有し、install時に`f_count`を増やす。`struct file`は
`struct inode *`を所有する。`ialloc`直後の`i_count`はpipe組立中の一時参照も
表せるため、commentをfileまたはconstruction referenceとした。pipe完成後は
live file objectからの参照数になる。

`struct inode`はV7 filesystem全体を持たず、pipeに必要なbuffer、size、lock、
明示的なread/write wait channelだけを持つ。`i_mode`、device、inode number、
owner、block address、timestampは意図的に追加していない。

#### 定数とflag

| 定数 | 値 | 配置理由 |
|---|---|---|
| `NOFILE` | 12 | process fd上限、`param.h` |
| `NFILE` | 12 | global file table上限、`param.h` |
| `NINODE` | 3 | 同時pipe inode上限、`param.h` |
| `PIPELINE_MAX` | 4 | scheduler process上限、`param.h` |
| `PIPSIZ` | `BSIZE`、現在256 | buffer cache一block制約、`pipe.h` |
| `FREAD/FWRITE/FPIPE` | `0x01/0x02/0x04` | V7 `file.h`と同じbit |
| `ILOCK/IWANT` | `0x01/0x10` | V7 `inode.h`と同じbit |

`pipe.h`は`NBUF >= NINODE`をcompile時に検査する。errorはStep 0どおり正値の
`SYS_E*` enumとし、APIは失敗時に負値を返す。

#### header依存とAPI

- `sched.h`は`struct file`をforward declarationし、循環includeを避ける
- `file.h`は`struct inode`と`struct proc`をforward declarationする
- `inode.h`はwait channel実体を埋め込むため`sched.h`をincludeする
- `pipe.h`は`PIPSIZ`とbuffer数検査のため`buf.h`、`param.h`をincludeする
- `sched_adapt.h`だけが`sched.h`へ依存する

V7名の`getf`、`ufalloc`、`falloc`、`closef`、`ialloc`、`iput`、`readp`、
`writep`、`readi`、`writei`、`plock`、`prele`を宣言した。MCUFW補助として
`fd_install`、`fd_install_at`、`fd_close`、`close_all`も宣言した。
`sys_pipe`、`sys_read`、`sys_write`、`sys_close`は`file.h`の公開入口とした。

#### 試験

`src/core/sys/test/header/`を追加し、次を`-Werror -pedantic`で確認した。

- 各header単独compile、C/C++、O0/O2
- 全headerの通常順、逆順compile、C/C++、O0/O2
- 定数、flag、fd配列長、offset/size型のcompile時検査
- 新header内に`esh_`、`ESH_`、`typedef`がないこと
- buffer cache O0/O2
- pipe coroutine O0

すべて成功した。

#### 未確定事項

未確定fieldはない。table実体、初期化処理、allocation実装は後続Stepの責務で
あり、このStepでは宣言だけとした。`struct pipe_io_status`を使用する一回転送
helperはStep 4で`pipe.c`の`static`関数として実装する。

#### Step 2への引継事項

- `conf/conf.c`へ`struct file file[NFILE]`と
  `struct inode inode[NINODE]`を一度だけ定義する
- static storageのzero初期化により`f_count == 0`、`i_count == 0`を保証する
- `file_system_init`実装はStep 3の`fio.c`まで追加しない
- fd枯渇は`SYS_EMFILE`、global file/inode枯渇は`SYS_ENFILE`とする
- fileとinodeの枯渇stageは同じerror値でも試験上は別々に確認する
- 旧`esh_file_table`、`esh_pipe_table`は変更または削除しない

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

### Step 2実施結果（2026-10-12）

状態: `done`

`src/core/sys/conf/conf.c`へ次の順でstatic storageを定義した。

1. `struct buf buf[NBUF]`
2. `struct file file[NFILE]`
3. `struct inode inode[NINODE]`
4. `struct buf bfreelist`

V7 `conf/c.c`のtable配置に合わせ、buffer tableの後、buffer free listの前を
file tableとinode tableの所有場所とした。定義は`conf.c`だけに置き、
`file.h`と`inode.h`には不完全配列の`extern`宣言だけを残した。
`NFILE`、`NINODE`、`NOFILE`の定義場所は`param.h`から変更していない。

#### 初期状態と所有権

明示的な初期化処理は追加していない。Cのstatic storage zero初期化により、
全entryはprogram開始時にzeroとなる。`f_count == 0`を未使用file entry、
`i_count == 0`を未使用inode entryの判定条件とする。他fieldも未使用時はzeroを
保つ。`file_system_init`による再初期化は予定どおりStep 3の`fio.c`で実装する。

旧`esh_file_table`と`esh_pipe_table`は変更、削除していない。新旧tableは異なる
symbolと利用経路を持つため、現段階でも同じbinaryへlinkできる。

#### 試験

`src/core/sys/test/table/`へtable単体試験を追加し、次を確認した。

- `NFILE == 12`、`NINODE == 3`、`NOFILE == 12`のcompile時検査
- static storage初期状態で全file/inode entryがzeroかつ未使用
- file tableとinode tableの先頭、末尾を含む全entry走査
- 全entry使用状態からzeroへ戻した後の再走査
- `-Werror -pedantic`によるO0/O2 buildと実行

次の最小試験はすべて成功した。

- 新規table単体試験 O0/O2
- buffer cache O0/O2
- pipe coroutine O0

#### Step 3への引継事項

- `file_system_init`は`file[]`の全fieldをzeroへ戻す
- 未使用entryは`f_count == 0`または`i_count == 0`で判定する
- fd枯渇は`SYS_EMFILE`、global file table枯渇は`SYS_ENFILE`とする
- inode table枯渇も`SYS_ENFILE`とし、file枯渇とは別の試験で確認する
- allocation途中失敗ではfd、file、inodeの使用数を開始前へ戻す
- 旧esh tableと本番経路はStep 3でも削除、切替しない

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

### Step 3実施結果（2026-10-12）

状態: `done`

V7の責務名に合わせ、次のsourceを追加した。

| source | 実装した責務 |
|---|---|
| `sys/fio.c` | `getf`、`ufalloc`、`falloc`、`closef`、fd補助、初期化 |
| `sys/alloc.c` | pipe用最小`ialloc`、inode使用数取得 |
| `sys/iget.c` | 最終参照でbufferを返す`iput` |

V7の大域`u`と暗黙error状態は導入していない。`struct proc *`を明示引数とし、
整数を返すAPIは失敗時に負の`SYS_E*`を返す。pointerを返す`falloc`は失敗時に
`NULL`を返し、`fd`出力へ負errorを格納する。`ialloc`は失敗時に`NULL`を返す。

`file_system_init`は、所有中resourceがないという呼出し条件の下で`binit`を
呼び、`file[]`と`inode[]`をzeroへ戻す。旧`src/core/esh/file.c`、旧table、
esh本番経路は変更していない。

#### 参照数遷移

- `falloc`成功時に空fdへfileをinstallし、`f_count`を0から1にする
- `fd_install`と`fd_install_at`はfd複製として`f_count`を1増やす
- `fd_close`はfdを先に空にし、`closef`で`f_count`を1減らす
- `f_count`が0になった時だけfileをzeroへ戻して`iput`を呼ぶ
- `ialloc`はbufferを確保し、組立中参照として`i_count`を1にする
- `iput`は`i_count`を1減らし、0になった時だけbufferをzero化して返す
- fd複製と途中closeでは`i_count`を変更しない
- `close_all`は各非NULL fdを一度ずつ閉じ、重複fdも個別に解放する

#### 失敗時の状態

- fd枯渇: `falloc`は`-SYS_EMFILE`をfd出力へ格納し、fileを変更しない
- file枯渇: `falloc`は`-SYS_ENFILE`をfd出力へ格納し、fdを変更しない
- inode枯渇: `ialloc`は`NULL`を返し、既存inodeを変更しない
- buffer枯渇: `ialloc`はinodeを使用状態にせず`NULL`を返す
- 参照数上限: fd installは`-SYS_ENFILE`を返し、fdを変更しない

`falloc`はV7と同様にfd検索後にfile tableを検索するが、file枯渇時にはfdへ
一時pointerを置かない。このため明示的な予約解除は不要であり、開始前の状態が
保たれる。

#### 試験

`src/core/sys/test/file/`へ専用単体試験を追加した。次を
`-Werror -pedantic`のO0/O2で確認した。

- `getf`、`ufalloc`、`falloc`の正常系と不正引数
- fd自動install、指定位置install、fd複製、個別close、全close
- fd、file、inode、bufferの各枯渇stageと失敗後の使用数
- file最終closeだけがinode参照を解放すること
- inode最終参照だけがbufferを返すこと
- 全close後にfile、inode、bufferを先頭から再利用できること

新規file/inode単体試験 O0/O2、buffer cache O0/O2、既存pipeの
coroutine/thread O0はすべて成功した。

#### Step 4への引継事項

- `ialloc`が返す`i_count == 1`はpipe組立中参照として扱う
- read/write両file確保後にinodeを両endpointへ設定し、`i_count`を2にする
- 各`falloc`はfdへinstall済みなので、失敗cleanupは`fd_close`を使用する
- 2個目の`falloc`失敗時はread fdを閉じた後、組立中inodeを`iput`する
- `FREAD|FPIPE`と`FWRITE|FPIPE`、各`f_offset`はpipe作成側で設定する
- 最終endpoint close時のpeer wakeupはStep 4で`closef`へ接続する
- pipe作成側はfd枯渇とfile枯渇を`falloc`のfd出力で区別する
- inodeまたはbuffer確保失敗は`SYS_ENFILE`として公開する

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

### Step 4実施結果（2026-10-13）

状態: `done`

V7の責務名と状態遷移に合わせ、`sys/pipe.c`と`sys/rdwri.c`を追加した。
`pipe_for_proc`と`sys_pipe`がinode、read file、write fileを組み立てる。
`readp`と`writep`が待機を含むpipe処理、`readi`と`writei`が一つのbuffer内の
転送を担当する。旧esh pipeと本番経路は変更していない。

#### dataとoffset

- pipe dataは`struct inode.i_buf`が所有する一つのbufferへ置く
- reader位置はread fileの`f_offset`が所有する
- writer終端と有効data終端はinodeの`i_size`が所有する
- readは`f_offset`から`i_size`までを転送する
- writeは`i_size`から`PIPSIZ`までを転送する
- readerが`i_size`へ追いついた時だけ`f_offset`と`i_size`を0へ戻す
- partial readだけでは先頭側の空き領域を再利用しない
- `PIPSIZ == BSIZE == 256`を上限とし、一つのbufferを越えない

この方式はV7のoffset/size resetを維持する。旧esh実装のring bufferとは異なり、
partial read直後に先頭領域へwrapしてwriteしない。

#### 状態遷移

| 状態 | read | write |
|---|---|---|
| empty、peerあり | `WAIT_READ` | data転送 |
| empty、writerなし | EOF | - |
| dataあり | 現在読める分を転送 | 末尾空きへ転送 |
| full、readerあり | data転送 | `WAIT_WRITE` |
| readerなし | 残存dataを読み切った後EOF | `SYS_EPIPE` |

backend未接続processで待機が必要な場合、read/writeは`-SYS_EAGAIN`を返す。
writeがfullになるまで一部転送済みなら、その転送byte数を返す。backend接続時は
`sched_sleep`から戻った後にinode lockを取り直し、条件を再検査する。

`closef`は最終file参照を閉じる時、inode参照を減らす前にread/write両wait
channelを起床する。これによりwriter close後のEOFとreader close後のEPIPEを
待機中peerが再検査できる。fd複製の途中closeでは起床しない。

#### pipe lockとscheduler契約

inodeはlock、read、writeの三つのwait channelを所有する。`plock`は`ILOCK`を
検査し、競合時に`IWANT`を設定してlock channelで待つ。`prele`は`ILOCK`を
解除し、`IWANT`があればlock waiterを起床する。

このため`sched.h`へ`PROC_WAIT_LOCK`と`WAIT_LOCK`を追加した。Step 5の
schedulerは次を守る。

- callerはcondition検査からwait登録までkernel lockを保持する
- `sched_sleep`はwait list登録とbackend yieldを分離不能にする
- backend yieldは再開時にkernel lockを保持した状態で戻る
- `sched_wakeup`は対象channelの全waiterをlistから外してrunnableにする
- `WAIT_LOCK`、`WAIT_READ`、`WAIT_WRITE`を対応するprocess状態へ変換する
- close中のwakeupはfile/inode解放完了後にwaiterが実行される順序を保つ

Step 4単体試験では上記interfaceだけを実装するscheduler stubを使用した。
実schedulerへの移行は予定どおりStep 5で行う。

#### allocationとcleanup

pipe作成は`ialloc`、read側`falloc`、write側`falloc`の順で行う。両file確保後に
flagとinodeを設定し、組立中参照をendpoint二つの参照へ変換して
`i_count == 2`とする。

- inode失敗: `SYS_ENFILE`を返す
- read file失敗: `iput`でinodeとbufferを返す
- write file失敗: read fdを閉じ、続いて`iput`する
- fd枯渇とfile枯渇: `falloc`の`SYS_EMFILE`と`SYS_ENFILE`を維持する
- 成功後の全close: file、inode、bufferをすべて再利用可能にする

#### V7との差分

- trapまたは大域`u`を使わず、`struct proc *`と引数を明示する
- signalを送らず、reader不在を`-SYS_EPIPE`で通知する
- 物理block allocation、disk I/O、`bmap`を実装しない
- 一つのbufferだけを使用し、V7の`PIPSIZ`超過書込みを許可しない
- coroutine backendではsleepをyield/resumeへ変換する
- thread backendでも同じ状態遷移と戻り値を使用する

#### 試験

新規`src/core/sys/test/pipe/`で次をO0/O2、`-Werror -pedantic`で確認した。

- pipe作成、flag、file/inode参照数、全close
- partial read/write、data順序、V7式offset/size reset
- empty待機、full待機、EOF、EPIPE、zero-length操作
- sleep後に条件を再検査してread/writeを完了すること
- lock待機と`WAIT_LOCK`、close時のread/write wakeup
- fd、file、inode、pipe容量の各枯渇stageとrollback
- `readi`、`writei`のbuffer境界と`PIPSIZ`上限

次の試験はすべて成功した。

- 新規sys pipe O0/O2
- file/inode O0/O2
- header C/C++、通常順/逆順、O0/O2
- buffer cache O0/O2
- 既存pipe coroutine/thread O0/O2

#### Step 5への引継事項

- schedulerへ`PROC_WAIT_LOCK`と`WAIT_LOCK`を実装する
- 三つのwait channelは各inodeが所有し、inode最終解放まで有効とする
- sleepはkernel lock保持中に登録し、再開時もlock保持状態で戻る
- wakeup後は`readp`、`writep`、`plock`が必ず条件を再検査する
- closeはkernel lock保持中に両peer channelを起床してから`iput`する
- Step 7で旧ring固有のwrap-around期待値をV7 offset方式へ変更する
- `sys_read`、`sys_write`、`sys_close`を置く`sys2.c`はStep 7切替時に追加する

## 13. Step 5 scheduler汎用化

### 目的

scheduler本体と公開型をeshからsys層へ移し、file/pipeから利用できる汎用待機
機構にする。

### 前提

- Step 4が`done`
- sys pipeが必要とするwait契約が記録済み

### 対象

- `src/core/esh/sched.c`から`src/core/sys/sys/sched.c`への移行
- `sched.h`、`sched_adapt.h`の確定
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

### Step 5実施結果（2026-10-13）

`src/core/sys/sys/sched.c`へportable scheduler本体を追加し、Step 1で
用意した`struct proc`、`struct wait_channel`、汎用APIへ移行した。wait listへの
登録、backend yield、wakeup、releaseの順序は旧実装を維持した。

Step 4から引き継いだinode lock待機のため、`WAIT_LOCK`を
`PROC_WAIT_LOCK`へ遷移させる経路をschedulerとcoroutine backendへ追加した。
sleepはkernel lock保持中にwait listへ登録し、backend yieldからlock保持状態で
戻る。resume結果がwait状態でも、並行wakeupが公開したrunnable状態を上書き
しない。

releaseはkernel lock保持中にwait listからprocessを切り離し、backend pointerを
退避してからlock外でbackendを解放する。thread backendのjoinがkernel lock外に
残るため、fd close後にreleaseするcaller契約を維持できる。

executor側の既存`struct proc`は`struct exe_proc`へ変更した。Step 7までは旧
file/pipe/scheduler経路を維持し、executor内部の`struct esh_proc`埋込みと旧APIは
切り替えていない。

Windows backendの物理移動をStep 6へ残すため、現配置の
`sched_cortn.c`、`sched_thread.c`は`SYS_SCHED`指定時だけ汎用symbolを生成する
一時compatibility定義を持つ。通常buildでは従来symbolを生成し、既存経路を
維持する。

新規`src/core/sys/test/sched`では次を確認した。

- coroutine/thread、O0/O2、`-Werror`でschedulerとsys pipeの結合成功
- `WAIT_LOCK`、複数waiter、待機中release、wake直後releaseの成功
- test-only yield失敗時のwait list cleanup成功
- backend slot枯渇と再利用成功
- pipe blocking transfer、EOF、resource回収成功

既存schedulerとpipeはcoroutine/thread、O0/O2で成功した。sys pipe単体は
O0/O2で成功した。pipeline executorはcoroutine/thread、O0で成功した。
coroutine単体もO0/O2、`-Werror`で成功した。
pipeline試験が従来から出力するparser error診断とcompiler warningは残るが、
試験結果は`all tests passed`だった。

#### Step 6への引継事項

確定したbackend APIは次のとおり。

- `sched_adapt_lock`、`sched_adapt_unlock`、
  `sched_adapt_is_locked`
- `sched_adapt_current`、`sched_adapt_set_current`
- `sched_adapt_prepare`、`sched_adapt_release`
- `sched_adapt_yield`、`sched_adapt_wake`、
  `sched_adapt_resume`

backendは非再帰kernel lockを提供する。`sched_adapt_yield`はlock保持状態で
呼ばれ、待機中だけlockを解放し、lockを再取得して戻る。
`sched_adapt_current`は実行中processを各実行contextで返す。wakeは待機中の
backendを再開可能にし、実際のrun token付与は`sched_resume`が行う。

Step 6ではWindows backendを移動し、`SYS_SCHED`分岐と`esh_`/`ESH_`の一時
compatibility定義を削除して、汎用symbolを通常定義にする。旧esh backendを
必要とするbuildはStep 7切替までの間、新配置と汎用APIへ追従させる。

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
src/core/esh/target/windows/sched_adapt.mk
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

### Step 6実施結果（2026-10-13）

指定された次の6ファイルを`src/core/sys/dev/windows/`へ移動した。

- `cortn_context.c`
- `cortn_context.h`
- `cortn_switch.S`
- `sched_adapt.mk`
- `sched_cortn.c`
- `sched_thread.c`

移動後のscheduler backendは`struct proc`、`PROC_*`、`WAIT_*`、
`sched_adapt_*`、`SYS_EINVAL`を直接使用する。Step 5で設けた
`SYS_SCHED`分岐とbackend内の`esh_`/`ESH_` compatibility定義は削除した。
`src/core/sys`内の`esh_`、`ESH_`検索結果は0件である。

build変数は`SCHED_BACKEND`、`SCHED_CSOURCES`、`SCHED_ASOURCES`、
`SCHED_CPPFLAGS`へ変更した。次のbuild fileを新pathへ追従させた。

- `src/core/sys/test/sched/Makefile`
- `src/core/esh/test/cortn/Makefile`
- `src/core/esh/test/sched/Makefile`
- `src/core/esh/test/pipe/Makefile`
- `src/core/esh/test/pipeline_exec/Makefile`
- `src/core/esh/test/exe/Makefile`
- `src/target/windows/Makefile`
- `src/target/windows/TDM-GCC/Makefile`
- scheduler、pipe、pipeline testの`run_test.sh`

Step 7まで旧esh schedulerを動かすため、Windows依存の一時互換実装を
`src/core/esh/target/windows/`へ分離した。

- `sched_compat.mk`
- `sched_cortn_compat.c`
- `sched_thread_compat.c`

汎用backend sourceは一時互換symbolを含まない。旧esh buildだけが
`SCHED_COMPAT_CSOURCES`と`SCHED_COMPAT_CPPFLAGS`を使い、context switchと
assemblyは新配置の実装を共有する。

`cortn.c`と`cortn.h`はtarget非依存のcoroutine本体であり、Step 6の指定移動
対象ではないため`src/core/esh/`に残した。`cortn_context.h`は新しいsys Windows
include pathから参照する。

次の試験とbuildが成功した。

- coroutine単体 O0/O2、`-Werror`
- 汎用scheduler coroutine/thread O0/O2、`-Werror`
- 既存scheduler coroutine/thread O0/O2、`-Werror`
- 既存pipe coroutine/thread O0/O2、`-Werror`
- pipeline executor coroutine/thread O0
- Windows target coroutine/thread build

`cortn_context.c`の`CORTN_OFFSET_CHECK`はO0/O2 compileで成功し、assemblyの
offset定義は変更していない。pipelineの既存warningと意図されたparser error
診断は残るが、試験結果は`all tests passed`だった。

#### Step 7への引継事項

- esh callerをsys schedulerへ切り替えた後、`sched_compat.mk`、
  `sched_cortn_compat.c`、`sched_thread_compat.c`を削除する
- 旧esh buildの`SCHED_COMPAT_*`参照を`SCHED_CSOURCES`、
  `SCHED_CPPFLAGS`へ切り替える
- `src/core/esh/target/windows/`にはesh adapterだけを残す
- `cortn.c`と`cortn.h`は今回移動していない
- source treeのMakefile、mk、test scriptに旧6ファイルのpath参照はない
- source treeのMakefile、mk、test scriptに`ESH_SCHED_*`変数はない

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

### Step 7実施結果（2026-10-13）

状態: `done`

開始HEAD: `e8516c4`

完了HEAD: `7c6a1f9`

executor、Windows stdio、Windows targetを汎用sys file/pipe/schedulerへ
切り替えた。`struct exe_proc.p_exec`は`struct proc`を直接保持し、fd tableは
`p_ofile[]`へ統合した。pipeline作成は`pipe_for_proc`、`getf`、
`fd_install_at`、`fd_close`、`close_all`を使用する。組立てとcleanupはkernel
lock内、backend release/joinはfd close後かつlock外の順序を維持した。

`src/core/sys/sys/sys2.c`を追加し、V7 `sys2.c`の`rdwr`と`close`に対応する
`sys_read`、`sys_write`、`sys_close`を実装した。hostのtrap、大域`u`、user空間
copyは使わず、`current_proc()`から明示的な`struct proc`を得る。file flagを
検査後、pipe endpointを`readp`または`writep`へ渡す。

Windows stdio adapterは`current_proc`、`getf`、`sys_read`、`sys_write`を使う。
Windows `eshinit`は最初のpipeline実行前にbuffer、file、inode tableを
初期化するため`file_system_init`を呼ぶ。

次の旧esh専用実装を削除した。

- `include/esh_file.h`、`include/esh_pipe.h`
- `include/esh_sched.h`、`include/esh_sched_backend.h`
- `file.c`、`pipe.c`、`sched.c`
- `target/windows/sched_compat.mk`
- `target/windows/sched_cortn_compat.c`
- `target/windows/sched_thread_compat.c`

Windows target、executor test、pipe test、scheduler test、pipeline testは
`SCHED_CSOURCES`、`SCHED_CPPFLAGS`とsys sourceを直接使用する。旧
`SCHED_COMPAT_*`参照は残っていない。esh scheduler testはsys schedulerの
試験sourceを両backendで共有する。tracked cscope listは存在しないため更新対象は
なく、既存の未追跡`cscope.files`と`cscope.out`は変更していない。

旧ring buffer固有試験はV7式offset試験へ置き換えた。partial read後も
`i_size == PIPSIZ`なら先頭空きへwrapせず、未読dataをすべて消費して
`f_offset`と`i_size`が0へ戻るまでwriteは待機する。新しいpipe結合試験で
public API、部分転送、EOF、EPIPE、reader/writer close wakeup、resource回収を
coroutine/thread双方で確認した。

次の試験とbuildが成功した。

- file/inode単体 O0/O2、`-Werror`
- sys pipe単体 O0/O2、`-Werror`
- pipe coroutine/thread O0/O2、`-Werror`
- scheduler coroutine/thread O0/O2、`-Werror`
- pipeline executor coroutine/thread O0/O2
- executor test 10件
- parser `run_test1.sh`、`run_test2.sh`
- Windows target coroutine/thread build

pipelineには従来のhelp、lex、par、test由来warningと意図されたparser error
診断が残る。移行による新規warningはない。Windows対話試験と関連文書全体の
現行名への更新は予定どおりStep 8へ残す。

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

### Step 8実施結果（2026-10-13）

状態: `done`

開始HEAD: `7c6a1f9`

完了HEAD: `uncommitted`

#### 文書更新

次の文書を現行の型名、path、V7型offset方式、buffer cache構成へ更新した。

- `docs/esh_pipe_api.md`
- `docs/esh_pipe_core.md`
- `docs/esh_pipe_sched.md`
- `docs/esh_pipe_thread.md`
- `docs/esh_pipe_stdio.md`
- `docs/esh_pipe_regression.md`
- `docs/cortn_windows.md`
- 本引継ぎ書のStep状態と完了記録

#### V7から採用した要素

- processごとの`p_ofile[]`、global file table、共有inode
- `FREAD`、`FWRITE`、`FPIPE`とendpointごとのfile offset
- `falloc`、`getf`、`closef`、`readp`、`writep`、`readi`、`writei`
- inode `ILOCK`、`IWANT`、`plock`、`prele`
- pipe dataの単調offsetと全data読取後のoffset/size reset
- wait channel、sleep/wakeup、再開後の条件再検査
- close後のEOF/EPIPE再判定とresource参照数管理

#### 意図的なV7との差分

- trap、global user area `u`、MMU user copyを使わない
- disk inode、directory、mount、block mapping、physical storageを持たない
- pipeはmemory-only buffer cacheの1 blockだけを使用する
- fixed table/poolを使用し、portable sys層でdynamic allocationしない
- `SIGPIPE`を送らず、reader不在を`-SYS_EPIPE`で返す
- schedulerをportable本体とcoroutine/thread backendへ分離する
- priority、signal、timeout、swapping、preemptionを実装しない

#### 静的確認

- `src/core/sys`の`esh_`、`ESH_`: 0件
- tracked sourceの旧header名、旧source/backend path参照: 0件
- source/header変更: 0件、新規`typedef`: 0件
- `git diff --check`: 成功
- 変更は文書8件だけで、試験生成物と既存未追跡fileはdiff対象外

#### 回帰試験

次をすべて成功した。

- sys header C/C++、include順序反転 O0/O2、`-Werror`
- sys table O0/O2、`-Werror`
- buffer cache O0/O2、`-Werror`
- block device O0/O2、`-Werror`
- file/inode O0/O2、`-Werror`
- sys pipe O0/O2、`-Werror`
- coroutine O0/O2、`-Werror`
- pipe coroutine/thread O0/O2、`-Werror`
- scheduler coroutine/thread O0/O2、`-Werror`
- pipeline executor coroutine/thread O0/O2
- parser `run_test1.sh`、`run_test2.sh`
- executor test 10件
- cquote build・実行
- Windows target coroutine/thread build

Windows対話確認は両backendで次を成功した。

- `sam1`
- `sam2 1 10 100`
- `sam3 -a -b string`
- `echo hello|cat`
- `echo four|cat|cat|cat`
- `k=3`後の`echo "$k"|cat`

#### Warningと残課題

sys、pipe、scheduler、coroutineのstrict suiteにwarningはない。pipelineの広い
buildには移行前から次のwarningが残る。

- `help.c`: 未使用parameter
- `lex.c`: 未使用static関数、format/string切詰め
- `par.c`: `char`型array subscript
- `test.c`: 未使用static関数

移行による新規warningはない。lexer/parserのerror-path試験が意図された診断を
出すが、全suiteは成功した。Windows-first移行は完了した。STM32、Cortex-M、
RTOS移植には着手していない。

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
