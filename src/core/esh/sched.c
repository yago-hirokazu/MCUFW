/* sched.c */

#include <string.h>

#include "esh_errno.h"
#include "esh_sched.h"

static struct esh_proc *esh_current;

int esh_sleep(void *chan, enum esh_wait_reason reason)
{
	(void)chan;
	(void)reason;
	return -ESH_ENOSYS;
}

void esh_wakeup(void *chan)
{
	(void)chan;
}

struct esh_proc *esh_current_proc(void)
{
	return esh_current;
}

void esh_proc_init(struct esh_proc *proc)
{
	if (!proc)
		return;
	memset(proc, 0, sizeof(*proc));
	proc->ep_state = ESH_PROC_NEW;
}

void esh_sched_set_current_proc(struct esh_proc *proc)
{
	esh_current = proc;
}
