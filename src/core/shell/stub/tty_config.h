
/* tty_cfg.h */

#ifndef __TTY_CFG_H__
#define __TTY_CFG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#undef	TTY_NARG
#undef	TTY_NHIST
#undef	TTY_NBUF

#define	TTY_NARG	32	/* Max # of arguments used by a command */
#define	TTY_NHIST	8	/* Max # of commands logged by history */
#define	TTY_NBUF	64	/* Size of tty buffer */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

