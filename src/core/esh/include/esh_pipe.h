/* esh_pipe.h */

#ifndef __ESH_PIPE_H__
#define __ESH_PIPE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#include "esh_sched.h"

#define ESH_PIPE_MAX		3
#define ESH_PIPE_BUFFER_SIZE	256

struct esh_pipe_buffer {
	unsigned char pb_data[ESH_PIPE_BUFFER_SIZE];
	size_t pb_read;
	size_t pb_write;
	size_t pb_count;
};

/* Inode owns buffer and remains live while either endpoint is referenced */
struct esh_pipe_inode {
	unsigned char pi_refs;		/* Number of live endpoint files */
	unsigned char pi_readers;	/* Number of read endpoint files */
	unsigned char pi_writers;	/* Number of write endpoint files */
	struct esh_pipe_buffer pi_buffer;
	struct esh_wait_chan pi_read_chan;
	struct esh_wait_chan pi_write_chan;
};

enum esh_io_result {
	ESH_IO_PROGRESS,
	ESH_IO_WAIT_READ,
	ESH_IO_WAIT_WRITE,
	ESH_IO_EOF,
	ESH_IO_BROKEN_PIPE,
	ESH_IO_ERROR
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
