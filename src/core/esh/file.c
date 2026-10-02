/* file.c */

#include <limits.h>
#include <string.h>

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"

static struct esh_file esh_file_table[ESH_FILE_MAX];

void esh_file_system_init(void)
{
	memset(esh_file_table, 0, sizeof(esh_file_table));
	esh_pipe_pool_init();
}

void esh_fd_table_init(struct esh_fd_table *table)
{
	if (table)
		memset(table, 0, sizeof(*table));
}

struct esh_file *esh_file_alloc(enum esh_file_direction direction,
				struct esh_pipe_inode *inode)
{
	struct esh_file *file;
	size_t i;

	if (!inode)
		return NULL;
	for (i = 0; i < ESH_FILE_MAX; i++) {
		file = &esh_file_table[i];
		if (file->f_refs)
			continue;
		file->f_refs = 1;
		file->f_direction = direction;
		file->f_inode = inode;
		esh_pipe_endpoint_open(inode, direction);
		return file;
	}
	return NULL;
}

void esh_file_put(struct esh_file *file)
{
	struct esh_pipe_inode *inode;
	enum esh_file_direction direction;

	if (!file || !file->f_refs)
		return;
	file->f_refs--;
	if (file->f_refs)
		return;

	inode = file->f_inode;
	direction = file->f_direction;
	file->f_inode = NULL;
	file->f_direction = ESH_FILE_READ;
	esh_pipe_endpoint_close(inode, direction);
}

int esh_fd_install(struct esh_fd_table *table, struct esh_file *file)
{
	int fd;

	if (!table || !file || !file->f_refs)
		return -ESH_EINVAL;
	if (file->f_refs == UCHAR_MAX)
		return -ESH_ENFILE;
	for (fd = 0; fd < ESH_OPEN_MAX; fd++) {
		if (table->fd_file[fd])
			continue;
		table->fd_file[fd] = file;
		file->f_refs++;
		return fd;
	}
	return -ESH_EMFILE;
}

int esh_fd_install_at(struct esh_fd_table *table, int fd,
		      struct esh_file *file)
{
	if (!table || fd < 0 || fd >= ESH_OPEN_MAX || !file ||
	    !file->f_refs)
		return -ESH_EINVAL;
	if (table->fd_file[fd])
		return -ESH_EMFILE;
	if (file->f_refs == UCHAR_MAX)
		return -ESH_ENFILE;
	table->fd_file[fd] = file;
	file->f_refs++;
	return fd;
}

struct esh_file *esh_fd_get(struct esh_fd_table *table, int fd)
{
	if (!table || fd < 0 || fd >= ESH_OPEN_MAX)
		return NULL;
	return table->fd_file[fd];
}

int esh_fd_close(struct esh_fd_table *table, int fd)
{
	struct esh_file *file;

	file = esh_fd_get(table, fd);
	if (!file)
		return -ESH_EBADF;
	table->fd_file[fd] = NULL;
	esh_file_put(file);
	return 0;
}

void esh_close_all(struct esh_fd_table *table)
{
	int fd;

	if (!table)
		return;
	for (fd = 0; fd < ESH_OPEN_MAX; fd++) {
		if (table->fd_file[fd])
			esh_fd_close(table, fd);
	}
}

size_t esh_file_in_use(void)
{
	size_t count;
	size_t i;

	count = 0;
	for (i = 0; i < ESH_FILE_MAX; i++) {
		if (esh_file_table[i].f_refs)
			count++;
	}
	return count;
}
