#include <stddef.h>

#include "buf.h"
#include "conf.h"

static int bdev_queue_contains(struct bdev_queue *queue, struct buf *target);

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
	int count;

	if (queue->dq_active == target)
		return 1;
	count = 0;
	for (bp = queue->dq_head; bp && count < NBUF;
	     bp = bp->b_io_next) {
		if (bp == target)
			return 1;
		count++;
	}
	return bp != NULL;
}
