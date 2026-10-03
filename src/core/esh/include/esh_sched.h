/* esh_sched.h */

#ifndef __ESH_SCHED_H__
#define __ESH_SCHED_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esh_file.h"

#define ESH_PIPELINE_MAX	4

enum esh_proc_state {
	ESH_PROC_NEW,
	ESH_PROC_RUNNABLE,
	ESH_PROC_RUNNING,
	ESH_PROC_WAIT_READ,
	ESH_PROC_WAIT_WRITE,
	ESH_PROC_FINISHED,
	ESH_PROC_ERROR
};

enum esh_wait_reason {
	ESH_WAIT_READ,
	ESH_WAIT_WRITE
};

struct esh_wait_chan;
/* Portable execution state embedded in executor process */
struct esh_proc {
	enum esh_proc_state ep_state;
	struct esh_fd_table ep_fd_table;
	struct esh_wait_chan *ep_wait_chan;
	struct esh_proc *ep_wait_next;
	void *ep_backend;
	int ep_status;
};

/* Wait list ownership belongs to scheduler backend */
struct esh_wait_chan {
	struct esh_proc *wc_head;
	struct esh_proc *wc_tail;
};

/* Non-recursive kernel lock protects pipe predicates and wait lists */
/* Release joins backend outside lock; close fds before releasing process */
void esh_kernel_lock(void);
void esh_kernel_unlock(void);
int esh_kernel_is_locked(void);
int esh_sleep(void *chan, enum esh_wait_reason reason);
void esh_wakeup(void *chan);
struct esh_proc *esh_current_proc(void);
void esh_proc_init(struct esh_proc *proc);
int esh_proc_bind_backend(struct esh_proc *proc, void *backend);
int esh_sched_prepare(struct esh_proc *proc, void (*entry)(void *arg),
		      void *arg);
void esh_sched_release(struct esh_proc *proc);
int esh_sched_resume(struct esh_proc *proc);
void esh_sched_set_current_proc(struct esh_proc *proc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
