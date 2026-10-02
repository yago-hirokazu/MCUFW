# esh pipe sleep・wakeup

更新日: 2026-10-02

## 役割

Milestone 4ではpipeの空・満杯をcoroutineの待機と再開へ接続する。
portableな`sched.c`はwait listとprocess状態を管理し、context切替とlock実装を
scheduler backendへ委譲する。

Windows coroutine backendは`target/windows/sched_cortn.c`に置く。後続のthread
backendは同じ内部backend APIを実装し、pipeコアを変更せずに切り替える。

## Kernel lock

pipe状態の検査、wait list登録、lock解放を単一kernel lockの下で行う。
coroutine backendでは同じthread上の協調実行であり、schedulerへ戻らない限り
preemptされない。このためlockは所有状態だけを保持する。

thread backendでは同じAPIをmutexとcondition相当へ置き換える。

## Sleep

`esh_sleep()`はlock保持中に呼び出す。

```text
pipe状態を検査
  -> processをwait channel末尾へ登録
  -> processをWAIT_READまたはWAIT_WRITEへ変更
  -> kernel lock解放
  -> backend yield
  -> wake後にkernel lock再取得
  -> 呼出し元のloopでpipe状態を再検査
```

wait登録前に実行権をschedulerへ返さないため、状態検査直後のwakeupを失わない。

## Wakeup

`esh_wakeup()`はchannel上の全processをwait listから外し、backendをwakeして
RUNNABLEへ変更する。wake後にI/O可能とは限らない。各processは必ず条件を
再検査し、満たされていなければ再びsleepする。

使用するchannelは次のとおり。

- writeがデータを書いた後、read channelをwake
- readが空きを作った後、write channelをwake
- 最終writer close後、read channelをwakeしてEOFを判定
- 最終reader close後、write channelをwakeしてbroken pipeを判定

## Blocking I/O

`esh_read()`はbufferにデータがあれば現在取得可能なbyte数を返す。空でwriterが
存在すればsleepし、最終writer close後は0を返す。

`esh_write()`は要求全体を転送するまでpartial writeとsleepを繰り返す。待機中に
最終readerがcloseした場合は`-ESH_EPIPE`を返す。

coroutine backendをbindしていないprocessではMilestone 3の単体利用を維持し、
待機が必要な場合は`-ESH_EAGAIN`または転送済みbyte数を返す。

## Scheduler状態

`esh_sched_resume()`はcurrent processを設定してbackendをresumeし、戻った後に
coroutine状態を次のportable状態へ写す。

- `ESH_PROC_RUNNABLE`
- `ESH_PROC_WAIT_READ`
- `ESH_PROC_WAIT_WRITE`
- `ESH_PROC_FINISHED`
- `ESH_PROC_ERROR`

runnable processがなく未完了processが残る場合、schedulerはidleまたはdeadlock
として扱える。
