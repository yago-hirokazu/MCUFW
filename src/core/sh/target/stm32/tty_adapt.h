
/* tty_adapt.h */

#ifndef __TTY_ADAPT_H__
#define __TTY_ADAPT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "qbyte.h"

extern void cls(void);
extern void cll(void);
extern int scan_core(int c, struct qbyte *line_p);
extern int scan_normal_adapt(int *c, int *delim, struct qbyte *line_p);
extern void prompt(void);
extern void echo_right(void);
extern void echo_left(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
