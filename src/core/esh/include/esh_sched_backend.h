/* esh_sched_backend.h */

#ifndef __ESH_SCHED_BACKEND_H__
#define __ESH_SCHED_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esh_sched.h"

void esh_sched_backend_lock(void);
void esh_sched_backend_unlock(void);
int esh_sched_backend_is_locked(void);
void *esh_sched_backend_prepare(esh_proc_entry_t entry, void *arg);
void esh_sched_backend_release(void *backend);
int esh_sched_backend_yield(struct esh_proc *proc,
			    enum esh_wait_reason reason);
int esh_sched_backend_wake(struct esh_proc *proc);
enum esh_proc_state esh_sched_backend_resume(struct esh_proc *proc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
