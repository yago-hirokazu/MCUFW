# ESH pipeline executor

更新日: 2026-10-22

## Scope

`src/core/esh/exe.c` connects `OP_PIPE_NEXT` bytecode to process allocation,
anonymous pipe setup, scheduling, command completion, and resource cleanup.
Windows console stdio routing and coroutine/thread backends are implemented.
Pipe applets use `sys_read()` and `sys_write()` or mapped stdio functions.

## Process and descriptor mapping

Executor assigns commands to `e_proc[]` in bytecode order. `PROCS` and
`PIPELINE_MAX` both limit pipeline to four commands. For each adjacent
pair, executor creates one anonymous pipe and installs endpoints at fixed
descriptor numbers.

```text
process N stdout fd 1 -> pipe N write endpoint
process N+1 stdin fd 0 -> pipe N read endpoint
```

Temporary construction descriptors are closed after endpoint installation.
Each process owns only descriptors needed by its position in pipeline.

`struct exe_proc` embeds portable `p_exec` and dedicated `p_user` alongside
shell command arguments. `proc_init(&proc->p_exec, &proc->p_user)` initializes
both and sets `p_addr` before connecting descriptors. `exe_connect_proc()`
uses temporary owner proc with dedicated user to construct each pipe.

## Scheduling

Portable scheduler prepares one execution backend for each process and resumes
runnable processes in round-robin order. Windows coroutine backend owns fixed
four-slot coroutine and stack pool. No core allocation is dynamic.

Command return stores status in `p_exec.p_status`. Command wrapper then closes every
remaining process descriptor before coroutine finishes. Endpoint close wakes
blocked peers, producing EOF after final writer close or broken pipe after
final reader close.

Scheduler reports error when processes remain unfinished and no process is
runnable. Cleanup closes all descriptors and releases every prepared backend
slot on both success and failure paths.

Thread backend uses native threads with execution tokens assigned sequentially
by executor. Cleanup closes descriptors under kernel lock, then releases or
joins backends outside lock. Dedicated user storage outlives backend unwind
and join. `current_user()` resolves current proc's `p_addr` on each access.

Single command runs directly without backend. Executor sets current proc
before applet call, closes all descriptors after return, and restores previous
current proc. Bytecode, operands, command IDs, stack and process limits are
validated before execution.

## Pipeline status

Pipeline status is final command status. Earlier command errors remain in each
process `p_exec.p_status` and do not replace final status. Executor stores resulting
status in `e_status`.

## Deliberate omissions

- redirection and background jobs
- signal delivery for broken pipe
- regular files and nested pipelines
- forced cancellation of CPU-only commands

These do not alter current anonymous pipe ownership and EOF rules.

See [regression tests](pipe_regression.md) for coroutine/thread O0/O2 suites,
stdio routing, rollback, reuse and single-command user context checks.
