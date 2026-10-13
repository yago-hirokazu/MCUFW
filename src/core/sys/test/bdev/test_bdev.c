#include <stdio.h>
#include <stdlib.h>

#include "buf.h"
#include "conf.h"

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
static struct bdev_queue mock_queue;
static struct buf *mock_started[NBUF * 3];
static int mock_started_count;
static int mock_completed_count;

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

int main(void)
{
	test_empty();
	test_one_request();
	test_multiple_requests();
	test_error_retry();
	test_async_completion();
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
	if (bdev_queue_enqueue(&mock_queue, bp) < 0)
		return -1;
	mock_start();
	return 0;
}

static void mock_start(void)
{
	struct buf *bp;

	bp = bdev_queue_start(&mock_queue);
	if (!bp)
		return;
	mock_started[mock_started_count++] = bp;
}

static int mock_interrupt(int error)
{
	int result;

	if (error) {
		result = bdev_queue_error(&mock_queue, MOCK_RETRY_LIMIT,
					  MOCK_ERROR);
		if (result == BDEV_QUEUE_RETRY) {
			mock_started[mock_started_count++] =
				mock_queue.dq_active;
			return result;
		}
	} else {
		if (!bdev_queue_complete(&mock_queue))
			return BDEV_QUEUE_FAILED;
		result = BDEV_QUEUE_DONE;
	}
	mock_completed_count++;
	mock_start();
	return result;
}

static void setup(void)
{
	bdevsw[major(PIPEDEV)].d_strategy = mock_strategy;
	bdevsw[major(PIPEDEV)].d_queue = &mock_queue;
	binit();
	mock_started_count = 0;
	mock_completed_count = 0;
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
	bdevsw[major(PIPEDEV)].d_strategy = NULL;
	bdevsw[major(PIPEDEV)].d_queue = NULL;
}

static void test_empty(void)
{
	struct bdev_queue queue;
	struct buf free_buffer;

	setup();
	queue.dq_head = &free_buffer;
	queue.dq_tail = &free_buffer;
	queue.dq_active = &free_buffer;
	queue.dq_errcnt = 1;
	bdev_queue_init(&queue);
	CHECK(queue.dq_head == NULL);
	CHECK(queue.dq_tail == NULL);
	CHECK(queue.dq_active == NULL);
	CHECK(queue.dq_errcnt == 0);
	bdev_queue_init(NULL);
	CHECK(bdev_queue_start(&mock_queue) == NULL);
	CHECK(bdev_queue_complete(&mock_queue) == NULL);
	CHECK(bdev_queue_error(&mock_queue, 0, MOCK_ERROR) ==
	      BDEV_QUEUE_FAILED);
	free_buffer.b_flags = 0;
	CHECK(bdev_queue_enqueue(&mock_queue, &free_buffer) < 0);
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
	CHECK(bdevsw[major(PIPEDEV)].d_strategy(bp) == 0);
	CHECK(mock_queue.dq_head == NULL);
	CHECK(mock_queue.dq_tail == NULL);
	CHECK(mock_queue.dq_active == bp);
	CHECK(mock_started_count == 1);
	CHECK(bdev_queue_enqueue(&mock_queue, bp) < 0);
	CHECK(mock_interrupt(0) == BDEV_QUEUE_DONE);
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
		CHECK(bdevsw[major(PIPEDEV)].d_strategy(bp[i]) == 0);
		CHECK(bp[i]->av_forw == NULL);
		CHECK(bp[i]->av_back == NULL);
	}
	CHECK(mock_queue.dq_active == bp[0]);
	CHECK(mock_queue.dq_head == bp[1]);
	CHECK(mock_queue.dq_tail == bp[NBUF - 1]);
	CHECK(bp[1]->b_io_next == bp[2]);
	CHECK(bp[NBUF - 1]->b_io_next == NULL);
	CHECK(bdev_queue_enqueue(&mock_queue, bp[1]) < 0);
	for (i = 0; i < NBUF; i++) {
		CHECK(mock_queue.dq_active == bp[i]);
		CHECK(mock_interrupt(0) == BDEV_QUEUE_DONE);
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
	CHECK(bdevsw[major(PIPEDEV)].d_strategy(bp) == 0);
	CHECK(bdevsw[major(PIPEDEV)].d_strategy(next) == 0);
	CHECK(mock_interrupt(1) == BDEV_QUEUE_RETRY);
	CHECK(mock_queue.dq_active == bp);
	CHECK(mock_queue.dq_errcnt == 1);
	CHECK(!(bp->b_flags & (B_DONE | B_ERROR)));
	CHECK(mock_interrupt(1) == BDEV_QUEUE_RETRY);
	CHECK(mock_queue.dq_active == bp);
	CHECK(mock_queue.dq_errcnt == MOCK_RETRY_LIMIT);
	CHECK(mock_interrupt(1) == BDEV_QUEUE_DONE);
	CHECK(mock_queue.dq_active == next);
	CHECK(mock_queue.dq_errcnt == 0);
	CHECK(bp->b_flags & B_DONE);
	CHECK(bp->b_flags & B_ERROR);
	CHECK(bp->b_error == MOCK_ERROR);
	CHECK(mock_started_count == 4);
	CHECK(mock_completed_count == 1);
	CHECK(mock_interrupt(0) == BDEV_QUEUE_DONE);
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
	CHECK(bdevsw[major(PIPEDEV)].d_strategy(bp) == 0);
	CHECK(mock_interrupt(0) == BDEV_QUEUE_DONE);
	CHECK(bp->b_flags & B_DONE);
	CHECK(!(bp->b_flags & B_BUSY));
	CHECK(!(bp->b_flags & B_ASYNC));
	CHECK(on_av_list(bp));
	CHECK(mock_completed_count == 1);
	teardown();
}
