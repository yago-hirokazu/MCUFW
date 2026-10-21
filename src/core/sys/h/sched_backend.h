/* sched_backend.h */

#ifndef __SCHED_BACKEND_H__
#define __SCHED_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "sched.h"

void sched_backend_lock(void);
void sched_backend_unlock(void);
int sched_backend_is_locked(void);
struct proc *sched_backend_current(void);
struct user *sched_backend_base_user(void);
void sched_backend_set_current(struct proc *proc);
void *sched_backend_prepare(void (*entry)(void *arg), void *arg);
void sched_backend_release(void *backend);
/* Yield enters and returns with kernel lock held */
int sched_backend_yield(struct proc *proc, enum wait_reason reason);
int sched_backend_wake(struct proc *proc);
enum proc_state sched_backend_resume(struct proc *proc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
