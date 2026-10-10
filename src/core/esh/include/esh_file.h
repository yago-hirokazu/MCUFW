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

/* Standard fds plus descriptors needed while building pipeline */
#define ESH_OPEN_MAX		12
#define ESH_FILE_MAX		12

struct esh_pipe_inode;

enum esh_file_direction {
	ESH_FILE_READ,
	ESH_FILE_WRITE
};

/* Entry represents open pipe endpoint */
struct esh_file {
	unsigned char f_refs;		/* Number of fd table references */
	enum esh_file_direction f_direction;
	struct esh_pipe_inode *f_inode;
};

/* NULL file pointer marks unused descriptor */
struct esh_fd_table {
	struct esh_file *fd_file[ESH_OPEN_MAX];
};

int esh_pipe(int fd[2]);
ptrdiff_t esh_read(int fd, void *buf, size_t count);
ptrdiff_t esh_write(int fd, const void *buf, size_t count);
int esh_close(int fd);

/* Call only when no process owns file references */
void esh_file_system_init(void);
/* Internal helpers need kernel lock once processes can access pools */
/* Alloc returns owned ref; install adds ref; get borrows; close drops ref */
void esh_fd_table_init(struct esh_fd_table *table);
struct esh_file *esh_file_alloc(enum esh_file_direction direction,
				struct esh_pipe_inode *inode);
void esh_file_put(struct esh_file *file);
int esh_fd_install(struct esh_fd_table *table, struct esh_file *file);
int esh_fd_install_at(struct esh_fd_table *table, int fd,
		      struct esh_file *file);
struct esh_file *esh_fd_get(struct esh_fd_table *table, int fd);
int esh_fd_close(struct esh_fd_table *table, int fd);

/* Process exit closes each non-NULL entry exactly once */
void esh_close_all(struct esh_fd_table *table);
size_t esh_file_in_use(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
