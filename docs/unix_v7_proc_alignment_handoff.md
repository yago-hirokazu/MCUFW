# UNIX V7 proc 構造体整合 引き継ぎ書

作成日: 2026-10-24
更新日: 2026-10-25
対象: `C:\usr\src\MCUFW`
計画作成時 HEAD: `82b7617` (`Rename proc members to p_xctx and p_ret`)

## 現況と依頼

ユーザーの依頼は、現在の MCUFW の `struct proc` の変数名と役割を、
可能な限り UNIX V7 に合わせ、無理なく組み込むことである。
本書は別タスクから実装を継続できるよう、設計方針、順序、検証条件を整理する。
Step 0 の設計確定と Step 1 のメンバ改名・ヘッダ分離は完了した。
Step 2～5 は未着手である。次は Step 2 の状態統合を進める。
状態・寿命・異常の確定契約は
[Step 0 設計決定](unix_v7_proc_alignment_step0.md)を参照する。
以下の「現在の実装」はメンバ改名後のソースを示す。
状態分類・異常契約の移行は未実装であり、確定契約の実装完了を意味しない。
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

- 状態は NEW、RUNNABLE、RUNNING、WAIT_LOCK/READ/WRITE、FINISHED、ERROR
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

| 現在 | 移行先 | 意味 |
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
| 2 | scheduler/backend/executor の状態統合 | pending |
| 3 | 直接実行・管理用 proc・解放処理の整合 | pending |
| 4 | 状態遷移と異常系の集中検証 | pending |
| 5 | 全回帰、文書更新、完了確認 | pending |

### Step 0: 状態・寿命・異常の契約

完了。契約の詳細と出典は[Step 0 設計決定](unix_v7_proc_alignment_step0.md)に記載した。
本書、現行コード、V7 原典を照合し、次を確定した。

- 状態値と、各遷移を更新する関数・lock 条件
- SIDL の管理専用 proc と、SRUN の backend なし直接実行の扱い
- 通常終了、処理中の回復可能な失敗、backend 再開不能、強制解放の違い
- release 中と完了後の p_stat の扱い、状態を上書きしてよい条件
- p_ret の書込担当と、結果回収時点
- backend resume の返却状態と共有 p_stat が競合した場合の優先規則

注意: 現行 test_yield_failure_cleanup は yield 失敗を呼出元が受け取り、
entry が戻った後に PROC_FINISHED になることを確認している。
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
