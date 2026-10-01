/* esh_file.h */

#ifndef __ESH_FILE_H__
#define __ESH_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#define ESH_FD_STDIN		0
#define ESH_FD_STDOUT		1
#define ESH_FD_STDERR		2

/* Three standard fds plus descriptors needed while building pipeline */
#define ESH_OPEN_MAX		12
#define ESH_FILE_MAX		12

struct esh_pipe_inode;

enum esh_file_direction {
	ESH_FILE_READ,
	ESH_FILE_WRITE
};

/* One entry represents one open pipe endpoint */
struct esh_file {
	unsigned char f_refs;		/* Number of fd table references */
	enum esh_file_direction f_direction;
	struct esh_pipe_inode *f_inode;
};

/* NULL file pointer marks unused descriptor */
struct esh_fd_table {
	struct esh_file *fd_file[ESH_OPEN_MAX];
};

typedef ptrdiff_t esh_ssize_t;

int esh_pipe(int fd[2]);
esh_ssize_t esh_read(int fd, void *buf, size_t count);
esh_ssize_t esh_write(int fd, const void *buf, size_t count);
int esh_close(int fd);

/* Process exit closes each non-NULL entry exactly once */
void esh_close_all(struct esh_fd_table *table);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
