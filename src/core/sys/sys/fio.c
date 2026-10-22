/*
 * fd helpers => proc.p_addr => user.u_ofile[] => file => inode
 * close_all() => fd_close() => closef() => iput()
 */

#include <limits.h>
#include <string.h>

#include "file.h"
#include "inode.h"
#include "param.h"
#include "sched.h"
#include "sys_error.h"
#include "user.h"

static int find_fd(struct user *user);
static int alloc_fd(struct user *user);

void finit(void)
{
	memset(file, 0, NFILE * sizeof(file[0]));
	memset(inode, 0, NINODE * sizeof(inode[0]));
}

/* Look up target descriptor without changing error or results */
struct file *getf_for_proc(struct proc *proc, int fd)
{
	if (!proc || !proc->p_addr || fd < 0 || fd >= NOFILE)
		return NULL;
	return proc->p_addr->u_ofile[fd];
}

/* Convert current user descriptor into file reference */
struct file *getf(int fd)
{
	struct file *fp;

	fp = getf_for_proc(u.u_procp, fd);
	if (!fp)
		u.u_error = SYS_EBADF;
	return fp;
}

/* Find current user descriptor and record return value */
int ufalloc(void)
{
	return alloc_fd(&u);
}

/* Allocate current user descriptor and file */
struct file *falloc(void)
{
	return falloc_for_user(&u);
}

/* Share file allocation with preparation of another user */
struct file *falloc_for_user(struct user *user)
{
	struct file *fp;
	int fd;
	int i;

	fd = alloc_fd(user);
	if (fd < 0)
		return NULL;
	for (i = 0; i < NFILE; i++) {
		fp = &file[i];
		if (fp->f_count)
			continue;
		memset(fp, 0, sizeof(*fp));
		fp->f_count = 1;
		user->u_ofile[fd] = fp;
		return fp;
	}
	user->u_error = SYS_ENFILE;
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
		sched_wakeup(&ip->i_read_chan);
		sched_wakeup(&ip->i_write_chan);
	}
	if (ip)
		iput(ip);
}

int fd_install(struct proc *proc, struct file *fp)
{
	int fd;

	if (!proc || !proc->p_addr || !fp || !fp->f_count)
		return -SYS_EINVAL;
	if (fp->f_count == UCHAR_MAX)
		return -SYS_ENFILE;
	fd = find_fd(proc->p_addr);
	if (fd < 0)
		return fd;
	proc->p_addr->u_ofile[fd] = fp;
	fp->f_count++;
	return fd;
}

int fd_install_at(struct proc *proc, int fd, struct file *fp)
{
	if (!proc || !proc->p_addr || fd < 0 || fd >= NOFILE ||
	    !fp || !fp->f_count)
		return -SYS_EINVAL;
	if (proc->p_addr->u_ofile[fd])
		return -SYS_EMFILE;
	if (fp->f_count == UCHAR_MAX)
		return -SYS_ENFILE;
	proc->p_addr->u_ofile[fd] = fp;
	fp->f_count++;
	return fd;
}

int fd_close(struct proc *proc, int fd)
{
	struct file *fp;

	fp = getf_for_proc(proc, fd);
	if (!fp)
		return -SYS_EBADF;
	proc->p_addr->u_ofile[fd] = NULL;
	closef(fp);
	return 0;
}

void close_all(struct proc *proc)
{
	int fd;

	if (!proc || !proc->p_addr)
		return;
	for (fd = 0; fd < NOFILE; fd++) {
		if (proc->p_addr->u_ofile[fd])
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

/* Find free slot without changing target user state */
static int find_fd(struct user *user)
{
	int fd;

	for (fd = 0; fd < NOFILE; fd++) {
		if (!user->u_ofile[fd])
			return fd;
	}
	return -SYS_EMFILE;
}

/* Record descriptor result or allocation error for target user */
static int alloc_fd(struct user *user)
{
	int fd;

	if (!user->u_procp) {
		user->u_error = SYS_EINVAL;
		return -1;
	}
	fd = find_fd(user);
	if (fd < 0) {
		user->u_error = -fd;
		return -1;
	}
	user->u_r.r_val1 = fd;
	return fd;
}
