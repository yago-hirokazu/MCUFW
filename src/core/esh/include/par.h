
/* par.h */

#ifndef __PAR_H__
#define __PAR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "tok.h"
#include "code.h"

#define PARINVLD	0
#define PARONGO		1
#define PARDONE		2

enum par_state1 {
	PAR_HALT = 0,
	PAR_IDLE,
	PAR_CMDLINE,
	PAR_ASSIGN,
	PAR_WHILE,
	PAR_IF,
};

enum par_state2 {
	PAR_UNDEF = 0,
	PAR_CMDLINE_ARG,
	PAR_ASSIGN_EQUAL,
	PAR_ASSIGN_SYMBOL,
	PAR_ASSIGN_CMDRESULT,
	PAR_WHILE_LBRACKET,
	PAR_WHILE_CONDITION,
	PAR_WHILE_EOL,
	PAR_WHILE_DO,
	PAR_WHILE_BLOCK,
	PAR_WHILE_DONE,
	PAR_IF_LBRACKET,
	PAR_IF_CONDITION,
	PAR_IF_EOL,
	PAR_IF_THEN,
	PAR_IF_BLOCK,
};

struct parse {
	enum par_state1 p_state1;
	enum par_state2 p_state2;
	char p_varid;
	char p_elses;
	struct label
	{
		int l_loop;
		int l_end;
		int l_else[ELSES];
	} p_label;
};

char par_make_code(struct token *tok, struct code *code);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
