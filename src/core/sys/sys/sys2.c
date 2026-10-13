/* sys2.c */

#include "file.h"
#include "pipe.h"
#include "sched.h"
#include "sys_error.h"

ptrdiff_t sys_read(int fd, void *buf, size_t count)
{
	struct proc *proc;
	struct file *fp;
	ptrdiff_t result;

	if (!count)
		return 0;
	if (!buf)
		return -SYS_EINVAL;
	proc = current_proc();
	kernel_lock();
	fp = getf(proc, fd);
	if (!fp || !(fp->f_flag & FREAD)) {
		result = -SYS_EBADF;
		goto out;
	}
	result = readp(proc, fp, buf, count);
out:
	kernel_unlock();
	return result;
}

ptrdiff_t sys_write(int fd, const void *buf, size_t count)
{
	struct proc *proc;
	struct file *fp;
	ptrdiff_t result;

	if (!count)
		return 0;
	if (!buf)
		return -SYS_EINVAL;
	proc = current_proc();
	kernel_lock();
	fp = getf(proc, fd);
	if (!fp || !(fp->f_flag & FWRITE)) {
		result = -SYS_EBADF;
		goto out;
	}
	result = writep(proc, fp, buf, count);
out:
	kernel_unlock();
	return result;
}

int sys_close(int fd)
{
	struct proc *proc;
	int result;

	proc = current_proc();
	if (!proc)
		return -SYS_EINVAL;
	kernel_lock();
	result = fd_close(proc, fd);
	kernel_unlock();
	return result;
}
