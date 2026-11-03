# sys pipe sleep・wakeup

更新日: 2026-11-01

## 役割

portableな`src/core/sys/sys/sched.c`はwait listと`struct proc`状態を管理し、
context切替とlock実装をscheduler backendへ委譲する。Windows backendは
`src/core/sys/dev/windows/sched_cortn.c`と`sched_thread.c`にある。

`src/core/sys/dev/windows/sched_adapt.mk`の`SCHED_BACKEND`でbackendを選ぶ。
defaultは`cortn`で、`thread`指定時はnative Windows threadを使用する。

## Sys lock

pipe状態、inode lock、file/inode参照、wait listを単一の非再帰sys lockで
保護する。coroutine backendは所有状態を追跡し、yield時にlockを解放してcontext
切替後に再取得する。thread backendはSRW lockとcondition variableを使う。

## Sleep

`sched_sleep()`は`sched_prepare()`に成功した実行コンテキストから、sys lockを
保持して呼び出す。schedulerがそのprocessをcurrent processとして実行中である
ことを前提とし、`p_ctx`のNULL判定は行わない。

current processの存在、sys lockの保持、`p_wchan`が未登録であることは
呼出し側の前提とし、実行時の検査は行わない。
backendのyield失敗時は待機登録を除去し、エラーを返す。

```text
pipeまたはinode lock条件を検査
  -> channelアドレスをhashし、slpque[]の該当list先頭へ登録
  -> PROC_WAIT_LOCK/READ/WRITEへ変更
  -> backendがlock解放と待機を不可分に実行
  -> wake後にlock保持状態で復帰
  -> 呼出し元loopで条件を再検査
```

wait登録前に実行権を返さないため、条件検査直後のwakeupを失わない。

## Wakeup

`sched_wakeup()`はsys lock保持中に呼び出す。wait list上のprocessは、
`sched_prepare()`で準備した有効な実行コンテキストを保持していることを
前提とし、起床対象の`p_ctx`のNULL判定は行わない。起床を通知する呼出し元は
実行コンテキストを持たなくてもよい。

channel上の全processをwait listから外し、backendをwakeする。
`sched_adapt_wake()`が成功すれば`PROC_RUNNABLE`、失敗すれば`PROC_ERROR`へ
変更する。wake後にI/O可能とは限らない。各processは条件を再検査し、
満たされていなければ再びsleepする。

channelは格納領域を持つ構造体ではなく、出来事を識別する`char *`である。
pipeではV7と同じく、inode先頭アドレスをlock、先頭+1 byteをwrite、
先頭+2 byteをreadに使う。指す値は読み書きしない。

portable schedulerは64 bucketの`slpque[]`を所有し、`p_link`でwaiterをつなぐ。
hashは`((uintptr_t)chan >> 5) & 63`とし、Windowsでもpointer幅を保つ。
同じbucketに異なるchannelが混在するため、wakeupは`p_wchan == chan`の
processだけを取り外す。channelの対象は待機終了まで有効でなければならない。
inode解放・再利用までに関係する待機登録を除去する。

起床後は`p_wchan`と`p_link`をNULLにする。runqは導入せず、executorによる
runnable processの選択を維持する。`pinit()`で共通wait hashを初期化しない。

各channelの起床契機は次のとおり。

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

pipe I/Oと`plock()`は、`sched_prepare()`に成功した実行コンテキストを
schedulerが実行していることを前提とする。`readp()`、`writep()`、`plock()`は
`p_ctx`を検査せず、待機条件が成立したら`sched_sleep()`を呼ぶ。満杯による
部分writeでは呼出し元へ戻らず、残りの転送を再開する。

backend未設定process向けの`-SYS_EAGAIN`や部分byte数の返却はサポートしない。
`sched_sleep()`も準備済みコンテキストを前提とし、backendからの
エラー伝播を維持する。パイプラインは
全processの`sched_prepare()`が成功してから`sched_resume()`で実行を開始し、
準備失敗時はコマンドを開始せず資源を回収する。

## Scheduler状態

`sched_prepare()`と`sched_bind_proc_to_coroutine()`は、`pinit()`で初期化した
backend未設定のprocessを対象とし、`PROC_NEW`の検査は行わない。
backend準備時の資源不足は`-SYS_ENOMEM`で返す。

`sched_resume()`と`sched_release()`は、実行コンテキストの準備に成功し、
まだ解放していないprocessを対象とする。両関数は`p_ctx`のNULL判定を行わない。
準備失敗時は成功済みのprocessだけを解放し、二重解放や解放後の再開は行わない。

`sched_resume()`は、呼出し元にcurrent processがなく、対象processが
`PROC_RUNNABLE`であることを前提とし、両条件の検査は行わない。
backendを再開し、戻った状態を次のportable状態へ写す。

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

`sched_release()`はsys lock保持中にprocessをwait listから外してから
`p_ctx`をNULLにする。この順序により、起床処理が実行コンテキストを失った
processをwait listから取り出すことを防ぐ。backend handleはsys lock外で
解放する。thread join中にworkerがlockを必要とするため、lock内でjoinしない。

## V7との差分

V7のwait channelと条件再検査を採用するが、process切替機構は移植可能backendへ
分離する。priority、signal、timeout、swapping、CPU preemptionは実装しない。
coroutineとthreadは同じ`struct proc`状態とpipe結果を公開する。
