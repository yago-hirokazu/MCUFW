#ifndef __LOG_ADAPT_H__
#define __LOG_ADAPT_H__

#include <stdint.h>

enum log_lock_id {
	LOG_ERROR_LOCK,
	LOG_LOCK_COUNT
};

int log_adapt_init(void);
void log_adapt_get_time(uint32_t *time, uint32_t *wrap);
uint32_t log_adapt_frequency(void);
unsigned int log_adapt_time_bits(void);
void log_adapt_lock(enum log_lock_id id);
void log_adapt_unlock(enum log_lock_id id);

#endif
