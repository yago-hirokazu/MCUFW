/* sched_adapt.h */

#ifndef __SCHED_ADAPT_H__
#define __SCHED_ADAPT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "sched.h"

void sched_adapt_lock(void);
void sched_adapt_unlock(void);
int sched_adapt_is_locked(void);
struct proc *sched_adapt_current(void);
void sched_adapt_set_current(struct proc *proc);
void *sched_adapt_prepare(void (*entry)(void *arg), void *arg);
void sched_adapt_release(void *backend);
/* Yield enters and returns with kernel lock held */
int sched_adapt_yield(struct proc *proc, enum wait_reason reason);
int sched_adapt_wake(struct proc *proc);
enum proc_state sched_adapt_resume(struct proc *proc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
