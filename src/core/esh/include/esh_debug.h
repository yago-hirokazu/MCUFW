
/* esh_debug.h */

#ifndef __ESH_DEBUG_H__
#define __ESH_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define	DEBUG_TTY	0	/* Enable debug print on tty.c */
#define	DEBUG_TTY_A	0	/* Enable debug print on tty_adapt.c */
#define	DEBUG_STDIO	0	/* Enable debug print on stdio_adapt.c */
#define	DEBUG_SHELL	0	/* Enable debug print on run.c */
#define DEBUG_LEX	0	/* Enable debug print on slex.c */
#define DEBUG_PAR	1	/* Enable debug print on par.c */
#define DEBUG_EXE	0	/* Enable debug print on exe.c */
#define DEBUG_TEST	0	/* Enable debug print on test.c */
#define DEBUG_EXPR	0	/* Enable debug pinrt on expr.c */
#define DEBUG_BREAK	0	/* Enable debug print on break.c */
#define	ENABLE_ASSERT	1	/* Eanble shell assert */


#if ENABLE_ASSERT
#include <assert.h>
#define	ESHASSERT(_a_)	assert(_a_)
#else
#define ESHASSERT(_a_)
#endif


extern char * const	token_type_str[];
extern char * const	lex_status_str[];
extern char * const	lex_state_str[];
extern char * const	par_status_str[];
extern char * const	par_state1_str[];
extern char * const	par_state2_str[];
extern char * const	opcode_str[];


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
