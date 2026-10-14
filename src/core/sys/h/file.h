/* file.h */

#ifndef __FILE_H__
#define __FILE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

#define FD_STDIN	0
#define FD_STDOUT	1
#define FD_STDERR	2

#define FREAD		0x01U
#define FWRITE		0x02U
#define FPIPE		0x04U

struct inode;
struct proc;

/* Entry represents open pipe endpoint */
struct file {
	unsigned char f_flag;
	unsigned char f_count;		/* Number of fd references */
	struct inode *f_inode;
	size_t f_offset;
};

extern struct file file[];

int sys_pipe(int fd[2]);
ptrdiff_t sys_read(int fd, void *buf, size_t count);
ptrdiff_t sys_write(int fd, const void *buf, size_t count);
int sys_close(int fd);

void finit(void);
struct file *getf(struct proc *proc, int fd);
int ufalloc(struct proc *proc);
/* Success installs fd-owned reference; failure stores negative error in fd */
struct file *falloc(struct proc *proc, int *fd);
/* closef consumes one file reference */
void closef(struct file *fp);
int fd_install(struct proc *proc, struct file *fp);
int fd_install_at(struct proc *proc, int fd, struct file *fp);
int fd_close(struct proc *proc, int fd);
void close_all(struct proc *proc);
size_t file_in_use(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
