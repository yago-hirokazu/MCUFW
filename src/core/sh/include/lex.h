
/* lex.h */

#ifndef __LEX_H__
#define __LEX_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "tok.h"

#define LEXDONE		1	/* lexical analysis done*/
#define LEXONGO		0	/* lexical analysis ongoing */
#define LEXERROR	-1	/* lexical analysis error */

void	slex_init(void);
int	slex_make_token(char *str, struct token *token);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
