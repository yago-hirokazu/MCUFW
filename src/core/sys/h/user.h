/* user.h */

#ifndef __USER_H__
#define __USER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Execution owner keeps storage alive through backend release */
struct user {
	int u_error;		/* Error code; caller clears before I/O */
};

/* Current proc selects p_addr; host context uses backend-local storage */
struct user *current_user(void);
#define u (*current_user())

#ifdef __cplusplus
}
#endif

#endif
