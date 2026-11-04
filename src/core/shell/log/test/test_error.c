/*
 * Deterministic clock => ring / assert tests => captured dump
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "custom_assert.h"
#include "log_adapt.h"

#define CHECK(c) do { if ((c) == 0) exit(__LINE__); } while (0)

static uint32_t clock_time, clock_wrap;
static int held, init_failure;
static int conditions, errors, returns, continued;
static unsigned int failure_line;

/*
 * Simulate platform initialization failure
 */
int log_adapt_init(void)
{
	return init_failure;
}

/*
 * Supply deterministic wrap-boundary samples
 */
void log_adapt_get_time(uint32_t *time, uint32_t *wrap)
{
	*time = clock_time;
	*wrap = clock_wrap;
}

/*
 * Detect nested or missing lock operations
 */
void log_adapt_lock(enum log_lock_id id)
{
	CHECK(id == LOG_ERROR_LOCK && held == 0);
	held = 1;
}

/*
 * Verify matching unlock
 */
void log_adapt_unlock(enum log_lock_id id)
{
	CHECK(id == LOG_ERROR_LOCK && held == 1);
	held = 0;
}

/*
 * Exercise single evaluation and caller line capture
 */
static int warm(int valid)
{
	failure_line = __LINE__ + 1;
	assert_warm((conditions++, valid), (errors++, 0x1203),
		    (returns++, -7));
	continued++;
	return 1;
}

/*
 * Exercise void return and surrounding if/else
 */
static void warm_void(int valid)
{
	if (valid != 2)
		assert_warm_void(valid, 0x1204);
	else
		continued += 2;
	continued++;
}

/*
 * Verify ring order, failed reads, assertions and dump format
 */
int main(void)
{
	struct log_error_t log = {0}, out = {0};
	uint32_t i;

	init_failure = -1;
	CHECK(error_init() == -1);
	init_failure = 0;
	CHECK(error_init() == 0);
	CHECK(error_count() == 0);
	out.err = 0xabcd;
	CHECK(error_read(0, &out) == -1 && out.err == 0xabcd);
	CHECK(error_read(UINT32_MAX, &out) == -1);
	for (i = 0; i < NERRLOG * 3 + 5; i++) {
		log.time = i;
		log.err = 0x1201;
		log.file = 0x12;
		error_write(&log);
		CHECK(error_count() == (i < NERRLOG ? i + 1 : NERRLOG));
	}
	for (i = 0; i < NERRLOG; i++) {
		CHECK(error_read(i, &out) == 0);
		CHECK(out.time == NERRLOG * 2 + 5 + i);
	}
	CHECK(error_read(NERRLOG, &out) == -1);
	CHECK(warm(1) == 1);
	CHECK(conditions == 1 && errors == 0 && returns == 0);
	CHECK(warm(0) == -7);
	CHECK(conditions == 2 && errors == 1 && returns == 1);
	CHECK(continued == 1);
	CHECK(error_read(NERRLOG - 1, &out) == 0);
	CHECK(out.err == 0x1203 && out.file == 0x12);
	CHECK(out.line == failure_line);
	warm_void(0);
	CHECK(continued == 1);
	warm_void(1);
	warm_void(2);
	CHECK(continued == 5);

	CHECK(error_init() == 0);
	clock_time = UINT32_MAX;
	clock_wrap = 7;
	error_record(0x1201, 42);
	clock_time = 0;
	clock_wrap = 8;
	error_record(0x1202, 43);
	CHECK(error_read(0, &out) == 0);
	CHECK(out.time == UINT32_MAX && out.wrap == 7);
	CHECK(error_read(1, &out) == 0);
	CHECK(out.time == 0 && out.wrap == 8);
	error_dump();
	CHECK(held == 0);
	return 0;
}
