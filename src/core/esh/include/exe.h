
/* exe.h */

#ifndef __EXE_H__
#define __EXE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "code.h"
#include "esh.h"
#include "esh_adapt.h"
#include "tty.h"


struct proc {
	struct applet *p_cmd;		/* Command */
	char p_argc;			/* # of command arguments */
	char *p_argv[TTY_NARG];		/* Command arguments */
};

struct exe {
	INT e_stack[STACKS];		/* Stack */
	char e_pc;			/* Program counter */
	char e_sp;			/* Stackk pointer */
	char e_procs;			/* # of procs to be executed */
	struct proc e_proc[PROCS];	/* Process */
};


extern struct exe exe;
int exe_run_codes(struct code *code, struct exe *exe);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
