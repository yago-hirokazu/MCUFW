/* cortn.h */

#ifndef __CORTN_H__
#define __CORTN_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#include "cortn_context.h"

#define CORTN_WINDOWS_STACK_SIZE	(16U * 1024U)
#define CORTN_STACK_GUARD_SIZE	16U
#define CORTN_STACK_MIN_SIZE	512U

enum cortn_state {
	CORTN_NEW,
	CORTN_RUNNABLE,
	CORTN_RUNNING,
	CORTN_WAIT_READ,
	CORTN_WAIT_WRITE,
	CORTN_FINISHED,
	CORTN_ERROR
};

enum cortn_yield_reason {
	CORTN_YIELD_NONE,
	CORTN_YIELD_VOLUNTARY,
	CORTN_YIELD_WAIT_READ,
	CORTN_YIELD_WAIT_WRITE,
	CORTN_YIELD_FINISHED,
	CORTN_YIELD_ERROR
};

enum cortn_result {
	CORTN_SUCCESS = 0,
	CORTN_ERR_ARG = -1,
	CORTN_ERR_STATE = -2,
	CORTN_ERR_STACK = -3
};

struct cortn;
typedef void (*cortn_entry_t)(struct cortn *self, void *arg);

struct cortn {
	struct cortn_context c_context;
	struct cortn_context c_caller;
	cortn_entry_t c_entry;
	void *c_arg;
	unsigned char *c_stack;
	size_t c_stack_size;
	enum cortn_state c_state;
	enum cortn_yield_reason c_reason;
};

int cortn_init(struct cortn *self, void *stack, size_t stack_size,
	       cortn_entry_t entry, void *arg);
int cortn_resume(struct cortn *self);
int cortn_yield(struct cortn *self, enum cortn_yield_reason reason);
int cortn_wake(struct cortn *self);
int cortn_stack_check(const struct cortn *self);
size_t cortn_stack_used(const struct cortn *self);

/* Called only from target context bootstrap */
void cortn_boot_main(struct cortn *self);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
