/*
 * conf.c => shared file, inode and buffer tables
 * bdevsw[RAM_MAJOR] => ramstrategy() => RAM disk units
 */

#include <stddef.h>

#include "buf.h"
#include "conf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "ramdisk.h"

#if (PIPEDEV >> 8) != RAM_MAJOR || (PIPEDEV & 0xffU) >= RAM_NDEV
#error "PIPEDEV must select available RAM disk"
#endif

#if NINODE > RAM_NBLK
#error "RAM disk must provide one block per pipe inode"
#endif

struct buf buf[NBUF];
struct file file[NFILE];
struct inode inode[NINODE];
struct buf bfreelist;

static struct buf cache_tab;
static struct buf ram_tab;

struct bdevsw bdevsw[] = {
	{ NULL, &cache_tab, NULL, 0 },
	{ ramstrategy, &ram_tab, &ramqueue, BDEV_SYNC }
};

int nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

struct bdevsw *bdevsw_lookup(unsigned int dev)
{
	unsigned int maj;

	if (dev == NODEV || dev > 0xffffU || nblkdev <= 0)
		return NULL;
	maj = major(dev);
	if (maj >= (unsigned int)nblkdev || !bdevsw[maj].d_tab)
		return NULL;
	return &bdevsw[maj];
}
