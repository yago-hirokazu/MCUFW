/* pipe.c */

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"

int esh_pipe(int fd[2])
{
	(void)fd;
	return -ESH_ENOSYS;
}

esh_ssize_t esh_read(int fd, void *buf, size_t count)
{
	(void)fd;
	(void)buf;
	if (!count)
		return 0;
	return -ESH_ENOSYS;
}

esh_ssize_t esh_write(int fd, const void *buf, size_t count)
{
	(void)fd;
	(void)buf;
	if (!count)
		return 0;
	return -ESH_ENOSYS;
}

int esh_close(int fd)
{
	(void)fd;
	return -ESH_ENOSYS;
}
