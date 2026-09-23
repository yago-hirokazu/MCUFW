
/* code.h */

#ifndef __CODE_H__
#define __CODE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "esh.h"

/*
 * coding with bytecodes
 * ---------------------
 * + applets:
 *   - Do searching symbol from applet table while counting.
 *   - Then put the count value to code.
 *   - Use the count value at execution phase to do
 *     direct access to applet tables.
 * + string literal
 *   - Log string literals to string table
 */

enum opcode {
	OP_NOP = 0x80,
	OP_USER_CMD = 0x81,
	OP_BLTN_CMD = 0x82,
	OP_CMD_ARG,
	OP_CMD_VARG,
	OP_CMD_EXE,
	OP_CMD_EXE_THEN_STACK,
	OP_CMD_EXE_THEN_ASSIGN,
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	OP_PUSH_SYMBOL,
	OP_POP_SYMBOL_THEN_ASSIGN,
	OP_BREAK_LOOP
};

struct variable_table {
	char v_var[SYMBOLS * SYMCHARS];
	char v_val[SYMBOLS * SYMCHARS];
	char v_num;
};

struct label_table {
	int l_addr[LABELS];
	char l_num;
};

struct string_table {
	char s_str[SYMBOLS * SYMCHARS];
	char s_num;
};

struct code {
	int c_code[CODES];		/* Byte codes */
	int c_codes;			/* # of byte codes */
	struct variable_table c_vtab;	/* Table of variables */
	struct label_table c_ltab;	/* Table of labels */
	struct string_table c_stab;	/* Table of strings */
};

void code_init(struct code *code);

/* TODO: make debugger */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
