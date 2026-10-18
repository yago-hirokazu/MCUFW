/* test_bio.c */

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
static struct bdevsw saved_ram;
static int io_reads;
static int io_writes;
static int io_reject;
static int io_fail;
static int io_error_code;

static int buffer_index(struct buf *bp);
static int on_av_list(struct buf *target);
static int count_av_list(void);
static int count_b_list(struct buf *head, int seen[NBUF]);
static void check_invariants(void);
static void test_binit(void);
static void test_cache_hit(void);
static void test_exhaustion(void);
static void test_lru(void);
static void test_age(void);
static void test_geteblk_and_clrbuf(void);
static void test_error_release(void);
static void test_device_numbers(void);
static void test_devices(void);
static void begin_io_test(void);
static void end_io_test(void);
static int io_strategy(struct buf *bp);
static void test_block_io(void);
static void test_read_cache(void);
static void test_io_errors(void);
static void test_io_registration(void);
static void test_io_exhaustion(void);
static void test_write_ownership(void);
static void check_write_rejected(struct buf *bp, int error);
static void test_delayed_write(void);
static void test_writeback_failure(void);
static void test_dirty_reuse(void);
static void test_unwritable_device(void);
static void test_dirty_registration_failure(void);
static void test_writeback_rejection(void);
static void check_payload(struct buf *bp, unsigned char value);

int main(void)
{
	test_binit();
	test_cache_hit();
	test_exhaustion();
	test_lru();
	test_age();
	test_geteblk_and_clrbuf();
	test_error_release();
	test_device_numbers();
	test_devices();
	test_block_io();
	test_read_cache();
	test_io_errors();
	test_io_registration();
	test_io_exhaustion();
	test_write_ownership();
	test_delayed_write();
	test_writeback_failure();
	test_dirty_reuse();
	test_unwritable_device();
	test_dirty_registration_failure();
	test_writeback_rejection();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static int buffer_index(struct buf *bp)
{
	if (bp < buf || bp >= buf + NBUF)
		return -1;
	return (int)(bp - buf);
}

static int on_av_list(struct buf *target)
{
	struct buf *bp;
	int count;

	count = 0;
	for (bp = bfreelist.av_forw; bp != &bfreelist;
	     bp = bp->av_forw) {
		if (bp == target)
			return 1;
		if (++count > NBUF)
			break;
	}
	return 0;
}

static int count_av_list(void)
{
	struct buf *bp;
	int count;

	count = 0;
	for (bp = bfreelist.av_forw; bp != &bfreelist;
	     bp = bp->av_forw) {
		CHECK(bp->av_forw->av_back == bp);
		CHECK(bp->av_back->av_forw == bp);
		CHECK(!(bp->b_flags & B_BUSY));
		if (++count > NBUF)
			break;
	}
	return count;
}

static int count_b_list(struct buf *head, int seen[NBUF])
{
	struct buf *bp;
	int count;
	int index;

	count = 0;
	for (bp = head->b_forw; bp != head; bp = bp->b_forw) {
		CHECK(bp->b_forw->b_back == bp);
		CHECK(bp->b_back->b_forw == bp);
		index = buffer_index(bp);
		CHECK(index >= 0);
		if (index >= 0)
			seen[index]++;
		if (++count > NBUF)
			break;
	}
	return count;
}

static void check_invariants(void)
{
	struct buf *dp;
	int seen[NBUF];
	int b_count;
	int av_count;
	int busy_count;
	int i;

	memset(seen, 0, sizeof(seen));
	b_count = count_b_list(&bfreelist, seen);
	for (i = 0; i < nblkdev; i++) {
		dp = bdevsw[i].d_tab;
		if (dp)
			b_count += count_b_list(dp, seen);
	}
	busy_count = 0;
	for (i = 0; i < NBUF; i++) {
		CHECK(seen[i] == 1);
		if (buf[i].b_flags & B_BUSY) {
			busy_count++;
			CHECK(!on_av_list(&buf[i]));
		} else {
			CHECK(on_av_list(&buf[i]));
		}
	}
	av_count = count_av_list();
	CHECK(b_count == NBUF);
	CHECK(av_count + busy_count == NBUF);
}

static void test_binit(void)
{
	int i;

	binit();
	CHECK(nblkdev == 2);
	CHECK(bdevsw[1].d_tab != bdevsw[0].d_tab);
	CHECK(bdevsw[RAM_MAJOR].d_strategy == ramstrategy);
	CHECK(bdevsw[RAM_MAJOR].d_queue == &ramqueue);
	CHECK(ramqueue.dq_head == NULL);
	CHECK(ramqueue.dq_tail == NULL);
	CHECK(ramqueue.dq_active == NULL);
	CHECK(bdevsw[major(PIPEDEV)].d_strategy == NULL);
	CHECK(bdevsw[major(PIPEDEV)].d_queue == NULL);
	for (i = 0; i < NBUF; i++) {
		CHECK(buf[i].b_addr == buffers[i]);
		CHECK(buf[i].b_dev == NODEV);
		CHECK(!(buf[i].b_flags & B_BUSY));
	}
	CHECK(count_av_list() == NBUF);
	check_invariants();
}

static void test_cache_hit(void)
{
	struct buf *bp;
	struct buf *hit;

	binit();
	bp = getblk(PIPEDEV, 7);
	CHECK(bp != NULL);
	CHECK(bp->b_flags & B_BUSY);
	CHECK(!on_av_list(bp));
	CHECK(incore(PIPEDEV, 7));
	CHECK(getblk(PIPEDEV, 7) == NULL);
	bp->b_addr[0] = 0x5a;
	brelse(bp);
	CHECK(!(bp->b_flags & B_BUSY));
	CHECK(on_av_list(bp));
	hit = getblk(PIPEDEV, 7);
	CHECK(hit == bp);
	CHECK(hit->b_addr[0] == 0x5a);
	CHECK(!on_av_list(hit));
	brelse(hit);
	CHECK(on_av_list(hit));
	check_invariants();
}

static void test_exhaustion(void)
{
	struct buf *held[NBUF];
	int i;

	binit();
	for (i = 0; i < NBUF; i++) {
		held[i] = getblk(PIPEDEV, (unsigned long)i);
		CHECK(held[i] != NULL);
	}
	CHECK(count_av_list() == 0);
	CHECK(getblk(PIPEDEV, NBUF) == NULL);
	CHECK(geteblk() == NULL);
	check_invariants();
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	check_invariants();
}

static void test_lru(void)
{
	struct buf *held[NBUF];
	struct buf *bp;
	int i;

	binit();
	for (i = 0; i < NBUF; i++)
		held[i] = getblk(PIPEDEV, (unsigned long)i);
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	CHECK(bfreelist.av_forw == held[0]);
	bp = getblk(PIPEDEV, NBUF + 10UL);
	CHECK(bp == held[0]);
	brelse(bp);
	check_invariants();
}

static void test_age(void)
{
	struct buf *held[NBUF];
	struct buf *bp;
	int i;

	binit();
	for (i = 0; i < NBUF; i++)
		held[i] = getblk(PIPEDEV, (unsigned long)i);
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	bp = getblk(PIPEDEV, 0);
	CHECK(bp == held[0]);
	bp->b_flags |= B_AGE;
	brelse(bp);
	CHECK(bfreelist.av_forw == held[0]);
	bp = getblk(PIPEDEV, NBUF + 20UL);
	CHECK(bp == held[0]);
	brelse(bp);
	check_invariants();
}

static void test_geteblk_and_clrbuf(void)
{
	struct buf *bp;

	binit();
	bp = geteblk();
	CHECK(bp != NULL);
	CHECK(bp->b_dev == NODEV);
	memset(bp->b_addr, 0xa5, BSIZE);
	bp->b_resid = BSIZE;
	clrbuf(bp);
	CHECK(bp->b_addr[0] == 0);
	CHECK(bp->b_addr[BSIZE - 1] == 0);
	CHECK(bp->b_resid == 0);
	brelse(bp);
	check_invariants();
}

static void test_error_release(void)
{
	struct buf *bp;

	binit();
	bp = getblk(PIPEDEV, 91);
	CHECK(bp != NULL);
	bp->b_flags |= B_ERROR;
	brelse(bp);
	CHECK(bp->b_dev == NODEV);
	CHECK(bp->b_flags & B_ERROR);
	CHECK(!incore(PIPEDEV, 91));
	check_invariants();
}

static void test_device_numbers(void)
{
	unsigned int maj;
	unsigned int min;
	unsigned int dev;

	for (maj = 0; maj <= 255; maj++) {
		for (min = 0; min <= 255; min++) {
			dev = makedev(maj, min);
			CHECK(major(dev) == maj);
			CHECK(minor(dev) == min);
		}
	}
	CHECK(makedev(256, 0) == NODEV);
	CHECK(makedev(0, 256) == NODEV);
	CHECK(makedev(UINT_MAX, 0) == NODEV);
	CHECK(makedev(0, UINT_MAX) == NODEV);
	CHECK(makedev(-1, 0) == NODEV);
	CHECK(makedev(0, -1) == NODEV);
	maj = 1;
	min = 2;
	CHECK(makedev(maj++, min++) == 0x0102U);
	CHECK(maj == 2 && min == 3);
}

static void test_devices(void)
{
	struct buf *first;
	struct buf *second;
	struct buf *head;
	unsigned int invalid[] = { NODEV, 0x10000U, 0x1000000U,
				   0xffffU, 0x0200U };
	size_t i;

	binit();
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		CHECK(bdevsw_lookup(invalid[i]) == NULL);
		CHECK(getblk(invalid[i], 0) == NULL);
		CHECK(!incore(invalid[i], 0));
	}
	head = bdevsw[1].d_tab;
	bdevsw[1].d_tab = NULL;
	CHECK(bdevsw_lookup(makedev(1, 0)) == NULL);
	CHECK(getblk(makedev(1, 0), 0) == NULL);
	bdevsw[1].d_tab = head;
	first = getblk(makedev(1, 0), 7);
	second = getblk(makedev(1, 1), 7);
	CHECK(first != NULL && second != NULL && first != second);
	first->b_addr[0] = 0x35;
	second->b_addr[0] = 0x79;
	brelse(first);
	brelse(second);
	CHECK(getblk(makedev(1, 0), 7) == first);
	CHECK(getblk(makedev(1, 1), 7) == second);
	CHECK(first->b_addr[0] == 0x35);
	CHECK(second->b_addr[0] == 0x79);
	brelse(first);
	brelse(second);
	CHECK(bdevsw_lookup(makedev(1, 255)) == &bdevsw[1]);
	check_invariants();
}

static void begin_io_test(void)
{
	raminit();
	binit();
	saved_ram = bdevsw[RAM_MAJOR];
	bdevsw[RAM_MAJOR].d_strategy = io_strategy;
	io_reads = 0;
	io_writes = 0;
	io_reject = 0;
	io_fail = 0;
	io_error_code = 0;
}

static void end_io_test(void)
{
	CHECK(count_av_list() == NBUF);
	CHECK(ramqueue.dq_head == NULL);
	CHECK(ramqueue.dq_tail == NULL);
	CHECK(ramqueue.dq_active == NULL);
	bdevsw[RAM_MAJOR] = saved_ram;
	check_invariants();
}

static int io_strategy(struct buf *bp)
{
	CHECK(bp->b_flags & B_BUSY);
	CHECK(!(bp->b_flags & (B_DONE | B_ERROR | B_ASYNC)));
	CHECK(bp->b_bcount == BSIZE);
	CHECK(bp->b_error == 0);
	CHECK(bp->b_resid == BSIZE);
	if (bp->b_flags & B_READ) {
		io_reads++;
	} else {
		CHECK(!(bp->b_flags & (B_DELWRI | B_AGE)));
		io_writes++;
	}
	if (io_reject)
		return -io_reject;
	if (io_fail) {
		bp->b_flags |= B_ERROR;
		bp->b_error = io_error_code;
		iodone(bp);
		return 0;
	}
	return ramstrategy(bp);
}

static void test_block_io(void)
{
	struct buf *bp;
	unsigned int unit;
	unsigned long block;
	size_t i;
	int error;

	begin_io_test();
	for (unit = 0; unit < RAM_NDEV; unit++) {
		for (block = 0; block < RAM_NBLK; block++) {
			bp = getblk(makedev(RAM_MAJOR, unit), block);
			CHECK(bp != NULL);
			for (i = 0; i < BSIZE; i++)
				bp->b_addr[i] = unit * 73 + block * 7 + i;
			bp->b_flags |= B_READ | B_DONE | B_ERROR;
			bp->b_flags |= B_DELWRI | B_AGE;
			bp->b_bcount = 1;
			bp->b_error = SYS_EIO;
			CHECK(bwrite(bp) == 0);
		}
	}
	CHECK(io_writes == (int)(RAM_NDEV * RAM_NBLK));
	CHECK(count_av_list() == NBUF);
	binit();
	for (unit = 0; unit < RAM_NDEV; unit++) {
		for (block = 0; block < RAM_NBLK; block++) {
			error = -1;
			bp = bread(makedev(RAM_MAJOR, unit), block, &error);
			CHECK(bp != NULL && error == 0);
			CHECK(bp->b_flags & B_BUSY);
			CHECK(!on_av_list(bp));
			for (i = 0; i < BSIZE; i++)
				CHECK(bp->b_addr[i] == (unsigned char)
				      (unit * 73 + block * 7 + i));
			brelse(bp);
		}
	}
	CHECK(io_reads == (int)(RAM_NDEV * RAM_NBLK));
	end_io_test();
}

static void test_read_cache(void)
{
	struct buf *bp;
	struct buf *cached;
	size_t i;
	int error;

	begin_io_test();
	bp = bread(makedev(RAM_MAJOR, 0), 0, &error);
	CHECK(bp != NULL && error == 0);
	CHECK(io_reads == 1);
	cached = bp;
	brelse(bp);
	bp = bread(makedev(RAM_MAJOR, 0), 0, &error);
	CHECK(bp == cached && error == 0);
	CHECK(io_reads == 1);
	bp->b_addr[9] = 0x4c;
	CHECK(bwrite(bp) == 0);
	CHECK(io_writes == 1);
	bp = bread(makedev(RAM_MAJOR, 0), 0, &error);
	CHECK(bp != NULL && error == 0);
	CHECK(io_reads == 1);
	brelse(bp);
	binit();
	bp = bread(makedev(RAM_MAJOR, 0), 0, &error);
	CHECK(bp != NULL && error == 0);
	CHECK(io_reads == 2);
	for (i = 0; i < BSIZE; i++)
		CHECK(bp->b_addr[i] == (i == 9 ? 0x4c : 0));
	brelse(bp);
	end_io_test();
}

static void test_io_errors(void)
{
	struct buf *bp;
	int error;
	int i;

	begin_io_test();
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, NULL) == NULL);
	CHECK(io_reads == 0);
	CHECK(bwrite(NULL) == -SYS_EINVAL);
	for (i = 0; i < NBUF * 2; i++) {
		CHECK(bread(makedev(RAM_MAJOR, 2), 0, &error) == NULL);
		CHECK(error == -SYS_ENODEV);
		CHECK(bread(makedev(RAM_MAJOR, 0), RAM_NBLK,
			    &error) == NULL);
		CHECK(error == -SYS_EIO);
		CHECK(count_av_list() == NBUF);
	}
	CHECK(bread(makedev(RAM_MAJOR, 0), ULONG_MAX, &error) == NULL);
	CHECK(error == -SYS_EIO);
	bp = getblk(makedev(RAM_MAJOR, 0), RAM_NBLK);
	CHECK(bwrite(bp) == -SYS_EIO);
	CHECK(!incore(makedev(RAM_MAJOR, 0), RAM_NBLK));
	io_fail = 1;
	io_error_code = SYS_EINVAL;
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_EINVAL);
	CHECK(!incore(makedev(RAM_MAJOR, 0), 0));
	io_error_code = 0;
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_EIO);
	bp = getblk(makedev(RAM_MAJOR, 0), 0);
	CHECK(bwrite(bp) == -SYS_EIO);
	CHECK(!incore(makedev(RAM_MAJOR, 0), 0));
	io_fail = 0;
	io_reject = SYS_EAGAIN;
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_EAGAIN);
	bp = getblk(makedev(RAM_MAJOR, 0), 0);
	CHECK(bwrite(bp) == -SYS_EAGAIN);
	CHECK(!incore(makedev(RAM_MAJOR, 0), 0));
	io_reject = 0;
	bp = bread(makedev(RAM_MAJOR, 0), 0, &error);
	CHECK(bp != NULL && error == 0);
	brelse(bp);
	end_io_test();
}

static void test_io_registration(void)
{
	struct buf *bp;
	struct buf *head;
	unsigned int invalid[] = { NODEV, 0x10000U, 0x0200U, PIPEDEV };
	size_t i;
	int error;

	begin_io_test();
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		CHECK(bread(invalid[i], 0, &error) == NULL);
		CHECK(error == -SYS_ENODEV);
	}
	bp = geteblk();
	CHECK(bwrite(bp) == -SYS_ENODEV);
	head = bdevsw[RAM_MAJOR].d_tab;
	bdevsw[RAM_MAJOR].d_tab = NULL;
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_ENODEV);
	bdevsw[RAM_MAJOR].d_tab = head;
	bdevsw[RAM_MAJOR].d_flags = 0;
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_ENOSYS);
	bp = getblk(makedev(RAM_MAJOR, 0), 0);
	bp->b_flags |= B_DONE | B_DELWRI | B_AGE;
	CHECK(bwrite(bp) == -SYS_ENOSYS);
	CHECK(incore(makedev(RAM_MAJOR, 0), 0));
	CHECK(bfreelist.av_back->b_flags & B_DELWRI);
	bdevsw[RAM_MAJOR].d_flags = BDEV_SYNC;
	CHECK(bflush(NODEV) == 0);
	io_writes = 0;
	bdevsw[RAM_MAJOR].d_strategy = NULL;
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_ENODEV);
	bp = getblk(makedev(RAM_MAJOR, 0), 0);
	CHECK(bwrite(bp) == -SYS_ENODEV);
	CHECK(io_reads == 0 && io_writes == 0);
	end_io_test();
}

static void test_io_exhaustion(void)
{
	struct buf *held[NBUF];
	struct buf *bp;
	int error;
	int i;

	begin_io_test();
	held[0] = bread(makedev(RAM_MAJOR, 0), 0, &error);
	CHECK(held[0] != NULL && error == 0);
	CHECK(bread(makedev(RAM_MAJOR, 0), 0, &error) == NULL);
	CHECK(error == -SYS_EAGAIN);
	for (i = 1; i < NBUF; i++)
		held[i] = getblk(makedev(RAM_MAJOR, 0), (unsigned long)i);
	CHECK(bread(makedev(RAM_MAJOR, 0), NBUF, &error) == NULL);
	CHECK(error == -SYS_EAGAIN);
	CHECK(bread(NODEV, 0, &error) == NULL);
	CHECK(error == -SYS_ENODEV);
	CHECK(io_reads == 1);
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	binit();
	bp = getblk(makedev(RAM_MAJOR, 0), 0);
	bp->b_flags |= B_DONE | B_DELWRI;
	brelse(bp);
	for (i = 1; i < NBUF; i++) {
		held[i] = getblk(makedev(RAM_MAJOR, 0), (unsigned long)i);
		brelse(held[i]);
	}
	bp = bread(makedev(RAM_MAJOR, 0), NBUF, &error);
	CHECK(bp != NULL && error == 0);
	CHECK(io_writes == 1);
	brelse(bp);
	end_io_test();
}

static void check_write_rejected(struct buf *bp, int error)
{
	struct buf saved[NBUF];
	struct buf saved_free;
	unsigned char saved_data[NBUF][BSIZE];

	memcpy(saved, buf, sizeof(saved));
	memcpy(&saved_free, &bfreelist, sizeof(saved_free));
	memcpy(saved_data, buffers, sizeof(saved_data));
	CHECK(bwrite(bp) == error);
	CHECK(bdwrite(bp) == error);
	CHECK(bawrite(bp) == error);
	CHECK(memcmp(saved, buf, sizeof(saved)) == 0);
	CHECK(memcmp(&saved_free, &bfreelist, sizeof(saved_free)) == 0);
	CHECK(memcmp(saved_data, buffers, sizeof(saved_data)) == 0);
}

static void test_write_ownership(void)
{
	struct buf *bp;
	struct bdev_io_queue other;

	begin_io_test();
	bp = getblk(makedev(RAM_MAJOR, 0), 0);
	bp->b_flags |= B_ASYNC;
	check_write_rejected(bp, -SYS_EINVAL);
	bp->b_flags &= ~B_ASYNC;
	CHECK(bdev_io_queue_enqueue(&ramqueue, bp) == 0);
	check_write_rejected(bp, -SYS_EAGAIN);
	CHECK(ramqueue.dq_head == bp && ramqueue.dq_tail == bp);
	CHECK(bdev_io_queue_start(&ramqueue) == bp);
	check_write_rejected(bp, -SYS_EAGAIN);
	CHECK(ramqueue.dq_active == bp);
	CHECK(bdev_io_queue_complete(&ramqueue) == bp);
	bdev_io_queue_init(&other);
	bdevsw[0].d_queue = &other;
	CHECK(bdev_io_queue_enqueue(&other, bp) == 0);
	check_write_rejected(bp, -SYS_EAGAIN);
	CHECK(other.dq_head == bp && other.dq_tail == bp);
	CHECK(bdev_io_queue_start(&other) == bp);
	CHECK(bdev_io_queue_complete(&other) == bp);
	other.dq_tail = bp;
	check_write_rejected(bp, -SYS_EINVAL);
	CHECK(other.dq_head == NULL && other.dq_tail == bp);
	bdevsw[0].d_queue = NULL;
	brelse(bp);
	check_write_rejected(bp, -SYS_EINVAL);
	CHECK(io_writes == 0);
	end_io_test();
}

static void test_delayed_write(void)
{
	struct buf *bp;
	struct buf *held;
	unsigned int dev = makedev(RAM_MAJOR, 0);
	unsigned int other = makedev(RAM_MAJOR, 1);
	int error;

	begin_io_test();
	CHECK(bdwrite(NULL) == -SYS_EINVAL);
	CHECK(bawrite(NULL) == -SYS_EINVAL);
	bp = getblk(PIPEDEV, 0);
	CHECK(bdwrite(bp) == -SYS_ENODEV);
	CHECK(bp->b_flags & B_BUSY);
	brelse(bp);
	bp = getblk(dev, 0);
	memset(bp->b_addr, 0x71, BSIZE);
	CHECK(bdwrite(bp) == 0);
	CHECK(io_writes == 0);
	CHECK(bdwrite(bp) == -SYS_EINVAL);
	CHECK(bawrite(bp) == -SYS_EINVAL);
	held = bread(dev, 0, &error);
	CHECK(held == bp && error == 0);
	CHECK(io_reads == 0 && held->b_addr[0] == 0x71);
	CHECK(bflush(dev) == -SYS_EAGAIN);
	bp = getblk(other, 0);
	memset(bp->b_addr, 0x92, BSIZE);
	CHECK(bdwrite(bp) == 0);
	CHECK(bflush(other) == 0);
	CHECK(io_writes == 1);
	CHECK(held->b_flags & B_DELWRI);
	brelse(held);
	CHECK(bflush(NODEV) == 0);
	CHECK(io_writes == 2);
	CHECK(bflush(NODEV) == 0 && io_writes == 2);
	CHECK(bflush(PIPEDEV) == -SYS_ENODEV);
	binit();
	bp = bread(dev, 0, &error);
	CHECK(bp != NULL && error == 0 && bp->b_addr[0] == 0x71);
	brelse(bp);
	bp = bread(other, 0, &error);
	CHECK(bp != NULL && error == 0 && bp->b_addr[0] == 0x92);
	brelse(bp);
	end_io_test();
}

static void test_writeback_failure(void)
{
	struct buf *bp;
	struct buf *held[NBUF];
	unsigned int dev = makedev(RAM_MAJOR, 0);
	int error;
	int i;

	begin_io_test();
	for (i = 0; i < NBUF; i++) {
		bp = getblk(dev, (unsigned long)i);
		memset(bp->b_addr, 0x30 + i, BSIZE);
		CHECK(bdwrite(bp) == 0);
	}
	io_fail = 1;
	io_error_code = SYS_EIO;
	CHECK(geteblk() == NULL);
	CHECK(io_writes == 1);
	CHECK(count_av_list() == NBUF);
	CHECK(bflush(NODEV) == -SYS_EIO);
	CHECK(io_writes == NBUF + 1);
	for (i = 0; i < NBUF; i++) {
		held[i] = bread(dev, (unsigned long)i, &error);
		CHECK(held[i] != NULL && error == 0);
		CHECK(held[i]->b_addr[0] == 0x30 + i);
		CHECK(held[i]->b_flags & B_DELWRI);
		CHECK(held[i]->b_error == SYS_EIO);
	}
	CHECK(io_reads == 0);
	CHECK(bflush(dev) == -SYS_EAGAIN);
	CHECK(bwrite(held[0]) == -SYS_EIO);
	CHECK(incore(dev, 0));
	held[0] = bread(dev, 0, &error);
	CHECK(held[0] != NULL && held[0]->b_addr[0] == 0x30);
	CHECK(held[0]->b_flags & B_DELWRI);
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	io_fail = 0;
	io_reject = SYS_EAGAIN;
	CHECK(bflush(dev) == -SYS_EAGAIN);
	CHECK(count_av_list() == NBUF);
	io_reject = 0;
	CHECK(bflush(dev) == 0);
	check_invariants();
	binit();
	for (i = 0; i < NBUF; i++) {
		bp = bread(dev, (unsigned long)i, &error);
		CHECK(bp != NULL && error == 0);
		CHECK(bp->b_addr[0] == 0x30 + i);
		brelse(bp);
	}
	end_io_test();
}

static void test_dirty_reuse(void)
{
	struct buf *bp;
	unsigned int dev = makedev(RAM_MAJOR, 0);
	int error;
	int i;

	begin_io_test();
	for (i = 0; i < NBUF; i++) {
		bp = getblk(dev, (unsigned long)i);
		memset(bp->b_addr, 0x50 + i, BSIZE);
		CHECK(bdwrite(bp) == 0);
	}
	bp = geteblk();
	CHECK(bp != NULL && bp->b_dev == NODEV);
	CHECK(io_writes == NBUF);
	brelse(bp);
	CHECK(bflush(NODEV) == 0);
	for (i = 0; i < NBUF; i++) {
		bp = bread(dev, (unsigned long)i, &error);
		CHECK(bp != NULL && error == 0);
		CHECK(bdwrite(bp) == 0);
	}
	bp = getblk(PIPEDEV, 90);
	CHECK(bp != NULL && bp->b_dev == PIPEDEV);
	CHECK(io_writes == NBUF * 2);
	brelse(bp);
	binit();
	for (i = 0; i < NBUF; i++) {
		bp = bread(dev, (unsigned long)i, &error);
		CHECK(bp != NULL && error == 0);
		CHECK(bp->b_addr[0] == 0x50 + i);
		brelse(bp);
	}
	bp = getblk(dev, NBUF);
	bp->b_addr[0] = 0xac;
	CHECK(bawrite(bp) == 0);
	CHECK(!(bp->b_flags & (B_BUSY | B_ASYNC | B_DELWRI)));
	end_io_test();
}

static void check_payload(struct buf *bp, unsigned char value)
{
	size_t i;

	for (i = 0; i < BSIZE; i++)
		CHECK(bp->b_addr[i] == value);
}

static void test_unwritable_device(void)
{
	struct bdevsw registered;
	struct buf *held[NBUF];
	struct buf *bp;
	unsigned int dev = makedev(RAM_MAJOR, 0);
	int error;
	int kind;
	int i;

	for (kind = 0; kind < 4; kind++) {
		begin_io_test();
		registered = bdevsw[RAM_MAJOR];
		if (kind == 2)
			bdevsw[RAM_MAJOR].d_strategy = NULL;
		if (kind == 3)
			bdevsw[RAM_MAJOR].d_flags = 0;
		for (i = 0; i < NBUF * 2; i++) {
			if (kind == 0)
				bp = geteblk();
			else
				bp = getblk(kind == 1 ? PIPEDEV : dev,
					    (unsigned long)i);
			CHECK(bp != NULL);
			if (!bp)
				break;
			memset(bp->b_addr, 0x61, BSIZE);
			error = kind == 3 ? -SYS_ENOSYS : -SYS_ENODEV;
			CHECK(bawrite(bp) == error);
			CHECK(count_av_list() == NBUF);
		}
		CHECK(io_reads == 0 && io_writes == 0);
		for (i = 0; i < NBUF; i++)
			CHECK(!(buf[i].b_flags & (B_BUSY | B_DELWRI)));
		bdevsw[RAM_MAJOR] = registered;
		CHECK(bflush(NODEV) == 0);
		for (i = 0; i < NBUF; i++) {
			held[i] = getblk(PIPEDEV, (unsigned long)i);
			CHECK(held[i] != NULL);
		}
		for (i = 0; i < NBUF; i++)
			brelse(held[i]);
		bp = bread(dev, 0, &error);
		CHECK(bp != NULL && error == 0);
		if (bp) {
			check_payload(bp, 0);
			brelse(bp);
		}
		end_io_test();
	}
}

static void test_dirty_registration_failure(void)
{
	struct bdevsw registered;
	struct buf *bp;
	unsigned int dev = makedev(RAM_MAJOR, 0);
	int error;
	int kind;

	for (kind = 0; kind < 2; kind++) {
		begin_io_test();
		registered = bdevsw[RAM_MAJOR];
		bp = getblk(dev, 0);
		CHECK(bp != NULL);
		memset(bp->b_addr, 0x83, BSIZE);
		CHECK(bdwrite(bp) == 0);
		bp = getblk(dev, 0);
		CHECK(bp != NULL);
		if (kind == 0)
			bdevsw[RAM_MAJOR].d_strategy = NULL;
		else
			bdevsw[RAM_MAJOR].d_flags = 0;
		error = kind == 0 ? -SYS_ENODEV : -SYS_ENOSYS;
		CHECK(bawrite(bp) == error);
		CHECK(incore(dev, 0));
		CHECK(count_av_list() == NBUF);
		bdevsw[RAM_MAJOR] = registered;
		bp = getblk(dev, 0);
		CHECK(bp != NULL);
		if (bp) {
			CHECK(bp->b_flags & B_DELWRI);
			CHECK(bp->b_flags & B_ERROR);
			CHECK(bp->b_error == -error);
			check_payload(bp, 0x83);
			brelse(bp);
		}
		CHECK(bflush(dev) == 0);
		binit();
		bp = bread(dev, 0, &error);
		CHECK(bp != NULL && error == 0);
		if (bp) {
			check_payload(bp, 0x83);
			brelse(bp);
		}
		end_io_test();
	}
}

static void test_writeback_rejection(void)
{
	struct bdev_io_queue other;
	struct bdev_io_queue *queue;
	struct buf dummy;
	struct buf *bp;
	unsigned int dev = makedev(RAM_MAJOR, 0);
	int error;
	int operation;
	int device;

	for (device = 0; device < 2; device++) {
		for (operation = 0; operation < 4; operation++) {
			begin_io_test();
			bp = getblk(dev, 0);
			CHECK(bp != NULL);
			memset(bp->b_addr, 0xa7, BSIZE);
			bp->b_flags |= B_AGE;
			CHECK(bdwrite(bp) == 0);
			CHECK(bfreelist.av_forw == bp);
			bdev_io_queue_init(&other);
			queue = device == 0 ? &ramqueue : &other;
			if (device == 1)
				bdevsw[0].d_queue = queue;
			memset(&dummy, 0, sizeof(dummy));
			dummy.b_flags = B_BUSY;
			queue->dq_tail = &dummy;
			if (operation == 0 || operation == 1) {
				CHECK(bflush(operation ? NODEV : dev) ==
				      -SYS_EINVAL);
			} else if (operation == 2) {
				CHECK(geteblk() == NULL);
			} else {
				CHECK(getblk(PIPEDEV, 90) == NULL);
			}
			CHECK(count_av_list() == NBUF);
			CHECK(io_writes == 0);
			check_invariants();
			bdev_io_queue_init(queue);
			bdevsw[0].d_queue = NULL;
			bp = getblk(dev, 0);
			CHECK(bp != NULL);
			if (bp) {
				CHECK(bp->b_flags & B_DELWRI);
				CHECK(bp->b_flags & B_DONE);
				check_payload(bp, 0xa7);
				brelse(bp);
			}
			error = bflush(dev);
			CHECK(error == 0);
			if (!error) {
				binit();
				bp = bread(dev, 0, &error);
				CHECK(bp != NULL && error == 0);
				if (bp) {
					check_payload(bp, 0xa7);
					brelse(bp);
				}
			}
			end_io_test();
		}
	}
}
