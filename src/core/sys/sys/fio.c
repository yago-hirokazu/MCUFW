/*
 * sys2.c / fd_close() => getf() => proc.p_ofile[] => file[]
 *   => invalid descriptor => proc.p_error
 */

#include <limits.h>
#include <string.h>

#include "file.h"
#include "inode.h"
#include "param.h"
#include "sched.h"
#include "sys_error.h"

void init_fsys(void)
{
	memset(file, 0, NFILE * sizeof(file[0]));
	memset(inode, 0, NINODE * sizeof(inode[0]));
}

/* Convert descriptor to file pointer and record lookup failure */
struct file *getf(struct proc *proc, int fd)
{
	struct file *fp = NULL;

	/* Check descriptor before indexing process file table */
	if (fd < 0 || fd >= NOFILE)
		goto bad;
	fp = proc->p_ofile[fd];
	if (fp != NULL)
		return fp;
bad:
	proc->p_error = -SYS_EBADF;
	return NULL;
}

int ufalloc(struct proc *proc)
{
	int fd = 0;

	for (fd = 0; fd < NOFILE; fd++) {
		if (!proc->p_ofile[fd])
			return fd;
	}
	return -SYS_EMFILE;
}

struct file *falloc(struct proc *proc, int *fd)
{
	struct file *fp = NULL;
	int new_fd = 0;
	int i = 0;

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
	struct inode *ip = NULL;
	unsigned char flag = 0;

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
	int fd = 0;

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
	struct file *fp = NULL;

	fp = getf(proc, fd);
	if (!fp)
		return -SYS_EBADF;
	proc->p_ofile[fd] = NULL;
	closef(fp);
	return 0;
}

void close_all(struct proc *proc)
{
	int fd = 0;

	for (fd = 0; fd < NOFILE; fd++) {
		if (proc->p_ofile[fd])
			fd_close(proc, fd);
	}
}

size_t file_in_use(void)
{
	size_t count = 0;
	int i = 0;

	count = 0;
	for (i = 0; i < NFILE; i++) {
		if (file[i].f_count)
			count++;
	}
	return count;
}
