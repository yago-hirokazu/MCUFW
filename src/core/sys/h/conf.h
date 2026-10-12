/* conf.h */

#ifndef __CONF_H__
#define __CONF_H__

struct buf;

struct bdev_queue {
	struct buf *dq_head;
	struct buf *dq_tail;
	struct buf *dq_active;
	unsigned int dq_errcnt;
};

struct bdevsw {
	int (*d_strategy)(struct buf *bp);
	struct buf *d_tab;
	struct bdev_queue *d_queue;
};

extern struct bdevsw bdevsw[];
extern int nblkdev;

#endif
