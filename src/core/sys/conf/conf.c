/* conf.c */

#include <stddef.h>

#include "buf.h"
#include "conf.h"
#include "file.h"
#include "inode.h"
#include "ramdisk.h"

struct buf buf[NBUF];
struct file file[NFILE];
struct inode inode[NINODE];
struct buf bfreelist;

static struct buf pipe_tab;
static struct buf ram_tab;

struct bdevsw bdevsw[] = {
	{ NULL, &pipe_tab, NULL },
	{ ramstrategy, &ram_tab, &ramqueue }
};

int nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);

struct bdevsw *bdev_get(unsigned int dev)
{
	unsigned int maj;

	if (dev == NODEV || dev > 0xffffU || nblkdev <= 0)
		return NULL;
	maj = major(dev);
	if (maj >= (unsigned int)nblkdev || !bdevsw[maj].d_tab)
		return NULL;
	return &bdevsw[maj];
}
