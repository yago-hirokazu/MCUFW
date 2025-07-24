
/* shdebug.h */

#ifndef __SHDEBUG_H__
#define __SHDEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define	DEBUG_TTY		0	/* Enable debug print on tty.c */
#define	DEBUG_TTY_ADAPT		0	/* Enable debug print on tty_adapt.c */
#define	DEBUG_STDIO_ADAPT	0	/* Enable debug print on stdio_adapt.c */
#define	DEBUG_SHELL		0	/* Enable debug print on run.c */
#define	DEBUG_CLEX		0	/* Enable debug print on clex.c */
#define DEBUG_SLEX		0	/* Enable debug print on slex.c */
#define DEBUG_PAR		0	/* Enable debug print on par.c */
#define DEBUG_EXE		0	/* Enable debug print on exe.c */
#define DEBUG_TEST		0	/* Enable debug print on test.c */
#define DEBUG_EXPR		0	/* Enable debug pinrt on expr.c */
#define	ENABLE_SHASSERT		1	/* Eanble shell assert */

#if ENABLE_SHASSERT
#include <assert.h>
#define	SHASSERT(_a_)		assert(_a_)
#else
#define SHASSERT(_a_)
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
