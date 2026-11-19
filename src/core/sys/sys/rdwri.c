/*
 * readp() => readi() => bread() / memcpy() / brelse()
 * writep() => writei() => bread() or getblk() / memcpy() / bdwrite()
 *   => bio.c cache hit or RAM disk I/O after cache eviction
 */

#include <string.h>

#include "buf.h"
#include "inode.h"
#include "pipe.h"
#include "sys_error.h"

/* Copy available inode data from cached block */
ptrdiff_t readi(struct inode *ip, size_t *offset, void *buf, size_t count)
{
	struct buf *bp = NULL;
	size_t available = 0;
	size_t transferred = 0;
	int error = 0;

	if (count == 0)
		return 0;
	available = ip->i_size - *offset;
	if (available == 0)
		return 0;
	transferred = count;
	if (transferred > available)
		transferred = available;
	bp = bread(ip->i_dev, ip->i_blkno);
	if (bp == NULL)
		return -SYS_EAGAIN;
	if (bp->b_error != 0) {
		error = bp->b_error;
		brelse(bp);
		return -error;
	}
	memcpy(buf, bp->b_addr + *offset, transferred);
	brelse(bp);
	*offset += transferred;
	return (ptrdiff_t)transferred;
}

/* Copy inode data into cached block and schedule delayed write */
ptrdiff_t writei(struct inode *ip, size_t *offset, const void *buf,
		 size_t count)
{
	struct buf *bp = NULL;
	size_t available = 0;
	size_t transferred = 0;
	int error = 0;

	if (count == 0)
		return 0;
	available = PIPSIZ - *offset;
	if (available == 0)
		return 0;
	transferred = count;
	if (transferred > available)
		transferred = available;
	/* Full block overwrite needs no previous data from device */
	if (transferred == BSIZE)
		bp = getblk(ip->i_dev, ip->i_blkno);
	else
		bp = bread(ip->i_dev, ip->i_blkno);
	if (bp == NULL)
		return -SYS_EAGAIN;
	if (bp->b_error != 0) {
		error = bp->b_error;
		brelse(bp);
		return -error;
	}
	memcpy(bp->b_addr + *offset, buf, transferred);
	bdwrite(bp);
	*offset += transferred;
	if (*offset > ip->i_size)
		ip->i_size = *offset;
	return (ptrdiff_t)transferred;
}
