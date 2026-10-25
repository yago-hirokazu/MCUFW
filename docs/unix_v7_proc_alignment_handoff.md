# UNIX V7 proc 構造体整合 引き継ぎ書

作成日: 2026-10-24
更新日: 2026-10-26
対象: `C:\usr\src\MCUFW`
計画作成時 HEAD: `82b7617` (`Rename proc members to p_xctx and p_ret`)

## 現況と依頼

ユーザーの依頼は、現在の MCUFW の `struct proc` の変数名と役割を、
可能な限り UNIX V7 に合わせ、無理なく組み込むことである。
本書は別タスクから実装を継続できるよう、設計方針、順序、検証条件を整理する。
Step 0 の設計確定と Step 1 のメンバ改名・ヘッダ分離は完了した。
Step 2 の状態統合と関連試験は完了した。
Step 3 の直接実行・管理用 proc・解放経路の監査と関連試験も完了した。
Step 4 の状態遷移・異常系の集中検証も完了した。
Step 5 の全回帰と最終文書確認も完了し、本計画の Step 0～5 は全て完了した。
状態・寿命・異常の確定契約は
[Step 0 設計決定](unix_v7_proc_alignment_step0.md)を参照する。
以下の「現在の実装」はメンバ改名後のソースを示す。
各 Step の実装・監査・検証結果は末尾の記録を参照する。
実装を依頼された後続タスクは、各 Step の確認・実装・検証・記録を進める。
通常の設計具体化を、追加のユーザー承認待ちとして扱う必要はない。

既に実施済みの改名は次のとおり。取り消さず、そのまま維持する。

| 旧名 | 現在の名前 | ユーザー指定の意味 |
|---|---|---|
| `p_backend` | `p_xctx` | eXecution ConTeXt |
| `p_status` | `p_ret` | コマンド関数の戻り値 |

この改名は `82b7617` でコミット済み。ソース・試験・文書の参照を更新した。
改名時には sys header/pipe/sched、esh pipe/pipeline_exec の既存試験が通過した。
両 backend、O0/O2、scheduler 小容量構成を含み、make clean 済みである。
これは改名時の検証記録であり、以下の状態統合を検証した記録ではない。
`user` 導入 Step 0～7 も完了済みであり、再実装しない。

## 最初に読むもの

- [AGENTS.md](../AGENTS.md)
- [Linux kernel coding style 要約](coding_style_summary_linux_kernel.md)
- [Scheduler](pipe_sched.md)
- [Windows coroutine](cortn_windows.md)
- [Windows thread](pipe_thread.md)
- [Executor](pipe_executor.md)
- [回帰試験](pipe_regression.md)
- [User 導入引き継ぎ書](unix_v7_user_context_handoff.md)
- [User 導入の設計決定](unix_v7_user_context_step0.md)
- [Proc 整合 Step 0 設計決定](unix_v7_proc_alignment_step0.md)

V7 調査には `.agents/skills/ancient_unix/SKILL.md` を使う。
原典の基準ディレクトリは次のとおり。

```text
.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/
```

| 原典 | 確認する内容 |
|---|---|
| `h/proc.h` | メンバ、状態値、flag、終了時の xproc |
| `h/user.h` | u_procp、user と proc の相互参照 |
| `sys/slp.c` | sleep/wakeup、p_wchan、p_link、SRUN、swtch |
| `sys/sys1.c` | exit/wait、SZOMB、xp_xstat、資源回収 |

V7 の `p_link` は header の説明だけで判断しない。
`slp.c` では実行キューと睡眠キューの両方で使っている。
V7 の `p_addr` はイメージの配置を表し、常駐時と swap 時で解釈が異なる。
MCUFW の user 直接ポインタとの対応は、既存 user 導入方針を維持する。

## 着手時の作業ツリー確認

必ず最新の `git status --short`、HEAD、関連 diff を確認する。
本書作成時、追跡済みファイルに未コミット変更はなかった。
次の既存未追跡項目は今回の作業成果ではない。

```text
src/cscope.files
src/cscope.out
src/target/NUCLEO-U575ZI-Q/
src/target/windows/libMPSSE.dll
src/target/windows/libMPSSE_i2c.dll
```

既存作業を削除・上書き・一括追加しない。最新状態が本書と異なる場合は、
ソースと差分から完了範囲を判定し、済んだ Step を繰り返さない。
他タスクで並行変更される可能性があるため、作業終了時も差分を確認する。

## 現在の実装

`src/core/sys/h/proc.h` が次の定義を持つ。
`sched.h` は `proc.h` を include するため、既存利用者も同じ型を参照できる。

```c
struct proc {
	enum proc_state p_stat;
	struct user *p_addr;
	struct wait_chan *p_wchan;
	struct proc *p_link;
	void *p_xctx;
	int p_ret;
};
```

- 状態は SIDL=4、SRUN=3、SSLEEP=1、SZOMB=5、PROC_ERROR=7
- 待機理由 WAIT_LOCK/READ/WRITE は引数と backend の内部表現で維持する
- 回復可能な待機失敗は SRUN、継続不能・取消は PROC_ERROR とする
- `proc_init()` は caller 所有 user と proc を初期化し、相互参照を設定する
- `p_xctx` は coroutine では `struct cortn *`、thread では slot への参照
- 実行機構は固定容量 pool を使い、`PIPELINE_MAX` は現在 4
- portable scheduler は channel ごとの待機リストと proc 状態を管理する
- wait channel は待機対象側に埋め込み、head/tail で FIFO 登録する
- thread backend も executor が順番に実行権を渡す協調方式である
- current proc は coroutine では backend global、thread では TLS に保持する
- 単一コマンドは backend を作らず、current を設定して直接 main を呼ぶ
- fd 接続用の一時 owner proc も backend を持たない
- backend のない proc は、pipe 待機が必要なら EAGAIN または転送済み量を返す
- release は待機登録を解除し、p_xctx を NULL にしてから lock 外で解放する
- thread は停止通知後に unwind/join し、coroutine は停止中の stack を破棄する

## 目標とする構成

| 移行開始時 | 移行先 | 意味 |
|---|---|---|
| `p_state` | `p_stat` | プロセス状態 |
| `p_wait_chan` | `p_wchan` | 待機対象 |
| `p_wait_next` | `p_link` | scheduler のキュー連結 |
| `p_addr` | 維持 | 専用 user を直接参照 |
| `p_xctx` | 維持 | MCUFW 固有の実行コンテキスト |
| `p_ret` | 維持 | MCUFW 固有のコマンド関数戻り値 |

目標形は次のとおり。型・メンバ順のバイナリ互換性は V7 に合わせない。

```c
struct proc {
	enum proc_state p_stat;
	struct user *p_addr;
	struct wait_chan *p_wchan;
	struct proc *p_link;
	void *p_xctx;
	int p_ret;
};
```

`h/proc.h` を新設し、構造体と既存状態定義を移した。
`sched.h` は `proc.h` を include し、scheduler API、wait_reason、wait_chan を持つ。
`proc.h` は user/wait_chan を前方宣言し、循環 include を避ける。
既存利用者の include を一斉に変更する必要はない。

### 状態の対応

| 移行開始時 | 現在 | 意味 |
|---|---|---|
| PROC_NEW | SIDL | 初期化済み、実行準備中 |
| PROC_RUNNABLE / PROC_RUNNING | SRUN | 実行可能または実行中 |
| PROC_WAIT_LOCK/READ/WRITE | SSLEEP | イベント待機中 |
| PROC_FINISHED | SZOMB | コマンド処理完了、結果回収待ち |
| PROC_ERROR | MCUFW 拡張として維持 | backend などの実行異常 |

V7 の値 SSLEEP=1、SRUN=3、SIDL=4、SZOMB=5 を踏襲する。
PROC_ERROR=7 と確定した。V7 の SWAIT=2 や SSTOP=6 を異常状態へ転用しない。
回復可能な操作失敗は SRUN へ戻し、PROC_ERROR は継続不能・取消の状態とする。
通常 return や古い backend 報告で PROC_ERROR を上書きしない。
状態 0 を有効な初期化済み proc とみなさず、proc_init を必要とする。
proc table や空き slot 管理を新設する計画ではない。

SRUN だけで実行中と実行可能を区別しない。
実行中の特定には current proc と backend の実行権管理を使う。
thread の current は TLS なので、別 thread からその TLS を直接参照しない。
WAIT_LOCK/READ/WRITE は既存引数と backend 内部の待機理由として維持する。
これを p_pri に格納したり、理由を残すためだけに proc を拡大したりしない。

### 維持する契約

1. kernel lock 内で条件検査、待機登録、状態公開を行う
2. yield は kernel lock 保持で入り、復帰時も保持する
3. wakeup は対象 channel の待機者全員を外し、条件は復帰後に再検査する
4. 起床済みの SRUN を backend が返した古い SSLEEP で上書きしない
5. 実行用 backend を持たない SRUN を sched_resume しない
6. 同一 proc を待機キューへ二重登録しない
7. p_link を将来別キューへ共用する場合も、同時に二つのキューへ入れない
8. p_ret は main の int 戻り値を保持し、負の SYS_E* もそのまま扱う
9. scheduler 異常を p_ret や u_error へ無条件に混在させない
10. p_addr->u_procp == proc を維持し、sleep/wakeup で user を初期化しない
11. p_xctx == NULL だけで worker の停止完了を判断しない
12. proc/user の再利用は sched_release の完了後に行う

SZOMB は MCUFW の論理的な処理完了を表す。V7 の exit と同じ資源解放、
親子関係、wait system call が存在するという意味ではない。
SZOMB と native thread の実際の終了・join 完了は区別する。
強制 release した coroutine は main に戻らないため、通常完了とは区別する。
release 後の状態から再利用可否を推測せず、release 完了を条件にする。

### 今回導入しない機能

MMU、trap、swap、物理 storage、動的な proc table、PID/親子関係、signal、
alarm、CPU 使用量、優先度、preemption、独立した thread 並列実行は導入しない。
p_flag/p_pri/p_pid/p_ppid/p_size/p_textp などは、対応機能なしに追加しない。
V7 の SLOCK は swap 禁止であり、kernel lock や inode lock へ転用しない。
V7 の xproc overlay や終了値の bit encoding も導入しない。
現在の待機リストを V7 の睡眠 hash 表へ変更する必要はない。
file/inode/buffer-cache の基本構造、I/O のエラー契約、shell 文法は維持する。

## 実装 Step

| Step | 内容 | 状態 |
|---|---|---|
| 0 | 状態・寿命・異常の契約を確定 | complete |
| 1 | メンバ改名と proc.h 分離 | complete |
| 2 | scheduler/backend/executor の状態統合 | complete |
| 3 | 直接実行・管理用 proc・解放処理の整合 | complete |
| 4 | 状態遷移と異常系の集中検証 | complete |
| 5 | 全回帰、文書更新、完了確認 | complete |

### Step 0: 状態・寿命・異常の契約

完了。契約の詳細と出典は[Step 0 設計決定](unix_v7_proc_alignment_step0.md)に記載した。
本書、現行コード、V7 原典を照合し、次を確定した。

- 状態値と、各遷移を更新する関数・lock 条件
- SIDL の管理専用 proc と、SRUN の backend なし直接実行の扱い
- 通常終了、処理中の回復可能な失敗、backend 再開不能、強制解放の違い
- release 中と完了後の p_stat の扱い、状態を上書きしてよい条件
- p_ret の書込担当と、結果回収時点
- backend resume の返却状態と共有 p_stat が競合した場合の優先規則

設計確定時の test_yield_failure_cleanup は yield 失敗を呼出元が受け取り、
entry が戻った後に PROC_FINISHED になることを確認していた。
全ての一時失敗を永続的な PROC_ERROR にする変更は行わない。
逆に再開不能な異常を通常終了で隠さないよう、異常経路ごとに確認する。

完了条件: 通常・待機・異常・解放について、状態と資源の寿命を説明できる。
この Step で決定した内容を以後の実装・試験の基準とする。
回復可能な yield 失敗では、従来の一時的な PROC_ERROR を SRUN 復帰へ変更する。
fatal/cancel の PROC_ERROR は保持し、thread の通常 return で消さない。
これらは Step 2 の実装対象であり、Step 0 ではコードを変更していない。

### Step 1: メンバ改名とヘッダ整理

完了。実装・試験結果は末尾の Step 1 完了記録を参照する。
p_state→p_stat、p_wait_chan→p_wchan、p_wait_next→p_link を一括更新する。
proc.h に proc と既存状態定義を移し、この段階では PROC_* の分類を維持する。
本体、stub、試験、現行文書の参照を更新する。

主な対象:

```text
src/core/sys/h/proc.h                 新設
src/core/sys/h/sched.h
src/core/sys/sys/sched.c
src/core/sys/dev/windows/sched_cortn.c
src/core/sys/dev/windows/sched_thread.c
src/core/sys/test/stub/sched_stub.c
src/core/sys/test/header/
src/core/sys/test/sched/
src/core/esh/exe.c
src/core/esh/test/pipe/
src/core/esh/test/pipeline_exec/
```

リストは限定列挙ではない。rg で利用箇所を確認する。
proc.h を header 試験の単独 include、順序入替え、C/C++ の対象へ追加する。
完了条件: 関連試験が成功し、旧メンバ参照が本体・試験に残っていない。

### Step 2: 状態統合

完了。実装・検証結果と前倒しした整合は末尾の Step 2 完了記録を参照する。
SIDL/SRUN/SSLEEP/SZOMB と MCUFW 異常状態へ移行する。
portable scheduler、両 backend、executor、関連試験を一まとまりで変更する。
片方の backend だけ旧状態を返す中間状態で Step 完了としない。

```text
SIDL -- prepare/bind --> SRUN -- command completion --> SZOMB
                         |
                         +-- sleep --> SSLEEP -- wakeup --> SRUN
```

処理内容:

- sched_sleep の待機理由検査を維持し、公開状態を SSLEEP にまとめる
- sched_wakeup は p_wchan/p_link を外し、SRUN を公開する
- sched_resume は SRUN と有効な p_xctx を検査する
- coroutine の内部 CORTN_* は維持し、公開状態への変換を更新する
- thread の実行 token、condition variable の predicate、TLS を維持する
- executor は SRUN を実行候補、SZOMB を完了として数える
- 起床後の SRUN を古い backend 報告で上書きしない
- PROC_RUNNING の削除で失われる検査がないか、全参照箇所を確認する
- 回復可能な待機失敗は SRUN に戻し、fatal/cancel は通常完了で上書きしない
- 未完了 backend の release は PROC_ERROR を切離しと同じ lock 内で公開する

完了条件: 両 backend の scheduler/pipe/pipeline executor 試験が通る。
待機理由の詳細は引数・backend・試験用記録で確認し、p_stat から推測しない。

### Step 3: 実行経路と解放の整合

完了。監査結果・追加試験は末尾の Step 3 完了記録を参照する。
exe_execute_commands の単一コマンド経路を確認し、直接実行中は SRUN、
処理完了後は SZOMB とし、main の戻り値を p_ret に保存する。
直接実行は p_xctx なしを維持し、元の current の復元と fd close を維持する。
fd 接続専用 owner は初期化済みの管理用 proc とし、実行対象へ入れない。

release では p_wchan/p_link の解除、p_xctx の切離し、lock 外での backend 解放、
thread の停止・unwind・join の順序を維持する。
coroutine の stack 破棄と thread の unwind の違いを通常終了と混同しない。
通常終了時の p_ret と、異常・キャンセル時の結果有効性を Step 0 の決定に合わせる。
backend がない pipe I/O の EAGAIN/部分転送契約を維持する。

Step 2 の検証を成立させるため必要な整合は Step 2 内で行い、ここで全経路を監査する。
完了条件: 単一コマンド、管理用 proc、待機中 release、起床後 release、再利用が通る。

### Step 4: 集中検証

完了。契約と試験の対応、補強内容、検証結果は末尾の Step 4 完了記録を参照する。
既存試験を新状態へ更新し、統合で見落としやすい条件のみ追加・補強する。
実装をそのまま写した試験や、既存試験と同じ検証を大量に増やさない。

| 対象 | 必須の確認 |
|---|---|
| 初期化 | SIDL、p_addr/u_procp、bind 前後、二重 prepare の拒否 |
| 待機 | 各理由で SSLEEP、p_wchan 設定、キュー連結、lock 保持 |
| 起床 | 複数待機者、link 解除、早い通知、条件未成立時の再待機 |
| pipe | 空/full、部分転送、最終 close 後の EOF/EPIPE |
| 異常 | yield 失敗、prepare 枯渇、再開不能、待機登録の後始末 |
| 終了 | p_ret、最終段結果、正常完了と失敗・キャンセルの区別 |
| 寿命 | 待機中/起床後 release、unwind/join、proc/user の再利用 |
| 直接実行 | SRUN/SZOMB、p_ret、current 復元、backend なし I/O |

両 backend、O0/O2、scheduler の既定/小容量構成を維持する。
完了条件: focused suite が全構成で成功し、生成物を clean したことを確認する。

### Step 5: 全回帰と文書

完了。実行範囲・結果・既存問題は末尾の Step 5 完了記録を参照する。
[回帰試験](pipe_regression.md)の現行入口に沿って全回帰する。
sys header/table/bdev/ramdisk/bio/file/pipe/sched、esh cortn/pipe/sched、
pipeline/pipeline_exec、parser、exe、cquote、Windows 本体を対象にする。
Windows 本体は両 backend で AGENTS.md の sam1/sam2/sam3 と、
回帰ガイドの pipe・変数展開例を確認する。

pipe_sched.md、pipe_executor.md、pipe_thread.md、cortn_windows.md、
pipe_regression.md、docs/README.md と本書を実装に合わせて更新する。
過去の記録を現在の実装と混同しないよう、必要に応じ現況注記を追加する。

完了条件:

- 旧メンバと旧 PROC_* 分類が実装・試験へ残っていない
- 維持する MCUFW 異常状態は検索時の例外として明記されている
- 各試験が成功し、既存 warning と新規不具合を区別して記録している
- make clean 後、今回生成した d/o/exe と一時スクリプトが残っていない
- git diff --check が成功し、既存未追跡ファイルを巻き込んでいない
- 各 Step の結果、残る制約、次の担当者が行うことを記録している

## テスト実行と clean の注意

Windows-first 方針に従い、MSYS2 MINGW64 shell で実行する。
PowerShell から mingw64 gcc を直接起動しない。
既存 run_test.sh の timeout・判定条件を緩めない。

集中試験の入口例は、repo root から次のとおり。

```sh
bash src/core/sys/test/header/run_test.sh
bash src/core/sys/test/pipe/run_test.sh
bash src/core/sys/test/sched/run_test.sh
bash src/core/esh/test/pipe/run_test.sh
bash src/core/esh/test/pipeline_exec/run_test.sh
```

各 script が生成する OUTDIR/TARGET を読み、同じ値で make clean する。
script が終了しても、全構成の生成物が自動で消えるとは限らない。
失敗時も clean されるよう、実行用 script には EXIT trap 等を用意する。
make clean の対象パスが repo 内の意図した生成物であることを確認する。

例: pipeline executor は cortn/thread × O0/O2 それぞれについて、次を実行する。

```sh
make -C src/core/esh/test/pipeline_exec clean \
    OUTDIR=_output_cortn_O0 TARGET=test_pipeline_cortn_O0.exe
```

sys scheduler には `_small` 構成もある。header 試験は executable を作らないが、
OUTDIR 内の object を clean する。Makefile/script 変更後は現行定義を優先する。

## タスク間の引き継ぎ方法

各 Step の開始時に本書と最新ソースを読み、終了時に次を記録する。
Step 0～5 は順に進める。依存するファイルへの同時実装を前提にしない。
ビルド可能で両 backend が検証済みの区切りで引き継ぐ。
コミットはユーザー依頼または当該タスクの指示に従う。
未コミットで引き継ぐ場合は、その旨と対象ファイルを明示する。

```text
Step:
状態: pending / in progress / complete
開始 HEAD:
終了 HEAD または未コミット対象:
確定した設計:
変更内容:
実行した試験と構成:
試験結果・既存 warning:
make clean・生成物確認:
残件・次の着手点:
```

計画作成時の見通しは、Step 1 が小規模、Step 2～3 が中規模である。
主な難所は状態統合時の通知順序と、終了・異常・解放の区別であり、
file/inode/buffer-cache の基本設計変更は必要ない見込みである。

## 作業記録

2026-10-24: 本書を作成。Step 0～5 は全て pending。
計画用のソース確認のみ実施。本書作成に伴う本体変更・試験実行はなし。

### Step 0 完了記録

- 実施日: 2026-10-24
- 開始 HEAD: `81d3fab`
- 状態: complete。Step 1～5 は pending
- 確定契約: [Step 0 設計決定](unix_v7_proc_alignment_step0.md)
- 状態値: SSLEEP=1、SRUN=3、SIDL=4、SZOMB=5、PROC_ERROR=7
- 回復可能な操作失敗、backend fatal、通常 return、取消を区別した
- 状態更新担当・lock 条件、backend 報告の優先規則、p_ret の有効性を確定した
- release 中の PROC_ERROR 保持と、unwind/join 完了後の再利用条件を確定した
- V7 原典、現行 scheduler/backend/executor、関連試験をソースで照合した
- 文書のみ変更。実行試験なし、生成物なし、make clean 対象なし
- 次の着手点: Step 1 のメンバ改名と proc.h 分離。状態分類はまだ変更しない
- 本記録時点は未コミット。対象は本書、Step 0 設計決定、docs/README.md

### Step 1 完了記録

- 実施日: 2026-10-25
- 開始 HEAD: `6f79b0e`。Step 0 は同コミットで保存済み
- 状態: complete。Step 2～5 は pending
- p_state/p_wait_chan/p_wait_next を p_stat/p_wchan/p_link へ改名した
- h/proc.h に struct proc と既存 enum proc_state を移した
- sched.h から proc.h を include し、既存利用者の include 互換を維持した
- user/wait_chan は proc.h 内で前方宣言し、循環 include を導入していない
- 本体、stub、試験、現行文書の参照と、header 試験の include 対象を更新した
- enum proc_state の定義は移行前と同一。状態分類・値・異常契約は未変更
- header 分離と header 試験以外の C ソース差分は、メンバ名置換のみと照合した

MSYS2 MINGW64 shell で既存 run_test.sh を実行した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | 単独 include・順序入替え、C/C++、O0/O2、proc.h 追加 | passed |
| sys file/pipe | 各 O0/O2、stub 経由の初期化・待機を含む | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| esh pipe/scheduler | 各 cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、既存 timeout 維持 | passed |

strict suite は warning なし。pipeline executor の広い shell build には、
未変更の help.c、lex.c、par.c、test.c の既存 warning が残る。
文法異常系の意図された診断を含めて全 suite は成功した。
Step 5 の全回帰や Windows 本体の対話試験は今回の実施範囲ではない。

各構成の make clean は正常終了した。生成した d/o/exe の残存なし、
旧メンバ参照のソース残存なし、git diff --check 正常を確認した。
一時実行 script と log は削除した。既存未追跡ファイルは変更していない。

次の着手点は Step 2。Step 0 設計決定に従い、状態統合と回復可能な失敗の
SRUN 復帰、fatal/cancel の保持を両 backend・executor と一緒に実装する。
現在はまだ従来の PROC_* 状態分類と一時的な PROC_ERROR 動作である。
本記録時点は未コミット。対象は proc.h、改名した本体/stub/試験、header 試験、
本書、pipe_sched.md、proc Step 0 文書、user 引き継ぎ書である。

### Step 2 完了記録

- 実施日: 2026-10-25
- 開始 HEAD: `3fd0091`。Step 1 は同コミットで保存済み
- 状態: complete。Step 3～5 は pending
- SSLEEP=1、SRUN=3、SIDL=4、SZOMB=5、PROC_ERROR=7 へ状態を統合した
- scheduler、両 backend、executor、stub と関連試験を同時に更新した
- 待機理由 WAIT_LOCK/READ/WRITE と内部 CORTN_* は維持した
- proc_bind_backend は kernel lock 内で context と SRUN を公開する
- resume_report で共有状態と backend 報告を照合し、早い起床と fatal を保持する
- 回復可能な yield 失敗は待機解除後 SRUN、継続不能・取消は PROC_ERROR とした
- 不正な状態報告、未解除の待機を伴う完了報告は、登録を解除して異常扱いとする
- coroutine の完了後 guard 異常も正常完了より優先する
- 未完了 backend の release は PROC_ERROR と p_xctx 切離しを同時に公開する
- thread の entry return は fatal/cancel を SZOMB で上書きしない
- native run wait 失敗では fatal を公開し、実行tokenを保持して unwind する
- controller の報告待ち失敗では停止を通知し、lock 外で worker 停止を待つ

Step 2 の状態契約を成立させるため、Step 3 対象の単一コマンド経路も整合させた。
backend なしで SIDL→SRUN→SZOMB とし、main 戻り値を p_ret に保存する。
直接実行が fatal となった場合は PROC_ERROR と診断用 p_ret を保持し、
executor の結果は -SYS_EINVAL として正常結果に採用しない。
管理用 owner は SIDL のまま、元の current の復元と fd close 順序を維持する。
Step 3 ではこの実装を再作成せず、全経路の監査と必要な補強を行う。

#### Step 2 検証結果

MSYS2 MINGW64 shell で既存 run_test.sh を実行した。
native wait 失敗の修正・試験追加後に、scheduler、esh pipe、pipeline executor を再実行した。
直接実行の fatal 結果を拒否する修正後も、pipeline executor の全構成を再実行した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | C/C++、単独・順序入替え、O0/O2、V7 状態値 | passed |
| sys file/pipe | 各 O0/O2、stub 経由の初期化・I/O | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| esh pipe/scheduler | 各 cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、既存 timeout 維持 | passed |

既存の EOF/EPIPE、部分転送、複数待機者、資源不足、user 保持・再利用に加え、
以下を試験した。

- 3 種の待機理由で SSLEEP を公開し、早い wakeup 後の古い報告でも SRUN を保持
- 回復可能な yield 失敗は SRUN に復帰し、その後の通常 return は SZOMB
- 未定義・SIDL・不整合な完了報告を拒否し、待機情報を解除
- fatal 設定後の通常 entry return でも PROC_ERROR を保持
- coroutine の guard を entry 内で破損させ、resume 後の検査で fatal を公開
- native worker の run wait 失敗を注入し、entry return 後も fatal を保持
- 準備済み未開始、待機中、起床後の release は取消状態を保持
- SZOMB の通常 release、backend なし release、繰返し release の状態を保持
- 単一コマンドの実行中 SRUN、終了後 SZOMB と p_ret、current 復元を確認
- 直接実行の fatal では、main 戻り値 37 を保持しても正常結果には採用しない

報告境界の早い wakeup と不正報告には SCHED_TEST 限定の one-shot hook を使う。
native run wait 失敗も SCHED_TEST/SCHED_THREAD 限定で注入した。
production API・proc メンバの追加はない。controller 側 OS wait 失敗の実故障は
誘発しておらず、その停止・join 経路はソース確認である。

strict suite は warning なし。pipeline executor の広い shell build には、
未変更の help.c、lex.c、par.c、test.c の既存 warning が残る。
文法異常系の意図された診断を含めて全 suite は成功した。
Step 4 の追加監査・集中検証、Step 5 の全回帰・Windows 対話試験は未実施である。

各構成の make clean は正常終了した。生成した d/o/exe の残存なし、
旧 PROC_NEW/RUNNABLE/RUNNING/WAIT_*/FINISHED のソース残存なし、
git diff --check 正常を確認した。一時 script/log は削除した。
既存未追跡ファイルは変更していない。

次の着手点: Step 3。直接実行、管理用 proc、各 release 経路、p_ret 有効性を監査する。
本記録時点は未コミット。対象は scheduler/両 backend/executor と関連 header・
stub・試験、本書、pipe_sched/pipe_thread/cortn_windows/pipe_executor の文書である。

### Step 3 完了記録

- 実施日: 2026-10-25
- 開始 HEAD: `2afbec4`。Step 2 は同コミットで保存済み
- 状態: complete。Step 4～5 は pending
- Step 0 の確定契約と Step 2 の実装を、直接実行から解放・再利用まで照合した
- 本体の動作修正が必要な不整合は見つからず、本体ソース・API・状態契約は未変更
- 試験を補強し、既存仕様の所有権説明と本書の進行状況を更新した

#### Step 3 経路別の監査結果

| 経路 | 主なソース・関数 | 確認結果 |
|---|---|---|
| 単一コマンド | esh/exe.c: exe_execute_commands | backend なし SRUN、main return を p_ret へ保存、fd close 後 SZOMB、元の current 復元 |
| 直接実行の fatal | 同上 | PROC_ERROR を保持し、p_ret は診断用、executor は -SYS_EINVAL を採用 |
| 管理用 owner | exe_connect_proc、pipe_for_proc、fd_install_at | SIDL を維持。専用 user で fd を構築・受渡しし、実行候補へ入れない |
| pipe 構築途中の失敗 | exe_connect_proc、exe_cleanup_pipeline | 一時 owner の fd と部分接続を回収。未準備 proc は SIDL、p_ret は未実行値 |
| backend 準備途中の失敗 | sched_prepare、exe_cleanup_pipeline | 準備済みだけを取消・release、未準備は SIDL。全接続 fd を先に close |
| 通常 pipeline 終了 | exe_proc_entry、sched_resume | main 戻り値を保持し fd close 後に完了。全段 SZOMB 時のみ最終段結果を回収 |
| 開始前・待機中・起床後の取消 | sched_release、両 sched backend | PROC_ERROR と context 切離しを lock 内で公開し、待機 link を解除 |
| thread の unwind/join | sched_thread.c | 保存済み slot と元の user を使い、lock 外で停止・join。entry return でも取消を消さない |
| coroutine の取消 | sched_cortn.c | 停止中 stack を破棄。待機後の処理と main return は実行しない |
| fd 最終 close | fio.c: close_all/closef、iget.c: iput | 参照回収と peer 起床。user の error/result を保持し、終了・取消の公開は scheduler が担当 |
| backend なしの I/O | pipe.c、sys2.c | 空/full では EAGAIN、転送済みなら部分成功。待機登録をせず、直接実行の SRUN を維持 |
| release 後の再利用 | proc_init、executor の再実行 | backend 停止・join と fd close 完了後にだけ proc/user を初期化 |

原典の V7 sys/sys1.c: exit と sys/fio.c: closef も参照し、fd 解放・peer 起床と
終了結果の役割を照合した。V7 の MMU、親子関係、xproc overlay は導入しない。

release は fd close と user 初期化を代行しない。backend のない管理用 owner では、
release は live fd があっても状態・参照を変更しない。owner の明示 close が必要である。
SZOMB と p_xctx == NULL は join 完了の代わりにならず、既存の release 完了契約を維持する。
破棄した coroutine stack を指す u_base は参照しない。既存の pipeline 取消試験も
残量等の値だけを確認し、古い stack の転送先を dereference していない。

#### Step 3 試験補強

- 直接実行を、live pipe を持つ非 NULL の caller proc から行い current/user 復元を確認
- caller の fd、file 参照数、u_procp、error/result/転送状態を子コマンドの cleanup 前後で保持
- 直接コマンドが自分で開いた pipe の fd を残して return し、executor の close を確認
- backend なしの空 read/full write の EAGAIN、容量を超える write の部分成功を確認
- main の戻り値 0、37、-SYS_EPIPE の全てが通常完了の SZOMB/p_ret として保持されることを確認
- 同じ executor 領域の再実行で、前回の fd/error/result/転送状態を引き継がないことを確認
- fd 受渡しと close 後も管理用 proc が SIDL、backend なし、専用 user の相互参照を保持
- pipe 構築失敗後の全 proc が SIDL、backend 部分準備失敗では準備済みだけ PROC_ERROR と確認
- deadlock 取消では両 proc が PROC_ERROR。thread は unwind の main return 値を診断用に保持
- coroutine では取消後の main return がなく p_ret=0 のままでも、pipeline が成功扱いしないことを確認
- thread の I/O unwind 中に、p_xctx 切離し済み、PROC_ERROR、元の user が有効であることを確認
- 起床後 release の待機解除、lock 保持での失敗復帰、取消状態を確認

#### Step 3 検証結果

MSYS2 MINGW64 shell で既存 run_test.sh を実行した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys file | O0/O2、管理用 proc の fd 受渡しと状態保持 | passed |
| sys pipe | O0/O2、既存の EAGAIN/部分転送/close 回帰 | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3、取消・unwind・再利用 | passed |
| pipeline executor | cortn/thread、O0/O2、直接実行・部分準備失敗・取消・再利用 | passed |

strict suite は warning なし。広い shell build には未変更の
help.c、lex.c、par.c、test.c の既存 warning が残り、文法異常系の診断も出る。
これらを含め全 suite は成功した。新規 production 分岐や test hook の追加はない。
Step 4 の集中検証監査と Step 5 の全回帰・Windows 本体対話試験は未実施である。

各構成の make clean は正常終了した。今回生成した d/o/exe の残存なし、
git diff --check 正常を確認し、一時 script/log は削除した。
既存未追跡ファイルは変更していない。

次の着手点: Step 4。既存・追加試験と契約の対応を確認し、状態遷移・異常系の不足を補う。
本記録時点は未コミット。対象は sys file/sched と pipeline executor の試験、
本書、pipe_sched.md、pipe_executor.md である。

### Step 4 完了記録

- 実施日: 2026-10-25
- 開始 HEAD: `ee9ad2e`。Step 3 は同コミットで保存済み
- 状態: complete。Step 5 は pending
- Step 0 の状態・寿命・異常契約と、Step 1～3 の試験を対応付けて不足を補った
- 新しい試験でも動作不整合は見つからず、通常ビルドの状態契約・API・proc メンバは未変更
- backend wake と native report wait の故障注入を SCHED_TEST 限定で追加した
- native report wait の結果を局所変数へ受ける形に整理し、試験用の失敗置換を分離した
- OS wait API の呼出し・停止通知・join の通常ビルドでの処理順序は変更していない

#### Step 4 契約と試験の対応

| 契約 | 主な試験 | 確認する動作 |
|---|---|---|
| 状態値・初期化・bind | header/test_headers、test_state_reports、test_prepare_exhaustion、test_user_isolation | V7 値、SIDL/SRUN、user 相互参照、二重 prepare 拒否、容量不足 |
| 待機理由・登録・lock | test_state_reports、test_wait_lock、test_multiple_waiters | 各 WAIT 理由で SSLEEP、p_wchan、待機登録と lock 保持 |
| 待機キューの取消 | test_waiter_removal | head/middle/tail の取消、残存順序と tail、拒否した resume の状態保持 |
| 早い起床と条件再検査 | wake_before_report、test_retry_wait、pipe の test_wait_rechecks | 古い SSLEEP 報告の抑止、条件未成立時の再待機、重複登録防止 |
| pipe データ・peer 終了 | test_blocking_transfer、test_full_and_offset_reset、test_eof_epipe_and_close_wakeup、test_early_filter_exit | 空/full、部分転送、close 後の EOF/EPIPE、資源回収 |
| 回復可能な失敗 | test_yield_failure_cleanup、test_retry_wait | 失敗後 SRUN に復帰し、同じ entry が再待機・通常完了できる |
| wake 不能 | test_wake_failure | 失敗した peer は PROC_ERROR、他の peer は起床し正常完了 |
| 報告の異常・fatal 保持 | test_state_reports、test_fatal_completion | 不正・矛盾する報告の拒否、登録除去、通常 return より fatal を優先 |
| coroutine 再開不能 | test_guard_completion | 開始前 guard 異常は entry を実行せず拒否、完了後 guard 異常も fatal |
| native wait 異常 | test_native_wait_failure、test_report_wait_failure | worker/controller 両側の待機失敗、停止・unwind・join、元の user 保持 |
| 結果の有効性 | test_three_stage_pipeline、test_blocked_user_cleanup、test_single_user、test_direct_io_context | 最終段結果、非ゼロ・負の正常 return、fatal/cancel の診断値を正常結果に採用しない |
| 解放・再利用 | test_release_waiter、test_wake_then_release、test_release_user_io、test_blocked_user_cleanup | 待機/起床後の取消、link/context 除去、join 後にだけ proc/user を再利用 |
| 直接実行・管理用 proc | test_direct_io_context、test_user_descriptors、準備失敗 cleanup 試験 | SRUN/SZOMB と SIDL の区別、caller 復元、fd close、backend なし I/O |

test_ 名は原則 src/core/sys/test/sched/test_sched.c を指す。
pipe のデータ試験は sys/test/pipe、executor と直接実行の試験は
esh/test/pipeline_exec、test_user_descriptors は sys/test/file にある。
esh/test/sched は sys scheduler と同じ試験ソースを別の include 経路で実行する。

#### Step 4 追加した集中試験

1. 3 waiter の head/middle/tail をそれぞれ取消し、残存者の順序、p_wchan/p_link、
   channel head/tail を確認した。取消前の SSLEEP proc の resume 拒否も確認した。
2. yield の回復可能な失敗後に、同じ entry が実際に再待機する経路を追加した。
   条件未成立で一度起こし、再度 SSLEEP に入り、条件成立後だけ SZOMB になることを確認した。
3. backend wake の一度だけの失敗を両 backend へ注入した。
   失敗者を PROC_ERROR として除去しても、同じ channel の他の待機者を起床できた。
4. native controller の report wait に、実際の worker 通知後の失敗を注入した。
   worker が失敗復帰を終えるまで sched_resume が戻らず、user/error/診断用 p_ret と
   backend 参照を保持し、owner の release で context を回収できることを確認した。
5. coroutine 開始前の guard 異常を追加し、entry が呼ばれないことを確認した。
   既存の通常 return 後の guard 異常と合わせ、切替前後の再開不能を検証した。

故障注入の関数・flag は SCHED_TEST 限定で、report wait 注入は SCHED_THREAD も必要である。
report wait 注入は、実待機から戻り実行 token が返った後にだけ発火するため、
spurious notification で処理開始前に失敗を消費しない。
OS 自体の障害や無効な native handle を作った試験ではなく、既存の失敗経路を
制御して状態・資源の契約を確認する試験である。
Step 2 でソース確認のみだった controller の停止・join 経路も今回実行できた。

#### Step 4 検証結果

MSYS2 MINGW64 shell で既存 run_test.sh の構成・timeout を維持して実行した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | C/C++、単独 include・順序入替え、O0/O2 | passed |
| sys file/pipe | 各 O0/O2、管理用 proc・I/O の既存回帰 | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3、追加故障注入を含む | passed |
| esh pipe/scheduler | 各 cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、結果・cleanup・直接実行 | passed |

strict suite は warning なし。pipeline executor の広い shell build には、
未変更の help.c、lex.c、par.c、test.c の既存 warning と文法異常系の診断が残る。
これらを含め全 suite は成功した。parser/exe 全体、RAM/device 系、Windows 本体の
対話試験を含む全回帰は Step 5 で実施する。

全構成の make clean は正常終了した。生成した d/o/exe の残存なし、
git diff --check 正常を確認し、一時 script/log は削除した。
既存未追跡ファイルは変更していない。

次の着手点: Step 5 の全回帰・最終文書確認。
本記録時点は未コミット。対象は両 scheduler backend の試験用注入、sched.h の
試験用宣言、scheduler 試験、本書、pipe_regression.md である。

### Step 5 完了記録

- 実施日: 2026-10-25～26
- 開始 HEAD: `a7bd5b6`。Step 4 は同コミットで保存済み
- 状態: complete。Step 0～5 は全て完了
- 本体・試験コードの追加変更は不要。既存の試験入口・構成・timeout を維持した
- Windows 本体は SCHED_TEST を定義しない通常 build を使用した

#### Step 5 検証結果

MSYS2 MINGW64 shell で実行した。PowerShell から gcc を直接起動していない。

| 試験 | 構成・確認内容 | 結果 |
|---|---|---|
| sys header | C/C++、単独 include・順序入替え、O0/O2 | passed |
| sys table/bdev/ramdisk/bio/file/pipe | 各 O0/O2、固定資源、I/O、user と所有権 | passed |
| sys scheduler | cortn/thread、O0/O2、既定 NBUF=5 / 小容量 NBUF=3 | passed |
| esh coroutine | O0/O2、context・ABI・stack guard | passed |
| esh pipe/scheduler | 各 cortn/thread、O0/O2 | passed |
| esh pipeline | backend なし、parser bytecode の O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、console・境界・異常・再利用 | passed |
| parser | run_test1.sh / run_test2.sh の既存 golden 照合 | passed |
| exe | run_test.sh の test1～10、既存 golden 照合 | passed |
| cquote | 既存の出力表示型試験、assert/golden 照合なし | 終了値0、表示に既存問題あり |
| Windows 本体 | cortn/thread、既定 O0、実 console 入力 | passed |

自動判定 suite は全て成功した。strict suite は warning なしである。
広い shell build には未変更の help.c、lex.c、par.c、test.c の既存 warning が残る。
未使用引数・関数、format 引数過剰、文字列切詰め、char 添字の分類は従来どおりで、
新規 warning はない。文法異常系の lexer/parser 診断も意図した結果である。

cquote は正常終了したが、表示確認で単独引用引数の欠落・余分な連結を確認した。
関連ソース・試験・Makefile は proc 整合前から未変更であり、proc/scheduler を
link しない。Windows 本体の script 経路も cquote を使用しない。
本移行の回帰とは区別し、表示型試験の正常終了を引用処理の正しさとは扱わない。
具体例と別作業の範囲は[回帰ガイド](pipe_regression.md)に記録した。

Windows 本体は両 backend で同じ入力を行い、次を確認した。

- `sam1` は sample1 applet を表示した
- `sam2 1 10 100` は argc=4、arg1=1、arg2=10、arg3=0x00000064 を表示した
- `sam3 -a -b string` は -a option、-b option with string を表示した
- `echo hello|cat` は hello、`echo four|cat|cat|cat` は four を表示した
- `k=3` の次の入力 `echo "$k"|cat` は 3 を表示し、変数を保持した
- 各入力後に prompt へ戻り、Ctrl-D で終了値 0 となった

既存 DLL をコピーし直さず、`main.exe` target を build した。
初回は `_output` 不足で build に失敗したため、`make _output` 後に再実行した。
同 target が directory を作らない既存の前提であり、本体の変更は不要だった。
両 backend の確認後に Windows directory の make clean は完了した。

#### Step 5 文書整理

- pipe_sched.md に全メンバの役割、V7 との対応・差分、wake 失敗時の扱いを整理した
- pipe_executor.md に全段 SZOMB を結果採用の条件とし、PROC_ERROR の p_ret は
  診断用であることを明記した
- pipe_thread.md と cortn_windows.md に状態・異常系の結合試験範囲を追記した
- pipe_thread.md の単独 main.exe build 例に出力 directory 作成を補った
- pipe_regression.md に parser pipeline suite の入口、backend を使う suite との
  区別、小容量 scheduler 構成を補った
- README.md に現在の proc 定義・状態・所有権と記録への案内を追加した
- Step 0 の設計確定時の観察と現在の実装を混同しないよう、時点を明示した

#### 最終確認・残る範囲

追跡済み src 全体を検索し、p_state/p_wait_chan/p_wait_next、p_backend/p_status、
PROC_NEW/RUNNABLE/RUNNING/WAIT_LOCK/WAIT_READ/WAIT_WRITE/FINISHED の残存なしを
確認した。PROC_ERROR は値 7 の意図した MCUFW 拡張であり、削除対象ではない。
旧名は移行対応表・設計時の観察・過去の作業記録にだけ残す。

本計画は proc の名前・意味・状態・寿命の整合であり、V7 kernel の完全移植ではない。
MMU/trap/swap、PID・親子関係・wait、priority/signal、preemption は導入していない。
固定容量・最大4段・単一controllerと、coroutine stack破棄 / thread unwind・join の
違いを維持する。今回の実行確認は Windows x64 であり、STM32/RTOS 移植は別作業である。
sanitizer は今回の試験対象外とし、過去の環境記録は回帰ガイドに残した。

全構成の make clean は正常終了した。対象の sys/esh test directory と Windows
本体に、今回生成した d/o/exe が残っていないことを確認した。
一時 script/log は削除し、文書内の local link と git diff --check も確認した。
既存未追跡の cscope、STM32 tree、DLL は変更していない。

次の担当者が行う proc 整合 Step はない。cquote の既存問題の修正、STM32/RTOS 移植、
新しい機能追加は別の依頼として扱う。
本記録時点は未コミット。対象は README.md、pipe_sched.md、pipe_executor.md、
pipe_thread.md、cortn_windows.md、pipe_regression.md、本書、Step 0 設計決定の
計8文書であり、本体・試験コードに追加差分はない。
