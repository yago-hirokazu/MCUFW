# Milestone 8: Windows thread backend

## Backend selection

All scheduler-enabled Windows Makefiles include
`src/core/sys/dev/windows/sched_backend.mk`.
Default `SCHED_BACKEND=cortn` retains coroutine behavior.
`SCHED_BACKEND=thread` selects native thread implementation;
thread builds do not link coroutine code or context-switch assembly.

Run builds inside MSYS2 MINGW64 shell. Rebuild when switching backend:

```sh
cd src/target/windows
make -B SCHED_BACKEND=thread main.exe
./main.exe
make -B SCHED_BACKEND=cortn main.exe
```

Separate OUTDIR and TARGET values can preserve both build variants.
Test scripts build both backends at O0 and O2 in separate directories.

## Execution and ownership

- Fixed four-slot pool owns native thread handles
- `_beginthreadex` creates CRT-compatible threads; Windows owns stacks
- Prepared threads wait before calling command entry
- Existing executor grants one runnable process execution token at time
- Resume waits until chosen command blocks on I/O or returns
- Wakeup marks process runnable; executor grants next execution token
- This preserves existing round-robin contract without executor changes;
  applets are not automatically run simultaneously
- TLS stores current process and kernel-lock ownership for each thread
- Worker sets its own current process before calling applet
- Entry return publishes FINISHED; release joins thread, closes handle,
  and clears slot before reuse
- Releasing unstarted thread skips entry; releasing waiter requests
  cooperative unwind. No TerminateThread or forced stack destruction
- Scheduler removes released process from wait list

Pipeline controller remains single-threaded. Nested pipelines, concurrent
controllers, asynchronous background execution, and cancellation of
CPU-only commands are outside current scheduler contract.

## Lock and wait contract

Single Windows SRW exclusive lock protects pipe state and wait lists.
Lock is non-recursive; lock ownership diagnostic is thread-local.

`esh_sleep` registers waiter while holding kernel lock and calls backend
yield without unlocking. Backend yield enters and returns with lock held.
Native wait uses `SleepConditionVariableSRW` to release lock and enter
wait atomically, then reacquires lock before returning.
Coroutine yield explicitly unlocks, switches context, and relocks.

Condition-variable notifications never replace predicates. Worker waits
in loop for execution token or stop request. A wakeup before native wait
remains visible as runnable state. Core preserves runnable state if
wakeup races with resume's wait-state report. Pipe loops recheck I/O
conditions after wakeup, including EOF and broken pipe.

Release waits outside kernel lock so worker can unwind I/O and close fds.
All threads must finish before resetting file/pipe pools.

## Tests

```sh
cd src/core/esh/test/sched
bash ./run_test.sh
cd ../pipe
bash ./run_test.sh
cd ../pipeline_exec
bash ./run_test.sh
```

Shared tests verify blocking transfer, EOF, broken pipe, spurious wakeup,
multiple waiters, pool exhaustion, wait-list cleanup, 2/3-stage pipelines,
stdio routing, partial preparation rollback, and repeated resource reuse.
Scheduler and pipe suites use -Wall -Wextra -Werror -pedantic.

Native scheduler test adds 100 external-waker races. It checks TLS before
and after sleep, worker thread identity, atomic check/register/wait path,
and unchanged process handle count after all native handles close.

## Verification (2026-10-03)

- Scheduler, pipe, pipeline suites: cortn/thread at O0/O2 all pass
- Native race/TLS/handle tests: 100 iterations pass at each optimization
- Thread O2 with DEBUG_STDIO=1: consumer data remains unchanged
- Partial pipeline preparation failure: rollback and next execution pass
- Executor cases 1 through 10: golden output matches for both backends
- Parser run_test1.sh and run_test2.sh: test pass
- Quote test: successful build and execution
- Windows thread target: successful build and link through MSYS2
- Interactive sam1, sam2 1 10 100, sam3 -a -b string: expected output
- Whitespace and 80-column checks on changed C/header sources: pass

No warnings in strict scheduler/pipe builds. Existing warnings in unrelated
shell/parser/tty modules remain in wider integration builds.
