/*
 * Native worker threads => shared logger => retained record validation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "log_adapt.h"
#include "log_error.h"

#define CHECK(c) do { if ((c) == 0) exit(__LINE__); } while (0)
#define NWORKERS 4
#define NWRITES 10000

/*
 * Interleave writes, reads and count queries across native threads
 */
static DWORD WINAPI worker(void *arg)
{
	uint16_t code = (uint16_t)(uintptr_t)arg;
	struct log_error log;
	unsigned int i;

	for (i = 0; i < NWRITES; i++) {
		error_record(code >> 8, code);
		CHECK(error_read(0, &log) == 0);
		CHECK(log.file == (log.line >> 8) && (log.line & 0xff) == 1);
		CHECK(error_count() <= NERRLOG);
	}
	return 0;
}

/*
 * Verify Windows clock and concurrent ring access
 */
int main(void)
{
	HANDLE threads[NWORKERS];
	struct log_error log;
	uint32_t t1, w1, t2, w2;
	unsigned int i;
	DWORD result;

	CHECK(init_error() == 0);
	CHECK(log_adapt_frequency() != 0);
	CHECK(log_adapt_time_bits() == 32);
	log_adapt_get_time(&t1, &w1);
	Sleep(20);
	log_adapt_get_time(&t2, &w2);
	CHECK((((uint64_t)w2 << 32) | t2) >
	      (((uint64_t)w1 << 32) | t1));
	for (i = 0; i < NWORKERS; i++) {
		void *arg = (void *)(uintptr_t)((i + 1) * 256 + 1);

		threads[i] = CreateThread(NULL, 0, worker,
					  arg, 0, NULL);
		CHECK(threads[i] != NULL);
	}
	CHECK(WaitForMultipleObjects(NWORKERS, threads, TRUE, INFINITE) ==
	      WAIT_OBJECT_0);
	for (i = 0; i < NWORKERS; i++) {
		CHECK(GetExitCodeThread(threads[i], &result) != 0);
		CHECK(result == 0);
		CloseHandle(threads[i]);
	}
	CHECK(error_count() == NERRLOG);
	for (i = 0; i < NERRLOG; i++) {
		CHECK(error_read(i, &log) == 0);
		CHECK(log.file == (log.line >> 8) && (log.line & 0xff) == 1);
	}
	puts("all logging tests passed");
	return 0;
}
