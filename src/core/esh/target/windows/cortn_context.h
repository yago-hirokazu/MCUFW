/* cortn_context.h */

#ifndef __CORTN_CONTEXT_H__
#define __CORTN_CONTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

struct cortn;

struct cortn_xmm {
	uint64_t cx_low;
	uint64_t cx_high;
};

/* Layout offsets must match cortn_switch.S */
struct cortn_context {
	uint64_t cc_rsp;
	uint64_t cc_rbx;
	uint64_t cc_rbp;
	uint64_t cc_rdi;
	uint64_t cc_rsi;
	uint64_t cc_r12;
	uint64_t cc_r13;
	uint64_t cc_r14;
	uint64_t cc_r15;
	uint64_t cc_padding;
	struct cortn_xmm cc_xmm6_15[10];
	uint32_t cc_mxcsr;
	uint16_t cc_x87_control;
	uint16_t cc_reserved;
};

void cortn_context_setup(struct cortn_context *context, void *stack_top,
			 struct cortn *self);
void cortn_switch(struct cortn_context *from, struct cortn_context *to);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
