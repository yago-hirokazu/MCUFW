/* rdwri.c */

#include <string.h>

#include "buf.h"
#include "inode.h"
#include "pipe.h"
#include "sys_error.h"

ptrdiff_t readi(struct inode *ip, size_t *offset, void *buf, size_t count)
{
	size_t available;
	size_t transferred;

	if (!count)
		return 0;
	if (!ip || !offset || !buf || !ip->i_buf)
		return -SYS_EINVAL;
	if (ip->i_size > PIPSIZ || *offset > ip->i_size)
		return -SYS_EINVAL;
	available = ip->i_size - *offset;
	if (!available)
		return 0;
	transferred = count;
	if (transferred > available)
		transferred = available;
	memcpy(buf, ip->i_buf->b_addr + *offset, transferred);
	*offset += transferred;
	return (ptrdiff_t)transferred;
}

ptrdiff_t writei(struct inode *ip, size_t *offset, const void *buf,
		 size_t count)
{
	size_t available;
	size_t transferred;

	if (!count)
		return 0;
	if (!ip || !offset || !buf || !ip->i_buf)
		return -SYS_EINVAL;
	if (ip->i_size > PIPSIZ || *offset > PIPSIZ)
		return -SYS_EINVAL;
	available = PIPSIZ - *offset;
	if (!available)
		return 0;
	transferred = count;
	if (transferred > available)
		transferred = available;
	memcpy(ip->i_buf->b_addr + *offset, buf, transferred);
	*offset += transferred;
	if (*offset > ip->i_size)
		ip->i_size = *offset;
	return (ptrdiff_t)transferred;
}
