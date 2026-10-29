/* fio.c */

#include <limits.h>
#include <string.h>

#include "file.h"
#include "inode.h"
#include "param.h"
#include "sched.h"
#include "sys_error.h"

void finit(void)
{
	memset(file, 0, NFILE * sizeof(file[0]));
	memset(inode, 0, NINODE * sizeof(inode[0]));
}

struct file *getf(struct proc *proc, int fd)
{
	if (!proc || fd < 0 || fd >= NOFILE)
		return NULL;
	return proc->p_ofile[fd];
}

int ufalloc(struct proc *proc)
{
	int fd;

	if (!proc)
		return -SYS_EINVAL;
	for (fd = 0; fd < NOFILE; fd++) {
		if (!proc->p_ofile[fd])
			return fd;
	}
	return -SYS_EMFILE;
}

struct file *falloc(struct proc *proc, int *fd)
{
	struct file *fp;
	int new_fd;
	int i;

	if (!fd)
		return NULL;
	if (!proc) {
		*fd = -SYS_EINVAL;
		return NULL;
	}
	new_fd = ufalloc(proc);
	if (new_fd < 0) {
		*fd = new_fd;
		return NULL;
	}
	for (i = 0; i < NFILE; i++) {
		fp = &file[i];
		if (fp->f_count)
			continue;
		memset(fp, 0, sizeof(*fp));
		fp->f_count = 1;
		proc->p_ofile[new_fd] = fp;
		*fd = new_fd;
		return fp;
	}
	*fd = -SYS_ENFILE;
	return NULL;
}

void closef(struct file *fp)
{
	struct inode *ip;
	unsigned char flag;

	if (!fp || !fp->f_count)
		return;
	fp->f_count--;
	if (fp->f_count)
		return;
	ip = fp->f_inode;
	flag = fp->f_flag;
	memset(fp, 0, sizeof(*fp));
	if (ip && (flag & FPIPE)) {
		sched_wakeup((char *)ip + 2);
		sched_wakeup((char *)ip + 1);
	}
	if (ip)
		iput(ip);
}

int fd_install(struct proc *proc, struct file *fp)
{
	int fd;

	if (!proc || !fp || !fp->f_count)
		return -SYS_EINVAL;
	if (fp->f_count == UCHAR_MAX)
		return -SYS_ENFILE;
	fd = ufalloc(proc);
	if (fd < 0)
		return fd;
	proc->p_ofile[fd] = fp;
	fp->f_count++;
	return fd;
}

int fd_install_at(struct proc *proc, int fd, struct file *fp)
{
	if (!proc || fd < 0 || fd >= NOFILE || !fp || !fp->f_count)
		return -SYS_EINVAL;
	if (proc->p_ofile[fd])
		return -SYS_EMFILE;
	if (fp->f_count == UCHAR_MAX)
		return -SYS_ENFILE;
	proc->p_ofile[fd] = fp;
	fp->f_count++;
	return fd;
}

int fd_close(struct proc *proc, int fd)
{
	struct file *fp;

	fp = getf(proc, fd);
	if (!fp)
		return -SYS_EBADF;
	proc->p_ofile[fd] = NULL;
	closef(fp);
	return 0;
}

void close_all(struct proc *proc)
{
	int fd;

	if (!proc)
		return;
	for (fd = 0; fd < NOFILE; fd++) {
		if (proc->p_ofile[fd])
			fd_close(proc, fd);
	}
}

size_t file_in_use(void)
{
	size_t count;
	int i;

	count = 0;
	for (i = 0; i < NFILE; i++) {
		if (file[i].f_count)
			count++;
	}
	return count;
}
