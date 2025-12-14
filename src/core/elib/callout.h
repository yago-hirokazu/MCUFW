
/* callout.h */

#ifndef __CALLOUT_H__
#define __CALLOUT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "callout_adapt.h"

struct callo
{
	int c_time;
	int c_arg;
	int c_rep;
	void (*c_func)(int arg);
};

extern struct callo callo[CALLOUTS];

extern void callout(void);
extern void later(void (*fun)(int arg), int arg, int tim);
extern void repeat(void (*fun)(int arg), int arg, int tim, int rep);
extern void nocall(void (*fun)(int arg));



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
