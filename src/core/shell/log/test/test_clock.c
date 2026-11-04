/*
 * Mock Windows counter API => production adapter => wrap boundary tests
 */

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#define CHECK(c) do { if ((c) == 0) exit(__LINE__); } while (0)

static LONGLONG sample, rate = 10000000;
static int available = 1;

/*
 * Supply controlled counter samples
 */
static BOOL fake_counter(LARGE_INTEGER *value)
{
	value->QuadPart = sample;
	return available;
}

/*
 * Supply controlled frequency
 */
static BOOL fake_frequency(LARGE_INTEGER *value)
{
	value->QuadPart = rate;
	return available;
}

#define QueryPerformanceCounter fake_counter
#define QueryPerformanceFrequency fake_frequency
#include "../dev/windows/log_adapt.c"

/*
 * Verify origin subtraction and low/high split across multiple wraps
 */
int main(void)
{
	uint32_t time, wrap;

	available = 0;
	CHECK(log_adapt_init() == -1);
	available = 1;
	rate = 0;
	CHECK(log_adapt_init() == -1);
	rate = (LONGLONG)UINT32_MAX + 1;
	CHECK(log_adapt_init() == -1);
	rate = 10000000;
	sample = 123;
	CHECK(log_adapt_init() == 0);
	CHECK(log_adapt_frequency() == 10000000);
	log_adapt_get_time(&time, &wrap);
	CHECK(time == 0 && wrap == 0);
	sample += UINT32_MAX;
	log_adapt_get_time(&time, &wrap);
	CHECK(time == UINT32_MAX && wrap == 0);
	sample++;
	log_adapt_get_time(&time, &wrap);
	CHECK(time == 0 && wrap == 1);
	sample += ((LONGLONG)3 << 32) + 17;
	log_adapt_get_time(&time, &wrap);
	CHECK(time == 17 && wrap == 4);
	return 0;
}
