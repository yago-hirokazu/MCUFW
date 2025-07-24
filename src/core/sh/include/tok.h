
/* tok.h */

#ifndef __TOK_H__
#define __TOK_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "esh_adapt.h"

enum	token_type {
	T_UNDEF=0,
	T_USER_CMD,	/* User command */
	T_BLTN_CMD,     /* Builtin command */
	T_CMD_ARG,	/* Command argument */
	T_CMD_VARG,	/* Command variable argument */
	T_VALUE,	/* Ineger value */
	T_SYMBOL,	/* Keyword, Variable, Label */
	T_ASSIGN,	/* = */
	T_SEMICOLON,	/* ; */
	T_LBRACKET,	/* [ */
	T_RBRACKET,	/* ] */
	T_LBACKQUOTE,	/* ` */
	T_RBACKQUOTE,	/* ` */
	T_WHILE,	/* while */
	T_DO,		/* do */
	T_DONE,		/* done */
	T_EOL,		/* End of line */
	T_COMMENT,	/* Comment */
	T_UNKNOWN
};

struct	token {
	enum token_type t_type;
	char t_symbol[SYMCHARS];
};

extern	char * const token_type_str[];

void	tok_init(struct token *token);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
