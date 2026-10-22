/* user.h */

#ifndef __USER_H__
#define __USER_H__

#include <stddef.h>

#include "param.h"

#ifdef __cplusplus
extern "C" {
#endif

struct proc;
struct file;

/*
 * proc.p_addr => user => u_ofile[] => file => inode
 * Execution owner keeps user alive through backend release
 */
struct user {
	int u_error;		/* Error code; caller clears before I/O */
	struct {
		ptrdiff_t r_val1;
		ptrdiff_t r_val2;
	} u_r;			/* System call results */
	struct proc *u_procp;	/* Owning process; NULL for host context */
	struct file *u_ofile[NOFILE]; /* Open file references */
};

/* Current proc selects p_addr; host context uses backend-local storage */
struct user *current_user(void);
#define u (*current_user())

#ifdef __cplusplus
}
#endif

#endif
