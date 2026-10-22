/*
 * ialloc() => ialloc_error() => getblk()
 * Pipe preparation uses explicit error output for target user
 */

#include <string.h>

#include "buf.h"
#include "inode.h"
#include "param.h"
#include "sys_error.h"
#include "user.h"

/* Allocate inode and report failure in current user */
struct inode *ialloc(void)
{
	struct inode *ip;
	int error = 0;

	ip = ialloc_error(&error);
	if (!ip)
		u.u_error = error;
	return ip;
}

/* Allocate in-core inode and buffer without changing current user */
struct inode *ialloc_error(int *error)
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
	*error = SYS_ENFILE;
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
