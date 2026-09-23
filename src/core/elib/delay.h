
/* delay.h */

#ifndef __DELAY_H__
#define __DELAY_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "delay_adapt.h"

void dlyinit(void);
void msec(DELAY_UWORD ms);
void usec(DELAY_UWORD us);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
