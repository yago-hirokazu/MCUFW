/*
 * Scheduler backend => resume => context switch => entry / yield
 * Resume => stack boundary guard checks before and after context switch
 * Wake => waiting coroutine becomes runnable for next resume
 */

#include <stdint.h>
#include <string.h>

#include "cortn.h"

#define CORTN_STACK_BOUNDARY_GUARD_PATTERN	0xa5
#define CORTN_UNUSED_PATTERN	0xcc

static int cortn_stack_boundary_guard_is_valid(const unsigned char *guard);
static unsigned char *cortn_stack_top(const struct cortn *self);

int init_cortn(struct cortn *self, void *stack, size_t stack_size,
	       void (*entry)(struct cortn *self, void *arg), void *arg)
{
	unsigned char *bytes;
	unsigned char *upper_guard;

	bytes = stack;
	memset(self, 0, sizeof(*self));
	memset(bytes, CORTN_UNUSED_PATTERN, stack_size);
	memset(bytes, CORTN_STACK_BOUNDARY_GUARD_PATTERN,
	       CORTN_STACK_BOUNDARY_GUARD_SIZE);
	upper_guard = bytes + stack_size - CORTN_STACK_BOUNDARY_GUARD_SIZE;
	memset(upper_guard, CORTN_STACK_BOUNDARY_GUARD_PATTERN,
	       CORTN_STACK_BOUNDARY_GUARD_SIZE);

	self->c_entry = entry;
	self->c_arg = arg;
	self->c_stack = bytes;
	self->c_stack_size = stack_size;
	self->c_state = CORTN_RUNNABLE;
	self->c_reason = CORTN_YIELD_NONE;
	cortn_context_setup(&self->c_context, cortn_stack_top(self), self);
	return CORTN_SUCCESS;
}

/* Resume runnable coroutine with boundary guards checked on both sides */
int cortn_resume(struct cortn *self)
{
	if (self->c_state != CORTN_RUNNABLE)
		return CORTN_ERR_STATE;
	if (cortn_stack_check(self) != CORTN_SUCCESS) {
		self->c_state = CORTN_ERROR;
		self->c_reason = CORTN_YIELD_ERROR;
		return CORTN_ERR_STACK;
	}

	self->c_state = CORTN_RUNNING;
	self->c_reason = CORTN_YIELD_NONE;
	cortn_switch(&self->c_caller, &self->c_context);

	if (cortn_stack_check(self) != CORTN_SUCCESS) {
		self->c_state = CORTN_ERROR;
		self->c_reason = CORTN_YIELD_ERROR;
		return CORTN_ERR_STACK;
	}
	return CORTN_SUCCESS;
}

/* Suspend running coroutine with requested wait or voluntary state */
int cortn_yield(struct cortn *self, enum cortn_yield_reason reason)
{
	switch (reason) {
	case CORTN_YIELD_VOLUNTARY:
		self->c_state = CORTN_RUNNABLE;
		break;
	case CORTN_YIELD_WAIT_LOCK:
		self->c_state = CORTN_WAIT_LOCK;
		break;
	case CORTN_YIELD_WAIT_READ:
		self->c_state = CORTN_WAIT_READ;
		break;
	case CORTN_YIELD_WAIT_WRITE:
		self->c_state = CORTN_WAIT_WRITE;
		break;
	default:
		break;
	}

	self->c_reason = reason;
	cortn_switch(&self->c_context, &self->c_caller);
	return CORTN_SUCCESS;
}

/* Make waiting coroutine runnable */
int cortn_wake(struct cortn *self)
{
	self->c_state = CORTN_RUNNABLE;
	return CORTN_SUCCESS;
}

int cortn_stack_check(const struct cortn *self)
{
	const unsigned char *upper_guard;

	upper_guard = self->c_stack + self->c_stack_size -
		      CORTN_STACK_BOUNDARY_GUARD_SIZE;
	if (cortn_stack_boundary_guard_is_valid(self->c_stack) == 0 ||
	    cortn_stack_boundary_guard_is_valid(upper_guard) == 0)
		return CORTN_ERR_STACK;
	return CORTN_SUCCESS;
}

size_t cortn_stack_used(const struct cortn *self)
{
	const unsigned char *first;
	const unsigned char *top;

	if (cortn_stack_check(self) != CORTN_SUCCESS)
		return 0;
	first = self->c_stack + CORTN_STACK_BOUNDARY_GUARD_SIZE;
	top = cortn_stack_top(self);
	while (first < top && *first == CORTN_UNUSED_PATTERN)
		first++;
	return (size_t)(top - first);
}

void cortn_boot_main(struct cortn *self)
{
	self->c_entry(self, self->c_arg);
	self->c_state = CORTN_FINISHED;
	self->c_reason = CORTN_YIELD_FINISHED;
	cortn_switch(&self->c_context, &self->c_caller);

	/* Finished coroutine must never resume */
	self->c_state = CORTN_ERROR;
	self->c_reason = CORTN_YIELD_ERROR;
	for (;;)
		;
}

static int cortn_stack_boundary_guard_is_valid(const unsigned char *guard)
{
	size_t i;

	for (i = 0; i < CORTN_STACK_BOUNDARY_GUARD_SIZE; i++) {
		if (guard[i] != CORTN_STACK_BOUNDARY_GUARD_PATTERN)
			return 0;
	}
	return 1;
}

static unsigned char *cortn_stack_top(const struct cortn *self)
{
	uintptr_t top;

	top = (uintptr_t)(self->c_stack + self->c_stack_size -
			  CORTN_STACK_BOUNDARY_GUARD_SIZE);
	top &= ~(uintptr_t)0xf;
	return (unsigned char *)top;
}
