/* sched_cortn.c */

#include <string.h>

#include "cortn.h"
#include "sched_backend.h"
#include "sys_error.h"

struct sched_cortn_slot {
	struct cortn cs_cortn;
	void (*cs_entry)(void *arg);
	void *cs_arg;
	unsigned char cs_stack[CORTN_WINDOWS_STACK_SIZE]
		__attribute__((aligned(16)));
	unsigned char cs_used;
};

static unsigned char kernel_lock_held;
static struct proc *current;
static struct sched_cortn_slot cortn_slots[PIPELINE_MAX];

static void sched_cortn_entry(struct cortn *cortn, void *arg);

struct proc *sched_backend_current(void)
{
	return current;
}

void sched_backend_set_current(struct proc *proc)
{
	current = proc;
}

void sched_backend_lock(void)
{
	/* Coroutine cannot be preempted while scheduler is not running */
	kernel_lock_held = 1;
}

void sched_backend_unlock(void)
{
	kernel_lock_held = 0;
}

int sched_backend_is_locked(void)
{
	return kernel_lock_held != 0;
}

void *sched_backend_prepare(void (*entry)(void *arg), void *arg)
{
	struct sched_cortn_slot *slot;
	int i;

	if (!entry)
		return NULL;
	for (i = 0; i < PIPELINE_MAX; i++) {
		slot = &cortn_slots[i];
		if (slot->cs_used)
			continue;
		memset(slot, 0, sizeof(*slot));
		slot->cs_used = 1;
		slot->cs_entry = entry;
		slot->cs_arg = arg;
		if (cortn_init(&slot->cs_cortn, slot->cs_stack,
			       sizeof(slot->cs_stack), sched_cortn_entry,
			       slot) != CORTN_SUCCESS) {
			memset(slot, 0, sizeof(*slot));
			return NULL;
		}
		return &slot->cs_cortn;
	}
	return NULL;
}

void sched_backend_release(void *backend)
{
	struct sched_cortn_slot *slot;
	int i;

	if (!backend)
		return;
	for (i = 0; i < PIPELINE_MAX; i++) {
		slot = &cortn_slots[i];
		if (&slot->cs_cortn != backend)
			continue;
		memset(slot, 0, sizeof(*slot));
		return;
	}
}

int sched_backend_yield(struct proc *proc, enum wait_reason reason)
{
	struct cortn *cortn;
	enum cortn_yield_reason yield_reason;

	cortn = proc->p_backend;
	if (reason == WAIT_LOCK)
		yield_reason = CORTN_YIELD_WAIT_LOCK;
	else if (reason == WAIT_READ)
		yield_reason = CORTN_YIELD_WAIT_READ;
	else if (reason == WAIT_WRITE)
		yield_reason = CORTN_YIELD_WAIT_WRITE;
	else
		return -SYS_EINVAL;
	kernel_unlock();
	if (cortn_yield(cortn, yield_reason) != CORTN_SUCCESS) {
		kernel_lock();
		return -SYS_EINVAL;
	}
	kernel_lock();
	return 0;
}

int sched_backend_wake(struct proc *proc)
{
	struct cortn *cortn;

	cortn = proc->p_backend;
	if (cortn_wake(cortn) != CORTN_SUCCESS)
		return -SYS_EINVAL;
	return 0;
}

enum proc_state sched_backend_resume(struct proc *proc)
{
	struct cortn *cortn;

	cortn = proc->p_backend;
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
	slot->cs_entry(slot->cs_arg);
}
