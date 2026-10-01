# Windows stackful coroutine

更新日: 2026-10-02

## 役割

Windows coroutine backendでappletのC呼出しstackを保ったまま実行権を
schedulerへ返す。pipeが空または満杯の間だけcommandを待機させるための
基盤として使用する。

coroutineはOS threadではない。同じthread上でschedulerが明示的に
`cortn_resume()`を呼んだ場合だけ実行される。

## API

- `cortn_init()`：利用者所有stackとentry関数を登録する
- `cortn_resume()`：RUNNABLE coroutineを再開する
- `cortn_yield()`：自発、read待ち、write待ちの理由付きで中断する
- `cortn_wake()`：read/write待ちをRUNNABLEへ移す
- `cortn_stack_check()`：上下guard patternを検査する
- `cortn_stack_used()`：stack watermarkから最大使用量を返す

entry関数がreturnすると`CORTN_FINISHED`へ遷移し、schedulerへ戻る。
FINISHEDまたはERROR状態のcoroutineは再開できない。

## Windows x64 context

`cortn_switch.S`はMicrosoft x64 ABIに従い、次を保存・復元する。

- `rsp`
- `rbx`、`rbp`、`rdi`、`rsi`、`r12`～`r15`
- `xmm6`～`xmm15`
- x87 control word
- `MXCSR`

初回contextは16-byte stack alignmentと32-byte shadow spaceを満たす
`cortn_boot`へ入る。`cortn_boot`は`r12`に保持した`struct cortn`を第1引数
`rcx`へ渡す。entry関数には`self`と`arg`を別々の引数として渡し、関数型の
castは使用しない。

context構造のassembler依存offsetは`cortn_context.c`でコンパイル時に検査する。

## Stack

初期Windows stack容量は16 KiBとする。stack領域は利用者が所有し、coroutineの
実行中と待機中は解放してはならない。

上下16 byteをguard patternに使用する。残りをwatermark patternで初期化し、
使用量を測定する。guard検査はresume前後に行う。guardは事後検出であり、
Windows guard pageのように不正書込みを発生時点で停止する機構ではない。

## 制約

- 同じthread内でのみresume/yieldする
- 1個のcoroutineを複数threadから同時にresumeしない
- coroutineからschedulerを入れ子でresumeしない
- stack上のC++例外やWindows SEHをcontext境界越しに伝播させない
- FINISHED後のstackと`struct cortn`はschedulerが安全に解放する

## 試験

`src/core/esh/test/cortn/run_test.sh`は`-O0`と`-O2`で次を確認する。

- 初回resumeとentry引数
- nested関数からの複数yield
- yield前後のローカル変数
- 2個のcoroutineのround-robin
- WAIT_READとwake
- entry return後のFINISHED
- FINISHED後のresume拒否
- stack guardとwatermark
- XMM6、x87 control word、MXCSRの復元
