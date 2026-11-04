/*
 * log_error.c => QPC elapsed ticks => low 32 bits / high 32 bits
 * log_error.c => lock ID => dedicated Windows SRWLOCK
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <stdint.h>
#include <windows.h>

#include "log_adapt.h"

static SRWLOCK locks[LOG_LOCK_COUNT];
static LARGE_INTEGER origin;
static uint32_t frequency;

/*
 * Establish counter origin and initialize independent log locks
 */
int log_adapt_init(void)
{
	LARGE_INTEGER value;
	unsigned int i;

	if (QueryPerformanceFrequency(&value) == 0)
		return -1;
	/* Public frequency API represents rates up to UINT32_MAX */
	if (value.QuadPart <= 0 || value.QuadPart > UINT32_MAX)
		return -1;
	frequency = (uint32_t)value.QuadPart;
	if (QueryPerformanceCounter(&origin) == 0)
		return -1;
	for (i = 0; i < LOG_LOCK_COUNT; i++)
		InitializeSRWLock(&locks[i]);
	return 0;
}

/*
 * Split single counter sample, preserving wrap consistency
 */
void log_adapt_get_time(uint32_t *time, uint32_t *wrap)
{
	LARGE_INTEGER value;
	uint64_t elapsed;

	QueryPerformanceCounter(&value);
	elapsed = (uint64_t)value.QuadPart - (uint64_t)origin.QuadPart;
	*time = (uint32_t)elapsed;
	*wrap = (uint32_t)(elapsed >> 32);
}

/*
 * Return counter ticks per second
 */
uint32_t log_adapt_frequency(void)
{
	return frequency;
}

/*
 * Return timestamp low-part width
 */
unsigned int log_adapt_time_bits(void)
{
	return 32;
}

/*
 * Acquire lock belonging to selected log
 */
void log_adapt_lock(enum log_lock_id id)
{
	AcquireSRWLockExclusive(&locks[id]);
}

/*
 * Release lock belonging to selected log
 */
void log_adapt_unlock(enum log_lock_id id)
{
	ReleaseSRWLockExclusive(&locks[id]);
}
