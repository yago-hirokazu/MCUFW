/* pipe.c */

#include <string.h>

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"

static struct esh_pipe_inode esh_pipe_table[ESH_PIPE_MAX];

static struct esh_io_status esh_io_status(enum esh_io_result result,
					  esh_ssize_t count, int error)
{
	struct esh_io_status status;

	status.io_result = result;
	status.io_count = count;
	status.io_error = error;
	return status;
}

void esh_pipe_pool_init(void)
{
	memset(esh_pipe_table, 0, sizeof(esh_pipe_table));
}

struct esh_pipe_inode *esh_pipe_inode_alloc(void)
{
	struct esh_pipe_inode *inode;
	size_t i;

	for (i = 0; i < ESH_PIPE_MAX; i++) {
		inode = &esh_pipe_table[i];
		if (inode->pi_used)
			continue;
		memset(inode, 0, sizeof(*inode));
		inode->pi_used = 1;
		return inode;
	}
	return NULL;
}

void esh_pipe_inode_discard(struct esh_pipe_inode *inode)
{
	if (!inode || inode->pi_refs)
		return;
	memset(inode, 0, sizeof(*inode));
}

void esh_pipe_endpoint_open(struct esh_pipe_inode *inode,
			    enum esh_file_direction direction)
{
	if (!inode || !inode->pi_used)
		return;
	inode->pi_refs++;
	if (direction == ESH_FILE_READ)
		inode->pi_readers++;
	else
		inode->pi_writers++;
}

void esh_pipe_endpoint_close(struct esh_pipe_inode *inode,
			     enum esh_file_direction direction)
{
	if (!inode || !inode->pi_used || !inode->pi_refs)
		return;
	if (direction == ESH_FILE_READ) {
		if (!inode->pi_readers)
			return;
		inode->pi_readers--;
	} else {
		if (!inode->pi_writers)
			return;
		inode->pi_writers--;
	}
	inode->pi_refs--;
	if (!inode->pi_readers && !inode->pi_writers)
		memset(inode, 0, sizeof(*inode));
}

int esh_pipe_for_proc(struct esh_proc *proc, int fd[2])
{
	struct esh_pipe_inode *inode;
	struct esh_file *read_file;
	struct esh_file *write_file;
	int read_fd;
	int write_fd;
	int error;

	if (!proc || !fd)
		return -ESH_EINVAL;
	inode = esh_pipe_inode_alloc();
	if (!inode)
		return -ESH_ENFILE;
	read_file = esh_file_alloc(ESH_FILE_READ, inode);
	if (!read_file) {
		error = -ESH_ENFILE;
		goto discard_inode;
	}
	write_file = esh_file_alloc(ESH_FILE_WRITE, inode);
	if (!write_file) {
		error = -ESH_ENFILE;
		goto put_read_file;
	}

	read_fd = esh_fd_install(&proc->ep_fd_table, read_file);
	if (read_fd < 0) {
		error = read_fd;
		goto put_write_file;
	}
	write_fd = esh_fd_install(&proc->ep_fd_table, write_file);
	if (write_fd < 0) {
		error = write_fd;
		goto close_read_fd;
	}

	/* Transfer construction references to fd table */
	esh_file_put(read_file);
	esh_file_put(write_file);
	fd[0] = read_fd;
	fd[1] = write_fd;
	return 0;

close_read_fd:
	esh_fd_close(&proc->ep_fd_table, read_fd);
put_write_file:
	esh_file_put(write_file);
put_read_file:
	esh_file_put(read_file);
	return error;

discard_inode:
	esh_pipe_inode_discard(inode);
	return error;
}

int esh_pipe(int fd[2])
{
	return esh_pipe_for_proc(esh_current_proc(), fd);
}

struct esh_io_status esh_pipe_read_nonblock(struct esh_proc *proc, int fd,
					    void *buf, size_t count)
{
	struct esh_pipe_inode *inode;
	struct esh_pipe_buffer *pipe;
	struct esh_file *file;
	unsigned char *out;
	size_t transferred;

	if (!count)
		return esh_io_status(ESH_IO_PROGRESS, 0, 0);
	if (!proc || !buf)
		return esh_io_status(ESH_IO_ERROR, 0, -ESH_EINVAL);
	file = esh_fd_get(&proc->ep_fd_table, fd);
	if (!file || file->f_direction != ESH_FILE_READ)
		return esh_io_status(ESH_IO_ERROR, 0, -ESH_EBADF);
	inode = file->f_inode;
	pipe = &inode->pi_buffer;
	if (!pipe->pb_count) {
		if (!inode->pi_writers)
			return esh_io_status(ESH_IO_EOF, 0, 0);
		return esh_io_status(ESH_IO_WAIT_READ, 0, 0);
	}

	transferred = count;
	if (transferred > pipe->pb_count)
		transferred = pipe->pb_count;
	out = buf;
	for (count = 0; count < transferred; count++) {
		out[count] = pipe->pb_data[pipe->pb_read];
		pipe->pb_read++;
		if (pipe->pb_read == ESH_PIPE_BUFFER_SIZE)
			pipe->pb_read = 0;
	}
	pipe->pb_count -= transferred;
	return esh_io_status(ESH_IO_PROGRESS, transferred, 0);
}

struct esh_io_status esh_pipe_write_nonblock(struct esh_proc *proc, int fd,
					     const void *buf,
					     size_t count)
{
	const unsigned char *in;
	struct esh_pipe_inode *inode;
	struct esh_pipe_buffer *pipe;
	struct esh_file *file;
	size_t available;
	size_t transferred;

	if (!count)
		return esh_io_status(ESH_IO_PROGRESS, 0, 0);
	if (!proc || !buf)
		return esh_io_status(ESH_IO_ERROR, 0, -ESH_EINVAL);
	file = esh_fd_get(&proc->ep_fd_table, fd);
	if (!file || file->f_direction != ESH_FILE_WRITE)
		return esh_io_status(ESH_IO_ERROR, 0, -ESH_EBADF);
	inode = file->f_inode;
	if (!inode->pi_readers)
		return esh_io_status(ESH_IO_BROKEN_PIPE, 0, -ESH_EPIPE);
	pipe = &inode->pi_buffer;
	available = ESH_PIPE_BUFFER_SIZE - pipe->pb_count;
	if (!available)
		return esh_io_status(ESH_IO_WAIT_WRITE, 0, 0);

	transferred = count;
	if (transferred > available)
		transferred = available;
	in = buf;
	for (count = 0; count < transferred; count++) {
		pipe->pb_data[pipe->pb_write] = in[count];
		pipe->pb_write++;
		if (pipe->pb_write == ESH_PIPE_BUFFER_SIZE)
			pipe->pb_write = 0;
	}
	pipe->pb_count += transferred;
	return esh_io_status(ESH_IO_PROGRESS, transferred, 0);
}

esh_ssize_t esh_read(int fd, void *buf, size_t count)
{
	struct esh_io_status status;

	status = esh_pipe_read_nonblock(esh_current_proc(), fd, buf, count);
	switch (status.io_result) {
	case ESH_IO_PROGRESS:
		return status.io_count;
	case ESH_IO_EOF:
		return 0;
	case ESH_IO_WAIT_READ:
		return -ESH_EAGAIN;
	default:
		return status.io_error;
	}
}

esh_ssize_t esh_write(int fd, const void *buf, size_t count)
{
	struct esh_io_status status;

	status = esh_pipe_write_nonblock(esh_current_proc(), fd, buf, count);
	switch (status.io_result) {
	case ESH_IO_PROGRESS:
		return status.io_count;
	case ESH_IO_WAIT_WRITE:
		return -ESH_EAGAIN;
	default:
		return status.io_error;
	}
}

int esh_close(int fd)
{
	struct esh_proc *proc;

	proc = esh_current_proc();
	if (!proc)
		return -ESH_EINVAL;
	return esh_fd_close(&proc->ep_fd_table, fd);
}

size_t esh_pipe_in_use(void)
{
	size_t count;
	size_t i;

	count = 0;
	for (i = 0; i < ESH_PIPE_MAX; i++) {
		if (esh_pipe_table[i].pi_used)
			count++;
	}
	return count;
}
