/*
 * sys_read() / sys_write() => rdwr() => readp() / writep()
 * Request minus user.u_count => u_r.r_val1; u_error takes precedence
 * sys_close() => fd_close() => closef() => iput()
 */

#include <stdint.h>

#include "file.h"
#include "pipe.h"
#include "sched.h"
#include "sys_error.h"
#include "user.h"

static ptrdiff_t rdwr(int fd, const void *buf, size_t count, int mode);

/* Read pipe using current user context */
ptrdiff_t sys_read(int fd, void *buf, size_t count)
{
	return rdwr(fd, buf, count, FREAD);
}

/* Write pipe without modifying source buffer */
ptrdiff_t sys_write(int fd, const void *buf, size_t count)
{
	return rdwr(fd, buf, count, FWRITE);
}

/* Close current descriptor and translate user error */
int sys_close(int fd)
{
	int result;

	kernel_lock();
	u.u_error = 0;
	u.u_r.r_val1 = 0;
	u.u_r.r_val2 = 0;
	if (!current_proc()) {
		u.u_error = SYS_EINVAL;
	} else {
		result = fd_close(current_proc(), fd);
		if (result < 0)
			u.u_error = -result;
	}
	result = -u.u_error;
	kernel_unlock();
	return result;
}

/* Prepare request, transfer through user and translate result */
static ptrdiff_t rdwr(int fd, const void *buf, size_t count, int mode)
{
	struct file *fp;
	ptrdiff_t result;

	kernel_lock();
	u.u_error = 0;
	u.u_r.r_val1 = 0;
	u.u_r.r_val2 = 0;
	u.u_base = (unsigned char *)buf;
	u.u_count = count;
	u.u_offset = 0;
	if (!count)
		goto out;
	if (!buf || count > PTRDIFF_MAX) {
		u.u_error = SYS_EINVAL;
		goto out;
	}
	fp = getf(fd);
	if (!fp)
		goto out;
	if (!(fp->f_flag & mode)) {
		u.u_error = SYS_EBADF;
		goto out;
	}
	if (mode == FREAD)
		readp(fp);
	else
		writep(fp);
out:
	u.u_r.r_val1 = (ptrdiff_t)(count - u.u_count);
	result = u.u_error ? -u.u_error : u.u_r.r_val1;
	kernel_unlock();
	return result;
}
