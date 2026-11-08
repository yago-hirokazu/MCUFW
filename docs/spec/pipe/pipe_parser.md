# pipeline parser

## Syntax

Whitespace around `|` is optional.

```sh
producer | consumer
producer|filter|consumer
```

`|` inside single or double quotes remains argument text. Pipeline cannot
start or end with `|`, and adjacent `|` operators are rejected.

## Tokenization

`par_make_code()` requires valid lexer tokens with terminated symbols and code
storage initialized by `init_code()`. Call `init_par()` before starting a parser
session and `init_lex()` before lexing each input line. NULL pointers, malformed token storage and corrupted table counters
are outside parser API contract and are not checked.

Code, symbol and label capacity limits and syntax checks remain enforced.
Label allocation failure is checked before writing label address table.
Caller must discard invalid bytecode and reset parser after `PARINVLD`.

`tokenize()` inserts token boundaries around unquoted `|` characters before
splitting input line. Quote state is tracked during this pass so quoted pipe
characters are not changed. Expansion uses existing fixed `TTY_NBUF` storage;
input is rejected when inserted boundaries would exceed buffer.

Lexer returns `T_PIPE` after command or argument and requires another command
after it. Parser tracks the same requirement in `p_pipe_command` and accepts a
registered user command or builtin for the next stage. A `break` command
followed by `|` is rejected.

## Bytecode

`OP_PIPE_NEXT` terminates each non-final command in pipeline. Final command
keeps existing execution opcode, normally `OP_CMD_EXE`.

```text
command 0
OP_PIPE_NEXT
command 1
OP_PIPE_NEXT
command 2
OP_CMD_EXE
```

Number of `OP_PIPE_NEXT` instructions plus one gives command count. Each
instruction connects preceding command stdout to following command stdin.
`exe_run_codes()` collects stages until `OP_CMD_EXE`, then runs multi-stage
commands through `exe_run_pipeline()`. Supported pipelines have up to four
stages. See [pipeline executor](pipe_executor.md).
