/* sched.h */

#ifndef __SCHED_H__
#define __SCHED_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "param.h"
#include "proc.h"

enum wait_reason {
	WAIT_LOCK,
	WAIT_READ,
	WAIT_WRITE
};

/* Channel storage belongs to waiting object; scheduler manages list */
struct wait_chan {
	struct proc *wc_head;
	struct proc *wc_tail;
};

/* Non-recursive kernel lock protects pipes, cache, queues and wait lists */
/* Release joins backend outside lock; close fds before releasing process */
void kernel_lock(void);
void kernel_unlock(void);
int kernel_is_locked(void);
int sched_sleep(struct wait_chan *chan, enum wait_reason reason);
void sched_wakeup(struct wait_chan *chan);
struct proc *current_proc(void);
/* User storage must outlive proc, including backend unwind and join */
void proc_init(struct proc *proc, struct user *user);
/* Bind and prepare enter without kernel lock */
int proc_bind_backend(struct proc *proc, void *backend);
int sched_prepare(struct proc *proc, void (*entry)(void *arg), void *arg);
void sched_release(struct proc *proc);
int sched_resume(struct proc *proc);
void sched_set_current_proc(struct proc *proc);
#ifdef SCHED_TEST
void sched_test_fail_next_yield(void);
/* One-shot hook runs with kernel lock held after backend report */
void sched_test_set_resume_hook(void (*hook)(struct proc *, enum proc_state *));
#ifdef SCHED_THREAD
void sched_test_fail_next_thread_wait(void);
#endif
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
