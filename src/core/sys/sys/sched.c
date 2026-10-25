/*
 * current_user() => current_proc() => p_addr => dedicated user area
 * Host without proc => backend-local base user area
 * sleep / wakeup => proc wait list => scheduler backend
 * backend report => resume_report() => wakeup / fatal state preserved
 */

#include <assert.h>
#include <string.h>

#include "sched.h"
#include "sched_backend.h"
#include "sys_error.h"
#include "user.h"

#ifdef SCHED_TEST
static int test_fail_yield;
static void (*test_resume_hook)(struct proc *, enum proc_state *);
#endif

static void wait_remove(struct wait_chan *chan, struct proc *proc);
static void proc_error(struct proc *proc);
static int resume_report(struct proc *proc, enum proc_state report);

#ifdef SCHED_TEST
void sched_test_fail_next_yield(void);
void sched_test_set_resume_hook(void (*hook)(struct proc *, enum proc_state *));
#endif

#ifdef SCHED_TEST
void sched_test_fail_next_yield(void)
{
	test_fail_yield = 1;
}

/* Inject event or report at backend/portable scheduler boundary */
void sched_test_set_resume_hook(void (*hook)(struct proc *, enum proc_state *))
{
	test_resume_hook = hook;
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
	if (!chan || !proc || !proc->p_xctx || !kernel_is_locked())
		return -SYS_EINVAL;
	if (proc->p_stat != SRUN || proc->p_wchan || proc->p_link)
		return -SYS_EINVAL;

	if (reason != WAIT_LOCK && reason != WAIT_READ &&
	    reason != WAIT_WRITE)
		return -SYS_EINVAL;

	proc->p_stat = SSLEEP;
	proc->p_wchan = chan;
	proc->p_link = NULL;
	if (chan->wc_tail)
		chan->wc_tail->p_link = proc;
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
	if (result != 0 || proc->p_stat == PROC_ERROR || !proc->p_xctx) {
		wait_remove(chan, proc);
		/* Safe return from failed wait does not terminate execution */
		if (proc->p_stat != PROC_ERROR && proc->p_xctx)
			proc->p_stat = SRUN;
		else
			proc_error(proc);
		return -SYS_EINVAL;
	}
	if (proc->p_stat != SRUN || proc->p_wchan || proc->p_link) {
		proc_error(proc);
		return -SYS_EINVAL;
	}
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
		next = proc->p_link;
		proc->p_wchan = NULL;
		proc->p_link = NULL;
		if (proc->p_stat == SSLEEP && proc->p_xctx &&
		    sched_backend_wake(proc) == 0)
			proc->p_stat = SRUN;
		else
			proc->p_stat = PROC_ERROR;
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
	proc->p_stat = SIDL;
}

int proc_bind_backend(struct proc *proc, void *backend)
{
	if (!proc || !proc->p_addr || !backend)
		return -SYS_EINVAL;
	kernel_lock();
	if (proc->p_stat != SIDL || proc->p_xctx) {
		kernel_unlock();
		return -SYS_EINVAL;
	}
	proc->p_xctx = backend;
	proc->p_stat = SRUN;
	kernel_unlock();
	return 0;
}

int sched_prepare(struct proc *proc, void (*entry)(void *arg), void *arg)
{
	void *backend;

	if (!proc || !proc->p_addr || !entry || proc->p_stat != SIDL)
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

	if (!proc || !proc->p_xctx)
		return;
	kernel_lock();
	if (proc->p_stat != SZOMB)
		proc->p_stat = PROC_ERROR;
	if (proc->p_wchan)
		wait_remove(proc->p_wchan, proc);
	proc->p_wchan = NULL;
	proc->p_link = NULL;
	backend = proc->p_xctx;
	proc->p_xctx = NULL;
	kernel_unlock();
	/* Worker may still access p_addr while unwinding outside kernel lock */
	sched_backend_release(backend);
}

int sched_resume(struct proc *proc)
{
	int result;
	enum proc_state report;
#ifdef SCHED_TEST
	void (*hook)(struct proc *, enum proc_state *);
#endif

	if (!proc || !proc->p_xctx || current_proc())
		return -SYS_EINVAL;
	kernel_lock();
	if (proc->p_stat != SRUN || proc->p_wchan || proc->p_link) {
		kernel_unlock();
		return -SYS_EINVAL;
	}
	sched_set_current_proc(proc);
	kernel_unlock();
	report = sched_backend_resume(proc);
	sched_set_current_proc(NULL);
	kernel_lock();
#ifdef SCHED_TEST
	hook = test_resume_hook;
	test_resume_hook = NULL;
	if (hook)
		hook(proc, &report);
#endif
	result = resume_report(proc, report);
	kernel_unlock();
	return result;
}

void sched_set_current_proc(struct proc *proc)
{
	sched_backend_set_current(proc);
}

/* Remove process from waiting object with kernel lock held */
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
		current = current->p_link;
	}
	if (!current)
		return;
	if (previous)
		previous->p_link = current->p_link;
	else
		chan->wc_head = current->p_link;
	if (chan->wc_tail == current)
		chan->wc_tail = previous;
	current->p_wchan = NULL;
	current->p_link = NULL;
}

/* Keep fatal state and remove stale wait registration */
static void proc_error(struct proc *proc)
{
	if (proc->p_wchan)
		wait_remove(proc->p_wchan, proc);
	proc->p_wchan = NULL;
	proc->p_link = NULL;
	proc->p_stat = PROC_ERROR;
}

/* Merge backend report without losing wakeup or fatal state */
static int resume_report(struct proc *proc, enum proc_state report)
{
	if (report == PROC_ERROR || proc->p_stat == PROC_ERROR)
		goto error;
	switch (report) {
	case SSLEEP:
		if (proc->p_stat == SSLEEP && proc->p_wchan)
			return 0;
		/* Wakeup may precede collection of backend wait report */
		if (proc->p_stat == SRUN && !proc->p_wchan && !proc->p_link)
			return 0;
		break;
	case SRUN:
		if (proc->p_stat == SRUN && !proc->p_wchan && !proc->p_link)
			return 0;
		break;
	case SZOMB:
		if ((proc->p_stat == SRUN || proc->p_stat == SZOMB) &&
		    !proc->p_wchan && !proc->p_link) {
			proc->p_stat = SZOMB;
			return 0;
		}
		break;
	default:
		break;
	}
error:
	proc_error(proc);
	return -SYS_EINVAL;
}
