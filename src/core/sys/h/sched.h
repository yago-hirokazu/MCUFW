/* sched.h */

#ifndef __SCHED_H__
#define __SCHED_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "param.h"

enum proc_state {
	PROC_NEW,
	PROC_RUNNABLE,
	PROC_RUNNING,
	PROC_WAIT_LOCK,
	PROC_WAIT_READ,
	PROC_WAIT_WRITE,
	PROC_FINISHED,
	PROC_ERROR
};

enum wait_reason {
	WAIT_LOCK,
	WAIT_READ,
	WAIT_WRITE
};

struct file;
struct proc_wait;

/* Portable execution state embedded in executor process */
struct proc {
	enum proc_state p_state;
	struct file *p_ofile[NOFILE];
	struct proc_wait *p_wchan;
	struct proc *p_link;
	void *p_ctx;
	int p_ret;
};

/* Channel storage belongs to waiting object; scheduler manages list */
struct proc_wait {
	struct proc *w_head;
	struct proc *w_tail;
};

/* Non-recursive kernel lock protects pipes, cache, queues and wait lists */
/* Release joins backend outside lock; close fds before releasing process */
void kernel_lock(void);
void kernel_unlock(void);
int kernel_is_locked(void);
int sched_sleep(struct proc_wait *chan, enum wait_reason reason);
void sched_wakeup(struct proc_wait *chan);
struct proc *current_proc(void);
/* Initialize process before descriptors or backend setup */
void pinit(struct proc *proc);
int sched_bind_proc_to_coroutine(struct proc *proc, void *backend);
int sched_prepare(struct proc *proc, void (*entry)(void *arg), void *arg);
void sched_release(struct proc *proc);
int sched_resume(struct proc *proc);
void sched_set_current_proc(struct proc *proc);
#ifdef SCHED_TEST
void sched_test_fail_next_yield(void);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
