/* conf.c */

#include <stddef.h>

#include "buf.h"
#include "conf.h"

struct buf buf[NBUF];
struct buf bfreelist;

static struct buf pipe_tab;

struct bdevsw bdevsw[] = {
	{ NULL, &pipe_tab, NULL }
};

int nblkdev = sizeof(bdevsw) / sizeof(bdevsw[0]);
