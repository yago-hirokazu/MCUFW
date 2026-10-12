/* iget.c */

#include <string.h>

#include "buf.h"
#include "inode.h"

void iput(struct inode *ip)
{
	struct buf *bp;

	if (!ip || !ip->i_count)
		return;
	ip->i_count--;
	if (ip->i_count)
		return;
	bp = ip->i_buf;
	memset(ip, 0, sizeof(*ip));
	if (bp) {
		clrbuf(bp);
		brelse(bp);
	}
}
