/*
 * ialloc() => getblk() / clrbuf() / bdwrite() => buffer cache
 *   => inode retains device and block number across cache eviction
 */

#include <string.h>

#include "buf.h"
#include "inode.h"
#include "param.h"

/* Allocate inode and initialize its fixed RAM disk block */
struct inode *ialloc(void)
{
	struct inode *ip = NULL;
	struct buf *bp = NULL;
	int i = 0;

	for (i = 0; i < NINODE; i++) {
		ip = &inode[i];
		if (ip->i_count != 0)
			continue;
		bp = getblk(PIPEDEV, (unsigned long)i);
		if (bp == NULL)
			continue;
		clrbuf(bp);
		memset(ip, 0, sizeof(*ip));
		ip->i_count = 1;
		ip->i_dev = PIPEDEV;
		ip->i_blkno = (unsigned long)i;
		/* Preserve initialized block if cache reuses its buffer */
		bdwrite(bp);
		return ip;
	}
	return NULL;
}

/* Count referenced inodes */
size_t inode_in_use(void)
{
	size_t count = 0;
	int i = 0;

	for (i = 0; i < NINODE; i++) {
		if (inode[i].i_count != 0)
			count++;
	}
	return count;
}
