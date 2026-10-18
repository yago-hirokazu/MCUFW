#include <stddef.h>

#include "buf.h"
#include "conf.h"
#include "sys_error.h"

/*
 * This file consolidates codes previously found in `bio.c` and other
 * block device drivers of UNIX V7 into a set of common routines.
 */

/*
 * bio.c：bread() / bwrite() etc.
 *   => bdev_strategy()：validation and select driver strategy function
 *     => ramstrategy()：RAM device routine
 *       => enqueue => start => data transfer => complete
 *         => iodone()：notify data transfer completion to buffer
 */

static int bdev_queue_contains(struct bdev_queue *queue, struct buf *target);

/* Validate request and pass it to block device driver */
int bdev_strategy(struct buf *bp)
{
	int result;

	/* Reject invalid or already submitted buffer before driver call */
	result = bdev_check_io_request_valid(bp);
	if (result < 0)
		return result;
	return bdevsw[major(bp->b_dev)].d_strategy(bp);
}

/* Check request without changing buffer or queues */
int bdev_check_io_request_valid(struct buf *bp)
{
	struct bdevsw *dp;

	/* Require owned buffer and registered strategy */
	if (!bp || !(bp->b_flags & B_BUSY))
		return -SYS_EINVAL;
	dp = bdevsw_lookup(bp->b_dev);
	if (!dp || !dp->d_strategy)
		return -SYS_ENODEV;
	/* Check queue ownership independently of device registration */
	return bdev_check_buffer_availabe(bp);
}

/* Check buffer ownership across registered queues */
int bdev_check_buffer_availabe(struct buf *bp)
{
	int found;
	int result;
	int i;

	if (!bp || !(bp->b_flags & B_BUSY))
		return -SYS_EINVAL;
	/* Inspect every queue to detect ownership by another device */
	found = 0;
	for (i = 0; i < nblkdev; i++) {
		if (!bdevsw[i].d_queue)
			continue;
		result = bdev_queue_contains(bdevsw[i].d_queue, bp);
		if (result < 0)
			return result;
		found |= result;
	}
	/* Keep queued or active buffer unavailable until completion */
	if (found)
		return -SYS_EAGAIN;
	return 0;
}

/* Initialize queue with no outstanding requests */
void bdev_queue_init(struct bdev_queue *queue)
{
	if (!queue)
		return;
	/* Clear waiting list, active request and retry count */
	queue->dq_head = NULL;
	queue->dq_tail = NULL;
	queue->dq_active = NULL;
	queue->dq_errcnt = 0;
}

/* Append owned buffer to waiting list */
int bdev_queue_enqueue(struct bdev_queue *queue, struct buf *bp)
{
	if (!queue || !bp || !(bp->b_flags & B_BUSY))
		return -1;
	/* Reject duplicate request or malformed destination queue */
	if (bdev_queue_contains(queue, bp))
		return -1;

	/* Append at tail to preserve FIFO order */
	bp->b_io_next = NULL;
	if (queue->dq_tail)
		queue->dq_tail->b_io_next = bp;
	else
		queue->dq_head = bp;
	queue->dq_tail = bp;
	return 0;
}

/* Move oldest waiting request into active slot */
struct buf *bdev_queue_start(struct bdev_queue *queue)
{
	struct buf *bp;

	/* Allow only one active request per queue */
	if (!queue || queue->dq_active || !queue->dq_head)
		return NULL;
	/* Detach waiting head before handing buffer to driver */
	bp = queue->dq_head;
	queue->dq_head = bp->b_io_next;
	if (!queue->dq_head)
		queue->dq_tail = NULL;
	bp->b_io_next = NULL;
	/* Track active request with fresh retry budget */
	queue->dq_active = bp;
	queue->dq_errcnt = 0;
	return bp;
}

/* Clear active slot and notify buffer completion */
struct buf *bdev_queue_complete(struct bdev_queue *queue)
{
	struct buf *bp;

	if (!queue || !queue->dq_active)
		return NULL;
	/* Drop queue ownership before iodone may release async buffer */
	bp = queue->dq_active;
	queue->dq_active = NULL;
	queue->dq_errcnt = 0;
	bp->b_io_next = NULL;
	/* Mark completion without starting next waiting request */
	iodone(bp);
	return bp;
}

/* Request retry or complete active request with error */
int bdev_queue_error(struct bdev_queue *queue, unsigned int retry_limit,
		     int error)
{
	struct buf *bp;

	if (!queue || !queue->dq_active)
		return BDEV_QUEUE_FAILED;
	/* Leave active request in place for caller to retry transfer */
	if (queue->dq_errcnt < retry_limit) {
		queue->dq_errcnt++;
		return BDEV_QUEUE_RETRY;
	}
	/* Report final error through normal completion path */
	bp = queue->dq_active;
	bp->b_flags |= B_ERROR;
	bp->b_error = error;
	bdev_queue_complete(queue);
	return BDEV_QUEUE_DONE;
}

/*
 * Validate non-NULL queue and search active slot plus waiting list
 * Return 1 if found, 0 if absent or negative error on malformed queue
 */
static int bdev_queue_contains(struct bdev_queue *queue, struct buf *target)
{
	struct buf *bp;
	struct buf *last;
	int count;
	int found;

	/* Require matching list endpoints and detached owned active buffer */
	if ((!queue->dq_head) != (!queue->dq_tail))
		return -SYS_EINVAL;
	if (queue->dq_active &&
	    (!(queue->dq_active->b_flags & B_BUSY) ||
	     queue->dq_active->b_io_next))
		return -SYS_EINVAL;
	/* Count active buffer toward fixed buffer pool limit */
	found = queue->dq_active == target;
	count = queue->dq_active ? 1 : 0;
	last = NULL;
	/* Bound traversal to reject cycles and invalid queue membership */
	for (bp = queue->dq_head; bp; bp = bp->b_io_next) {
		if (count >= NBUF || !(bp->b_flags & B_BUSY) ||
		    bp == queue->dq_active)
			return -SYS_EINVAL;
		found |= bp == target;
		last = bp;
		count++;
	}
	/* Verify stored tail matches actual end of waiting list */
	if (last != queue->dq_tail)
		return -SYS_EINVAL;
	return found;
}
