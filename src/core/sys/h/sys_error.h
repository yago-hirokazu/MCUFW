/* sys_error.h */

#ifndef __SYS_ERROR_H__
#define __SYS_ERROR_H__

/* Keep errors independent from host errno values */
enum sys_error {
	SYS_EBADF = 1,
	SYS_EMFILE,
	SYS_ENFILE,
	SYS_ENOMEM,
	SYS_EINVAL,
	SYS_EPIPE,
	SYS_EAGAIN,
	SYS_ENOSYS
};

#endif
