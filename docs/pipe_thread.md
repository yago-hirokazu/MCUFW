# Windows thread scheduler backend

更新日: 2026-10-24

## Backend選択

schedulerを使うWindows Makefileは
`src/core/sys/dev/windows/sched_backend.mk`をincludeする。defaultの
`SCHED_BACKEND=cortn`はcoroutineを使い、`SCHED_BACKEND=thread`はnative
Windows threadを使う。thread buildはcoroutine codeとcontext switch assemblyを
linkしない。

buildはMSYS2 MINGW64 shell内で実行する。

```sh
cd src/target/windows
make -B SCHED_BACKEND=thread main.exe
./main.exe
make -B SCHED_BACKEND=cortn main.exe
make clean
```

backendごとに`OUTDIR`と`TARGET`を分ければ両buildを保持できる。回帰scriptは
coroutine/threadをO0/O2の別directoryへbuildする。

## 実行と所有権

- 固定4 slotのpoolがnative thread handleを所有する
- `_beginthreadex`でCRT互換threadを作る
- 準備済みthreadはcommand entryを呼ぶ前に実行tokenを待つ
- executorは一度に1 processへ実行tokenを与える
- resumeは選択processがI/O待ちになるかreturnするまで待つ
- wakeupはprocessをrunnableにし、executorが次のtokenを与える
- TLSがthreadごとのcurrent processとkernel lock所有状態を保持する
- `current_user()`はcurrentの`p_addr`を返し、currentなしではTLSのbase userを使う
- workerはapplet呼出し前に自身をcurrent processへ設定する
- entry returnは`PROC_FINISHED`を通知する
- releaseはthreadをjoinし、handleを閉じ、slotを再利用可能にする
- 未開始threadのreleaseはentryを呼ばずに終了させる
- 待機中threadのreleaseは協調的なunwindを要求する
- schedulerはrelease対象processをwait listから外す

pipeline controllerは単一threadのままである。nested pipeline、複数controller、
background実行、CPU-only commandの強制cancelはscheduler契約外である。

## Lockとwait契約

1個のWindows SRW exclusive lockがpipe state、inode lock、参照数、wait listを
保護する。lockは非再帰で、所有診断はTLSに保持する。

`sched_sleep()`はkernel lock保持中にwaiterを登録し、backend yieldを呼ぶ。
backend yieldはlock保持状態で呼ばれ、lock保持状態で戻る。native waitは
`SleepConditionVariableSRW`でlock解放とwait開始を不可分にし、wake後にlockを
再取得する。coroutine yieldは明示的にunlock、context switch、relockする。

condition notificationはpredicateの代わりではない。workerは実行tokenまたは
stop requestをloopで待つ。native wait直前のwakeupもrunnable stateとして残る。
pipeとinode lockはwake後に条件を再検査する。

releaseとjoinはkernel lock外で行う。workerがI/Oをunwindしてfdを閉じるために
同じlockを必要とするからである。file/inode/buffer tableを初期化する前に全thread
を終了させる。
userもrelease完了まで保持する。releaseはuserをゼロ化せず、停止したworkerは
元のp_addrで残量とerror/resultを確定する。全fd解放とjoinの完了後、proc_initで
procとuserを再初期化する。sleep/wakeup中もu_base/u_count/u_offsetとu_rを保持する。

## 試験

```sh
cd src/core/sys/test/sched
bash ./run_test.sh
cd ../../../esh/test/pipe
bash ./run_test.sh
cd ../pipeline_exec
bash ./run_test.sh
```

schedulerとpipe suiteはblocking transfer、EOF、EPIPE、spurious wakeup、複数
waiter、pool枯渇、wait-list cleanup、close wakeup、resource再利用を確認する。
pipeline suiteは2/3/4段、stdio routing、部分準備失敗のrollbackを確認する。

native scheduler試験はexternal waker raceを100回実行し、sleep前後のTLS、worker
thread identity、check/register/waitの不可分性、終了後のhandle数を確認する。
専用userのerrorがsleep/wakeupをまたいで保持されること、hostとworkerの分離、
current procのないnative thread間のbase user分離も試験する。

## V7との差分

V7のsleep/wakeupと条件再検査を維持するが、kernel process switchは使わない。
native thread backendでもexecutorが実行tokenを直列に与えるため、appletは自動的
には並列実行されない。priority、signal、timeout、強制終了は実装しない。
