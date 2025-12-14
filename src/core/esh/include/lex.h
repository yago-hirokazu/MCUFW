
/* lex.h */

#ifndef __LEX_H__
#define __LEX_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "tok.h"

#define LEXERROR	0	/* lexical analysis error */
#define LEXONGO		1	/* lexical analysis ongoing */
#define LEXDONE		2	/* lexical analysis done */
#define LEXQUOTE	3	/* lexical analysis processing quote */

void	lex_init(void);
int	lex_analyze_token(char *str, struct token *token);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
