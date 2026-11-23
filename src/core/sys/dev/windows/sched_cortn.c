/*
 * Scheduler => prepared slot => coroutine init / resume / yield / wake
 * Backend handle => first slot member => release slot for reuse
 */

#include <string.h>

#include "cortn.h"
#include "sched_adapt.h"

struct sched_cortn_slot {
	struct cortn c_cortn;
	void (*c_entry)(void *arg);
	void *c_arg;
	unsigned char c_stack[CORTN_WINDOWS_STACK_SIZE]
		__attribute__((aligned(16)));
	unsigned char c_used;
};

static unsigned char sys_lock_owned;
static struct proc *current;
static struct sched_cortn_slot cortn_slots[PIPELINE_MAX];

static void sched_cortn_entry(struct cortn *cortn, void *arg);

struct proc *sched_adapt_current(void)
{
	return current;
}

void sched_adapt_set_current(struct proc *proc)
{
	current = proc;
}

void sched_adapt_lock(void)
{
	/* Coroutine cannot be preempted while scheduler is not running */
	sys_lock_owned = 1;
}

void sched_adapt_unlock(void)
{
	sys_lock_owned = 0;
}

int sched_adapt_is_locked(void)
{
	return sys_lock_owned != 0;
}

/* Prepare coroutine in available slot */
void *sched_adapt_prepare(void (*entry)(void *arg), void *arg)
{
	struct sched_cortn_slot *slot;
	int i;

	for (i = 0; i < PIPELINE_MAX; i++) {
		slot = &cortn_slots[i];
		if (slot->c_used != 0)
			continue;
		memset(slot, 0, sizeof(*slot));
		slot->c_used = 1;
		slot->c_entry = entry;
		slot->c_arg = arg;
		init_cortn(&slot->c_cortn, slot->c_stack,
			   sizeof(slot->c_stack), sched_cortn_entry, slot);
		return &slot->c_cortn;
	}
	return NULL;
}

/* Release slot owning prepared coroutine handle */
void sched_adapt_release(void *backend)
{
	struct sched_cortn_slot *slot;

	/* Coroutine is first slot member */
	slot = backend;
	memset(slot, 0, sizeof(*slot));
}

/* Yield running coroutine and restore sys lock on resume */
int sched_adapt_yield(struct proc *proc, enum wait_reason reason)
{
	struct cortn *cortn;
	enum cortn_yield_reason yield_reason;

	cortn = proc->p_ctx;
	if (reason == WAIT_LOCK)
		yield_reason = CORTN_YIELD_WAIT_LOCK;
	else if (reason == WAIT_READ)
		yield_reason = CORTN_YIELD_WAIT_READ;
	else
		yield_reason = CORTN_YIELD_WAIT_WRITE;
	sys_unlock();
	cortn_yield(cortn, yield_reason);
	sys_lock();
	return 0;
}

/* Mark waiting coroutine runnable */
int sched_adapt_wake(struct proc *proc)
{
	struct cortn *cortn;

	cortn = proc->p_ctx;
	cortn_wake(cortn);
	return 0;
}

enum proc_state sched_adapt_resume(struct proc *proc)
{
	struct cortn *cortn;

	cortn = proc->p_ctx;
	if (cortn_resume(cortn) != CORTN_SUCCESS)
		return PROC_ERROR;
	switch (cortn->c_state) {
	case CORTN_RUNNABLE:
		return PROC_RUNNABLE;
	case CORTN_WAIT_LOCK:
		return PROC_WAIT_LOCK;
	case CORTN_WAIT_READ:
		return PROC_WAIT_READ;
	case CORTN_WAIT_WRITE:
		return PROC_WAIT_WRITE;
	case CORTN_FINISHED:
		return PROC_FINISHED;
	default:
		return PROC_ERROR;
	}
}

static void sched_cortn_entry(struct cortn *cortn, void *arg)
{
	struct sched_cortn_slot *slot;

	(void)cortn;
	slot = arg;
	slot->c_entry(slot->c_arg);
}
