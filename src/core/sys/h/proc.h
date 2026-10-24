/*
 * sched.h / executor => proc => p_addr => dedicated user area
 * p_wchan / p_link => scheduler wait list
 * p_xctx => backend execution context
 */

#ifndef __PROC_H__
#define __PROC_H__

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

struct user;
struct wait_chan;

/* Portable execution state embedded in executor process */
struct proc {
	enum proc_state p_stat;
	struct user *p_addr;	/* Dedicated user area owned by caller */
	struct wait_chan *p_wchan; /* Awaited event */
	struct proc *p_link;	/* Next process in scheduler wait list */
	void *p_xctx;		/* eXecution ConTeXt owned by backend */
	int p_ret;		/* Command function return value */
};

#endif
