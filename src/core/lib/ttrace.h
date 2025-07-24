
/* ttrace.h */

#ifndef __TTRACE_H__
#define __TTRACE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "ttrace_adapt.h"

void ttrinit(void);
void ttrterm(void);
void ttrstart(void);
uint32_t ttrstop(void);
void ttraceint(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
