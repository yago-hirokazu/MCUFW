
/* dwt.h */

#ifndef __DWT_H__
#define __DWT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

void dwtstart(void);
void dwtstop(void);
void dwtreset(void);
uint32_t dwtcount(void);
double dwtms(uint32_t scycnt, uint32_t ecycnt);
double dwtus(uint32_t scycnt, uint32_t ecycnt);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
