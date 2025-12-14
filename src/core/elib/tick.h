
/* tick.h */

#ifndef __TICK_H__
#define __TICK_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

int tickinit(void);
int tickstart(void);
int tickstop(void);
uint32_t tickget(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
