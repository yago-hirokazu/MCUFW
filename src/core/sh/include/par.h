
/* par.h */

#ifndef __PAR_H__
#define __PAR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "tok.h"
#include "code.h"

#define PARINVLD	-1
#define PARONGO		0
#define PARDONE		1

char par_make_code(struct token *tok, struct code *code);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
