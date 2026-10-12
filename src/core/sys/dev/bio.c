/* bio.c */

#include <string.h>

#include "buf.h"
#include "conf.h"

unsigned char buffers[NBUF][BSIZE];

static struct buf *getdev(unsigned int dev);
static void b_list_init(struct buf *head);
static void b_list_remove(struct buf *bp);
static void b_list_insert(struct buf *head, struct buf *bp);
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
	bfreelist.av_forw = &bfreelist;
	bfreelist.av_back = &bfreelist;

	for (i = 0; i < nblkdev; i++) {
		dp = bdevsw[i].d_tab;
		if (dp) {
			memset(dp, 0, sizeof(*dp));
			b_list_init(dp);
		}
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

	bp = bfreelist.av_forw;
	if (bp == &bfreelist)
		return NULL;
	if (bp->b_flags & B_DELWRI)
		return NULL;

	notavail(bp);
	reassociate(bp, dp, dev, blkno);
	return bp;
}

struct buf *geteblk(void)
{
	struct buf *bp;

	bp = bfreelist.av_forw;
	if (bp == &bfreelist)
		return NULL;
	if (bp->b_flags & B_DELWRI)
		return NULL;

	notavail(bp);
	reassociate(bp, &bfreelist, NODEV, 0);
	return bp;
}

void brelse(struct buf *bp)
{
	if (!bp || !(bp->b_flags & B_BUSY))
		return;
	if (bp->b_flags & B_ERROR) {
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

static struct buf *getdev(unsigned int dev)
{
	unsigned int maj;

	if (dev == NODEV)
		return NULL;
	maj = major(dev);
	if (maj >= (unsigned int)nblkdev)
		return NULL;
	return bdevsw[maj].d_tab;
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
