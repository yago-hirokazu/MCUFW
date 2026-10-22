# sys pipe sleep・wakeup

更新日: 2026-10-22

## 役割

portableな`src/core/sys/sys/sched.c`はwait listと`struct proc`状態を管理し、
context切替とlock実装をscheduler backendへ委譲する。Windows backendは
`src/core/sys/dev/windows/sched_cortn.c`と`sched_thread.c`にある。

`src/core/sys/dev/windows/sched_backend.mk`の`SCHED_BACKEND`でbackendを選ぶ。
defaultは`cortn`で、`thread`指定時はnative Windows threadを使用する。

## Kernel lock

pipe状態、inode lock、file/inode参照、wait listを単一の非再帰kernel lockで
保護する。coroutine backendは所有状態を追跡し、yield時にlockを解放してcontext
切替後に再取得する。thread backendはSRW lockとcondition variableを使う。

## Sleep

`sched_sleep()`はlock保持中に呼び出す。

```text
pipeまたはinode lock条件を検査
  -> processをwait channel末尾へ登録
  -> PROC_WAIT_LOCK/READ/WRITEへ変更
  -> backendがlock解放と待機を不可分に実行
  -> wake後にlock保持状態で復帰
  -> 呼出し元loopで条件を再検査
```

wait登録前に実行権を返さないため、条件検査直後のwakeupを失わない。

## Wakeup

`sched_wakeup()`はchannel上の全processをwait listから外し、backendをwakeして
`PROC_RUNNABLE`へ変更する。wake後にI/O可能とは限らない。各processは条件を
再検査し、満たされていなければ再びsleepする。

使用するchannelは次のとおり。

- write後にread channelをwake
- 全dataをreadしてbufferを空にした後にwrite channelをwake
- endpoint close後にread/write channelをwakeし、EOF/EPIPEを再判定
- inode lock解放時に`IWANT`があればlock channelをwake

## Blocking I/O

`sys_read()`はcurrent processとfdを解決し、`readp()`を呼ぶ。dataがあれば現在
取得可能なbyte数を返し、空でwriterが存在すればsleepする。最終writer close後
かつdataが空なら0を返す。

`sys_write()`は`writep()`を呼び、要求全体を転送するまで部分writeとsleepを
繰り返す。待機中に最終readerがcloseした場合は`-SYS_EPIPE`を返す。

backendをbindしていないprocessでは、待機が必要な場合に`-SYS_EAGAIN`または
転送済みbyte数を返す。

## Scheduler状態

`sched_resume()`はbackendを再開し、戻った状態を次のportable状態へ写す。

- `PROC_NEW`
- `PROC_RUNNABLE`
- `PROC_RUNNING`
- `PROC_WAIT_LOCK`
- `PROC_WAIT_READ`
- `PROC_WAIT_WRITE`
- `PROC_FINISHED`
- `PROC_ERROR`

`current_proc()`はcoroutineではbackend global、threadではTLSからcurrent
processを返す。runnable processがなく未完了processが残る場合、executorは
idleまたはdeadlockとして扱う。

processは`proc_init(proc, user)`で初期化する。`p_addr`は呼出元所有userを指し、
`current_user()`と`u`マクロはcurrent procからその参照先を求める。currentが
なければbackend-local base userを使い、thread backendではTLSに置く。
sleep/wakeupで`u_error`を初期化しない。fd tableは引き続き`p_ofile[]`にある。
専用userはbackendのrelease/join完了まで生存させる。

`sched_release()`はprocessをwait listから外し、backend handleをkernel lock外で
解放する。thread join中にworkerがlockを必要とするため、lock内でjoinしない。

## V7との差分

V7のwait channelと条件再検査を採用するが、process切替機構は移植可能backendへ
分離する。priority、signal、timeout、swapping、CPU preemptionは実装しない。
coroutineとthreadは同じ`struct proc`状態とpipe結果を公開する。
