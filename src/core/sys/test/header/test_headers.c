/* test_headers.c */

#if defined(TEST_REVERSE)
#include "ramdisk.h"
#include "conf.h"
#include "sched_backend.h"
#include "pipe.h"
#include "inode.h"
#include "file.h"
#include "sys_error.h"
#include "param.h"
#include "proc.h"
#else
#include "proc.h"
#include "param.h"
#include "sys_error.h"
#include "conf.h"
#include "file.h"
#include "inode.h"
#include "pipe.h"
#include "sched.h"
#include "sched_backend.h"
#endif

#include "ramdisk.h"
#include "user.h"

#if defined(__cplusplus)
#define STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

STATIC_ASSERT(NOFILE == 12, "NOFILE value");
STATIC_ASSERT(NFILE == 12, "NFILE value");
STATIC_ASSERT(NINODE == 3, "NINODE value");
STATIC_ASSERT(PIPELINE_MAX == 4, "PIPELINE_MAX value");
STATIC_ASSERT(SSLEEP == 1 && SRUN == 3 && SIDL == 4 && SZOMB == 5,
	      "V7 process states");
STATIC_ASSERT(PROC_ERROR == 7, "MCUFW process error state");
STATIC_ASSERT(PIPSIZ == BSIZE, "PIPSIZ value");
STATIC_ASSERT(NBUF >= NINODE, "Buffer count");
STATIC_ASSERT(FREAD == 0x01U, "FREAD value");
STATIC_ASSERT(FWRITE == 0x02U, "FWRITE value");
STATIC_ASSERT(FPIPE == 0x04U, "FPIPE value");
STATIC_ASSERT(SYS_EBADF > 0, "Error sign");
STATIC_ASSERT(SYS_ENOSYS == 8, "Existing error values");
STATIC_ASSERT(SYS_ENODEV == 9, "Device error value");
STATIC_ASSERT(SYS_EIO == 10, "I/O error value");
STATIC_ASSERT(RAM_MAJOR == 1U, "RAM major");
STATIC_ASSERT(RAM_NDEV == 2U, "RAM device count");
STATIC_ASSERT(RAM_NBLK == 16UL, "RAM block count");
STATIC_ASSERT(sizeof(((struct user *)0)->u_ofile) /
	      sizeof(((struct user *)0)->u_ofile[0]) == NOFILE,
	      "Descriptor count");
STATIC_ASSERT(sizeof(((struct file *)0)->f_offset) == sizeof(size_t),
	      "File offset type");
STATIC_ASSERT(sizeof(((struct inode *)0)->i_size) == sizeof(size_t),
	      "Inode size type");

int main(void)
{
	return 0;
}
