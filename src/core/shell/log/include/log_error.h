#ifndef __LOG_ERROR_H__
#define __LOG_ERROR_H__

#include <stdint.h>

#define NERRLOG 64

struct log_error {
	uint32_t time;		/* Timestamp counter */
	uint32_t wrap;		/* Counter wrap count */
	uint16_t line;		/* Source line number */
	uint8_t file;		/* File code */
};

int error_init(void);
void error_write(const struct log_error *log);
void error_record(uint8_t file, uint16_t line);
int error_read(uint32_t index, struct log_error *log);
uint32_t error_count(void);
void error_dump(void);

#endif
