# Milestone 7: Windows stdio connection

Windows builds enable process stdio routing with `ESH_STDIO_PIPE`.
Parser-only and quote-only test builds retain console-only adapters.

- `printf`, `putchar`, and `fprintf(stdout, ...)` write current
  process stdout pipe when descriptor 1 owns pipe endpoint
- `getchar` reads current process stdin pipe and returns EOF on
  end of stream or read error
- Missing standard pipe endpoint delegates to existing Windows console I/O
- `fprintf(stderr, ...)` and shell debug/error macros use console
- `scanf` remains console-only; pipe stdin returns EOF without consuming
  data. Pipe commands use `esh_read` or `getchar`
- Include `esh_file.h` for public read/write/close APIs. These APIs
  currently accept pipe descriptors only
- Direct host `fwrite`, `fputs`, `puts`, `vprintf`, and stdio FILE
  buffering are outside adapter compatibility. Applets must use mapped
  printf/putchar/fprintf APIs or public pipe I/O

Windows formatted pipe output measures length with vsnprintf, allocates
per-call host buffer, then writes complete bytes through blocking I/O.
Buffer belongs to coroutine stack frame and is freed after write completes,
including broken-pipe error. This host-only allocation does not add dynamic
allocation to portable pipe core; MCU formatter is separate porting work.

Windows coroutine stack grows from 16 KiB to 64 KiB for CRT formatting.
16 KiB crashed in context return during formatted-output integration test;
64 KiB passes same test. Fixed four-process pool therefore reserves
256 KiB stack storage on Windows. MCU stack sizing remains separate work.

Tests cover existing echo producer, getchar EOF, formatted output larger
than pipe capacity, putchar, fprintf stdout, stderr exclusion, scanf
rejection without consumption, resource cleanup, and original executor
pipeline tests at O0 and O2.

## Verification (2026-10-03)

- Pipeline executor integration: O0 and O2 pass
- O2 with DEBUG_STDIO=1: pass; debug logs excluded from consumer bytes
- Existing executor cases 1 through 10: golden output matches
- Existing parser run_test1.sh and run_test2.sh: test pass
- Quote test: successful build and execution
- Windows main.exe: successful build and link through MSYS2 shell
- Console sam1, sam2 1 10 100, sam3 -a -b string: expected output
- Whitespace check and new stdio/test source 80-column check: pass

Existing compiler warnings in unrelated parser/tty code remain.
