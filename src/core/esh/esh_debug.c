
/* esh_debug.c */

#include "machdep.h"
#include "esh_debug.h"

char * const token_type_str[] = {
	"T_UNDEF",
	"T_USER_CMD",
	"T_BLTN_CMD",
	"T_CMD_ARG",
	"T_CMD_VARG",
	"T_VALUE",
	"T_STRING",
	"T_VARIABLE",
	"T_ASSIGN",
	"T_SEMICOLON",
	"T_LBRACKET",
	"T_RBRACKET",
	"T_LBACKQUOTE",
	"T_RBACKQUOTE",
	"T_WHILE",
	"T_DO",
	"T_DONE",
	"T_IF",
	"T_THEN",
	"T_ELIF",
	"T_ELSE",
	"T_FI",
	"T_BREAK",
	"T_EOL",
	"T_COMMENT",
	"T_UNKNOWN",
	NULL
};

char * const lex_status_str[] = {
	"LEXERROR",
	"LEXONGO",
	"LEXDONE",
	"LEXQUOTE",
	NULL
};

char * const lex_state_str[] = {
	"LEX_IDLE",
	"LEX_CMD",
	"LEX_CMD_ARG",
	"LEX_SQUOTE",
	NULL
};

char * const par_status_str[] = {
	"PARINVLD",
	"PARONGO",
	"PARDONE",
	NULL
};

char * const par_state1_str[] = {
	"PAR_HALT",
	"PAR_IDLE",
	"PAR_CMDLINE",
	"PAR_ASSIGN",
	"PAR_WHILE",
	"PAR_IF",
	NULL
};

char * const par_state2_str[] = {
	"PAR_UNDEF",
	"PAR_CMDLINE_ARG",
	"PAR_ASSIGN_EQUAL",
	"PAR_ASSIGN_SYMBOL",
	"PAR_ASSIGN_CMDRESULT",
	"PAR_WHILE_LBRACKET",
	"PAR_WHILE_CONDITION",
	"PAR_WHILE_EOL",
	"PAR_WHILE_DO",
	"PAR_WHILE_BLOCK",
	"PAR_WHILE_DONE",
	"PAR_IF_LBRACKET",
	"PAR_IF_CONDITION",
	"PAR_IF_EOL",
	"PAR_IF_THEN",
	"PAR_IF_BLOCK",
	NULL
};

char * const opcode_str[] = {
	"OP_NOP",
	"OP_USER_CMD",
	"OP_BLTN_CMD",
	"OP_CMD_ARG",
	"OP_CMD_VARG",
	"OP_CMD_EXE",
	"OP_CMD_EXE_THEN_STACK",
	"OP_CMD_EXE_THEN_ASSIGN",
	"OP_JUMP_IF_FALSE",
	"OP_JUMP",
	"OP_PUSH_SYMBOL",
	"OP_POP_SYMBOL_THEN_ASSIGN",
	"OP_BREAK_LOOP",
	NULL
};

/* EOF */

