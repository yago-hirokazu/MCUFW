/* test_bio.c */

#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "conf.h"

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

static int failures;

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

int main(void)
{
	test_binit();
	test_cache_hit();
	test_exhaustion();
	test_lru();
	test_age();
	test_geteblk_and_clrbuf();
	test_error_release();
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
	CHECK(nblkdev == 1);
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
	hit = getblk(PIPEDEV, 7);
	CHECK(hit == bp);
	CHECK(hit->b_addr[0] == 0x5a);
	brelse(hit);
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
