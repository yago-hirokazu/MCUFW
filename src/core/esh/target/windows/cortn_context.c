/* cortn_context.c */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "cortn.h"

extern void cortn_boot(void);

#define CORTN_OFFSET_CHECK(member, value) \
	typedef char member##_offset_mismatch[ \
		offsetof(struct cortn_context, member) == (value) ? 1 : -1]

CORTN_OFFSET_CHECK(cc_rsp, 0);
CORTN_OFFSET_CHECK(cc_rbx, 8);
CORTN_OFFSET_CHECK(cc_xmm6_15, 80);
CORTN_OFFSET_CHECK(cc_mxcsr, 240);
CORTN_OFFSET_CHECK(cc_x87_control, 244);

void cortn_context_setup(struct cortn_context *context, void *stack_top,
			 struct cortn *self)
{
	uintptr_t *stack;
	uint32_t mxcsr;
	uint16_t control;

	memset(context, 0, sizeof(*context));
	stack = stack_top;
	stack[-2] = (uintptr_t)cortn_boot;
	stack[-1] = 0;
	context->cc_rsp = (uintptr_t)&stack[-2];
	context->cc_r12 = (uintptr_t)self;

	__asm__ volatile ("stmxcsr %0" : "=m" (mxcsr));
	__asm__ volatile ("fnstcw %0" : "=m" (control));
	context->cc_mxcsr = mxcsr;
	context->cc_x87_control = control;
}
