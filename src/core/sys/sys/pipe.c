/* pipe.c */

#include "file.h"
#include "inode.h"
#include "param.h"
#include "pipe.h"
#include "sched.h"
#include "sys_error.h"

static struct pipe_io_status pipe_status(enum pipe_io_result result,
					 ptrdiff_t count, int error);
static struct pipe_io_status pipe_read_once(struct file *fp, void *buf,
					    size_t count);
static struct pipe_io_status pipe_write_once(struct file *fp,
					     const void *buf,
					     size_t count);

int pipe_for_proc(struct proc *proc, int fd[2])
{
	struct inode *ip;
	struct file *rf;
	struct file *wf;
	int read_fd;
	int write_fd;
	int error;

	if (!proc || !fd)
		return -SYS_EINVAL;
	ip = ialloc();
	if (!ip)
		return -SYS_ENFILE;
	rf = falloc(proc, &read_fd);
	if (!rf) {
		error = read_fd;
		goto put_inode;
	}
	wf = falloc(proc, &write_fd);
	if (!wf) {
		error = write_fd;
		goto close_read;
	}

	rf->f_flag = FREAD | FPIPE;
	rf->f_inode = ip;
	rf->f_offset = 0;
	wf->f_flag = FWRITE | FPIPE;
	wf->f_inode = ip;
	wf->f_offset = 0;
	ip->i_count = 2;
	fd[0] = read_fd;
	fd[1] = write_fd;
	return 0;

close_read:
	fd_close(proc, read_fd);
put_inode:
	iput(ip);
	return error;
}

int sys_pipe(int fd[2])
{
	int result;

	kernel_lock();
	result = pipe_for_proc(current_proc(), fd);
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
			if (!proc || !proc->p_ctx)
				return -SYS_EAGAIN;
			result = sched_sleep(&fp->f_inode->i_rchan,
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
			if (!proc || !proc->p_ctx) {
				if (transferred)
					return (ptrdiff_t)transferred;
				return -SYS_EAGAIN;
			}
			result = sched_sleep(&fp->f_inode->i_wchan,
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
		if (!proc || !proc->p_ctx)
			return -SYS_EAGAIN;
		result = sched_sleep(&ip->i_lchan, WAIT_LOCK);
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
		sched_wakeup(&ip->i_lchan);
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
		sched_wakeup(&ip->i_wchan);
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
	sched_wakeup(&ip->i_rchan);
	return pipe_status(PIPE_IO_PROGRESS, transferred, 0);
}
