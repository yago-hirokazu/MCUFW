/* sched.c */

#include "esh_errno.h"
#include "esh_sched.h"

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
	return NULL;
}
