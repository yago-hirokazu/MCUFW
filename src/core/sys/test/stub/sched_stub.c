/* sched_stub.c */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "sched.h"
#include "user.h"
#include "sched_stub.h"

#define WAKE_RECORD_MAX	8

struct wake_record {
	struct wait_chan *wr_chan;
	int wr_count;
};

static struct proc *stub_current;
static struct user base_user;
static void (*stub_sleep_hook)(struct wait_chan *chan,
			       enum wait_reason reason);
static struct wake_record wake_record[WAKE_RECORD_MAX];
static enum wait_reason last_reason;
static int sleep_result;
static int sleep_count;
static int lock_count;

/* Bind caller-owned user with same initialization as scheduler */
void proc_init(struct proc *proc, struct user *user)
{
	assert(proc != NULL && user != NULL);
	memset(proc, 0, sizeof(*proc));
	memset(user, 0, sizeof(*user));
	proc->p_addr = user;
	user->u_procp = proc;
	proc->p_stat = PROC_NEW;
}

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

void sched_stub_set_sleep_hook(void (*hook)(struct wait_chan *chan,
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

int sched_stub_wakeup_count(struct wait_chan *chan)
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

int sched_sleep(struct wait_chan *chan, enum wait_reason reason)
{
	sleep_count++;
	last_reason = reason;
	if (stub_sleep_hook)
		stub_sleep_hook(chan, reason);
	return sleep_result;
}

void sched_wakeup(struct wait_chan *chan)
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

/* Standalone cache tests execute in single host context */
struct user *current_user(void)
{
	if (!stub_current)
		return &base_user;
	assert(stub_current->p_addr != NULL);
	return stub_current->p_addr;
}
