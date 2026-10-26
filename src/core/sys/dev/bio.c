/*
 * getblk() / geteblk() => bawrite() => bwrite() => driver strategy
 * bread() => driver strategy => iodone() => brelse() for B_ASYNC
 * bdwrite() => brelse(); bflush() => bawrite()
 * bread() / bwrite() => geterror() => b_error
 */

#include <assert.h>
#include <string.h>

#include "buf.h"
#include "conf.h"
#include "sys_error.h"

unsigned char buffers[NBUF][BSIZE];

static void b_list_init(struct buf *head);
static void b_list_remove(struct buf *bp);
static void b_list_insert(struct buf *head, struct buf *bp);
static void av_list_init(struct buf *head);
static void av_list_remove(struct buf *bp);
static void av_list_insert_head(struct buf *bp);
static void av_list_insert_tail(struct buf *bp);
static void notavail(struct buf *bp);
static void assign_buffer_to_block(struct buf *bp, struct buf *head,
				   unsigned int dev, unsigned long blkno);

/* Initialize buffer cache */
void binit(void)
{
	struct buf *dp;
	int i;

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

/* Assign cached block or reuse oldest available buffer */
struct buf *getblk(unsigned int dev, unsigned long blkno)
{
	struct buf *bp;
	struct buf *dp;

	/* V7 panics on invalid major or missing device table */
	assert(major(dev) < (unsigned int)nblkdev);
	dp = bdevsw[major(dev)].d_tab;
	assert(dp != NULL);
loop:
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if (bp->b_dev != dev || bp->b_blkno != blkno)
			continue;
		/* Return to caller until scheduler can replace V7 sleep */
		if (bp->b_flags & B_BUSY)
			return NULL;
		notavail(bp);
		return bp;
	}
	bp = bfreelist.av_forw;
	if (bp == &bfreelist)
		return NULL;
	notavail(bp);
	if (bp->b_flags & B_DELWRI) {
		bawrite(bp);
		goto loop;
	}
	assign_buffer_to_block(bp, dp, dev, blkno);
	return bp;
}

/* Allocate buffer without device association */
struct buf *geteblk(void)
{
	struct buf *bp;

loop:
	bp = bfreelist.av_forw;
	if (bp == &bfreelist)
		return NULL;
	notavail(bp);
	if (bp->b_flags & B_DELWRI) {
		bawrite(bp);
		goto loop;
	}
	assign_buffer_to_block(bp, &bfreelist, NODEV, 0);
	return bp;
}

/* Read block through synchronous driver unless already cached */
struct buf *bread(unsigned int dev, unsigned long blkno)
{
	struct buf *bp;

	bp = getblk(dev, blkno);
	if (!bp)
		return NULL;
	if (bp->b_flags & B_DONE)
		return bp;
	bp->b_flags |= B_READ;
	bp->b_bcount = BSIZE;
	bdevsw[major(dev)].d_strategy(bp);
	/* Synchronous strategy completes I/O before error collection */
	geterror(bp);
	return bp;
}

/* Write buffer and release on synchronous completion */
void bwrite(struct buf *bp)
{
	unsigned int flag;

	flag = bp->b_flags;
	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI | B_AGE);
	bp->b_bcount = BSIZE;
	bp->b_error = 0;
	/* Synchronous driver may release async buffer before returning */
	if ((flag & (B_ASYNC | B_DELWRI)) == (B_ASYNC | B_DELWRI))
		bp->b_flags |= B_AGE;
	bdevsw[major(bp->b_dev)].d_strategy(bp);
	if (!(flag & B_ASYNC)) {
		geterror(bp);
		brelse(bp);
	}
}

/* Mark delayed write and release buffer */
void bdwrite(struct buf *bp)
{
	bp->b_flags |= B_DELWRI | B_DONE;
	brelse(bp);
}

/* Release buffer through driver completion */
void bawrite(struct buf *bp)
{
	bp->b_flags |= B_ASYNC;
	bwrite(bp);
}

/* Start delayed writes from available list for selected devices */
void bflush(unsigned int dev)
{
	struct buf *bp;

loop:
	for (bp = bfreelist.av_forw; bp != &bfreelist; bp = bp->av_forw) {
		if ((bp->b_flags & B_DELWRI) &&
		    (dev == NODEV || bp->b_dev == dev)) {
			notavail(bp);
			bawrite(bp);
			goto loop;
		}
	}
}

/* Normalize device error, defaulting to generic I/O error */
void geterror(struct buf *bp)
{
	if (bp->b_flags & B_ERROR) {
		if (!bp->b_error)
			bp->b_error = SYS_EIO;
	} else {
		bp->b_error = 0;
	}
}

/* Release buffer onto available list */
void brelse(struct buf *bp)
{
	if (bp->b_flags & B_ERROR)
		bp->b_dev = NODEV;
	if (bp->b_flags & B_AGE)
		av_list_insert_head(bp);
	else
		av_list_insert_tail(bp);
	bp->b_flags &= ~(B_WANTED | B_BUSY | B_ASYNC | B_AGE);
}

/* Find cached device block */
int incore(unsigned int dev, unsigned long blkno)
{
	struct buf *bp;
	struct buf *dp;

	dp = bdevsw[major(dev)].d_tab;
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if (bp->b_dev == dev && bp->b_blkno == blkno)
			return 1;
	}
	return 0;
}

/* Clear buffer payload */
void clrbuf(struct buf *bp)
{
	memset(bp->b_addr, 0, BSIZE);
	bp->b_resid = 0;
}

/* Mark completion and release asynchronous request */
void iodone(struct buf *bp)
{
	geterror(bp);
	bp->b_flags |= B_DONE;
	if (bp->b_flags & B_ASYNC)
		brelse(bp);
	else
		bp->b_flags &= ~B_WANTED;
}

/* Initialize device list head */
static void b_list_init(struct buf *head)
{
	head->b_forw = head;
	head->b_back = head;
}

/* Unlink buffer from device list */
static void b_list_remove(struct buf *bp)
{
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
}

/* Insert buffer at device list head */
static void b_list_insert(struct buf *head, struct buf *bp)
{
	bp->b_forw = head->b_forw;
	bp->b_back = head;
	head->b_forw->b_back = bp;
	head->b_forw = bp;
}

/* Initialize available list head */
static void av_list_init(struct buf *head)
{
	head->av_forw = head;
	head->av_back = head;
}

/* Unlink buffer from available list */
static void av_list_remove(struct buf *bp)
{
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
}

/* Insert buffer at oldest available position */
static void av_list_insert_head(struct buf *bp)
{
	bp->av_forw = bfreelist.av_forw;
	bp->av_back = &bfreelist;
	bfreelist.av_forw->av_back = bp;
	bfreelist.av_forw = bp;
}

/* Insert buffer at newest available position */
static void av_list_insert_tail(struct buf *bp)
{
	bp->av_forw = &bfreelist;
	bp->av_back = bfreelist.av_back;
	bfreelist.av_back->av_forw = bp;
	bfreelist.av_back = bp;
}

/* Remove available buffer and mark busy */
static void notavail(struct buf *bp)
{
	av_list_remove(bp);
	bp->b_flags |= B_BUSY;
}

/* Assign buffer to device and block */
static void assign_buffer_to_block(struct buf *bp, struct buf *head,
				   unsigned int dev, unsigned long blkno)
{
	b_list_remove(bp);
	b_list_insert(head, bp);
	bp->b_flags = B_BUSY;
	bp->b_error = 0;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
}
