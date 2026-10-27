/* sched_stub.c */

#include <assert.h>
#include <stddef.h>

#include "sched.h"
#include "sched_stub.h"

#define WAKE_RECORD_MAX	8

struct wake_record {
	struct proc_wait *wr_chan;
	int wr_count;
};

static struct proc *stub_current;
static void (*stub_sleep_hook)(struct proc_wait *chan,
			       enum wait_reason reason);
static struct wake_record wake_record[WAKE_RECORD_MAX];
static enum wait_reason last_reason;
static int sleep_result;
static int sleep_count;
static int lock_count;

void sched_stub_reset(struct proc *proc)
{
	int i;

	stub_current = proc;
	stub_sleep_hook = NULL;
	last_reason = WAIT_READ;
	sleep_result = 0;
	sleep_count = 0;
	lock_count = 0;
	for (i = 0; i < WAKE_RECORD_MAX; i++) {
		wake_record[i].wr_chan = NULL;
		wake_record[i].wr_count = 0;
	}
}

void sched_stub_set_sleep_result(int result)
{
	sleep_result = result;
}

void sched_stub_set_sleep_hook(void (*hook)(struct proc_wait *chan,
					    enum wait_reason reason))
{
	stub_sleep_hook = hook;
}

int sched_stub_sleep_count(void)
{
	return sleep_count;
}

enum wait_reason sched_stub_last_reason(void)
{
	return last_reason;
}

int sched_stub_wakeup_count(struct proc_wait *chan)
{
	int i;

	for (i = 0; i < WAKE_RECORD_MAX; i++) {
		if (wake_record[i].wr_chan == chan)
			return wake_record[i].wr_count;
	}
	return 0;
}

void kernel_lock(void)
{
	lock_count++;
}

void kernel_unlock(void)
{
	if (lock_count)
		lock_count--;
}

int kernel_is_locked(void)
{
	return lock_count != 0;
}

int sched_sleep(struct proc_wait *chan, enum wait_reason reason)
{
	sleep_count++;
	last_reason = reason;
	if (stub_sleep_hook)
		stub_sleep_hook(chan, reason);
	return sleep_result;
}

void sched_wakeup(struct proc_wait *chan)
{
	int i;

	if (!chan)
		return;
	for (i = 0; i < WAKE_RECORD_MAX; i++) {
		if (wake_record[i].wr_chan == chan ||
		    !wake_record[i].wr_chan) {
			wake_record[i].wr_chan = chan;
			wake_record[i].wr_count++;
			return;
		}
	}
}

struct proc *current_proc(void)
{
	return stub_current;
}
