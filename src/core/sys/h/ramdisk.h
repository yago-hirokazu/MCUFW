/* ramdisk.h */

#ifndef __RAMDISK_H__
#define __RAMDISK_H__

#define RAM_MAJOR	1U
#define RAM_NDEV	2U
#define RAM_NBLK	16UL

struct buf;
struct bdev_io_queue;

#ifdef __cplusplus
extern "C" {
#endif

extern struct bdev_io_queue ramqueue;

/* Clear storage only in quiescent state; follow with binit */
void raminit(void);
/* Caller holds kernel lock; owned, unqueued request completes on return */
void ramstrategy(struct buf *bp);

#ifdef __cplusplus
}
#endif

#endif
