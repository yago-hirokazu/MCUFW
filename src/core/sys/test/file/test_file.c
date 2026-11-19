/*
 * File tests => falloc() / fdfree() / fdclean() => ialloc() / iput()
 *   => device and block ownership across cache reuse
 */

/*
 * test_getf() => getf() => descriptor lookup / process error state
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "sched.h"
#include "sys_error.h"

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

static int failures;

static void init_test_proc(struct proc *proc);
static void test_file_allocation(void);
static void test_getf(void);
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
	test_getf();
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

static void init_test_proc(struct proc *proc)
{
	memset(proc, 0, sizeof(*proc));
}

static void test_file_allocation(void)
{
	struct proc proc;
	struct file *fp;
	int fd;

	init_bufc();
	init_fsys();
	init_test_proc(&proc);
	CHECK(file_in_use() == 0);
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
	CHECK(fdfree(&proc, fd) == 0);
	CHECK(fdfree(&proc, fd) == -SYS_EBADF);
	CHECK(file_in_use() == 0);
}

/* Check descriptor bounds, empty slots and process error preservation */
static void test_getf(void)
{
	struct proc proc = {0};
	struct file *fp = NULL;
	const int invalid[] = {INT_MIN, -1, NOFILE, INT_MAX};
	size_t i = 0;
	int fd = 0;

	init_bufc();
	init_fsys();
	CHECK(getf(&proc, 0) == NULL);
	CHECK(proc.p_error == -SYS_EBADF);
	proc.p_error = 0;
	CHECK(getf(&proc, NOFILE - 1) == NULL);
	CHECK(proc.p_error == -SYS_EBADF);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		proc.p_error = 0;
		CHECK(getf(&proc, invalid[i]) == NULL);
		CHECK(proc.p_error == -SYS_EBADF);
	}

	/* Valid nonzero descriptor must select requested slot */
	fp = falloc(&proc, &fd);
	CHECK(fp != NULL);
	CHECK(fdassign(&proc, NOFILE - 1, fp) == NOFILE - 1);
	CHECK(fdfree(&proc, fd) == 0);
	proc.p_error = 0;
	CHECK(getf(&proc, NOFILE - 1) == fp);
	CHECK(proc.p_error == 0);
	proc.p_error = -SYS_EBADF;
	CHECK(getf(&proc, NOFILE - 1) == fp);
	CHECK(proc.p_error == -SYS_EBADF);
	CHECK(fdfree(&proc, NOFILE - 1) == 0);
	CHECK(file_in_use() == 0);
}

static void test_descriptor_helpers(void)
{
	struct proc proc;
	struct file *fp;
	int fd;
	int copy;

	init_bufc();
	init_fsys();
	init_test_proc(&proc);
	fp = falloc(&proc, &fd);
	CHECK(fp != NULL);
	copy = fdalloc(&proc, fp);
	CHECK(copy == 1);
	CHECK(fp->f_count == 2);
	CHECK(fdassign(&proc, 4, fp) == 4);
	CHECK(fp->f_count == 3);
	CHECK(fdassign(&proc, 4, fp) == -SYS_EMFILE);
	CHECK(fdfree(&proc, fd) == 0);
	CHECK(fp->f_count == 2);
	CHECK(fdfree(&proc, copy) == 0);
	CHECK(fp->f_count == 1);
	CHECK(fdfree(&proc, 4) == 0);
	CHECK(file_in_use() == 0);

	fp = &file[0];
	fp->f_count = UCHAR_MAX;
	CHECK(fdalloc(&proc, fp) == -SYS_ENFILE);
	CHECK(fdassign(&proc, 3, fp) == -SYS_ENFILE);
	memset(fp, 0, sizeof(*fp));
}

static void test_fd_exhaustion(void)
{
	struct proc proc;
	struct file *fp;
	size_t used;
	int fd;
	int i;

	init_bufc();
	init_fsys();
	init_test_proc(&proc);
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
	fdclean(&proc);
}

static void test_file_exhaustion(void)
{
	struct proc owner;
	struct proc waiter;
	int fd;
	int i;

	init_bufc();
	init_fsys();
	init_test_proc(&owner);
	init_test_proc(&waiter);
	for (i = 0; i < NFILE; i++)
		CHECK(falloc(&owner, &fd) != NULL);
	CHECK(ufalloc(&waiter) == 0);
	CHECK(falloc(&waiter, &fd) == NULL);
	CHECK(fd == -SYS_ENFILE);
	CHECK(waiter.p_ofile[0] == NULL);
	CHECK(file_in_use() == NFILE);
	fdclean(&owner);
	CHECK(file_in_use() == 0);
}

/* Inode references retain block identity without occupying cache buffer */
static void test_inode_lifecycle(void)
{
	struct proc proc = {0};
	struct inode *ip = NULL;
	struct file *fp = NULL;
	struct buf *bp = NULL;
	int fd = 0;
	int copy = 0;

	init_bufc();
	init_fsys();
	init_test_proc(&proc);
	ip = ialloc();
	CHECK(ip == &inode[0]);
	CHECK(ip->i_count == 1);
	CHECK(ip->i_dev == PIPEDEV);
	CHECK(ip->i_blkno == 0);
	bp = bread(ip->i_dev, ip->i_blkno);
	CHECK(bp != NULL && bp->b_error == 0);
	brelse(bp);
	CHECK((bp->b_flags & B_BUSY) == 0);
	fp = falloc(&proc, &fd);
	CHECK(fp != NULL);
	fp->f_inode = ip;
	fp->f_flag = FREAD | FPIPE;
	copy = fdalloc(&proc, fp);
	CHECK(copy >= 0);
	CHECK(fdfree(&proc, fd) == 0);
	CHECK(ip->i_count == 1);
	CHECK((bp->b_flags & B_BUSY) == 0);
	CHECK(fdfree(&proc, copy) == 0);
	CHECK(inode_in_use() == 0);
	CHECK((bp->b_flags & B_BUSY) == 0);
	CHECK(ip->i_dev == 0);
	CHECK(ip->i_blkno == 0);
}

static void test_inode_exhaustion(void)
{
	struct inode *ips[NINODE];
	int i;

	init_bufc();
	init_fsys();
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

	init_bufc();
	init_fsys();
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

/* Reused inode block starts empty even when previous data remains cached */
static void test_reuse_after_close_all(void)
{
	struct proc proc = {0};
	struct inode *ip = NULL;
	struct file *fp = NULL;
	struct buf *bp = NULL;
	unsigned int dev = 0;
	unsigned long blkno = 0;
	size_t i = 0;
	int fd = 0;

	init_bufc();
	init_fsys();
	init_test_proc(&proc);
	ip = ialloc();
	fp = falloc(&proc, &fd);
	CHECK(ip != NULL);
	CHECK(fp != NULL);
	dev = ip->i_dev;
	blkno = ip->i_blkno;
	bp = bread(dev, blkno);
	CHECK(bp != NULL && bp->b_error == 0);
	memset(bp->b_addr, 0x5a, BSIZE);
	bdwrite(bp);
	fp->f_inode = ip;
	fdclean(&proc);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == 0);
	CHECK(falloc(&proc, &fd) == &file[0]);
	CHECK(fd == 0);
	ip = ialloc();
	CHECK(ip == &inode[0]);
	CHECK(ip->i_dev == dev);
	CHECK(ip->i_blkno == blkno);
	CHECK(ip->i_size == 0);
	bp = bread(dev, blkno);
	CHECK(bp != NULL && bp->b_error == 0);
	for (i = 0; i < BSIZE; i++)
		CHECK(bp->b_addr[i] == 0);
	brelse(bp);
	fdclean(&proc);
	iput(ip);
}
