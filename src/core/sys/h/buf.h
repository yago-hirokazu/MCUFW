/* buf.h */

#ifndef __BUF_H__
#define __BUF_H__

#include <stddef.h>

#ifndef BSIZE
#define BSIZE	256
#endif

#ifndef NBUF
#define NBUF	5
#endif

#define NODEV	((unsigned int)-1)
#define PIPEDEV	0U

#define B_WRITE	0x0000U
#define B_READ		0x0001U
#define B_DONE		0x0002U
#define B_ERROR		0x0004U
#define B_BUSY		0x0008U
#define B_WANTED	0x0010U
#define B_AGE		0x0020U
#define B_ASYNC		0x0040U
#define B_DELWRI	0x0080U

#ifndef major
#define major(dev)	((unsigned int)(dev) >> 8)
#endif

#ifndef minor
#define minor(dev)	((unsigned int)(dev) & 0xffU)
#endif

#ifndef makedev
static inline unsigned int makedev(unsigned int maj, unsigned int min)
{
	if (maj > 0xffU || min > 0xffU)
		return NODEV;
	return (maj << 8) | min;
}
#endif

struct buf {
	unsigned int b_flags;
	struct buf *b_forw;
	struct buf *b_back;
	struct buf *av_forw;
	struct buf *av_back;
	struct buf *b_io_next;
	unsigned int b_dev;
	unsigned long b_blkno;
	unsigned char *b_addr;
	size_t b_bcount;
	size_t b_resid;
	int b_error;
};

extern unsigned char buffers[NBUF][BSIZE];
extern struct buf buf[NBUF];
extern struct buf bfreelist;

/* Caller holds kernel lock for cache, I/O and owned payload access */
/* Quiescent initialization and single-execution tests may omit lock */
void binit(void);
struct buf *getblk(unsigned int dev, unsigned long blkno);
struct buf *geteblk(void);
/* Require valid device table and synchronous strategy for block I/O */
/* Busy hit or exhaustion returns EAGAIN; failed read releases buffer */
struct buf *bread(unsigned int dev, unsigned long blkno, int *error);
/* Submit owned buffer; failed write discards device association */
int bwrite(struct buf *bp);
/* Mark owned buffer for delayed write without submitting I/O */
int bdwrite(struct buf *bp);
/* Set B_ASYNC; iodone releases buffer, synchronous drivers required */
int bawrite(struct buf *bp);
/* NODEV selects all; submit available dirty buffers, skip busy buffers */
int bflush(unsigned int dev);
void brelse(struct buf *bp);
int incore(unsigned int dev, unsigned long blkno);
void clrbuf(struct buf *bp);
void iodone(struct buf *bp);

#endif
