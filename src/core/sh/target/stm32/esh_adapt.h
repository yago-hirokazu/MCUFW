
/* esh_adapt.h */

#ifndef __ESH_ADAPT_H__
#define __ESH_ADAPT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

#define	USE_STACKMACH	0	/* Switch to use stack machine */

#define	SYMCHARS	16	/* Max # of characters of a symbol */
#define	SYMBOLS		16	/* Max # of symbols */
#define	LABCHARS	8	/* Max # of characters of a label */
#define	LABELS		16	/* Max # of labels */
#define	PARNESTS	2	/* Max # of nests */
#define	TOKENS		128	/* Max # of tokens */
#define	CODES		128	/* Size of byte codes */
#define	STACKS		16	/* Size of stacks */
#define	PROCS		4	/* # of processes */

typedef int INT;
typedef unsigned int UINT;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
