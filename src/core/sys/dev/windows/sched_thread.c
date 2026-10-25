/*
 * sched.c => execution token / condition variables => native worker
 * stop / wait failure => PROC_ERROR => unwind => join
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <process.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "sched_backend.h"
#include "sys_error.h"
#include "user.h"

struct sched_thread_slot {
	HANDLE ts_thread;
	CONDITION_VARIABLE ts_run_cv;
	CONDITION_VARIABLE ts_report_cv;
	struct proc *ts_proc;
	void (*ts_entry)(void *arg);
	void *ts_arg;
	int ts_used;
	int ts_run;
	int ts_done;
	int ts_stop;
};

static SRWLOCK kernel_mutex = SRWLOCK_INIT;
static _Thread_local int kernel_lock_held;
static _Thread_local struct proc *current;
static _Thread_local struct user base_user;
static struct sched_thread_slot thread_slots[PIPELINE_MAX];

#ifdef SCHED_TEST
static int test_fail_thread_wait;
static int test_fail_wake;
static int test_fail_report_wait;
#endif

static int sched_thread_wait(CONDITION_VARIABLE *cv);
static unsigned __stdcall sched_thread_entry(void *arg);

#ifdef SCHED_TEST
/* Fail native run wait after worker publishes suspension */
void sched_test_fail_next_thread_wait(void)
{
	kernel_lock();
	test_fail_thread_wait = 1;
	kernel_unlock();
}

/* Fail wake without changing native wait predicate */
void sched_test_fail_next_wake(void)
{
	kernel_lock();
	test_fail_wake = 1;
	kernel_unlock();
}

/* Fail report wait after actual worker notification */
void sched_test_fail_next_report_wait(void)
{
	kernel_lock();
	test_fail_report_wait = 1;
	kernel_unlock();
}
#endif

/* Host-side operations retain separate error state from workers */
struct user *sched_backend_base_user(void)
{
	return &base_user;
}

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
	AcquireSRWLockExclusive(&kernel_mutex);
	kernel_lock_held = 1;
}

void sched_backend_unlock(void)
{
	kernel_lock_held = 0;
	ReleaseSRWLockExclusive(&kernel_mutex);
}

int sched_backend_is_locked(void)
{
	return kernel_lock_held;
}

void *sched_backend_prepare(void (*entry)(void *arg), void *arg)
{
	struct sched_thread_slot *slot;
	uintptr_t thread;
	int i;

	if (!entry)
		return NULL;
	kernel_lock();
	for (i = 0; i < PIPELINE_MAX; i++) {
		slot = &thread_slots[i];
		if (slot->ts_used)
			continue;
		memset(slot, 0, sizeof(*slot));
		slot->ts_used = 1;
		slot->ts_entry = entry;
		slot->ts_arg = arg;
		InitializeConditionVariable(&slot->ts_run_cv);
		InitializeConditionVariable(&slot->ts_report_cv);
		thread = _beginthreadex(NULL, 0, sched_thread_entry, slot,
				       0, NULL);
		if (!thread) {
			memset(slot, 0, sizeof(*slot));
			kernel_unlock();
			return NULL;
		}
		slot->ts_thread = (HANDLE)thread;
		kernel_unlock();
		return slot;
	}
	kernel_unlock();
	return NULL;
}

void sched_backend_release(void *backend)
{
	struct sched_thread_slot *slot;
	int i;

	for (i = 0; i < PIPELINE_MAX; i++) {
		slot = &thread_slots[i];
		if (slot != backend || !slot->ts_used)
			continue;
		kernel_lock();
		slot->ts_stop = 1;
		WakeAllConditionVariable(&slot->ts_run_cv);
		kernel_unlock();
		/* Wait for cooperative unwind before reclaiming slot */
		WaitForSingleObject(slot->ts_thread, INFINITE);
		CloseHandle(slot->ts_thread);
		kernel_lock();
		memset(slot, 0, sizeof(*slot));
		kernel_unlock();
		return;
	}
}

int sched_backend_yield(struct proc *proc, enum wait_reason reason)
{
	struct sched_thread_slot *slot = proc->p_xctx;
	int result;

	if (reason != WAIT_LOCK && reason != WAIT_READ &&
	    reason != WAIT_WRITE)
		return -SYS_EINVAL;
	slot->ts_run = 0;
	WakeAllConditionVariable(&slot->ts_report_cv);
	/* Predicate survives wakeup preceding native wait */
	while (!slot->ts_run && !slot->ts_stop) {
#ifdef SCHED_TEST
		if (test_fail_thread_wait) {
			test_fail_thread_wait = 0;
			result = -SYS_EINVAL;
		} else
#endif
			result = sched_thread_wait(&slot->ts_run_cv);
		if (result < 0) {
			proc->p_stat = PROC_ERROR;
			/* Keep run token while unwinding failed wait */
			slot->ts_run = 1;
			return -SYS_EINVAL;
		}
	}
	return slot->ts_stop ? -SYS_EINVAL : 0;
}

int sched_backend_wake(struct proc *proc)
{
	struct sched_thread_slot *slot = proc->p_xctx;

#ifdef SCHED_TEST
	if (test_fail_wake) {
		test_fail_wake = 0;
		return -SYS_EINVAL;
	}
#endif
	if (!slot->ts_used || slot->ts_done || slot->ts_stop)
		return -SYS_EINVAL;
	/* Executor grants run token after shared state becomes runnable */
	WakeAllConditionVariable(&slot->ts_run_cv);
	return 0;
}

enum proc_state sched_backend_resume(struct proc *proc)
{
	struct sched_thread_slot *slot = proc->p_xctx;
	enum proc_state state;
	int result;

	kernel_lock();
	if (slot->ts_done || slot->ts_stop) {
		kernel_unlock();
		return PROC_ERROR;
	}
	slot->ts_proc = proc;
	slot->ts_run = 1;
	WakeAllConditionVariable(&slot->ts_run_cv);
	while (slot->ts_run && !slot->ts_done) {
		result = sched_thread_wait(&slot->ts_report_cv);
#ifdef SCHED_TEST
		if (test_fail_report_wait && !slot->ts_run) {
			test_fail_report_wait = 0;
			result = -SYS_EINVAL;
		}
#endif
		if (result < 0) {
			proc->p_stat = PROC_ERROR;
			slot->ts_stop = 1;
			WakeAllConditionVariable(&slot->ts_run_cv);
			kernel_unlock();
			/* Keep proc/user alive until worker stops */
			WaitForSingleObject(slot->ts_thread, INFINITE);
			return PROC_ERROR;
		}
	}
	state = proc->p_stat;
	kernel_unlock();
	return state;
}

static int sched_thread_wait(CONDITION_VARIABLE *cv)
{
	BOOL result;

	kernel_lock_held = 0;
	result = SleepConditionVariableSRW(cv, &kernel_mutex,
					  INFINITE, 0);
	kernel_lock_held = 1;
	return result ? 0 : -SYS_EINVAL;
}

static unsigned __stdcall sched_thread_entry(void *arg)
{
	struct sched_thread_slot *slot = arg;

	kernel_lock();
	while (!slot->ts_run && !slot->ts_stop) {
		if (sched_thread_wait(&slot->ts_run_cv) < 0)
			slot->ts_stop = 1;
	}
	if (!slot->ts_stop) {
		sched_set_current_proc(slot->ts_proc);
		kernel_unlock();
		slot->ts_entry(slot->ts_arg);
		kernel_lock();
		if (slot->ts_proc->p_stat != PROC_ERROR)
			slot->ts_proc->p_stat = SZOMB;
	}
	slot->ts_done = 1;
	slot->ts_run = 0;
	WakeAllConditionVariable(&slot->ts_report_cv);
	kernel_unlock();
	sched_set_current_proc(NULL);
	return 0;
}
