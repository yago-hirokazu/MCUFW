/*
 * custom_assert.h => error_record() => timestamp => error_write()
 * error_write()/read()/count()/dump() => log_adapt_lock()/unlock()
 * error_dump() => snapshot => unlock => standard output
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "log_adapt.h"
#include "log_error.h"

static struct log_error_t errlog[NERRLOG];
static uint32_t next;
static uint32_t count;

/*
 * Initialize logger before starting concurrent callers
 */
int error_init(void)
{
	if (log_adapt_init() != 0)
		return -1;
	memset(errlog, 0, sizeof(errlog));
	next = 0;
	count = 0;
	return 0;
}

/*
 * Copy record into ring, replacing oldest record when full
 */
void error_write(const struct log_error_t *log)
{
	log_adapt_lock(LOG_ERROR_LOCK);
	errlog[next] = *log;
	next = (next + 1) % NERRLOG;
	if (count < NERRLOG)
		count++;
	log_adapt_unlock(LOG_ERROR_LOCK);
}

/*
 * Capture timestamp and derive file code from error code
 */
void error_record(uint16_t err, uint16_t line)
{
	struct log_error_t log = {0};

	log_adapt_get_time(&log.time, &log.wrap);
	log.err = err;
	log.line = line;
	log.file = err >> 8;
	error_write(&log);
}

/*
 * Read record by age, leaving destination unchanged when absent
 */
int error_read(uint32_t index, struct log_error_t *log)
{
	log_adapt_lock(LOG_ERROR_LOCK);
	if (index >= count) {
		log_adapt_unlock(LOG_ERROR_LOCK);
		return -1;
	}
	*log = errlog[(next + NERRLOG - count + index) % NERRLOG];
	log_adapt_unlock(LOG_ERROR_LOCK);
	return 0;
}

/*
 * Return number of retained records
 */
uint32_t error_count(void)
{
	uint32_t n;

	log_adapt_lock(LOG_ERROR_LOCK);
	n = count;
	log_adapt_unlock(LOG_ERROR_LOCK);
	return n;
}

/*
 * Print consistent snapshot without holding lock during output
 */
void error_dump(void)
{
	struct log_error_t snapshot[NERRLOG];
	uint32_t i, n;

	log_adapt_lock(LOG_ERROR_LOCK);
	n = count;
	for (i = 0; i < n; i++)
		snapshot[i] = errlog[(next + NERRLOG - n + i) % NERRLOG];
	log_adapt_unlock(LOG_ERROR_LOCK);

	printf("index time wrap err file line\n");
	for (i = 0; i < n; i++) {
		const struct log_error_t *log = &snapshot[i];

		printf("%" PRIu32 " %" PRIu32 " %" PRIu32
		       " %04x %02x %u\n", i, log->time, log->wrap,
		       (unsigned int)log->err, (unsigned int)log->file,
		       (unsigned int)log->line);
	}
}
