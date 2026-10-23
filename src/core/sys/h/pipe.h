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

struct file;
struct inode;
struct proc;

int pipe_for_proc(struct proc *proc, int fd[2]);
void pipe_create(void);
/* Current user carries transfer state; caller holds kernel lock */
void readp(struct file *fp);
void writep(struct file *fp);
void plock(struct inode *ip);
void prele(struct inode *ip);
size_t pipe_in_use(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
