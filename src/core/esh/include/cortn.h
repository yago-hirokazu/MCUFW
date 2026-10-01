/* cortn.h */

#ifndef __CORTN_H__
#define __CORTN_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CORTN_WINDOWS_STACK_SIZE	(16U * 1024U)
#define CORTN_STACK_GUARD_SIZE	16U

enum cortn_state {
	CORTN_NEW,
	CORTN_RUNNABLE,
	CORTN_RUNNING,
	CORTN_WAIT_READ,
	CORTN_WAIT_WRITE,
	CORTN_FINISHED,
	CORTN_ERROR
};

struct cortn;
typedef void (*cortn_entry_t)(struct cortn *self, void *arg);

/* Context layout and API become public in Milestone 2 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
