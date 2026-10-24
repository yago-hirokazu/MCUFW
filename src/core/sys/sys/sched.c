/*
 * current_user() => current_proc() => p_addr => dedicated user area
 * Host without proc => backend-local base user area
 */

#include <assert.h>
#include <string.h>

#include "sched.h"
#include "sched_backend.h"
#include "sys_error.h"
#include "user.h"

#ifdef SCHED_TEST
static int test_fail_yield;
#endif

static void wait_remove(struct wait_chan *chan, struct proc *proc);

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
	sched_backend_lock();
}

void kernel_unlock(void)
{
	sched_backend_unlock();
}

int kernel_is_locked(void)
{
	return sched_backend_is_locked();
}

int sched_sleep(struct wait_chan *chan, enum wait_reason reason)
{
	struct proc *proc;
	int result;

	proc = current_proc();
	if (!chan || !proc || !proc->p_backend || !kernel_is_locked())
		return -SYS_EINVAL;
	if (proc->p_wait_chan)
		return -SYS_EINVAL;

	if (reason == WAIT_LOCK)
		proc->p_state = PROC_WAIT_LOCK;
	else if (reason == WAIT_READ)
		proc->p_state = PROC_WAIT_READ;
	else if (reason == WAIT_WRITE)
		proc->p_state = PROC_WAIT_WRITE;
	else
		return -SYS_EINVAL;

	proc->p_wait_chan = chan;
	proc->p_wait_next = NULL;
	if (chan->wc_tail)
		chan->wc_tail->p_wait_next = proc;
	else
		chan->wc_head = proc;
	chan->wc_tail = proc;

#ifdef SCHED_TEST
	if (test_fail_yield) {
		test_fail_yield = 0;
		result = -SYS_EINVAL;
	} else {
		result = sched_backend_yield(proc, reason);
	}
#else
	result = sched_backend_yield(proc, reason);
#endif
	if (result != 0) {
		wait_remove(chan, proc);
		proc->p_state = PROC_ERROR;
		return -SYS_EINVAL;
	}
	proc->p_state = PROC_RUNNING;
	return 0;
}

void sched_wakeup(struct wait_chan *chan)
{
	struct proc *proc;
	struct proc *next;

	if (!chan)
		return;
	proc = chan->wc_head;
	chan->wc_head = NULL;
	chan->wc_tail = NULL;
	while (proc) {
		next = proc->p_wait_next;
		proc->p_wait_chan = NULL;
		proc->p_wait_next = NULL;
		if (proc->p_backend && sched_backend_wake(proc) == 0)
			proc->p_state = PROC_RUNNABLE;
		else
			proc->p_state = PROC_ERROR;
		proc = next;
	}
}

struct proc *current_proc(void)
{
	return sched_backend_current();
}

/* Select user area through current proc, without copying on switches */
struct user *current_user(void)
{
	struct proc *proc;

	proc = current_proc();
	if (!proc)
		return sched_backend_base_user();
	assert(proc->p_addr != NULL);
	return proc->p_addr;
}

/* Bind caller-owned user area before descriptors or backend setup */
void proc_init(struct proc *proc, struct user *user)
{
	assert(proc != NULL && user != NULL);
	memset(proc, 0, sizeof(*proc));
	memset(user, 0, sizeof(*user));
	proc->p_addr = user;
	user->u_procp = proc;
	proc->p_state = PROC_NEW;
}

int proc_bind_backend(struct proc *proc, void *backend)
{
	if (!proc || !proc->p_addr || !backend || proc->p_state != PROC_NEW)
		return -SYS_EINVAL;
	proc->p_backend = backend;
	proc->p_state = PROC_RUNNABLE;
	return 0;
}

int sched_prepare(struct proc *proc, void (*entry)(void *arg), void *arg)
{
	void *backend;

	if (!proc || !proc->p_addr || !entry || proc->p_state != PROC_NEW)
		return -SYS_EINVAL;
	backend = sched_backend_prepare(entry, arg);
	if (!backend)
		return -SYS_ENOMEM;
	if (proc_bind_backend(proc, backend) < 0) {
		sched_backend_release(backend);
		return -SYS_EINVAL;
	}
	return 0;
}

/* Retain caller-owned user until backend stop and join have completed */
void sched_release(struct proc *proc)
{
	void *backend;

	if (!proc || !proc->p_backend)
		return;
	kernel_lock();
	if (proc->p_wait_chan)
		wait_remove(proc->p_wait_chan, proc);
	backend = proc->p_backend;
	proc->p_backend = NULL;
	kernel_unlock();
	/* Worker may still access p_addr while unwinding outside kernel lock */
	sched_backend_release(backend);
}

int sched_resume(struct proc *proc)
{
	int result;

	if (!proc || !proc->p_backend || current_proc())
		return -SYS_EINVAL;
	kernel_lock();
	if (proc->p_state != PROC_RUNNABLE) {
		kernel_unlock();
		return -SYS_EINVAL;
	}
	sched_set_current_proc(proc);
	proc->p_state = PROC_RUNNING;
	kernel_unlock();
	result = sched_backend_resume(proc);
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
	sched_backend_set_current(proc);
}

static void wait_remove(struct wait_chan *chan, struct proc *proc)
{
	struct proc *current;
	struct proc *previous;

	previous = NULL;
	current = chan->wc_head;
	while (current) {
		if (current == proc)
			break;
		previous = current;
		current = current->p_wait_next;
	}
	if (!current)
		return;
	if (previous)
		previous->p_wait_next = current->p_wait_next;
	else
		chan->wc_head = current->p_wait_next;
	if (chan->wc_tail == current)
		chan->wc_tail = previous;
	current->p_wait_chan = NULL;
	current->p_wait_next = NULL;
}
