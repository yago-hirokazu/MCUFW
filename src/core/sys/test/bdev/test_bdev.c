#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "conf.h"
#include "ramdisk.h"
#include "sys_error.h"

#define MOCK_RETRY_LIMIT	2U
#define MOCK_ERROR	5

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

static int failures;
static struct bdev_io_queue mock_queue;
static struct bdevsw saved_devices[2];
static struct buf *mock_started[NBUF * 3];
static int mock_started_count;
static int mock_completed_count;
static int mock_calls;

static int on_av_list(struct buf *target);
static int mock_strategy(struct buf *bp);
static void mock_start(void);
static int mock_interrupt(int error);
static void setup(void);
static void teardown(void);
static void test_empty(void);
static void test_one_request(void);
static void test_multiple_requests(void);
static void test_error_retry(void);
static void test_async_completion(void);
static void check_rejection(struct buf *bp, int error);
static void test_strategy_rejection(void);
static void test_other_queue(void);
static void test_invalid_queue(void);
static void test_driver_result(void);
static int reject_strategy(struct buf *bp);
static int error_strategy(struct buf *bp);

int main(void)
{
	test_empty();
	test_one_request();
	test_multiple_requests();
	test_error_retry();
	test_async_completion();
	test_strategy_rejection();
	test_other_queue();
	test_invalid_queue();
	test_driver_result();
	CHECK(bdevsw[RAM_MAJOR].d_strategy == ramstrategy);
	CHECK(bdevsw[RAM_MAJOR].d_queue == &ramqueue);
	if (failures) {
		printf("%d tests failed\n", failures);
		return EXIT_FAILURE;
	}
	printf("all tests passed\n");
	return EXIT_SUCCESS;
}

static int on_av_list(struct buf *target)
{
	struct buf *bp;
	int count;

	count = 0;
	for (bp = bfreelist.av_forw; bp != &bfreelist && count < NBUF;
	     bp = bp->av_forw) {
		if (bp == target)
			return 1;
		count++;
	}
	return 0;
}

static int mock_strategy(struct buf *bp)
{
	mock_calls++;
	if (bdev_io_queue_enqueue(&mock_queue, bp) < 0)
		return -1;
	mock_start();
	return 0;
}

static void mock_start(void)
{
	struct buf *bp;

	bp = bdev_io_queue_start(&mock_queue);
	if (!bp)
		return;
	mock_started[mock_started_count++] = bp;
}

static int mock_interrupt(int error)
{
	int result;

	if (error) {
		result = bdev_io_queue_error(&mock_queue, MOCK_RETRY_LIMIT,
					  MOCK_ERROR);
		if (result == BDEV_IO_QUEUE_RETRY) {
			mock_started[mock_started_count++] =
				mock_queue.dq_active;
			return result;
		}
	} else {
		if (!bdev_io_queue_complete(&mock_queue))
			return BDEV_IO_QUEUE_FAILED;
		result = BDEV_IO_QUEUE_DONE;
	}
	mock_completed_count++;
	mock_start();
	return result;
}

static void setup(void)
{
	memcpy(saved_devices, bdevsw, sizeof(saved_devices));
	bdevsw[RAM_MAJOR].d_strategy = NULL;
	bdevsw[RAM_MAJOR].d_queue = NULL;
	bdevsw[major(PIPEDEV)].d_strategy = mock_strategy;
	bdevsw[major(PIPEDEV)].d_queue = &mock_queue;
	binit();
	mock_started_count = 0;
	mock_completed_count = 0;
	mock_calls = 0;
}

static void teardown(void)
{
	int i;

	CHECK(mock_queue.dq_head == NULL);
	CHECK(mock_queue.dq_tail == NULL);
	CHECK(mock_queue.dq_active == NULL);
	for (i = 0; i < NBUF; i++) {
		CHECK(!(buf[i].b_flags & B_BUSY));
		CHECK(buf[i].b_io_next == NULL);
	}
	memcpy(bdevsw, saved_devices, sizeof(saved_devices));
}

static void test_empty(void)
{
	struct bdev_io_queue queue;
	struct buf free_buffer;

	setup();
	queue.dq_head = &free_buffer;
	queue.dq_tail = &free_buffer;
	queue.dq_active = &free_buffer;
	queue.dq_errcnt = 1;
	bdev_io_queue_init(&queue);
	CHECK(queue.dq_head == NULL);
	CHECK(queue.dq_tail == NULL);
	CHECK(queue.dq_active == NULL);
	CHECK(queue.dq_errcnt == 0);
	bdev_io_queue_init(NULL);
	CHECK(bdev_io_queue_start(&mock_queue) == NULL);
	CHECK(bdev_io_queue_complete(&mock_queue) == NULL);
	CHECK(bdev_io_queue_error(&mock_queue, 0, MOCK_ERROR) ==
	      BDEV_IO_QUEUE_FAILED);
	free_buffer.b_flags = 0;
	CHECK(bdev_io_queue_enqueue(&mock_queue, &free_buffer) < 0);
	teardown();
}

static void test_one_request(void)
{
	struct buf *bp;

	setup();
	bp = getblk(PIPEDEV, 1);
	CHECK(bp != NULL);
	CHECK(!on_av_list(bp));
	bp->b_flags |= B_WANTED;
	CHECK(bdev_strategy(bp) == 0);
	CHECK(mock_queue.dq_head == NULL);
	CHECK(mock_queue.dq_tail == NULL);
	CHECK(mock_queue.dq_active == bp);
	CHECK(mock_started_count == 1);
	CHECK(bdev_io_queue_enqueue(&mock_queue, bp) < 0);
	CHECK(mock_interrupt(0) == BDEV_IO_QUEUE_DONE);
	CHECK(bp->b_flags & B_DONE);
	CHECK(bp->b_flags & B_BUSY);
	CHECK(!(bp->b_flags & B_WANTED));
	CHECK(!on_av_list(bp));
	CHECK(mock_completed_count == 1);
	brelse(bp);
	teardown();
}

static void test_multiple_requests(void)
{
	struct buf *bp[NBUF];
	int i;

	setup();
	for (i = 0; i < NBUF; i++) {
		bp[i] = getblk(PIPEDEV, (unsigned long)(10 + i));
		CHECK(bp[i] != NULL);
		CHECK(bdev_strategy(bp[i]) == 0);
		CHECK(!on_av_list(bp[i]));
	}
	CHECK(mock_queue.dq_active == bp[0]);
	CHECK(mock_queue.dq_head == bp[1]);
	CHECK(mock_queue.dq_tail == bp[NBUF - 1]);
	CHECK(bp[1]->b_io_next == bp[2]);
	CHECK(bp[NBUF - 1]->b_io_next == NULL);
	check_rejection(bp[1], -SYS_EAGAIN);
	CHECK(bdev_io_queue_enqueue(&mock_queue, bp[1]) < 0);
	for (i = 0; i < NBUF; i++) {
		CHECK(mock_queue.dq_active == bp[i]);
		CHECK(mock_interrupt(0) == BDEV_IO_QUEUE_DONE);
		CHECK(mock_started[i] == bp[i]);
		CHECK(bp[i]->b_flags & B_DONE);
		brelse(bp[i]);
	}
	CHECK(mock_completed_count == NBUF);
	teardown();
}

static void test_error_retry(void)
{
	struct buf *bp;
	struct buf *next;

	setup();
	bp = getblk(PIPEDEV, 30);
	next = getblk(PIPEDEV, 31);
	CHECK(bp != NULL);
	CHECK(next != NULL);
	CHECK(bdev_strategy(bp) == 0);
	CHECK(bdev_strategy(next) == 0);
	CHECK(mock_interrupt(1) == BDEV_IO_QUEUE_RETRY);
	CHECK(mock_queue.dq_active == bp);
	CHECK(mock_queue.dq_errcnt == 1);
	CHECK(!(bp->b_flags & (B_DONE | B_ERROR)));
	CHECK(mock_interrupt(1) == BDEV_IO_QUEUE_RETRY);
	CHECK(mock_queue.dq_active == bp);
	CHECK(mock_queue.dq_errcnt == MOCK_RETRY_LIMIT);
	CHECK(mock_interrupt(1) == BDEV_IO_QUEUE_DONE);
	CHECK(mock_queue.dq_active == next);
	CHECK(mock_queue.dq_errcnt == 0);
	CHECK(bp->b_flags & B_DONE);
	CHECK(bp->b_flags & B_ERROR);
	CHECK(bp->b_error == MOCK_ERROR);
	CHECK(mock_started_count == 4);
	CHECK(mock_completed_count == 1);
	CHECK(mock_interrupt(0) == BDEV_IO_QUEUE_DONE);
	CHECK(mock_queue.dq_active == NULL);
	CHECK(next->b_flags & B_DONE);
	CHECK(mock_completed_count == 2);
	brelse(bp);
	brelse(next);
	teardown();
}

static void test_async_completion(void)
{
	struct buf *bp;

	setup();
	bp = getblk(PIPEDEV, 40);
	CHECK(bp != NULL);
	bp->b_flags |= B_ASYNC;
	CHECK(bdev_strategy(bp) == 0);
	CHECK(mock_interrupt(0) == BDEV_IO_QUEUE_DONE);
	CHECK(bp->b_flags & B_DONE);
	CHECK(!(bp->b_flags & B_BUSY));
	CHECK(!(bp->b_flags & B_ASYNC));
	CHECK(on_av_list(bp));
	CHECK(mock_completed_count == 1);
	teardown();
}

static void check_rejection(struct buf *bp, int error)
{
	struct buf saved_buf[NBUF];
	struct buf saved_free;
	struct buf saved_tab[2];
	struct bdev_io_queue saved_queue[2];
	unsigned char saved_data[NBUF][BSIZE];
	int calls;
	int i;

	CHECK(nblkdev == 2);
	memcpy(saved_buf, buf, sizeof(saved_buf));
	memcpy(&saved_free, &bfreelist, sizeof(saved_free));
	memcpy(saved_data, buffers, sizeof(saved_data));
	for (i = 0; i < 2; i++) {
		if (bdevsw[i].d_tab)
			memcpy(&saved_tab[i], bdevsw[i].d_tab,
			       sizeof(saved_tab[i]));
		if (bdevsw[i].d_queue)
			memcpy(&saved_queue[i], bdevsw[i].d_queue,
			       sizeof(saved_queue[i]));
	}
	calls = mock_calls;
	CHECK(bdev_strategy(bp) == error);
	CHECK(mock_calls == calls);
	CHECK(memcmp(saved_buf, buf, sizeof(saved_buf)) == 0);
	CHECK(memcmp(&saved_free, &bfreelist, sizeof(saved_free)) == 0);
	CHECK(memcmp(saved_data, buffers, sizeof(saved_data)) == 0);
	for (i = 0; i < 2; i++) {
		if (bdevsw[i].d_tab)
			CHECK(memcmp(&saved_tab[i], bdevsw[i].d_tab,
				     sizeof(saved_tab[i])) == 0);
		if (bdevsw[i].d_queue)
			CHECK(memcmp(&saved_queue[i], bdevsw[i].d_queue,
				     sizeof(saved_queue[i])) == 0);
	}
}

static void test_strategy_rejection(void)
{
	struct buf *bp;
	struct buf *head;
	unsigned int invalid[] = { NODEV, 0x10000U, 0xffffU, 0x0200U };
	size_t i;

	setup();
	check_rejection(NULL, -SYS_EINVAL);
	bp = getblk(PIPEDEV, 50);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		bp->b_dev = invalid[i];
		check_rejection(bp, -SYS_ENODEV);
	}
	bp->b_dev = makedev(1, 0);
	check_rejection(bp, -SYS_ENODEV);
	bdevsw[1].d_strategy = mock_strategy;
	head = bdevsw[1].d_tab;
	bdevsw[1].d_tab = NULL;
	check_rejection(bp, -SYS_ENODEV);
	bdevsw[1].d_tab = head;
	bdevsw[1].d_strategy = NULL;
	bp->b_dev = PIPEDEV;
	CHECK(bdev_strategy(bp) == 0);
	check_rejection(bp, -SYS_EAGAIN);
	CHECK(mock_interrupt(0) == BDEV_IO_QUEUE_DONE);
	brelse(bp);
	check_rejection(bp, -SYS_EINVAL);
	teardown();
}

static void test_other_queue(void)
{
	struct bdev_io_queue other;
	struct buf *bp;
	struct buf *active;

	setup();
	bdev_io_queue_init(&other);
	bdevsw[1].d_queue = &other;
	bp = getblk(PIPEDEV, 60);
	active = getblk(makedev(1, 0), 61);
	CHECK(bdev_io_queue_enqueue(&other, active) == 0);
	CHECK(bdev_io_queue_start(&other) == active);
	CHECK(bdev_io_queue_enqueue(&other, bp) == 0);
	check_rejection(bp, -SYS_EAGAIN);
	CHECK(bdev_io_queue_complete(&other) == active);
	CHECK(bdev_io_queue_start(&other) == bp);
	check_rejection(bp, -SYS_EAGAIN);
	CHECK(bdev_io_queue_complete(&other) == bp);
	brelse(active);
	brelse(bp);
	bdevsw[1].d_queue = NULL;
	teardown();
}

static void test_invalid_queue(void)
{
	struct bdev_io_queue other;
	struct buf nodes[NBUF + 1];
	struct buf *bp;
	int i;

	setup();
	memset(nodes, 0, sizeof(nodes));
	for (i = 0; i <= NBUF; i++) {
		nodes[i].b_flags = B_BUSY;
		if (i < NBUF)
			nodes[i].b_io_next = &nodes[i + 1];
	}
	bdev_io_queue_init(&other);
	bdevsw[1].d_queue = &other;
	bp = getblk(PIPEDEV, 70);
	other.dq_head = nodes;
	other.dq_tail = &nodes[NBUF];
	check_rejection(bp, -SYS_EINVAL);
	other.dq_tail = NULL;
	check_rejection(bp, -SYS_EINVAL);
	other.dq_tail = nodes;
	nodes[0].b_io_next = nodes;
	check_rejection(bp, -SYS_EINVAL);
	nodes[0].b_io_next = NULL;
	nodes[0].b_flags = 0;
	check_rejection(bp, -SYS_EINVAL);
	nodes[0].b_flags = B_BUSY;
	other.dq_active = nodes;
	check_rejection(bp, -SYS_EINVAL);
	bdevsw[1].d_queue = NULL;
	brelse(bp);
	teardown();
}

static int reject_strategy(struct buf *bp)
{
	(void)bp;
	mock_calls++;
	return -SYS_EIO;
}

static int error_strategy(struct buf *bp)
{
	mock_calls++;
	bp->b_flags |= B_ERROR;
	bp->b_error = SYS_EIO;
	bp->b_resid = bp->b_bcount;
	iodone(bp);
	return 0;
}

static void test_driver_result(void)
{
	struct buf *bp;
	struct buf saved;

	setup();
	bp = getblk(PIPEDEV, 80);
	memcpy(&saved, bp, sizeof(saved));
	bdevsw[0].d_strategy = reject_strategy;
	CHECK(bdev_strategy(bp) == -SYS_EIO);
	CHECK(memcmp(&saved, bp, sizeof(saved)) == 0);
	CHECK(mock_calls == 1);
	bdevsw[0].d_strategy = error_strategy;
	CHECK(bdev_strategy(bp) == 0);
	CHECK(bp->b_flags & B_DONE);
	CHECK(bp->b_flags & B_ERROR);
	CHECK(bp->b_flags & B_BUSY);
	CHECK(bp->b_error == SYS_EIO);
	CHECK(mock_calls == 2);
	brelse(bp);
	bp = getblk(makedev(1, 255), 81);
	bdevsw[1].d_strategy = error_strategy;
	CHECK(bdev_strategy(bp) == 0);
	CHECK(bp->b_dev == makedev(1, 255));
	CHECK(bp->b_flags & B_DONE);
	CHECK(bp->b_error == SYS_EIO);
	CHECK(mock_calls == 3);
	brelse(bp);
	bdevsw[1].d_strategy = NULL;
	teardown();
}
