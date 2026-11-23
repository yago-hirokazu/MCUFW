/*
 * sys_dup() / sys_dup2() => current_proc() / sys_lock()
 *   => fddup() / fddup2() => getf(src) => shared file
 *     => fdalloc(dst) / fdassign(dst) => destination descriptor
 * fddup2() => fdfree(dst) => closef() => release replaced file
 * V6 sys/ken/sys3.c: dup; V7 usr/sys/sys/sys3.c: dup2 branch
 */

#include <limits.h>

#include "file.h"
#include "sched.h"
#include "sys_error.h"

/* Duplicate source descriptor into lowest free destination slot */
int fddup(struct proc *src, int oldfd, struct proc *dst)
{
	struct file *fp = NULL;

	/* Share open file, including offset and access flags */
	fp = getf(src, oldfd);
	if (fp == NULL)
		return -SYS_EBADF;
	return fdalloc(dst, fp);
}

/* Duplicate source descriptor, replacing specified destination slot */
int fddup2(struct proc *src, int oldfd, struct proc *dst, int newfd)
{
	struct file *fp = NULL;

	/* Validate both descriptors before releasing destination */
	fp = getf(src, oldfd);
	if (fp == NULL)
		return -SYS_EBADF;
	if (newfd < 0 || newfd >= NOFILE)
		return -SYS_EBADF;
	if (src == dst && oldfd == newfd)
		return newfd;

	/* Replacing alias has no net increase in file references */
	if (fp->f_count == UCHAR_MAX && dst->p_ofile[newfd] != fp)
		return -SYS_ENFILE;
	if (dst->p_ofile[newfd] != NULL)
		fdfree(dst, newfd);
	return fdassign(dst, newfd, fp);
}

/* Duplicate descriptor within current process under sys lock */
int sys_dup(int oldfd)
{
	struct proc *proc = NULL;
	int result = 0;

	proc = current_proc();
	sys_lock();
	/* Start new request and retain negative error convention */
	proc->p_error = 0;
	result = fddup(proc, oldfd, proc);
	if (result < 0)
		proc->p_error = result;
	sys_unlock();
	return result;
}

/* Replace descriptor within current process under sys lock */
int sys_dup2(int oldfd, int newfd)
{
	struct proc *proc = NULL;
	int result = 0;

	proc = current_proc();
	sys_lock();
	/* Keep validation, close and assignment in single lock interval */
	proc->p_error = 0;
	result = fddup2(proc, oldfd, proc, newfd);
	if (result < 0)
		proc->p_error = result;
	sys_unlock();
	return result;
}
