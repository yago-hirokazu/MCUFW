/* sched_thread.c */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <process.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "sched_adapt.h"
#include "sys_error.h"

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
static struct sched_thread_slot thread_slots[PIPELINE_MAX];

static int sched_thread_wait(CONDITION_VARIABLE *cv);
static unsigned __stdcall sched_thread_entry(void *arg);

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
	AcquireSRWLockExclusive(&kernel_mutex);
	kernel_lock_held = 1;
}

void sched_adapt_unlock(void)
{
	kernel_lock_held = 0;
	ReleaseSRWLockExclusive(&kernel_mutex);
}

int sched_adapt_is_locked(void)
{
	return kernel_lock_held;
}

void *sched_adapt_prepare(void (*entry)(void *arg), void *arg)
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

void sched_adapt_release(void *backend)
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

int sched_adapt_yield(struct proc *proc, enum wait_reason reason)
{
	struct sched_thread_slot *slot = proc->p_ctx;

	if (reason != WAIT_LOCK && reason != WAIT_READ &&
	    reason != WAIT_WRITE)
		return -SYS_EINVAL;
	slot->ts_run = 0;
	WakeAllConditionVariable(&slot->ts_report_cv);
	/* Predicate survives wakeup preceding native wait */
	while (!slot->ts_run && !slot->ts_stop) {
		if (sched_thread_wait(&slot->ts_run_cv) < 0)
			return -SYS_EINVAL;
	}
	return slot->ts_stop ? -SYS_EINVAL : 0;
}

int sched_adapt_wake(struct proc *proc)
{
	struct sched_thread_slot *slot = proc->p_ctx;

	if (!slot->ts_used || slot->ts_done || slot->ts_stop)
		return -SYS_EINVAL;
	/* Executor grants run token after shared state becomes runnable */
	WakeAllConditionVariable(&slot->ts_run_cv);
	return 0;
}

enum proc_state sched_adapt_resume(struct proc *proc)
{
	struct sched_thread_slot *slot = proc->p_ctx;
	enum proc_state state;

	kernel_lock();
	if (slot->ts_done || slot->ts_stop) {
		kernel_unlock();
		return PROC_ERROR;
	}
	slot->ts_proc = proc;
	slot->ts_run = 1;
	WakeAllConditionVariable(&slot->ts_run_cv);
	while (slot->ts_run && !slot->ts_done) {
		if (sched_thread_wait(&slot->ts_report_cv) < 0) {
			kernel_unlock();
			return PROC_ERROR;
		}
	}
	state = proc->p_state;
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
		slot->ts_proc->p_state = PROC_FINISHED;
	}
	slot->ts_done = 1;
	slot->ts_run = 0;
	WakeAllConditionVariable(&slot->ts_report_cv);
	kernel_unlock();
	sched_set_current_proc(NULL);
	return 0;
}
