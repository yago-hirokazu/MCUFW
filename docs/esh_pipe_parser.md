# ESH pipeline parser

## Syntax

Milestone 5 adds pipeline syntax to lexer and parser. Whitespace around `|` is
optional.

```sh
producer | consumer
producer|filter|consumer
```

`|` inside single or double quotes remains argument text. Pipeline cannot
start or end with `|`, and adjacent `|` operators are rejected.

## Tokenization

`tokenize()` inserts token boundaries around unquoted `|` characters before
splitting input line. Quote state is tracked during this pass so quoted pipe
characters are not changed. Expansion uses existing fixed `TTY_NBUF` storage;
input is rejected when inserted boundaries would exceed buffer.

Lexer returns `T_PIPE` after command or argument and requires another command
after it. Parser also validates this requirement to keep syntax checks
independent from lexer state.

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
Executor handling is deferred to Milestone 6. Existing non-pipeline bytecode
is unchanged.
