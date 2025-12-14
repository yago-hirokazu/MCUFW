
/* ctrace.h */

#ifndef __CTRACE_H__
#define __CTRACE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "ctrace_adapt.h"

void ctrinit(void);
void ctrterm(void);
void ctrstart(void);
uint32_t ctrstop(void);
void ctraceint(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
