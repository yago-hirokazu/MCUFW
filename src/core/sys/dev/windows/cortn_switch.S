	.text

	.set C_RSP, 0
	.set C_RBX, 8
	.set C_RBP, 16
	.set C_RDI, 24
	.set C_RSI, 32
	.set C_R12, 40
	.set C_R13, 48
	.set C_R14, 56
	.set C_R15, 64
	.set C_XMM6, 80
	.set C_MXCSR, 240
	.set C_X87_CONTROL, 244

	.globl cortn_switch
	.def cortn_switch; .scl 2; .type 32; .endef
	.seh_proc cortn_switch
cortn_switch:
	.seh_endprologue
	movq %rsp, C_RSP(%rcx)
	movq %rbx, C_RBX(%rcx)
	movq %rbp, C_RBP(%rcx)
	movq %rdi, C_RDI(%rcx)
	movq %rsi, C_RSI(%rcx)
	movq %r12, C_R12(%rcx)
	movq %r13, C_R13(%rcx)
	movq %r14, C_R14(%rcx)
	movq %r15, C_R15(%rcx)
	movdqu %xmm6, C_XMM6(%rcx)
	movdqu %xmm7, C_XMM6+16(%rcx)
	movdqu %xmm8, C_XMM6+32(%rcx)
	movdqu %xmm9, C_XMM6+48(%rcx)
	movdqu %xmm10, C_XMM6+64(%rcx)
	movdqu %xmm11, C_XMM6+80(%rcx)
	movdqu %xmm12, C_XMM6+96(%rcx)
	movdqu %xmm13, C_XMM6+112(%rcx)
	movdqu %xmm14, C_XMM6+128(%rcx)
	movdqu %xmm15, C_XMM6+144(%rcx)
	stmxcsr C_MXCSR(%rcx)
	fnstcw C_X87_CONTROL(%rcx)

	ldmxcsr C_MXCSR(%rdx)
	fldcw C_X87_CONTROL(%rdx)
	movdqu C_XMM6(%rdx), %xmm6
	movdqu C_XMM6+16(%rdx), %xmm7
	movdqu C_XMM6+32(%rdx), %xmm8
	movdqu C_XMM6+48(%rdx), %xmm9
	movdqu C_XMM6+64(%rdx), %xmm10
	movdqu C_XMM6+80(%rdx), %xmm11
	movdqu C_XMM6+96(%rdx), %xmm12
	movdqu C_XMM6+112(%rdx), %xmm13
	movdqu C_XMM6+128(%rdx), %xmm14
	movdqu C_XMM6+144(%rdx), %xmm15
	movq C_RBX(%rdx), %rbx
	movq C_RBP(%rdx), %rbp
	movq C_RDI(%rdx), %rdi
	movq C_RSI(%rdx), %rsi
	movq C_R12(%rdx), %r12
	movq C_R13(%rdx), %r13
	movq C_R14(%rdx), %r14
	movq C_R15(%rdx), %r15
	movq C_RSP(%rdx), %rsp
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
