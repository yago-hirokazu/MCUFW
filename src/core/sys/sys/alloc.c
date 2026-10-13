/* alloc.c */

#include <string.h>

#include "buf.h"
#include "inode.h"
#include "param.h"

struct inode *ialloc(void)
{
	struct inode *ip;
	struct buf *bp;
	int i;

	for (i = 0; i < NINODE; i++) {
		ip = &inode[i];
		if (ip->i_count)
			continue;
		bp = getblk(PIPEDEV, (unsigned long)i);
		if (!bp)
			continue;
		clrbuf(bp);
		memset(ip, 0, sizeof(*ip));
		ip->i_count = 1;
		ip->i_buf = bp;
		return ip;
	}
	return NULL;
}

size_t inode_in_use(void)
{
	size_t count;
	int i;

	count = 0;
	for (i = 0; i < NINODE; i++) {
		if (inode[i].i_count)
			count++;
	}
	return count;
}
