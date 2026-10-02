/* sched_thread.c */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <process.h>
#include <stdint.h>
#include <string.h>

#include "esh_errno.h"
#include "esh_sched_backend.h"

static SRWLOCK esh_kernel_mutex = SRWLOCK_INIT;
static _Thread_local int esh_lock_held;
static _Thread_local struct esh_proc *esh_current;

struct esh_thread_slot {
	HANDLE ts_thread;
	CONDITION_VARIABLE ts_run_cv;
	CONDITION_VARIABLE ts_report_cv;
	struct esh_proc *ts_proc;
	esh_proc_entry_t ts_entry;
	void *ts_arg;
	int ts_used;
	int ts_run;
	int ts_done;
	int ts_stop;
};

static struct esh_thread_slot esh_thread_slots[ESH_PIPELINE_MAX];

struct esh_proc *esh_sched_backend_current(void)
{
	return esh_current;
}

void esh_sched_backend_set_current(struct esh_proc *proc)
{
	esh_current = proc;
}

void esh_sched_backend_lock(void)
{
	AcquireSRWLockExclusive(&esh_kernel_mutex);
	esh_lock_held = 1;
}

void esh_sched_backend_unlock(void)
{
	esh_lock_held = 0;
	ReleaseSRWLockExclusive(&esh_kernel_mutex);
}

int esh_sched_backend_is_locked(void)
{
	return esh_lock_held;
}

static int esh_thread_wait(CONDITION_VARIABLE *cv)
{
	BOOL result;

	esh_lock_held = 0;
	result = SleepConditionVariableSRW(cv, &esh_kernel_mutex,
					  INFINITE, 0);
	esh_lock_held = 1;
	return result ? 0 : -ESH_EINVAL;
}

static unsigned __stdcall esh_thread_entry(void *arg)
{
	struct esh_thread_slot *slot = arg;

	esh_kernel_lock();
	while (!slot->ts_run && !slot->ts_stop) {
		if (esh_thread_wait(&slot->ts_run_cv) < 0)
			slot->ts_stop = 1;
	}
	if (!slot->ts_stop) {
		esh_sched_set_current_proc(slot->ts_proc);
		esh_kernel_unlock();
		slot->ts_entry(slot->ts_arg);
		esh_kernel_lock();
		slot->ts_proc->ep_state = ESH_PROC_FINISHED;
	}
	slot->ts_done = 1;
	slot->ts_run = 0;
	WakeAllConditionVariable(&slot->ts_report_cv);
	esh_kernel_unlock();
	esh_sched_set_current_proc(NULL);
	return 0;
}

void *esh_sched_backend_prepare(esh_proc_entry_t entry, void *arg)
{
	struct esh_thread_slot *slot;
	uintptr_t thread;
	int i;

	if (!entry)
		return NULL;
	esh_kernel_lock();
	for (i = 0; i < ESH_PIPELINE_MAX; i++) {
		slot = &esh_thread_slots[i];
		if (slot->ts_used)
			continue;
		memset(slot, 0, sizeof(*slot));
		slot->ts_used = 1;
		slot->ts_entry = entry;
		slot->ts_arg = arg;
		InitializeConditionVariable(&slot->ts_run_cv);
		InitializeConditionVariable(&slot->ts_report_cv);
		thread = _beginthreadex(NULL, 0, esh_thread_entry, slot,
				       0, NULL);
		if (!thread) {
			memset(slot, 0, sizeof(*slot));
			esh_kernel_unlock();
			return NULL;
		}
		slot->ts_thread = (HANDLE)thread;
		esh_kernel_unlock();
		return slot;
	}
	esh_kernel_unlock();
	return NULL;
}

void esh_sched_backend_release(void *backend)
{
	struct esh_thread_slot *slot;
	int i;

	for (i = 0; i < ESH_PIPELINE_MAX; i++) {
		slot = &esh_thread_slots[i];
		if (slot != backend || !slot->ts_used)
			continue;
		esh_kernel_lock();
		slot->ts_stop = 1;
		WakeAllConditionVariable(&slot->ts_run_cv);
		esh_kernel_unlock();
		/* Wait for cooperative unwind before reclaiming slot */
		WaitForSingleObject(slot->ts_thread, INFINITE);
		CloseHandle(slot->ts_thread);
		esh_kernel_lock();
		memset(slot, 0, sizeof(*slot));
		esh_kernel_unlock();
		return;
	}
}

int esh_sched_backend_yield(struct esh_proc *proc,
			    enum esh_wait_reason reason)
{
	struct esh_thread_slot *slot = proc->ep_backend;

	if (reason != ESH_WAIT_READ && reason != ESH_WAIT_WRITE)
		return -ESH_EINVAL;
	slot->ts_run = 0;
	WakeAllConditionVariable(&slot->ts_report_cv);
	/* Predicate survives wakeup preceding native wait */
	while (!slot->ts_run && !slot->ts_stop) {
		if (esh_thread_wait(&slot->ts_run_cv) < 0)
			return -ESH_EINVAL;
	}
	return slot->ts_stop ? -ESH_EINVAL : 0;
}

int esh_sched_backend_wake(struct esh_proc *proc)
{
	struct esh_thread_slot *slot = proc->ep_backend;

	if (!slot->ts_used || slot->ts_done || slot->ts_stop)
		return -ESH_EINVAL;
	/* Executor grants run token after shared state becomes runnable */
	WakeAllConditionVariable(&slot->ts_run_cv);
	return 0;
}

enum esh_proc_state esh_sched_backend_resume(struct esh_proc *proc)
{
	struct esh_thread_slot *slot = proc->ep_backend;
	enum esh_proc_state state;

	esh_kernel_lock();
	if (slot->ts_done || slot->ts_stop) {
		esh_kernel_unlock();
		return ESH_PROC_ERROR;
	}
	slot->ts_proc = proc;
	slot->ts_run = 1;
	WakeAllConditionVariable(&slot->ts_run_cv);
	while (slot->ts_run && !slot->ts_done) {
		if (esh_thread_wait(&slot->ts_report_cv) < 0) {
			esh_kernel_unlock();
			return ESH_PROC_ERROR;
		}
	}
	state = proc->ep_state;
	esh_kernel_unlock();
	return state;
}
