/* test_file.c */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "sched.h"
#include "sys_error.h"
#include "user.h"

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

static int failures;

static void init_proc(struct proc *proc, struct user *user);
static void test_file_allocation(void);
static void test_descriptor_helpers(void);
static void test_fd_exhaustion(void);
static void test_file_exhaustion(void);
static void test_inode_lifecycle(void);
static void test_inode_exhaustion(void);
static void test_buffer_failure(void);
static void test_reuse_after_close_all(void);

int main(void)
{
	test_file_allocation();
	test_descriptor_helpers();
	test_fd_exhaustion();
	test_file_exhaustion();
	test_inode_lifecycle();
	test_inode_exhaustion();
	test_buffer_failure();
	test_reuse_after_close_all();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static void init_proc(struct proc *proc, struct user *user)
{
	memset(proc, 0, sizeof(*proc));
	memset(user, 0, sizeof(*user));
	proc->p_addr = user;
}

static void test_file_allocation(void)
{
	struct proc proc;
	struct user proc_user;
	struct file *fp;
	int fd;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	CHECK(file_in_use() == 0);
	CHECK(getf(&proc, -1) == NULL);
	CHECK(getf(&proc, NOFILE) == NULL);
	CHECK(getf(NULL, 0) == NULL);
	CHECK(ufalloc(NULL) == -SYS_EINVAL);
	fp = falloc(&proc, &fd);
	CHECK(fp == &file[0]);
	CHECK(fd == 0);
	CHECK(proc.p_ofile[0] == fp);
	CHECK(fp->f_count == 1);
	CHECK(fp->f_flag == 0);
	CHECK(fp->f_inode == NULL);
	CHECK(fp->f_offset == 0);
	CHECK(getf(&proc, fd) == fp);
	CHECK(file_in_use() == 1);
	CHECK(fd_close(&proc, fd) == 0);
	CHECK(fd_close(&proc, fd) == -SYS_EBADF);
	CHECK(file_in_use() == 0);
	CHECK(falloc(NULL, &fd) == NULL);
	CHECK(fd == -SYS_EINVAL);
	CHECK(falloc(&proc, NULL) == NULL);
}

static void test_descriptor_helpers(void)
{
	struct proc proc;
	struct user proc_user;
	struct file *fp;
	int fd;
	int copy;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	fp = falloc(&proc, &fd);
	CHECK(fp != NULL);
	copy = fd_install(&proc, fp);
	CHECK(copy == 1);
	CHECK(fp->f_count == 2);
	CHECK(fd_install_at(&proc, 4, fp) == 4);
	CHECK(fp->f_count == 3);
	CHECK(fd_install_at(&proc, 4, fp) == -SYS_EMFILE);
	CHECK(fd_install_at(&proc, NOFILE, fp) == -SYS_EINVAL);
	CHECK(fd_close(&proc, fd) == 0);
	CHECK(fp->f_count == 2);
	CHECK(fd_close(&proc, copy) == 0);
	CHECK(fp->f_count == 1);
	CHECK(fd_close(&proc, 4) == 0);
	CHECK(file_in_use() == 0);

	fp = &file[0];
	fp->f_count = UCHAR_MAX;
	CHECK(fd_install(&proc, fp) == -SYS_ENFILE);
	CHECK(fd_install_at(&proc, 3, fp) == -SYS_ENFILE);
	memset(fp, 0, sizeof(*fp));
}

static void test_fd_exhaustion(void)
{
	struct proc proc;
	struct user proc_user;
	struct file *fp;
	size_t used;
	int fd;
	int i;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	for (i = 0; i < NOFILE; i++) {
		fp = falloc(&proc, &fd);
		CHECK(fp != NULL);
		CHECK(fd == i);
	}
	used = file_in_use();
	CHECK(ufalloc(&proc) == -SYS_EMFILE);
	CHECK(falloc(&proc, &fd) == NULL);
	CHECK(fd == -SYS_EMFILE);
	CHECK(file_in_use() == used);
	close_all(&proc);
}

static void test_file_exhaustion(void)
{
	struct proc owner;
	struct user owner_user;
	struct proc waiter;
	struct user waiter_user;
	int fd;
	int i;

	binit();
	finit();
	init_proc(&owner, &owner_user);
	init_proc(&waiter, &waiter_user);
	for (i = 0; i < NFILE; i++)
		CHECK(falloc(&owner, &fd) != NULL);
	CHECK(ufalloc(&waiter) == 0);
	CHECK(falloc(&waiter, &fd) == NULL);
	CHECK(fd == -SYS_ENFILE);
	CHECK(waiter.p_ofile[0] == NULL);
	CHECK(file_in_use() == NFILE);
	close_all(&owner);
	CHECK(file_in_use() == 0);
}

static void test_inode_lifecycle(void)
{
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	struct file *fp;
	struct buf *bp;
	int fd;
	int copy;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	ip = ialloc();
	CHECK(ip == &inode[0]);
	CHECK(ip->i_count == 1);
	CHECK(ip->i_buf != NULL);
	CHECK(ip->i_buf->b_flags & B_BUSY);
	bp = ip->i_buf;
	fp = falloc(&proc, &fd);
	CHECK(fp != NULL);
	fp->f_inode = ip;
	fp->f_flag = FREAD | FPIPE;
	copy = fd_install(&proc, fp);
	CHECK(copy >= 0);
	CHECK(fd_close(&proc, fd) == 0);
	CHECK(ip->i_count == 1);
	CHECK(bp->b_flags & B_BUSY);
	CHECK(fd_close(&proc, copy) == 0);
	CHECK(inode_in_use() == 0);
	CHECK(!(bp->b_flags & B_BUSY));
}

static void test_inode_exhaustion(void)
{
	struct inode *ips[NINODE];
	int i;

	binit();
	finit();
	for (i = 0; i < NINODE; i++) {
		ips[i] = ialloc();
		CHECK(ips[i] != NULL);
	}
	CHECK(inode_in_use() == NINODE);
	CHECK(ialloc() == NULL);
	CHECK(inode_in_use() == NINODE);
	for (i = 0; i < NINODE; i++)
		iput(ips[i]);
	CHECK(inode_in_use() == 0);
	CHECK(ialloc() == &inode[0]);
	iput(&inode[0]);
}

static void test_buffer_failure(void)
{
	struct buf *held[NBUF];
	int i;

	binit();
	finit();
	for (i = 0; i < NBUF; i++) {
		held[i] = getblk(PIPEDEV, (unsigned long)i);
		CHECK(held[i] != NULL);
	}
	CHECK(ialloc() == NULL);
	CHECK(inode_in_use() == 0);
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	CHECK(ialloc() == &inode[0]);
	iput(&inode[0]);
}

static void test_reuse_after_close_all(void)
{
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	struct file *fp;
	struct buf *bp;
	int fd;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	ip = ialloc();
	fp = falloc(&proc, &fd);
	CHECK(ip != NULL);
	CHECK(fp != NULL);
	bp = ip->i_buf;
	fp->f_inode = ip;
	close_all(&proc);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == 0);
	CHECK(falloc(&proc, &fd) == &file[0]);
	CHECK(fd == 0);
	ip = ialloc();
	CHECK(ip == &inode[0]);
	CHECK(ip->i_buf == bp);
	close_all(&proc);
	iput(ip);
}
