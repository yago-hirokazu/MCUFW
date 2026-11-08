# Pipeline executor

## Bytecode and process setup

`exe_run_codes()` collects commands and arguments in `e_proc[]` in bytecode
order. Each command opcode takes the next slot. `OP_PIPE_NEXT` marks a
stage boundary, and `OP_CMD_EXE` executes the collected commands. `PROCS` must match `PIPELINE_MAX` (four stages). A single
command runs directly with its process set as current. Two to four commands
run through `exe_run_pipeline()`.

For each adjacent pair, the executor creates one anonymous pipe using a
temporary process and installs the endpoints at fixed descriptor numbers.

```text
process N stdout fd 1 -> pipe N write endpoint
process N+1 stdin fd 0 -> pipe N read endpoint
```

The temporary descriptors are closed after installation. If the read
endpoint cannot be installed, the executor also closes the write descriptor
it installed for that pair.

## Scheduling and cleanup

The executor initializes each process with `init_proc()` and prepares it with
`sched_prepare()`. It scans the stages in order, resuming runnable processes
until all stages finish. A pass with unfinished processes and no runnable
stage returns `-SYS_EINVAL`.

Windows supports `SCHED_BACKEND=cortn` (the default) and
`SCHED_BACKEND=thread`. The coroutine backend switches stacks on the calling
thread. The thread backend prepares Windows worker threads and runs them
under scheduler control. See [Windows coroutine](../coroutine/coroutine_windows.md).

Each command wrapper stores its return value in `p_exec.p_ret` and closes
its descriptors before finishing. Closing an endpoint wakes waiting peers.
Readers drain buffered data before EOF, and writers return `-SYS_EPIPE` when
the last reader closes. On success or failure, the executor closes remaining
connected descriptors and releases prepared backend slots.

The status of a completed pipeline is the final command's return value.
`OP_CMD_EXE` stores the result in `e_status`.

## Windows standard I/O

With `ESH_STDIO_PIPE`, the Windows stdio adapter routes mapped input and
output through the current process's fd 0 and fd 1. A stage without the
corresponding descriptor uses host I/O. Stderr remains on the host, and
`scanf()` returns EOF for pipe input. See [Windows stdio](../stdio.md).

The pipeline executor does not implement redirection, background jobs or
signal delivery for broken pipes. The `pipeline_exec` tests exercise both
Windows backends with `-O0` and `-O2`.
