
/* async.h */

#ifndef __RSRVU_H__
#define __RSRVU_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "rsrvu_adapt.h"


void rsrvuinit(void);
void rsrvu(uint32_t us, void (*proc)(void));
void rsrvustop(void);
void rsrvuint(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
