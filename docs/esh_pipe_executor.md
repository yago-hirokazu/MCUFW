# ESH pipeline executor

## Scope

Milestone 6 connects `OP_PIPE_NEXT` bytecode to process allocation, anonymous
pipe setup, coroutine scheduling, command completion, and resource cleanup.
Console stdio routing remains Milestone 7 work. Pipeline test applets use
`esh_read()` and `esh_write()` directly.

## Process and descriptor mapping

Executor assigns commands to `e_proc[]` in bytecode order. `PROCS` and
`ESH_PIPELINE_MAX` both limit pipeline to four commands. For each adjacent
pair, executor creates one anonymous pipe and installs endpoints at fixed
descriptor numbers.

```text
process N stdout fd 1 -> pipe N write endpoint
process N+1 stdin fd 0 -> pipe N read endpoint
```

Temporary construction descriptors are closed after endpoint installation.
Each process owns only descriptors needed by its position in pipeline.

## Scheduling

Portable scheduler prepares one execution backend for each process and resumes
runnable processes in round-robin order. Windows coroutine backend owns fixed
four-slot coroutine and stack pool. No core allocation is dynamic.

Command return stores status in `ep_status`. Command wrapper then closes every
remaining process descriptor before coroutine finishes. Endpoint close wakes
blocked peers, producing EOF after final writer close or broken pipe after
final reader close.

Scheduler reports error when processes remain unfinished and no process is
runnable. Cleanup closes all descriptors and releases every prepared backend
slot on both success and failure paths.

## Pipeline status

Pipeline status is final command status. Earlier command errors remain in each
process `ep_status` and do not replace final status. Executor stores resulting
status in `e_status`.

## Deliberate omissions

- console stdin and stdout routing
- redirection and background jobs
- signal delivery for broken pipe
- thread backend process creation

These do not alter current anonymous pipe ownership and EOF rules.
