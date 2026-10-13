/* pipe.h */

#ifndef __PIPE_H__
#define __PIPE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#include "buf.h"
#include "param.h"

#define PIPSIZ	BSIZE

#if NBUF < NINODE
#error "NBUF must be at least NINODE"
#endif

enum pipe_io_result {
	PIPE_IO_PROGRESS,
	PIPE_IO_WAIT_READ,
	PIPE_IO_WAIT_WRITE,
	PIPE_IO_EOF,
	PIPE_IO_BROKEN,
	PIPE_IO_ERROR
};

struct pipe_io_status {
	enum pipe_io_result io_result;
	ptrdiff_t io_count;
	int io_error;
};

struct file;
struct inode;
struct proc;

int pipe_for_proc(struct proc *proc, int fd[2]);
ptrdiff_t readp(struct proc *proc, struct file *fp, void *buf,
		size_t count);
ptrdiff_t writep(struct proc *proc, struct file *fp, const void *buf,
		 size_t count);
int plock(struct inode *ip);
void prele(struct inode *ip);
size_t pipe_in_use(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
