/* sched_cortn.c */

#include "cortn.h"
#include "esh_errno.h"
#include "esh_sched_backend.h"

static unsigned char esh_lock_held;

void esh_sched_backend_lock(void)
{
	/* Coroutine cannot be preempted while scheduler is not running */
	esh_lock_held = 1;
}

void esh_sched_backend_unlock(void)
{
	esh_lock_held = 0;
}

int esh_sched_backend_is_locked(void)
{
	return esh_lock_held != 0;
}

int esh_sched_backend_yield(struct esh_proc *proc,
			    enum esh_wait_reason reason)
{
	struct cortn *cortn;
	enum cortn_yield_reason yield_reason;

	cortn = proc->ep_backend;
	if (reason == ESH_WAIT_READ)
		yield_reason = CORTN_YIELD_WAIT_READ;
	else if (reason == ESH_WAIT_WRITE)
		yield_reason = CORTN_YIELD_WAIT_WRITE;
	else
		return -ESH_EINVAL;
	if (cortn_yield(cortn, yield_reason) != CORTN_SUCCESS)
		return -ESH_EINVAL;
	return 0;
}

int esh_sched_backend_wake(struct esh_proc *proc)
{
	struct cortn *cortn;

	cortn = proc->ep_backend;
	if (cortn_wake(cortn) != CORTN_SUCCESS)
		return -ESH_EINVAL;
	return 0;
}

enum esh_proc_state esh_sched_backend_resume(struct esh_proc *proc)
{
	struct cortn *cortn;

	cortn = proc->ep_backend;
	if (cortn_resume(cortn) != CORTN_SUCCESS)
		return ESH_PROC_ERROR;
	switch (cortn->c_state) {
	case CORTN_RUNNABLE:
		return ESH_PROC_RUNNABLE;
	case CORTN_WAIT_READ:
		return ESH_PROC_WAIT_READ;
	case CORTN_WAIT_WRITE:
		return ESH_PROC_WAIT_WRITE;
	case CORTN_FINISHED:
		return ESH_PROC_FINISHED;
	default:
		return ESH_PROC_ERROR;
	}
}
