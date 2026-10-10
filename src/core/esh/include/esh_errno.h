/* esh_errno.h */

#ifndef __ESH_ERRNO_H__
#define __ESH_ERRNO_H__

/* Keep errors independent from host errno values */
enum esh_error {
	ESH_EBADF = 1,
	ESH_EMFILE,
	ESH_ENFILE,
	ESH_ENOMEM,
	ESH_EINVAL,
	ESH_EPIPE,
	ESH_EAGAIN,
	ESH_ENOSYS
};

#endif
