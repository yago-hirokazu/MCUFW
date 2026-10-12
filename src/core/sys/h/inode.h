/* inode.h */

#ifndef __INODE_H__
#define __INODE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#include "sched.h"

#define ILOCK	0x01U
#define IWANT	0x10U

struct buf;

/* Inode owns pipe buffer while reference count is nonzero */
struct inode {
	unsigned char i_flag;
	unsigned char i_count;		/* File or construction references */
	struct buf *i_buf;
	size_t i_size;
	struct wait_chan i_lock_chan;
	struct wait_chan i_read_chan;
	struct wait_chan i_write_chan;
};

extern struct inode inode[];

struct inode *ialloc(void);
void iput(struct inode *ip);
ptrdiff_t readi(struct inode *ip, size_t *offset, void *buf, size_t count);
ptrdiff_t writei(struct inode *ip, size_t *offset, const void *buf,
		 size_t count);
size_t inode_in_use(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
