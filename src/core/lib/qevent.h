
/* qevent.h */

#ifndef __QEVENT_H__
#define __QEVENT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "event.h"
#include "qbyte.h"

#define EVENTBUFS	8	// number of event queue buffer

extern struct qbyte tevq;

void qeventinit(void);
enum event gete(void);
void pute(enum event e);
void ipute(enum event e);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
