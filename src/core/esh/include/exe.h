
/* exe.h */

#ifndef __EXE_H__
#define __EXE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "code.h"
#include "esh.h"
#include "esh_adapt.h"
#include "esh_sched.h"
#include "tty.h"

#if PROCS != ESH_PIPELINE_MAX
#error "PROCS must match ESH_PIPELINE_MAX"
#endif


struct proc {
	struct esh_proc p_exec;		/* Portable execution state */
	struct applet *p_cmd;		/* Command */
	char p_argc;			/* # of command arguments */
	char *p_argv[TTY_NARG];		/* Command arguments */
	char p_argbuf[TTY_NARG][SYMCHARS]; /* Expanded arguments */
};

struct exe {
	INT e_stack[STACKS];		/* Stack */
	int e_pc;			/* Program counter */
	int e_sp;			/* Stack pointer */
	char e_procs;			/* # of procs to be executed */
	int e_status;			/* Last command or pipeline status */
	struct proc e_proc[PROCS];	/* Process */
};


extern struct exe exe;
int exe_run_codes(struct code *code, struct exe *exe);
int exe_run_pipeline(struct proc *proc, int count);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
