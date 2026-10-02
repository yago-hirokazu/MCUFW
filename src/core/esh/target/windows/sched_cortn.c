/* sched_cortn.c */

#include <string.h>

#include "cortn.h"
#include "esh_errno.h"
#include "esh_sched_backend.h"

static unsigned char esh_lock_held;
static struct esh_proc *esh_current;

struct esh_proc *esh_sched_backend_current(void)
{
	return esh_current;
}

void esh_sched_backend_set_current(struct esh_proc *proc)
{
	esh_current = proc;
}

struct esh_cortn_slot {
	struct cortn cs_cortn;
	esh_proc_entry_t cs_entry;
	void *cs_arg;
	unsigned char cs_stack[CORTN_WINDOWS_STACK_SIZE]
		__attribute__((aligned(16)));
	unsigned char cs_used;
};

static struct esh_cortn_slot esh_cortn_slots[ESH_PIPELINE_MAX];

static void esh_cortn_entry(struct cortn *cortn, void *arg)
{
	struct esh_cortn_slot *slot;

	(void)cortn;
	slot = arg;
	slot->cs_entry(slot->cs_arg);
}

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

void *esh_sched_backend_prepare(esh_proc_entry_t entry, void *arg)
{
	struct esh_cortn_slot *slot;
	int i;

	if (!entry)
		return NULL;
	for (i = 0; i < ESH_PIPELINE_MAX; i++) {
		slot = &esh_cortn_slots[i];
		if (slot->cs_used)
			continue;
		memset(slot, 0, sizeof(*slot));
		slot->cs_used = 1;
		slot->cs_entry = entry;
		slot->cs_arg = arg;
		if (cortn_init(&slot->cs_cortn, slot->cs_stack,
			       sizeof(slot->cs_stack), esh_cortn_entry,
			       slot) != CORTN_SUCCESS) {
			memset(slot, 0, sizeof(*slot));
			return NULL;
		}
		return &slot->cs_cortn;
	}
	return NULL;
}

void esh_sched_backend_release(void *backend)
{
	struct esh_cortn_slot *slot;
	int i;

	if (!backend)
		return;
	for (i = 0; i < ESH_PIPELINE_MAX; i++) {
		slot = &esh_cortn_slots[i];
		if (&slot->cs_cortn != backend)
			continue;
		memset(slot, 0, sizeof(*slot));
		return;
	}
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
	esh_kernel_unlock();
	if (cortn_yield(cortn, yield_reason) != CORTN_SUCCESS) {
		esh_kernel_lock();
		return -ESH_EINVAL;
	}
	esh_kernel_lock();
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
