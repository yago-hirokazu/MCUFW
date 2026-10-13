/* test_headers.c */

#if defined(TEST_REVERSE)
#include "sched_backend.h"
#include "pipe.h"
#include "inode.h"
#include "file.h"
#include "sys_error.h"
#include "param.h"
#else
#include "param.h"
#include "sys_error.h"
#include "file.h"
#include "inode.h"
#include "pipe.h"
#include "sched.h"
#include "sched_backend.h"
#endif

#if defined(__cplusplus)
#define STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

STATIC_ASSERT(NOFILE == 12, "NOFILE value");
STATIC_ASSERT(NFILE == 12, "NFILE value");
STATIC_ASSERT(NINODE == 3, "NINODE value");
STATIC_ASSERT(PIPELINE_MAX == 4, "PIPELINE_MAX value");
STATIC_ASSERT(PIPSIZ == BSIZE, "PIPSIZ value");
STATIC_ASSERT(NBUF >= NINODE, "Buffer count");
STATIC_ASSERT(FREAD == 0x01U, "FREAD value");
STATIC_ASSERT(FWRITE == 0x02U, "FWRITE value");
STATIC_ASSERT(FPIPE == 0x04U, "FPIPE value");
STATIC_ASSERT(SYS_EBADF > 0, "Error sign");
STATIC_ASSERT(PIPE_IO_PROGRESS == 0, "Pipe I/O result base");
STATIC_ASSERT(sizeof(((struct proc *)0)->p_ofile) /
	      sizeof(((struct proc *)0)->p_ofile[0]) == NOFILE,
	      "Descriptor count");
STATIC_ASSERT(sizeof(((struct file *)0)->f_offset) == sizeof(size_t),
	      "File offset type");
STATIC_ASSERT(sizeof(((struct inode *)0)->i_size) == sizeof(size_t),
	      "Inode size type");

int main(void)
{
	return 0;
}
