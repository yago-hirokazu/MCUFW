/*
 * sys_pipe() => pipe_create() => pipe_alloc() => inode and file allocation
 * pipe_for_proc() => pipe_alloc() with explicit target user
 * readp() / writep() => readi() / writei() => iomove()
 */

#include <stdint.h>

#include "file.h"
#include "inode.h"
#include "param.h"
#include "pipe.h"
#include "sched.h"
#include "sys_error.h"
#include "user.h"

static void pipe_alloc(struct user *user);
static struct inode *pipe_inode(struct file *fp, int mode);
static void pipe_sleep(struct wait_chan *chan, enum wait_reason reason);

/* Return pipe descriptors through current user results */
void pipe_create(void)
{
	pipe_alloc(&u);
}

/* Prepare inactive target without changing calling user */
int pipe_for_proc(struct proc *proc, int fd[2])
{
	struct user *user;

	if (!proc || !proc->p_addr || !fd)
		return -SYS_EINVAL;
	user = proc->p_addr;
	user->u_error = 0;
	user->u_r.r_val1 = 0;
	user->u_r.r_val2 = 0;
	pipe_alloc(user);
	if (user->u_error)
		return -user->u_error;
	fd[0] = (int)user->u_r.r_val1;
	fd[1] = (int)user->u_r.r_val2;
	return 0;
}

/* Translate user error/results into public pipe API */
int sys_pipe(int fd[2])
{
	int result;

	kernel_lock();
	u.u_error = 0;
	u.u_r.r_val1 = 0;
	u.u_r.r_val2 = 0;
	if (!fd || !current_proc())
		u.u_error = SYS_EINVAL;
	else
		pipe_create();
	result = -u.u_error;
	if (!result) {
		fd[0] = (int)u.u_r.r_val1;
		fd[1] = (int)u.u_r.r_val2;
	}
	kernel_unlock();
	return result;
}

/* Read available data or wait for writer to supply it */
void readp(struct file *fp)
{
	struct inode *ip;

	if (!u.u_count || u.u_error)
		return;
	ip = pipe_inode(fp, FREAD);
	if (!ip)
		return;
	for (;;) {
		plock(ip);
		if (u.u_error)
			return;
		if (!ip->i_size) {
			prele(ip);
			if (ip->i_count < 2)
				return;
			pipe_sleep(&ip->i_read_chan, WAIT_READ);
			if (u.u_error)
				return;
			continue;
		}
		if (fp->f_offset > PIPSIZ) {
			u.u_error = SYS_EINVAL;
			prele(ip);
			return;
		}
		u.u_offset = (ptrdiff_t)fp->f_offset;
		readi(ip);
		fp->f_offset = (size_t)u.u_offset;
		if (!u.u_error && fp->f_offset == ip->i_size) {
			fp->f_offset = 0;
			ip->i_size = 0;
			sched_wakeup(&ip->i_write_chan);
		}
		prele(ip);
		return;
	}
}

/* Write bounded intervals and retain whole-request remainder */
void writep(struct file *fp)
{
	struct inode *ip;
	size_t remaining;
	size_t requested;
	size_t count;

	if (!u.u_count || u.u_error)
		return;
	ip = pipe_inode(fp, FWRITE);
	if (!ip)
		return;
	requested = u.u_count;
	remaining = requested;
	while (remaining) {
		plock(ip);
		if (u.u_error)
			break;
		if (ip->i_size > PIPSIZ) {
			u.u_error = SYS_EINVAL;
			prele(ip);
			break;
		}
		if (ip->i_count < 2) {
			u.u_error = SYS_EPIPE;
			prele(ip);
			break;
		}
		if (ip->i_size == PIPSIZ) {
			prele(ip);
			if ((!u.u_procp || !u.u_procp->p_xctx) &&
			    remaining != requested)
				break;
			pipe_sleep(&ip->i_write_chan, WAIT_WRITE);
			if (u.u_error)
				break;
			continue;
		}
		u.u_offset = (ptrdiff_t)ip->i_size;
		count = PIPSIZ - ip->i_size;
		if (count > remaining)
			count = remaining;
		u.u_count = count;
		writei(ip);
		remaining -= count - u.u_count;
		u.u_count = remaining;
		prele(ip);
		sched_wakeup(&ip->i_read_chan);
		if (u.u_error)
			break;
	}
	u.u_count = remaining;
}

/* Acquire inode lock or record failure without unlocking peer */
void plock(struct inode *ip)
{
	if (u.u_error)
		return;
	if (!ip) {
		u.u_error = SYS_EINVAL;
		return;
	}
	while (ip->i_flag & ILOCK) {
		ip->i_flag |= IWANT;
		pipe_sleep(&ip->i_lock_chan, WAIT_LOCK);
		if (u.u_error)
			return;
	}
	ip->i_flag |= ILOCK;
}

void prele(struct inode *ip)
{
	if (!ip)
		return;
	ip->i_flag &= ~ILOCK;
	if (ip->i_flag & IWANT) {
		ip->i_flag &= ~IWANT;
		sched_wakeup(&ip->i_lock_chan);
	}
}

size_t pipe_in_use(void)
{
	size_t count;
	int i;

	count = 0;
	for (i = 0; i < NINODE; i++) {
		if (inode[i].i_count)
			count++;
	}
	return count;
}

/* Allocate complete pipe or release only resources acquired here */
static void pipe_alloc(struct user *user)
{
	struct inode *ip;
	struct file *rf;
	struct file *wf;
	int read_fd;
	int error = 0;

	if (!user->u_procp) {
		user->u_error = SYS_EINVAL;
		return;
	}
	ip = ialloc_error(&error);
	if (!ip) {
		user->u_error = error;
		return;
	}
	rf = falloc_for_user(user);
	if (!rf)
		goto put_inode;
	read_fd = (int)user->u_r.r_val1;
	wf = falloc_for_user(user);
	if (!wf) {
		user->u_ofile[read_fd] = NULL;
		closef(rf);
		goto put_inode;
	}
	user->u_r.r_val2 = user->u_r.r_val1;
	user->u_r.r_val1 = read_fd;
	rf->f_flag = FREAD | FPIPE;
	rf->f_inode = ip;
	wf->f_flag = FWRITE | FPIPE;
	wf->f_inode = ip;
	ip->i_count = 2;
	return;

put_inode:
	iput(ip);
}

/* Validate current request and pipe endpoint before waiting */
static struct inode *pipe_inode(struct file *fp, int mode)
{
	if (!u.u_base || u.u_count > PTRDIFF_MAX) {
		u.u_error = SYS_EINVAL;
		return NULL;
	}
	if (!fp || (fp->f_flag & (mode | FPIPE)) != (mode | FPIPE) ||
	    !fp->f_inode || !fp->f_inode->i_count || !fp->f_inode->i_buf) {
		u.u_error = SYS_EBADF;
		return NULL;
	}
	return fp->f_inode;
}

/* Sleep with kernel lock held and preserve originating user state */
static void pipe_sleep(struct wait_chan *chan, enum wait_reason reason)
{
	int result;

	if (!u.u_procp || !u.u_procp->p_xctx) {
		u.u_error = SYS_EAGAIN;
		return;
	}
	result = sched_sleep(chan, reason);
	if (result < 0)
		u.u_error = -result;
}
