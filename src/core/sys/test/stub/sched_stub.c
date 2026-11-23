/* sched_stub.c */

#include <assert.h>
#include <stddef.h>

#include "sched.h"
#include "sched_stub.h"

#define WAKE_RECORD_MAX	8

struct wake_record {
	char *wr_chan;
	int wr_count;
};

static struct proc *stub_current;
static void (*stub_sleep_hook)(char *chan,
			       enum wait_cause cause);
static struct wake_record wake_record[WAKE_RECORD_MAX];
static enum wait_cause last_cause;
static int sleep_result;
static int sleep_count;
static int lock_count;

void sched_stub_reset(struct proc *proc)
{
	int i;

	stub_current = proc;
	stub_sleep_hook = NULL;
	last_cause = WAIT_READ;
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

void sched_stub_set_sleep_hook(void (*hook)(char *chan,
					    enum wait_cause cause))
{
	stub_sleep_hook = hook;
}

int sched_stub_sleep_count(void)
{
	return sleep_count;
}

enum wait_cause sched_stub_last_cause(void)
{
	return last_cause;
}

int sched_stub_wakeup_count(char *chan)
{
	int i;

	for (i = 0; i < WAKE_RECORD_MAX; i++) {
		if (wake_record[i].wr_chan == chan)
			return wake_record[i].wr_count;
	}
	return 0;
}

void sys_lock(void)
{
	lock_count++;
}

void sys_unlock(void)
{
	if (lock_count)
		lock_count--;
}

int sys_lock_held(void)
{
	return lock_count != 0;
}

int sched_sleep(char *chan, enum wait_cause cause)
{
	sleep_count++;
	last_cause = cause;
	if (stub_sleep_hook)
		stub_sleep_hook(chan, cause);
	return sleep_result;
}

void sched_wakeup(char *chan)
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
