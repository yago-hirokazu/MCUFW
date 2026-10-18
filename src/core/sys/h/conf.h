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
	unsigned int d_flags;
};

/* Strategy completes accepted requests before returning */
#define BDEV_SYNC	0x01U

#define BDEV_QUEUE_FAILED	(-1)
#define BDEV_QUEUE_DONE		0
#define BDEV_QUEUE_RETRY	1

extern struct bdevsw bdevsw[];
extern int nblkdev;

/* Caller holds kernel lock; no internal lock or wait */
struct bdevsw *bdevsw_lookup(unsigned int dev);
/* Validate ownership independently of device registration */
int bdev_check_buffer_available(struct buf *bp);
/* Validate request without submission or state change */
int bdev_check_io_request_valid(struct buf *bp);
/* Return 0 on acceptance; rejection leaves buffer and queues unchanged */
int bdev_strategy(struct buf *bp);

void bdev_queue_init(struct bdev_queue *queue);
int bdev_queue_enqueue(struct bdev_queue *queue, struct buf *bp);
struct buf *bdev_queue_start(struct bdev_queue *queue);
struct buf *bdev_queue_complete(struct bdev_queue *queue);
int bdev_queue_error(struct bdev_queue *queue, unsigned int retry_limit,
		     int error);

#endif
