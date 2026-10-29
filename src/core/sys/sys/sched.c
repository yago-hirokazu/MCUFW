/*
 * Event address => wait_hash() => slpque[] => proc.p_link
 *   => matching proc.p_wchan => backend wake
 * current_proc() => scheduler backend => active process
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "sched.h"
#include "sched_adapt.h"
#include "sys_error.h"

#define SQSIZE	64

static struct proc *slpque[SQSIZE];

#ifdef SCHED_TEST
static int test_fail_yield;
#endif

static unsigned int wait_hash(char *chan);
static void remove_wait_proc(char *chan, struct proc *proc);

#ifdef SCHED_TEST
void sched_test_fail_next_yield(void);
#endif

#ifdef SCHED_TEST
void sched_test_fail_next_yield(void)
{
	test_fail_yield = 1;
}
#endif

void kernel_lock(void)
{
	sched_adapt_lock();
}

void kernel_unlock(void)
{
	sched_adapt_unlock();
}

int kernel_is_locked(void)
{
	return sched_adapt_is_locked();
}

/* Register wait event before yielding with kernel lock held */
int sched_sleep(char *chan, enum wait_reason reason)
{
	struct proc *proc;
	int result;
	unsigned int h;

	proc = current_proc();
	if (!chan || !proc || !proc->p_ctx || !kernel_is_locked())
		return -SYS_EINVAL;
	if (proc->p_wchan)
		return -SYS_EINVAL;

	if (reason == WAIT_LOCK)
		proc->p_state = PROC_WAIT_LOCK;
	else if (reason == WAIT_READ)
		proc->p_state = PROC_WAIT_READ;
	else if (reason == WAIT_WRITE)
		proc->p_state = PROC_WAIT_WRITE;
	else
		return -SYS_EINVAL;

	/* Insert at bucket head, following V7 sleep */
	h = wait_hash(chan);
	proc->p_wchan = chan;
	proc->p_link = slpque[h];
	slpque[h] = proc;

#ifdef SCHED_TEST
	if (test_fail_yield) {
		test_fail_yield = 0;
		result = -SYS_EINVAL;
	} else {
		result = sched_adapt_yield(proc, reason);
	}
#else
	result = sched_adapt_yield(proc, reason);
#endif
	if (result != 0) {
		remove_wait_proc(chan, proc);
		proc->p_state = PROC_ERROR;
		return -SYS_EINVAL;
	}
	proc->p_state = PROC_RUNNING;
	return 0;
}

/* Wake all processes awaiting matching event address */
void sched_wakeup(char *chan)
{
	struct proc *proc;
	struct proc *previous;
	struct proc *next;
	unsigned int h;

	if (!chan)
		return;
	assert(kernel_is_locked());
	h = wait_hash(chan);
	previous = NULL;
	proc = slpque[h];
	while (proc) {
		next = proc->p_link;
		/* Hash collisions do not identify same event */
		if (proc->p_wchan != chan) {
			previous = proc;
			proc = next;
			continue;
		}
		if (previous)
			previous->p_link = next;
		else
			slpque[h] = next;
		proc->p_wchan = NULL;
		proc->p_link = NULL;
		if (proc->p_ctx && sched_adapt_wake(proc) == 0)
			proc->p_state = PROC_RUNNABLE;
		else
			proc->p_state = PROC_ERROR;
		proc = next;
	}
}

struct proc *current_proc(void)
{
	return sched_adapt_current();
}

/* Initialize process before descriptors or backend setup */
void pinit(struct proc *proc)
{
	assert(proc != NULL);
	memset(proc, 0, sizeof(*proc));
	proc->p_state = PROC_NEW;
}

int sched_bind_proc_to_coroutine(struct proc *proc, void *backend)
{
	if (!proc || !backend || proc->p_state != PROC_NEW)
		return -SYS_EINVAL;
	proc->p_ctx = backend;
	proc->p_state = PROC_RUNNABLE;
	return 0;
}

int sched_prepare(struct proc *proc, void (*entry)(void *arg), void *arg)
{
	void *backend;

	if (!proc || !entry || proc->p_state != PROC_NEW)
		return -SYS_EINVAL;
	backend = sched_adapt_prepare(entry, arg);
	if (!backend)
		return -SYS_ENOMEM;
	if (sched_bind_proc_to_coroutine(proc, backend) < 0) {
		sched_adapt_release(backend);
		return -SYS_EINVAL;
	}
	return 0;
}

void sched_release(struct proc *proc)
{
	void *backend;

	if (!proc || !proc->p_ctx)
		return;
	kernel_lock();
	if (proc->p_wchan)
		remove_wait_proc(proc->p_wchan, proc);
	backend = proc->p_ctx;
	proc->p_ctx = NULL;
	kernel_unlock();
	sched_adapt_release(backend);
}

int sched_resume(struct proc *proc)
{
	int result;

	if (!proc || !proc->p_ctx || current_proc())
		return -SYS_EINVAL;
	kernel_lock();
	if (proc->p_state != PROC_RUNNABLE) {
		kernel_unlock();
		return -SYS_EINVAL;
	}
	sched_set_current_proc(proc);
	proc->p_state = PROC_RUNNING;
	kernel_unlock();
	result = sched_adapt_resume(proc);
	sched_set_current_proc(NULL);
	if (result == PROC_ERROR) {
		kernel_lock();
		proc->p_state = PROC_ERROR;
		kernel_unlock();
		return -SYS_EINVAL;
	}
	/* Keep wakeup published after backend reported wait state */
	kernel_lock();
	if (result != PROC_WAIT_LOCK && result != PROC_WAIT_READ &&
	    result != PROC_WAIT_WRITE)
		proc->p_state = (enum proc_state)result;
	kernel_unlock();
	return 0;
}

void sched_set_current_proc(struct proc *proc)
{
	sched_adapt_set_current(proc);
}

/* Preserve full pointer width in V7 wait-channel hash */
static unsigned int wait_hash(char *chan)
{
	return (unsigned int)(((uintptr_t)chan >> 5) & (SQSIZE - 1));
}

/* Unlink specified process without disturbing colliding waiters */
static void remove_wait_proc(char *chan, struct proc *proc)
{
	struct proc *current;
	struct proc *previous;
	unsigned int h;

	assert(kernel_is_locked());
	h = wait_hash(chan);
	previous = NULL;
	current = slpque[h];
	while (current) {
		if (current == proc)
			break;
		previous = current;
		current = current->p_link;
	}
	if (!current)
		return;
	if (previous)
		previous->p_link = current->p_link;
	else
		slpque[h] = current->p_link;
	current->p_wchan = NULL;
	current->p_link = NULL;
}
