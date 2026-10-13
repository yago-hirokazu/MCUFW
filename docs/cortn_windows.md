# Windows stackful coroutine

更新日: 2026-10-13

## 役割

Windows coroutine backendでappletのC call stackを保ったまま実行権をschedulerへ
返す。pipeが空、満杯、またはinode lock待ちの間だけcommandを待機させる。

coroutineはOS threadではない。同じthread上で`sched_resume()`がbackendを再開
した場合だけ実行される。portable schedulerは`src/core/sys/sys/sched.c`、Windows
backendは`src/core/sys/dev/windows/sched_cortn.c`にある。

## Coroutine API

coroutine engine本体は`src/core/esh/cortn.c`と`include/cortn.h`に残している。

- `cortn_init()`：利用者所有stackとentry関数を登録する
- `cortn_resume()`：RUNNABLE coroutineを再開する
- `cortn_yield()`：理由付きで中断する
- `cortn_wake()`：read/write待ちをRUNNABLEへ移す
- `cortn_stack_check()`：上下guard patternを検査する
- `cortn_stack_used()`：stack watermarkから最大使用量を返す

entry関数がreturnすると`CORTN_FINISHED`へ遷移する。FINISHEDまたはERROR状態の
coroutineは再開できない。scheduler backendがcoroutine状態を`PROC_*`へ写す。

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

stack領域はbackend slotが所有し、coroutineの実行中と待機中は解放しない。
MCU stack容量と配置はSTM32移植時に決定する。

## 制約

- 同じthread内でのみresume/yieldする
- 1個のcoroutineを複数threadから同時にresumeしない
- coroutineからschedulerを入れ子でresumeしない
- C++ exceptionやWindows SEHをcontext境界越しに伝播させない
- FINISHED後のstackと`struct cortn`はscheduler backendが安全に再利用する

## 試験

`src/core/esh/test/cortn/run_test.sh`はO0/O2、`-Werror`で次を確認する。

- 初回resumeとentry引数
- nested関数からの複数yield
- yield前後のlocal variable
- 2個のcoroutineのround-robin
- WAIT_READとwake
- entry return後のFINISHED
- FINISHED後のresume拒否
- stack guardとwatermark
- XMM6、x87 control word、MXCSRの復元

`src/core/sys/test/sched`とesh pipe/pipeline suiteがscheduler backendとの結合を
coroutine/thread双方で確認する。
