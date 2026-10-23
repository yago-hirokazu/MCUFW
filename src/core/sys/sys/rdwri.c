/*
 * readi() / writei() => iomove() => buffer <=> user.u_base
 * Successful copy advances u_base/u_offset and reduces u_count
 * Pipe inode keeps one buffer; no device or disk block mapping
 */

#include <stdint.h>
#include <string.h>

#include "buf.h"
#include "inode.h"
#include "pipe.h"
#include "sys_error.h"
#include "user.h"

#if PIPSIZ > PTRDIFF_MAX
#error "PIPSIZ must fit in ptrdiff_t"
#endif

/* Read inode data using current user transfer state */
void readi(struct inode *ip)
{
	size_t available;
	size_t count;

	if (!u.u_count)
		return;
	if (!ip || !ip->i_buf || !ip->i_buf->b_addr || !u.u_base ||
	    u.u_count > PTRDIFF_MAX || u.u_offset < 0 ||
	    u.u_offset > PIPSIZ || ip->i_size > PIPSIZ ||
	    (size_t)u.u_offset > ip->i_size) {
		u.u_error = SYS_EINVAL;
		return;
	}
	available = ip->i_size - (size_t)u.u_offset;
	count = u.u_count;
	if (count > available)
		count = available;
	if (count)
		iomove(ip->i_buf->b_addr + (size_t)u.u_offset, count, B_READ);
}

/* Write within fixed capacity using current user transfer state */
void writei(struct inode *ip)
{
	size_t available;
	size_t count;

	if (!u.u_count)
		return;
	if (!ip || !ip->i_buf || !ip->i_buf->b_addr || !u.u_base ||
	    u.u_count > PTRDIFF_MAX || u.u_offset < 0 ||
	    u.u_offset > PIPSIZ || ip->i_size > PIPSIZ) {
		u.u_error = SYS_EINVAL;
		return;
	}
	available = PIPSIZ - (size_t)u.u_offset;
	count = u.u_count;
	if (count > available)
		count = available;
	if (!count)
		return;
	iomove(ip->i_buf->b_addr + (size_t)u.u_offset, count, B_WRITE);
	if ((size_t)u.u_offset > ip->i_size)
		ip->i_size = (size_t)u.u_offset;
}

/* Copy bytes in specified direction and advance transfer state */
void iomove(void *cp, size_t count, int flag)
{
	if (!count)
		return;
	if ((flag != B_READ && flag != B_WRITE) || !cp || !u.u_base ||
	    count > u.u_count || u.u_count > PTRDIFF_MAX ||
	    u.u_offset < 0 || count > (size_t)(PTRDIFF_MAX - u.u_offset)) {
		u.u_error = SYS_EINVAL;
		return;
	}
	if (flag == B_WRITE)
		memcpy(cp, u.u_base, count);
	else
		memcpy(u.u_base, cp, count);
	u.u_base += count;
	u.u_count -= count;
	u.u_offset += (ptrdiff_t)count;
}
