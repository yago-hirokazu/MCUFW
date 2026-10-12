/* esh_pipe.h */

#ifndef __ESH_PIPE_H__
#define __ESH_PIPE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#include "buf.h"
#include "esh_sched.h"

#define ESH_PIPE_MAX		3
#define ESH_PIPE_BUFFER_SIZE	BSIZE

#if NBUF < ESH_PIPE_MAX
#error "NBUF must be at least ESH_PIPE_MAX"
#endif

/* Inode owns buffer and remains live while either endpoint is referenced */
struct esh_pipe_inode {
	unsigned char pi_used;		/* Entry allocated from inode pool */
	unsigned char pi_refs;		/* Number of live endpoint files */
	unsigned char pi_readers;	/* Number of read endpoint files */
	unsigned char pi_writers;	/* Number of write endpoint files */
	struct buf *pi_buf;
	size_t pi_read;
	size_t pi_write;
	size_t pi_count;
	struct esh_wait_chan pi_read_chan;
	struct esh_wait_chan pi_write_chan;
};

enum esh_io_result {
	ESH_IO_PROGRESS,
	ESH_IO_WAIT_READ,
	ESH_IO_WAIT_WRITE,
	ESH_IO_EOF,
	ESH_IO_BROKEN_PIPE,
	ESH_IO_ERROR
};

struct esh_io_status {
	enum esh_io_result io_result;
	ptrdiff_t io_count;
	int io_error;
};

void esh_pipe_pool_init(void);
struct esh_pipe_inode *esh_pipe_inode_alloc(void);
void esh_pipe_inode_discard(struct esh_pipe_inode *inode);
void esh_pipe_endpoint_open(struct esh_pipe_inode *inode,
			    enum esh_file_direction direction);
void esh_pipe_endpoint_close(struct esh_pipe_inode *inode,
			     enum esh_file_direction direction);
int esh_pipe_for_proc(struct esh_proc *proc, int fd[2]);
struct esh_io_status esh_pipe_read_nonblock(struct esh_proc *proc, int fd,
					    void *buf, size_t count);
struct esh_io_status esh_pipe_write_nonblock(struct esh_proc *proc, int fd,
					     const void *buf,
					     size_t count);
size_t esh_pipe_in_use(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
