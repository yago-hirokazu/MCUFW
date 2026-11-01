/**
 * @file tty.h
 */

#ifndef __TTY__
#define __TTY__

/** @addtogroup
 *  @ingroup
 *  @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
 * STANDARD INCLUDE FILES
 **************************************************************************/


/**************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 **************************************************************************/
#include "qbyte.h"


/**************************************************************************
 * EXPORTED DEFINITIONS
 **************************************************************************/
#ifndef TTY_NARG
#define TTY_NARG	16
#endif

#ifndef TTY_NHIST
#define TTY_NHIST	2
#endif

#ifndef TTY_NBUF
#define TTY_NBUF	64
#endif

#define	DELIM_LF	0x0001
#define	DELIM_SP	0x0002
#define	DO_ECHO		1
#define	DONT_ECHO	0
#define ONLY_LOWER	0


/**************************************************************************
 * EXPORTED TYPES
 **************************************************************************/
struct tty_arg {
	int c;			/* argument Counts */
	char *v[TTY_NARG];	/* argument Vectors */
};


/**************************************************************************
 * EXPORTED VARIABLES
 **************************************************************************/


/**************************************************************************
 * IMPORTED FUNCTIONS
 **************************************************************************/


/**************************************************************************
 * EXPORTED FUNCTIONS
 **************************************************************************/
extern void ttyinit(void);
extern void ttytask(int c);
extern int scan(int c, struct qbyte *line_p);
extern struct tty_arg *tokenize(char *line, struct tty_arg *arg);
extern int is_endofline(char c);
extern int is_before_space(struct qbyte *line_p);


#ifdef __cplusplus
}
#endif

/* End of doxygen group */
/*! @} */

#endif
