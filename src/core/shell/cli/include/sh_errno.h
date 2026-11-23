/* sh_errno.h */

#ifndef __SH_ERRNO_H__
#define __SH_ERRNO_H__

/* Keep errors independent from host errno values */
enum sh_error {
	SH_EBADF = 1,
	SH_EMFILE,
	SH_ENFILE,
	SH_ENOMEM,
	SH_EINVAL,
	SH_EPIPE,
	SH_EAGAIN,
	SH_ENOSYS
};

#endif
