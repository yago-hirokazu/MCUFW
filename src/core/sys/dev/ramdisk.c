/* ramdisk.c */

#include <assert.h>
#include <string.h>

#include "buf.h"
#include "conf.h"
#include "ramdisk.h"
#include "sys_error.h"

struct bdev_io_queue ramqueue;

static unsigned char ramblocks[RAM_NDEV][RAM_NBLK][BSIZE];

static int ramtransfer(struct buf *bp);

void raminit(void)
{
	memset(ramblocks, 0, sizeof(ramblocks));
}

/* Complete every submitted request, including validation failures */
void ramstrategy(struct buf *bp)
{
	int result;

	/* Duplicate submission must not complete original queued request */
	result = bdev_check_buffer_available(bp);
	if (result < 0) {
		assert(result == 0);
		return;
	}
	result = bdev_check_io_request_valid(bp);
	if (result < 0)
		goto error;
	if (major(bp->b_dev) != RAM_MAJOR) {
		result = -SYS_ENODEV;
		goto error;
	}
	if (ramqueue.dq_active || ramqueue.dq_head) {
		result = -SYS_EAGAIN;
		goto error;
	}
	if (bdev_io_queue_enqueue(&ramqueue, bp) < 0) {
		result = -SYS_EINVAL;
		goto error;
	}
	bp = bdev_io_queue_start(&ramqueue);
	bp->b_flags &= ~(B_DONE | B_ERROR | B_DELWRI);
	bp->b_error = 0;
	bp->b_resid = bp->b_bcount;
	result = ramtransfer(bp);
	if (result) {
		bp->b_flags |= B_ERROR;
		bp->b_error = result;
	} else {
		bp->b_resid = 0;
	}
	bdev_io_queue_complete(&ramqueue);
	return;

error:
	bdev_io_error(bp, -result);
}

static int ramtransfer(struct buf *bp)
{
	unsigned char *block;
	unsigned int unit;

	unit = minor(bp->b_dev);
	if (unit >= RAM_NDEV)
		return SYS_ENODEV;
	if (bp->b_blkno >= RAM_NBLK)
		return SYS_EIO;
	if (bp->b_bcount != BSIZE || !bp->b_addr)
		return SYS_EINVAL;
	block = ramblocks[unit][bp->b_blkno];
	if (bp->b_flags & B_READ)
		memcpy(bp->b_addr, block, BSIZE);
	else
		memcpy(block, bp->b_addr, BSIZE);
	return 0;
}
