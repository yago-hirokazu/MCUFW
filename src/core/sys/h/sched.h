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
struct wait_chan;

/* Portable execution state embedded in executor process */
struct proc {
	enum proc_state p_state;
	struct file *p_ofile[NOFILE];
	struct wait_chan *p_wait_chan;
	struct proc *p_wait_next;
	void *p_backend;
	int p_status;
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
void proc_init(struct proc *proc);
int proc_bind_backend(struct proc *proc, void *backend);
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
