/* test_ramdisk.c */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "conf.h"
#include "ramdisk.h"
#include "sys_error.h"

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

static int failures;

static struct buf *request(unsigned int unit, unsigned long block, int read);
static void complete(struct buf *bp, int error);
static void check_idle(void);
static void test_initial(void);
static void test_storage(void);
static void test_errors(void);
static void test_async(void);
static void test_rejection(void);
static void test_reset(void);

int main(void)
{
	test_initial();
	test_storage();
	test_errors();
	test_async();
	test_rejection();
	test_reset();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static struct buf *request(unsigned int unit, unsigned long block, int read)
{
	struct buf *bp;

	bp = getblk(makedev(RAM_MAJOR, unit), block);
	CHECK(bp != NULL);
	if (!bp)
		return NULL;
	bp->b_flags = B_BUSY | (read ? B_READ : B_WRITE);
	bp->b_bcount = BSIZE;
	bp->b_resid = BSIZE;
	bp->b_error = 0;
	return bp;
}

static void complete(struct buf *bp, int error)
{
	unsigned char saved[BSIZE];

	if (!bp)
		return;
	if (bp->b_addr)
		memcpy(saved, bp->b_addr, BSIZE);
	CHECK(bdev_strategy(bp) == 0);
	CHECK(bp->b_flags & B_DONE);
	CHECK(bp->b_flags & B_BUSY);
	CHECK(!!(bp->b_flags & B_ERROR) == !!error);
	CHECK(bp->b_error == error);
	CHECK(bp->b_resid == (error ? bp->b_bcount : 0));
	CHECK(bp->b_io_next == NULL);
	CHECK(ramqueue.dq_head == NULL);
	CHECK(ramqueue.dq_tail == NULL);
	CHECK(ramqueue.dq_active == NULL);
	if (error && bp->b_addr)
		CHECK(memcmp(saved, bp->b_addr, BSIZE) == 0);
}

static void check_idle(void)
{
	struct buf *bp;
	int count;
	int i;

	CHECK(ramqueue.dq_head == NULL);
	CHECK(ramqueue.dq_tail == NULL);
	CHECK(ramqueue.dq_active == NULL);
	CHECK(ramqueue.dq_errcnt == 0);
	count = 0;
	for (bp = bfreelist.av_forw; bp != &bfreelist && count <= NBUF;
	     bp = bp->av_forw) {
		CHECK(bp->av_forw->av_back == bp);
		CHECK(bp->av_back->av_forw == bp);
		count++;
	}
	CHECK(count == NBUF);
	for (i = 0; i < NBUF; i++) {
		CHECK(!(buf[i].b_flags & B_BUSY));
		CHECK(buf[i].b_io_next == NULL);
	}
}

static void test_initial(void)
{
	struct buf *bp;
	unsigned int unit;
	size_t i;

	raminit();
	binit();
	CHECK(bdevsw[RAM_MAJOR].d_strategy == ramstrategy);
	CHECK(bdevsw[RAM_MAJOR].d_queue == &ramqueue);
	for (unit = 0; unit < RAM_NDEV; unit++) {
		bp = request(unit, RAM_NBLK - 1, 1);
		memset(bp->b_addr, 0xa5, BSIZE);
		complete(bp, 0);
		for (i = 0; i < BSIZE; i++)
			CHECK(bp->b_addr[i] == 0);
		brelse(bp);
	}
	check_idle();
}

static void test_storage(void)
{
	struct buf *bp;
	unsigned long block;
	unsigned int unit;
	size_t i;

	raminit();
	binit();
	for (unit = 0; unit < RAM_NDEV; unit++) {
		for (block = 0; block < RAM_NBLK; block++) {
			bp = request(unit, block, 0);
			for (i = 0; i < BSIZE; i++)
				bp->b_addr[i] = unit * 71 + block * 5 + i;
			bp->b_flags |= B_DONE | B_ERROR | B_DELWRI;
			bp->b_error = SYS_EIO;
			complete(bp, 0);
			CHECK(!(bp->b_flags & B_DELWRI));
			brelse(bp);
		}
	}
	check_idle();
	binit();
	for (unit = 0; unit < RAM_NDEV; unit++) {
		for (block = 0; block < RAM_NBLK; block++) {
			bp = request(unit, block, 1);
			memset(bp->b_addr, 0xcc, BSIZE);
			complete(bp, 0);
			for (i = 0; i < BSIZE; i++)
				CHECK(bp->b_addr[i] == (unsigned char)
				      (unit * 71 + block * 5 + i));
			brelse(bp);
		}
	}
	check_idle();
}

static void test_errors(void)
{
	struct buf *bp;
	unsigned char *addr;
	size_t lengths[] = { 0, 1, BSIZE - 1, BSIZE + 1, (size_t)-1 };
	size_t i;
	int read;

	raminit();
	binit();
	for (read = 0; read <= 1; read++) {
		bp = request(2, 0, read);
		complete(bp, SYS_ENODEV);
		brelse(bp);
		bp = request(255, 0, read);
		complete(bp, SYS_ENODEV);
		brelse(bp);
		bp = request(0, RAM_NBLK, read);
		complete(bp, SYS_EIO);
		brelse(bp);
		bp = request(0, ULONG_MAX, read);
		complete(bp, SYS_EIO);
		brelse(bp);
		for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
			bp = request(0, 0, read);
			memset(bp->b_addr, 0xa7, BSIZE);
			bp->b_bcount = lengths[i];
			complete(bp, SYS_EINVAL);
			brelse(bp);
		}
		bp = request(0, 0, read);
		addr = bp->b_addr;
		bp->b_addr = NULL;
		complete(bp, SYS_EINVAL);
		bp->b_addr = addr;
		brelse(bp);
	}
	bp = request(0, 0, 1);
	complete(bp, 0);
	for (i = 0; i < BSIZE; i++)
		CHECK(bp->b_addr[i] == 0);
	brelse(bp);
	check_idle();
}

static void test_async(void)
{
	struct buf *bp;
	int index;
	size_t i;

	raminit();
	binit();
	bp = request(1, 2, 0);
	memset(bp->b_addr, 0x96, BSIZE);
	index = (int)(bp - buf);
	bp->b_flags |= B_ASYNC;
	CHECK(bdev_strategy(bp) == 0);
	/* Inspect pool after completion without retaining caller ownership */
	CHECK(buf[index].b_flags & B_DONE);
	CHECK(!(buf[index].b_flags & (B_BUSY | B_ASYNC | B_ERROR)));
	check_idle();
	binit();
	bp = request(1, 2, 1);
	complete(bp, 0);
	for (i = 0; i < BSIZE; i++)
		CHECK(bp->b_addr[i] == 0x96);
	brelse(bp);
	bp = request(0, RAM_NBLK, 1);
	index = (int)(bp - buf);
	bp->b_flags |= B_ASYNC;
	CHECK(bdev_strategy(bp) == 0);
	CHECK(buf[index].b_flags & B_DONE);
	CHECK(buf[index].b_flags & B_ERROR);
	CHECK(buf[index].b_error == SYS_EIO);
	CHECK(buf[index].b_resid == BSIZE);
	CHECK(buf[index].b_dev == NODEV);
	check_idle();
}

static void test_rejection(void)
{
	struct bdev_queue other;
	struct buf *bp;
	struct buf saved;

	raminit();
	binit();
	CHECK(ramstrategy(NULL) == -SYS_EINVAL);
	bp = request(0, 0, 1);
	bp->b_dev = PIPEDEV;
	memcpy(&saved, bp, sizeof(saved));
	CHECK(ramstrategy(bp) == -SYS_ENODEV);
	CHECK(memcmp(&saved, bp, sizeof(saved)) == 0);
	bp->b_dev = makedev(RAM_MAJOR, 0);
	CHECK(bdev_queue_enqueue(&ramqueue, bp) == 0);
	memcpy(&saved, bp, sizeof(saved));
	CHECK(ramstrategy(bp) == -SYS_EAGAIN);
	CHECK(memcmp(&saved, bp, sizeof(saved)) == 0);
	CHECK(bdev_queue_start(&ramqueue) == bp);
	CHECK(bdev_queue_complete(&ramqueue) == bp);
	bdev_queue_init(&other);
	bdevsw[0].d_queue = &other;
	CHECK(bdev_queue_enqueue(&other, bp) == 0);
	memcpy(&saved, bp, sizeof(saved));
	CHECK(ramstrategy(bp) == -SYS_EAGAIN);
	CHECK(memcmp(&saved, bp, sizeof(saved)) == 0);
	CHECK(bdev_queue_start(&other) == bp);
	CHECK(bdev_queue_complete(&other) == bp);
	bdevsw[0].d_queue = NULL;
	brelse(bp);
	memcpy(&saved, bp, sizeof(saved));
	CHECK(ramstrategy(bp) == -SYS_EINVAL);
	CHECK(memcmp(&saved, bp, sizeof(saved)) == 0);
	check_idle();
}

static void test_reset(void)
{
	struct buf *bp;
	struct buf saved_buf[NBUF];
	unsigned char saved_data[NBUF][BSIZE];
	size_t i;

	raminit();
	binit();
	bp = request(0, 0, 0);
	memset(bp->b_addr, 0x56, BSIZE);
	complete(bp, 0);
	brelse(bp);
	memcpy(saved_buf, buf, sizeof(saved_buf));
	memcpy(saved_data, buffers, sizeof(saved_data));
	raminit();
	CHECK(memcmp(saved_buf, buf, sizeof(saved_buf)) == 0);
	CHECK(memcmp(saved_data, buffers, sizeof(saved_data)) == 0);
	binit();
	bp = request(0, 0, 1);
	complete(bp, 0);
	for (i = 0; i < BSIZE; i++)
		CHECK(bp->b_addr[i] == 0);
	brelse(bp);
	check_idle();
}
