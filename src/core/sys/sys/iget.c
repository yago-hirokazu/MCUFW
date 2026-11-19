/*
 * closef() / allocation rollback => iput() => release inode reference
 *   => cached block remains available for delayed write or reuse
 */

#include <string.h>

#include "inode.h"

/* Release inode after final reference without retaining cache buffer */
void iput(struct inode *ip)
{
	ip->i_count--;
	if (ip->i_count != 0)
		return;
	/* Next ialloc clears same block through buffer cache */
	memset(ip, 0, sizeof(*ip));
}
