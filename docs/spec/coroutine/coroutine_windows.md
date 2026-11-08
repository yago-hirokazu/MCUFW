# Windows stackful coroutine

更新日: 2026-11-08

## 役割

Windows coroutine backendでappletのC call stackを保ったまま実行権をschedulerへ
返す。pipeが空または満杯のときや、inode lock待ちで実行を中断できる。

coroutineはOS threadではない。同じthread上で`sched_resume()`がbackendを再開
した場合だけ実行される。portable schedulerは`src/core/sys/sys/sched.c`、Windows
backendは`src/core/sys/dev/windows/sched_cortn.c`にある。

## Coroutine API

coroutine engine本体と公開headerはWindows固有context実装と同じdirectoryに置く。

- `src/core/sys/dev/windows/cortn.c`
- `src/core/sys/dev/windows/cortn.h`
- `src/core/sys/dev/windows/cortn_context.c`
- `src/core/sys/dev/windows/cortn_context.h`
- `src/core/sys/dev/windows/cortn_switch.S`

- `init_cortn()`：利用者所有stackを初期化し、entry関数と引数を登録する。
  coroutineを`CORTN_RUNNABLE`にする
- `cortn_resume()`：RUNNABLE coroutineを再開する
- `cortn_yield()`：待機理由を設定して中断する。自発的なyieldでは
  `CORTN_RUNNABLE`に戻る
- `cortn_wake()`：lock/read/write待ちをRUNNABLEへ移す
- `cortn_stack_check()`：上下guard patternを検査する
- `cortn_stack_used()`：stack watermarkから最大使用量を返す

entry関数がreturnすると`CORTN_FINISHED`へ遷移する。FINISHEDまたはERROR状態の
coroutineは再開できない。scheduler backendがcoroutine状態を`PROC_*`へ写す。

yieldは呼出しthreadで実行中のcoroutine、wakeはlock/read/write待ちのcoroutineを
対象とする。これらの状態検査と入れ子resumeの検査は行わず、呼出し側の前提とする。
resumeのRUNNABLE判定は維持し、終了済み・待機中coroutineの再開は拒否する。

## Scheduler backend選択

`src/core/sys/dev/windows/sched_adapt.mk`がWindows scheduler backendを選ぶ。
defaultの`SCHED_BACKEND=cortn`は次をbuildする。

- `cortn.c`
- `cortn_context.c`
- `cortn_switch.S`
- `sched_cortn.c`

`SCHED_BACKEND=thread`は`sched_thread.c`だけをbackend sourceとしてbuildし、
`SCHED_THREAD`を定義する。thread backendはcoroutine objectをlinkしない。

## Windows x64 context

`src/core/sys/dev/windows/cortn_switch.S`はMicrosoft x64 ABIに従い、次を保存・
復元する。

- `rsp`
- `rbx`、`rbp`、`rdi`、`rsi`、`r12`～`r15`
- `xmm6`～`xmm15`
- x87 control word
- `MXCSR`

初回contextは16-byte stack alignmentと32-byte shadow spaceを満たす`cortn_boot`へ
入る。`cortn_boot`は`r12`の`struct cortn`を第1引数`rcx`へ渡す。entry関数には
`self`と`arg`を別々に渡し、関数型castは使わない。

assembler依存offsetは`src/core/sys/dev/windows/cortn_context.c`でcompile時に
検査する。

## Stack

Windows stack容量は64 KiBとする。上下16 byteをguard patternに使用し、残りを
watermark patternで初期化する。guard検査はresume前後に行う。guardは事後検出
であり、Windows guard pageのような発生時停止機構ではない。

scheduler backendは`PIPELINE_MAX`個（4個）のslotを静的に確保し、各slotに
16 byte境界へ整列したstackを持つ。stack領域は実行中と待機中に保持し、
`sched_release()`でslotを再利用できる状態に戻す。
MCU stack容量と配置はSTM32移植時に決定する。

## 制約

- 同じthread内でのみresume/yieldする
- 1個のcoroutineを複数threadから同時にresumeしない
- coroutineからschedulerを入れ子でresumeしない
- C++ exceptionやWindows SEHをcontext境界越しに伝播させない
- FINISHED後のstackと`struct cortn`はscheduler backendが安全に再利用する

## 試験

`src/core/shell/cli/test/cortn/run_test.sh`はO0/O2、`-Werror`で次を確認する。

- 初回resumeとentry引数
- nested関数からの複数yield
- yield前後のlocal variable
- 2個のcoroutineのround-robin
- WAIT_LOCK/READ/WRITEとwake
- entry return後のFINISHED
- FINISHED後のresume拒否
- resume前後のstack guard破損検出とwatermark
- XMM6、x87 control word、MXCSRの復元

`src/core/sys/test/sched`と`src/core/shell/cli/test/pipeline_exec`は、
`cortn`と`thread`の両backendをO0/O2で確認する。scheduler試験では既定の
`NBUF=5`に加え、`NBUF=3`でも実行する。
