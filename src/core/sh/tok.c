
/* tok.c */

#include "tok.h"


char * const token_type_str[] = {
	"T_UNDEF",
	"T_USER_CMD",
	"T_BLTN_CMD",
	"T_CMD_ARG",
	"T_CMD_VARG",
	"T_VALUE",
	"T_SYMBOL",
	"T_ASSIGN",
	"T_SEMICOLON",
	"T_LBRACKET",
	"T_RBRACKET",
	"T_LBACKQUOTE",
	"T_RBACKQUOTE",
	"T_WHILE",
	"T_DO",
	"T_DONE",
	"T_EOL",
	"T_COMMENT",
	"T_UNKNOWN",
	NULL
};

void tok_init(struct token *token)
{
	token->t_type = T_UNDEF;
	memset(token->t_symbol, 0, SYMCHARS);
}

/* EOF */

