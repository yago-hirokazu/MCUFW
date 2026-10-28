/* sched_stub.h */

#ifndef __SCHED_STUB_H__
#define __SCHED_STUB_H__

#include "sched.h"

void sched_stub_reset(struct proc *proc);
void sched_stub_set_sleep_result(int result);
void sched_stub_set_sleep_hook(void (*hook)(struct wait_channel *chan,
					    enum wait_reason reason));
int sched_stub_sleep_count(void);
enum wait_reason sched_stub_last_reason(void);
int sched_stub_wakeup_count(struct wait_channel *chan);

#endif
