#include <stddef.h>

#include "buf.h"
#include "conf.h"
#include "sys_error.h"

static int bdev_queue_contains(struct bdev_queue *queue, struct buf *target);

int bdev_strategy(struct buf *bp)
{
	int result;

	result = bdev_request_check(bp);
	if (result < 0)
		return result;
	return bdevsw[major(bp->b_dev)].d_strategy(bp);
}

int bdev_request_check(struct buf *bp)
{
	struct bdevsw *dp;
	int found;
	int result;
	int i;

	if (!bp || !(bp->b_flags & B_BUSY))
		return -SYS_EINVAL;
	dp = bdev_get(bp->b_dev);
	if (!dp || !dp->d_strategy)
		return -SYS_ENODEV;
	found = 0;
	for (i = 0; i < nblkdev; i++) {
		if (!bdevsw[i].d_queue)
			continue;
		result = bdev_queue_contains(bdevsw[i].d_queue, bp);
		if (result < 0)
			return result;
		found |= result;
	}
	if (found)
		return -SYS_EAGAIN;
	return 0;
}

void bdev_queue_init(struct bdev_queue *queue)
{
	if (!queue)
		return;
	queue->dq_head = NULL;
	queue->dq_tail = NULL;
	queue->dq_active = NULL;
	queue->dq_errcnt = 0;
}

int bdev_queue_enqueue(struct bdev_queue *queue, struct buf *bp)
{
	if (!queue || !bp || !(bp->b_flags & B_BUSY))
		return -1;
	if (bdev_queue_contains(queue, bp))
		return -1;

	bp->b_io_next = NULL;
	if (queue->dq_tail)
		queue->dq_tail->b_io_next = bp;
	else
		queue->dq_head = bp;
	queue->dq_tail = bp;
	return 0;
}

struct buf *bdev_queue_start(struct bdev_queue *queue)
{
	struct buf *bp;

	if (!queue || queue->dq_active || !queue->dq_head)
		return NULL;
	bp = queue->dq_head;
	queue->dq_head = bp->b_io_next;
	if (!queue->dq_head)
		queue->dq_tail = NULL;
	bp->b_io_next = NULL;
	queue->dq_active = bp;
	queue->dq_errcnt = 0;
	return bp;
}

struct buf *bdev_queue_complete(struct bdev_queue *queue)
{
	struct buf *bp;

	if (!queue || !queue->dq_active)
		return NULL;
	bp = queue->dq_active;
	queue->dq_active = NULL;
	queue->dq_errcnt = 0;
	bp->b_io_next = NULL;
	iodone(bp);
	return bp;
}

int bdev_queue_error(struct bdev_queue *queue, unsigned int retry_limit,
		     int error)
{
	struct buf *bp;

	if (!queue || !queue->dq_active)
		return BDEV_QUEUE_FAILED;
	if (queue->dq_errcnt < retry_limit) {
		queue->dq_errcnt++;
		return BDEV_QUEUE_RETRY;
	}
	bp = queue->dq_active;
	bp->b_flags |= B_ERROR;
	bp->b_error = error;
	bdev_queue_complete(queue);
	return BDEV_QUEUE_DONE;
}

static int bdev_queue_contains(struct bdev_queue *queue, struct buf *target)
{
	struct buf *bp;
	struct buf *last;
	int count;
	int found;

	if ((!queue->dq_head) != (!queue->dq_tail))
		return -SYS_EINVAL;
	if (queue->dq_active &&
	    (!(queue->dq_active->b_flags & B_BUSY) ||
	     queue->dq_active->b_io_next))
		return -SYS_EINVAL;
	found = queue->dq_active == target;
	count = queue->dq_active ? 1 : 0;
	last = NULL;
	for (bp = queue->dq_head; bp; bp = bp->b_io_next) {
		if (count >= NBUF || !(bp->b_flags & B_BUSY) ||
		    bp == queue->dq_active)
			return -SYS_EINVAL;
		found |= bp == target;
		last = bp;
		count++;
	}
	if (last != queue->dq_tail)
		return -SYS_EINVAL;
	return found;
}
