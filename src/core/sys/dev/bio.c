/* bio.c */

#include <assert.h>
#include <string.h>

#include "buf.h"
#include "conf.h"
#include "sys_error.h"

unsigned char buffers[NBUF][BSIZE];

static struct buf *getdev(unsigned int dev);
static int sync_device(unsigned int dev);
static int sync_io(struct buf *bp);
static struct buf *available_buffer(void);
static int write_buffer_check(struct buf *bp);
static int write_buffer(struct buf *bp, int retain);
static int writeback(struct buf *bp);
static int geterror(struct buf *bp);
static void prepare_read(struct buf *bp);
static void prepare_write(struct buf *bp);
static void io_error(struct buf *bp, int error);
static void b_list_init(struct buf *head);
static void b_list_remove(struct buf *bp);
static void b_list_insert(struct buf *head, struct buf *bp);
static void av_list_init(struct buf *head);
static void av_list_remove(struct buf *bp);
static void av_list_insert_head(struct buf *bp);
static void av_list_insert_tail(struct buf *bp);
static void notavail(struct buf *bp);
static void reassociate(struct buf *bp, struct buf *head,
			unsigned int dev, unsigned long blkno);

void binit(void)
{
	struct buf *dp;
	int i;

	memset(buffers, 0, sizeof(buffers));
	memset(buf, 0, sizeof(buf));
	memset(&bfreelist, 0, sizeof(bfreelist));
	b_list_init(&bfreelist);
	av_list_init(&bfreelist);

	for (i = 0; i < nblkdev; i++) {
		dp = bdevsw[i].d_tab;
		if (dp) {
			memset(dp, 0, sizeof(*dp));
			b_list_init(dp);
		}
		if (bdevsw[i].d_queue)
			memset(bdevsw[i].d_queue, 0,
			       sizeof(*bdevsw[i].d_queue));
	}

	for (i = 0; i < NBUF; i++) {
		buf[i].b_dev = NODEV;
		buf[i].b_addr = buffers[i];
		b_list_insert(&bfreelist, &buf[i]);
		buf[i].b_flags = B_BUSY;
		brelse(&buf[i]);
	}
}

struct buf *getblk(unsigned int dev, unsigned long blkno)
{
	struct buf *bp;
	struct buf *dp;

	dp = getdev(dev);
	if (!dp)
		return NULL;

	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if (bp->b_dev != dev || bp->b_blkno != blkno)
			continue;
		if (bp->b_flags & B_BUSY)
			return NULL;
		notavail(bp);
		return bp;
	}

	bp = available_buffer();
	if (!bp)
		return NULL;

	reassociate(bp, dp, dev, blkno);
	return bp;
}

struct buf *geteblk(void)
{
	struct buf *bp;

	bp = available_buffer();
	if (!bp)
		return NULL;

	reassociate(bp, &bfreelist, NODEV, 0);
	return bp;
}

struct buf *bread(unsigned int dev, unsigned long blkno, int *error)
{
	struct buf *bp;

	if (!error)
		return NULL;
	*error = sync_device(dev);
	if (*error)
		return NULL;
	bp = getblk(dev, blkno);
	if (!bp) {
		*error = -SYS_EAGAIN;
		return NULL;
	}
	if ((bp->b_flags & B_DONE) &&
	    (!(bp->b_flags & B_ERROR) || (bp->b_flags & B_DELWRI)))
		return bp;
	prepare_read(bp);
	*error = sync_io(bp);
	if (*error) {
		io_error(bp, *error);
		brelse(bp);
		return NULL;
	}
	return bp;
}

int bwrite(struct buf *bp)
{
	int result;

	result = write_buffer_check(bp);
	if (result)
		return result;
	return write_buffer(bp, bp->b_flags & B_DELWRI);
}

int bdwrite(struct buf *bp)
{
	int result;

	result = write_buffer_check(bp);
	if (result)
		return result;
	result = sync_device(bp->b_dev);
	if (result)
		return result;
	bp->b_flags &= ~(B_READ | B_ERROR);
	bp->b_flags |= B_DELWRI | B_DONE;
	bp->b_error = 0;
	bp->b_resid = 0;
	brelse(bp);
	return 0;
}

int bawrite(struct buf *bp)
{
	int result;

	result = write_buffer_check(bp);
	if (result)
		return result;
	/* Synchronous drivers complete before release and error return */
	return write_buffer(bp, 1);
}

int bflush(unsigned int dev)
{
	struct buf *bp;
	int result;
	int error;
	int i;

	if (dev != NODEV) {
		error = sync_device(dev);
		if (error)
			return error;
	}
	result = 0;
	for (i = 0; i < NBUF; i++) {
		bp = &buf[i];
		if (!(bp->b_flags & B_DELWRI) ||
		    (dev != NODEV && bp->b_dev != dev))
			continue;
		if (bp->b_flags & B_BUSY) {
			if (!result)
				result = -SYS_EAGAIN;
			continue;
		}
		error = writeback(bp);
		if (error && (!result || result == -SYS_EAGAIN))
			result = error;
	}
	return result;
}

void brelse(struct buf *bp)
{
	if (!bp || !(bp->b_flags & B_BUSY))
		return;
	if ((bp->b_flags & B_ERROR) && !(bp->b_flags & B_DELWRI)) {
		b_list_remove(bp);
		b_list_insert(&bfreelist, bp);
		bp->b_dev = NODEV;
		bp->b_blkno = 0;
	}
	if (bp->b_flags & B_AGE)
		av_list_insert_head(bp);
	else
		av_list_insert_tail(bp);
	bp->b_flags &= ~(B_WANTED | B_BUSY | B_ASYNC | B_AGE);
	bp->b_io_next = NULL;
}

int incore(unsigned int dev, unsigned long blkno)
{
	struct buf *bp;
	struct buf *dp;

	dp = getdev(dev);
	if (!dp)
		return 0;
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if (bp->b_dev == dev && bp->b_blkno == blkno)
			return 1;
	}
	return 0;
}

void clrbuf(struct buf *bp)
{
	if (!bp || !bp->b_addr)
		return;
	memset(bp->b_addr, 0, BSIZE);
	bp->b_resid = 0;
}

void iodone(struct buf *bp)
{
	if (!bp || !(bp->b_flags & B_BUSY))
		return;
	bp->b_flags |= B_DONE;
	if (bp->b_flags & B_ASYNC)
		brelse(bp);
	else
		bp->b_flags &= ~B_WANTED;
}

static int write_buffer_check(struct buf *bp)
{
	if (!bp || (bp->b_flags & B_ASYNC))
		return -SYS_EINVAL;
	return bdev_buffer_check(bp);
}

/* Consume prevalidated owned buffer on every return path */
static int write_buffer(struct buf *bp, int retain)
{
	int result;

	result = sync_device(bp->b_dev);
	if (result)
		retain = bp->b_flags & B_DELWRI;
	prepare_write(bp);
	if (!result)
		result = sync_io(bp);
	if (result) {
		io_error(bp, result);
		if (retain)
			bp->b_flags |= B_DELWRI | B_DONE;
	}
	brelse(bp);
	return result;
}

/* Acquire available dirty buffer and release even on validation failure */
static int writeback(struct buf *bp)
{
	int result;

	notavail(bp);
	result = write_buffer_check(bp);
	if (result) {
		brelse(bp);
		return result;
	}
	return write_buffer(bp, 1);
}

static struct buf *getdev(unsigned int dev)
{
	struct bdevsw *dp;

	dp = bdev_get(dev);
	return dp ? dp->d_tab : NULL;
}

static struct buf *available_buffer(void)
{
	struct buf *bp;
	int i;

	/* Bound retries even when every available buffer needs writeback */
	for (i = 0; i <= NBUF; i++) {
		bp = bfreelist.av_forw;
		if (bp == &bfreelist)
			return NULL;
		if (!(bp->b_flags & B_DELWRI)) {
			notavail(bp);
			return bp;
		}
		if (writeback(bp))
			return NULL;
	}
	return NULL;
}

static int sync_device(unsigned int dev)
{
	struct bdevsw *dp;

	dp = bdev_get(dev);
	if (!dp || !dp->d_strategy)
		return -SYS_ENODEV;
	if (!(dp->d_flags & BDEV_SYNC))
		return -SYS_ENOSYS;
	return 0;
}

static int sync_io(struct buf *bp)
{
	int result;

	result = bdev_strategy(bp);
	if (result < 0)
		return result;
	/* BDEV_SYNC replaces V7 iowait sleep until asynchronous I/O support */
	assert(bp->b_flags & B_DONE);
	return geterror(bp);
}

static int geterror(struct buf *bp)
{
	if (bp->b_flags & B_ERROR)
		return bp->b_error ? -bp->b_error : -SYS_EIO;
	return 0;
}

static void prepare_read(struct buf *bp)
{
	bp->b_flags &= ~(B_DONE | B_ERROR | B_ASYNC);
	bp->b_flags |= B_READ;
	bp->b_bcount = BSIZE;
	bp->b_resid = BSIZE;
	bp->b_error = 0;
}

static void prepare_write(struct buf *bp)
{
	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI | B_AGE);
	bp->b_bcount = BSIZE;
	bp->b_resid = BSIZE;
	bp->b_error = 0;
}

static void io_error(struct buf *bp, int error)
{
	bp->b_flags |= B_ERROR;
	bp->b_error = -error;
	bp->b_resid = bp->b_bcount;
}

static void b_list_init(struct buf *head)
{
	head->b_forw = head;
	head->b_back = head;
}

static void b_list_remove(struct buf *bp)
{
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
}

static void b_list_insert(struct buf *head, struct buf *bp)
{
	bp->b_forw = head->b_forw;
	bp->b_back = head;
	head->b_forw->b_back = bp;
	head->b_forw = bp;
}

static void av_list_init(struct buf *head)
{
	head->av_forw = head;
	head->av_back = head;
}

static void av_list_remove(struct buf *bp)
{
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
	bp->av_forw = NULL;
	bp->av_back = NULL;
}

static void av_list_insert_head(struct buf *bp)
{
	bp->av_forw = bfreelist.av_forw;
	bp->av_back = &bfreelist;
	bfreelist.av_forw->av_back = bp;
	bfreelist.av_forw = bp;
}

static void av_list_insert_tail(struct buf *bp)
{
	bp->av_forw = &bfreelist;
	bp->av_back = bfreelist.av_back;
	bfreelist.av_back->av_forw = bp;
	bfreelist.av_back = bp;
}

static void notavail(struct buf *bp)
{
	av_list_remove(bp);
	bp->b_flags |= B_BUSY;
}

static void reassociate(struct buf *bp, struct buf *head,
			unsigned int dev, unsigned long blkno)
{
	b_list_remove(bp);
	b_list_insert(head, bp);
	bp->b_flags = B_BUSY;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	bp->b_bcount = 0;
	bp->b_resid = 0;
	bp->b_error = 0;
	bp->b_io_next = NULL;
}
