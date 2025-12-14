
/* tok.c */

#include "tok.h"

void tok_init(struct token *token)
{
	token->t_type = T_UNDEF;
	memset(token->t_symbol, 0, SYMCHARS);
}

/* EOF */

