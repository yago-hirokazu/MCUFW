/* sched.c */

#include <string.h>

#include "esh_errno.h"
#include "esh_sched.h"
#include "esh_sched_backend.h"

static void esh_wait_remove(struct esh_wait_chan *chan,
			    struct esh_proc *proc);

void esh_kernel_lock(void)
{
	esh_sched_backend_lock();
}

void esh_kernel_unlock(void)
{
	esh_sched_backend_unlock();
}

int esh_kernel_is_locked(void)
{
	return esh_sched_backend_is_locked();
}

int esh_sleep(void *channel, enum esh_wait_reason reason)
{
	struct esh_wait_chan *chan;
	struct esh_proc *proc;
	int result;

	chan = channel;
	proc = esh_current_proc();
	if (!chan || !proc || !proc->ep_backend || !esh_kernel_is_locked())
		return -ESH_EINVAL;
	if (proc->ep_wait_chan)
		return -ESH_EINVAL;

	if (reason == ESH_WAIT_READ) {
		proc->ep_state = ESH_PROC_WAIT_READ;
	} else if (reason == ESH_WAIT_WRITE) {
		proc->ep_state = ESH_PROC_WAIT_WRITE;
	} else {
		return -ESH_EINVAL;
	}

	proc->ep_wait_chan = chan;
	proc->ep_wait_next = NULL;
	if (chan->wc_tail)
		chan->wc_tail->ep_wait_next = proc;
	else
		chan->wc_head = proc;
	chan->wc_tail = proc;

	result = esh_sched_backend_yield(proc, reason);
	if (result != 0) {
		esh_wait_remove(chan, proc);
		proc->ep_state = ESH_PROC_ERROR;
		return -ESH_EINVAL;
	}
	proc->ep_state = ESH_PROC_RUNNING;
	return 0;
}

void esh_wakeup(void *channel)
{
	struct esh_wait_chan *chan;
	struct esh_proc *proc;
	struct esh_proc *next;

	chan = channel;
	if (!chan)
		return;
	proc = chan->wc_head;
	chan->wc_head = NULL;
	chan->wc_tail = NULL;
	while (proc) {
		next = proc->ep_wait_next;
		proc->ep_wait_chan = NULL;
		proc->ep_wait_next = NULL;
		if (proc->ep_backend && esh_sched_backend_wake(proc) == 0)
			proc->ep_state = ESH_PROC_RUNNABLE;
		else
			proc->ep_state = ESH_PROC_ERROR;
		proc = next;
	}
}

struct esh_proc *esh_current_proc(void)
{
	return esh_sched_backend_current();
}

void esh_proc_init(struct esh_proc *proc)
{
	if (!proc)
		return;
	memset(proc, 0, sizeof(*proc));
	proc->ep_state = ESH_PROC_NEW;
}

int esh_proc_bind_backend(struct esh_proc *proc, void *backend)
{
	if (!proc || !backend || proc->ep_state != ESH_PROC_NEW)
		return -ESH_EINVAL;
	proc->ep_backend = backend;
	proc->ep_state = ESH_PROC_RUNNABLE;
	return 0;
}

int esh_sched_prepare(struct esh_proc *proc, void (*entry)(void *arg),
		      void *arg)
{
	void *backend;

	if (!proc || !entry || proc->ep_state != ESH_PROC_NEW)
		return -ESH_EINVAL;
	backend = esh_sched_backend_prepare(entry, arg);
	if (!backend)
		return -ESH_ENOMEM;
	if (esh_proc_bind_backend(proc, backend) < 0) {
		esh_sched_backend_release(backend);
		return -ESH_EINVAL;
	}
	return 0;
}

void esh_sched_release(struct esh_proc *proc)
{
	if (!proc || !proc->ep_backend)
		return;
	esh_kernel_lock();
	if (proc->ep_wait_chan)
		esh_wait_remove(proc->ep_wait_chan, proc);
	esh_kernel_unlock();
	esh_sched_backend_release(proc->ep_backend);
	proc->ep_backend = NULL;
}

int esh_sched_resume(struct esh_proc *proc)
{
	int result;

	if (!proc || !proc->ep_backend || esh_current_proc())
		return -ESH_EINVAL;
	esh_kernel_lock();
	if (proc->ep_state != ESH_PROC_RUNNABLE) {
		esh_kernel_unlock();
		return -ESH_EINVAL;
	}
	esh_sched_set_current_proc(proc);
	proc->ep_state = ESH_PROC_RUNNING;
	esh_kernel_unlock();
	result = esh_sched_backend_resume(proc);
	esh_sched_set_current_proc(NULL);
	if (result == ESH_PROC_ERROR) {
		esh_kernel_lock();
		proc->ep_state = ESH_PROC_ERROR;
		esh_kernel_unlock();
		return -ESH_EINVAL;
	}
	/* Keep wakeup published after backend reported wait state */
	esh_kernel_lock();
	if (result != ESH_PROC_WAIT_READ && result != ESH_PROC_WAIT_WRITE)
		proc->ep_state = (enum esh_proc_state)result;
	esh_kernel_unlock();
	return 0;
}

void esh_sched_set_current_proc(struct esh_proc *proc)
{
	esh_sched_backend_set_current(proc);
}

static void esh_wait_remove(struct esh_wait_chan *chan,
			    struct esh_proc *proc)
{
	struct esh_proc *current;
	struct esh_proc *previous;

	previous = NULL;
	current = chan->wc_head;
	while (current) {
		if (current == proc)
			break;
		previous = current;
		current = current->ep_wait_next;
	}
	if (!current)
		return;
	if (previous)
		previous->ep_wait_next = current->ep_wait_next;
	else
		chan->wc_head = current->ep_wait_next;
	if (chan->wc_tail == current)
		chan->wc_tail = previous;
	current->ep_wait_chan = NULL;
	current->ep_wait_next = NULL;
}
