/*
 * sys_pipe() => pipe_create() => pipe_alloc() => inode and file allocation
 * pipe_for_proc() => pipe_alloc() with explicit target user
 */

#include "file.h"
#include "inode.h"
#include "param.h"
#include "pipe.h"
#include "sched.h"
#include "sys_error.h"
#include "user.h"

static void pipe_alloc(struct user *user);
static struct pipe_io_status pipe_status(enum pipe_io_result result,
					 ptrdiff_t count, int error);
static struct pipe_io_status pipe_read_once(struct file *fp, void *buf,
					    size_t count);
static struct pipe_io_status pipe_write_once(struct file *fp,
					     const void *buf,
					     size_t count);

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

ptrdiff_t readp(struct proc *proc, struct file *fp, void *buf, size_t count)
{
	struct pipe_io_status status;
	int result;

	if (!count)
		return 0;
	if (!buf)
		return -SYS_EINVAL;
	for (;;) {
		result = plock(fp ? fp->f_inode : NULL);
		if (result < 0)
			return result;
		status = pipe_read_once(fp, buf, count);
		prele(fp->f_inode);
		switch (status.io_result) {
		case PIPE_IO_PROGRESS:
		case PIPE_IO_EOF:
			return status.io_count;
		case PIPE_IO_WAIT_READ:
			if (!proc || !proc->p_backend)
				return -SYS_EAGAIN;
			result = sched_sleep(&fp->f_inode->i_read_chan,
					     WAIT_READ);
			if (result < 0)
				return result;
			break;
		default:
			return status.io_error;
		}
	}
}

ptrdiff_t writep(struct proc *proc, struct file *fp, const void *buf,
		 size_t count)
{
	const unsigned char *bytes;
	struct pipe_io_status status;
	size_t transferred;
	int result;

	if (!count)
		return 0;
	if (!buf)
		return -SYS_EINVAL;
	bytes = buf;
	transferred = 0;
	for (;;) {
		result = plock(fp ? fp->f_inode : NULL);
		if (result < 0)
			return result;
		status = pipe_write_once(fp, bytes + transferred,
					 count - transferred);
		prele(fp->f_inode);
		switch (status.io_result) {
		case PIPE_IO_PROGRESS:
			transferred += (size_t)status.io_count;
			if (transferred == count)
				return (ptrdiff_t)transferred;
			break;
		case PIPE_IO_WAIT_WRITE:
			if (!proc || !proc->p_backend) {
				if (transferred)
					return (ptrdiff_t)transferred;
				return -SYS_EAGAIN;
			}
			result = sched_sleep(&fp->f_inode->i_write_chan,
					     WAIT_WRITE);
			if (result < 0)
				return result;
			break;
		default:
			return status.io_error;
		}
	}
}

int plock(struct inode *ip)
{
	struct proc *proc;
	int result;

	if (!ip)
		return -SYS_EINVAL;
	while (ip->i_flag & ILOCK) {
		ip->i_flag |= IWANT;
		proc = current_proc();
		if (!proc || !proc->p_backend)
			return -SYS_EAGAIN;
		result = sched_sleep(&ip->i_lock_chan, WAIT_LOCK);
		if (result < 0)
			return result;
	}
	ip->i_flag |= ILOCK;
	return 0;
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

static struct pipe_io_status pipe_status(enum pipe_io_result result,
					 ptrdiff_t count, int error)
{
	struct pipe_io_status status;

	status.io_result = result;
	status.io_count = count;
	status.io_error = error;
	return status;
}

static struct pipe_io_status pipe_read_once(struct file *fp, void *buf,
					    size_t count)
{
	struct inode *ip;
	ptrdiff_t transferred;

	if (!fp || (fp->f_flag & (FREAD | FPIPE)) != (FREAD | FPIPE))
		return pipe_status(PIPE_IO_ERROR, 0, -SYS_EBADF);
	ip = fp->f_inode;
	if (!ip || !ip->i_count || !ip->i_buf)
		return pipe_status(PIPE_IO_ERROR, 0, -SYS_EBADF);
	if (!ip->i_size) {
		if (ip->i_count < 2)
			return pipe_status(PIPE_IO_EOF, 0, 0);
		return pipe_status(PIPE_IO_WAIT_READ, 0, 0);
	}
	transferred = readi(ip, &fp->f_offset, buf, count);
	if (transferred < 0)
		return pipe_status(PIPE_IO_ERROR, 0, (int)transferred);
	if (fp->f_offset == ip->i_size) {
		fp->f_offset = 0;
		ip->i_size = 0;
		sched_wakeup(&ip->i_write_chan);
	}
	return pipe_status(PIPE_IO_PROGRESS, transferred, 0);
}

static struct pipe_io_status pipe_write_once(struct file *fp,
					     const void *buf,
					     size_t count)
{
	struct inode *ip;
	size_t offset;
	ptrdiff_t transferred;

	if (!fp || (fp->f_flag & (FWRITE | FPIPE)) !=
		   (FWRITE | FPIPE))
		return pipe_status(PIPE_IO_ERROR, 0, -SYS_EBADF);
	ip = fp->f_inode;
	if (!ip || !ip->i_count || !ip->i_buf)
		return pipe_status(PIPE_IO_ERROR, 0, -SYS_EBADF);
	if (ip->i_count < 2)
		return pipe_status(PIPE_IO_BROKEN, 0, -SYS_EPIPE);
	if (ip->i_size >= PIPSIZ)
		return pipe_status(PIPE_IO_WAIT_WRITE, 0, 0);
	offset = ip->i_size;
	transferred = writei(ip, &offset, buf, count);
	if (transferred < 0)
		return pipe_status(PIPE_IO_ERROR, 0, (int)transferred);
	sched_wakeup(&ip->i_read_chan);
	return pipe_status(PIPE_IO_PROGRESS, transferred, 0);
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
