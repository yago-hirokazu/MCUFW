/*
 * closef() / allocation rollback => iput() => clrbuf() => brelse()
 * Final release has no device I/O and preserves caller's user state
 */

#include <string.h>

#include "buf.h"
#include "inode.h"

/* Drop inode reference and clear payload on final release */
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
