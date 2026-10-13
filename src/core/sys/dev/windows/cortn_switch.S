	.text

	.set CC_RSP, 0
	.set CC_RBX, 8
	.set CC_RBP, 16
	.set CC_RDI, 24
	.set CC_RSI, 32
	.set CC_R12, 40
	.set CC_R13, 48
	.set CC_R14, 56
	.set CC_R15, 64
	.set CC_XMM6, 80
	.set CC_MXCSR, 240
	.set CC_X87_CONTROL, 244

	.globl cortn_switch
	.def cortn_switch; .scl 2; .type 32; .endef
	.seh_proc cortn_switch
cortn_switch:
	.seh_endprologue
	movq %rsp, CC_RSP(%rcx)
	movq %rbx, CC_RBX(%rcx)
	movq %rbp, CC_RBP(%rcx)
	movq %rdi, CC_RDI(%rcx)
	movq %rsi, CC_RSI(%rcx)
	movq %r12, CC_R12(%rcx)
	movq %r13, CC_R13(%rcx)
	movq %r14, CC_R14(%rcx)
	movq %r15, CC_R15(%rcx)
	movdqu %xmm6, CC_XMM6(%rcx)
	movdqu %xmm7, CC_XMM6+16(%rcx)
	movdqu %xmm8, CC_XMM6+32(%rcx)
	movdqu %xmm9, CC_XMM6+48(%rcx)
	movdqu %xmm10, CC_XMM6+64(%rcx)
	movdqu %xmm11, CC_XMM6+80(%rcx)
	movdqu %xmm12, CC_XMM6+96(%rcx)
	movdqu %xmm13, CC_XMM6+112(%rcx)
	movdqu %xmm14, CC_XMM6+128(%rcx)
	movdqu %xmm15, CC_XMM6+144(%rcx)
	stmxcsr CC_MXCSR(%rcx)
	fnstcw CC_X87_CONTROL(%rcx)

	ldmxcsr CC_MXCSR(%rdx)
	fldcw CC_X87_CONTROL(%rdx)
	movdqu CC_XMM6(%rdx), %xmm6
	movdqu CC_XMM6+16(%rdx), %xmm7
	movdqu CC_XMM6+32(%rdx), %xmm8
	movdqu CC_XMM6+48(%rdx), %xmm9
	movdqu CC_XMM6+64(%rdx), %xmm10
	movdqu CC_XMM6+80(%rdx), %xmm11
	movdqu CC_XMM6+96(%rdx), %xmm12
	movdqu CC_XMM6+112(%rdx), %xmm13
	movdqu CC_XMM6+128(%rdx), %xmm14
	movdqu CC_XMM6+144(%rdx), %xmm15
	movq CC_RBX(%rdx), %rbx
	movq CC_RBP(%rdx), %rbp
	movq CC_RDI(%rdx), %rdi
	movq CC_RSI(%rdx), %rsi
	movq CC_R12(%rdx), %r12
	movq CC_R13(%rdx), %r13
	movq CC_R14(%rdx), %r14
	movq CC_R15(%rdx), %r15
	movq CC_RSP(%rdx), %rsp
	ret
	.seh_endproc

	.globl cortn_boot
	.def cortn_boot; .scl 2; .type 32; .endef
	.seh_proc cortn_boot
cortn_boot:
	subq $40, %rsp
	.seh_stackalloc 40
	.seh_endprologue
	movq %r12, %rcx
	call cortn_boot_main
	ud2
	.seh_endproc
