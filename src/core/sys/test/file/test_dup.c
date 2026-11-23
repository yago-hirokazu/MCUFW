/*
 * test_dup() => fddup() / fddup2() => shared file and descriptor lifetime
 * sys_dup() / sys_dup2() => scheduler stub => current process and sys lock
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
#include "sched.h"
#include "sched_stub.h"
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

static void test_duplicate(void);
static void test_replace(void);
static void test_dup_errors(void);
static void test_sys_dup(void);

int test_dup(void);

/* Run duplication checks alongside file lifecycle tests */
int test_dup(void)
{
	failures = 0;
	test_duplicate();
	test_replace();
	test_dup_errors();
	test_sys_dup();
	sched_stub_reset(NULL);
	return failures;
}

/* Share open file without allocating file entries, even when table is full */
static void test_duplicate(void)
{
	struct proc src = {0};
	struct proc dst = {0};
	struct file *fp = NULL;
	int fd = 0;
	int i = 0;

	init_bufc();
	init_fsys();
	sched_stub_reset(NULL);
	sys_lock();
	for (i = 0; i < NFILE; i++)
		CHECK(falloc(&src, &fd) != NULL);
	fp = src.p_ofile[0];
	fp->f_flag = FREAD;
	fp->f_offset = 7;
	CHECK(file_in_use() == NFILE);
	for (i = 0; i < NOFILE; i++) {
		CHECK(fddup(&src, 0, &dst) == i);
		CHECK(dst.p_ofile[i] == fp);
	}
	CHECK(file_in_use() == NFILE);
	CHECK(fp->f_count == NOFILE + 1);
	CHECK(dst.p_ofile[0]->f_flag == FREAD);
	CHECK(dst.p_ofile[0]->f_offset == 7);
	dst.p_ofile[0]->f_offset++;
	CHECK(src.p_ofile[0]->f_offset == 8);
	CHECK(fddup(&src, 0, &dst) == -SYS_EMFILE);
	CHECK(fp->f_count == NOFILE + 1);

	/* Lowest hole must be reused without consuming source descriptor */
	CHECK(fdfree(&dst, 4) == 0);
	CHECK(fddup(&src, 0, &dst) == 4);
	CHECK(fp->f_count == NOFILE + 1);
	CHECK(src.p_ofile[0] == fp);
	fdclean(&src);
	CHECK(fp->f_count == NOFILE);
	fdclean(&dst);
	sys_unlock();
	CHECK(file_in_use() == 0);
}

/* Replace occupied slot and release old endpoint only on final reference */
static void test_replace(void)
{
	struct proc src = {0};
	struct proc dst = {0};
	struct file *fp = NULL;
	struct file *old = NULL;
	struct inode *ip = NULL;
	int oldfd = 0;
	int newfd = 0;

	init_bufc();
	init_fsys();
	sched_stub_reset(NULL);
	sys_lock();
	fp = falloc(&src, &oldfd);
	old = falloc(&dst, &newfd);
	CHECK(fp != NULL && old != NULL);
	ip = ialloc();
	CHECK(ip != NULL);
	old->f_inode = ip;
	old->f_flag = FREAD | FPIPE;
	CHECK(fddup(&dst, newfd, &dst) == 1);

	/* Equal fd numbers in different processes still require replacement */
	CHECK(oldfd == newfd);
	CHECK(fddup2(&src, oldfd, &dst, newfd) == newfd);
	CHECK(src.p_ofile[oldfd] == fp);
	CHECK(dst.p_ofile[newfd] == fp);
	CHECK(fp->f_count == 2);
	CHECK(old->f_count == 1);
	CHECK(ip->i_count == 1);
	CHECK(sched_stub_wakeup_count((char *)ip + 1) == 0);
	CHECK(fddup2(&src, oldfd, &dst, 1) == 1);
	CHECK(old->f_count == 0);
	CHECK(inode_in_use() == 0);
	CHECK(sched_stub_wakeup_count((char *)ip + 1) == 1);
	CHECK(sched_stub_wakeup_count((char *)ip + 2) == 1);
	CHECK(fp->f_count == 3);
	CHECK(fddup2(&src, oldfd, &src, oldfd) == oldfd);
	CHECK(fddup2(&src, oldfd, &dst, newfd) == newfd);
	CHECK(fp->f_count == 3);
	CHECK(fddup2(&src, oldfd, &dst, NOFILE - 1) == NOFILE - 1);
	CHECK(dst.p_ofile[NOFILE - 1] == fp);
	CHECK(fp->f_count == 4);
	fdclean(&src);
	fdclean(&dst);
	sys_unlock();
	CHECK(file_in_use() == 0);
}

/* Preserve destination and reference counts on invalid fd or overflow */
static void test_dup_errors(void)
{
	struct proc src = {0};
	struct proc dst = {0};
	struct file *fp = NULL;
	struct file *old = NULL;
	struct file *saved[NOFILE] = {0};
	const int invalid[] = {INT_MIN, -1, NOFILE, INT_MAX};
	size_t i = 0;
	int oldfd = 0;
	int newfd = 0;

	init_bufc();
	init_fsys();
	sched_stub_reset(NULL);
	sys_lock();
	fp = falloc(&src, &oldfd);
	old = falloc(&dst, &newfd);
	CHECK(fp != NULL && old != NULL);
	memcpy(saved, dst.p_ofile, sizeof(saved));
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		CHECK(fddup(&src, invalid[i], &dst) == -SYS_EBADF);
		CHECK(fddup2(&src, invalid[i], &dst, newfd) == -SYS_EBADF);
		CHECK(fddup2(&src, oldfd, &dst, invalid[i]) == -SYS_EBADF);
		CHECK(memcmp(saved, dst.p_ofile, sizeof(saved)) == 0);
		CHECK(fp->f_count == 1 && old->f_count == 1);
	}
	CHECK(fddup(&src, 1, &dst) == -SYS_EBADF);
	CHECK(fddup2(&src, 1, &dst, newfd) == -SYS_EBADF);
	CHECK(fddup2(&src, 1, &src, 1) == -SYS_EBADF);
	CHECK(memcmp(saved, dst.p_ofile, sizeof(saved)) == 0);
	CHECK(fddup2(&src, oldfd, &src, 1) == 1);

	/* Simulate reference saturation without allocating hundreds of procs */
	fp->f_count = UCHAR_MAX;
	CHECK(fddup(&src, oldfd, &dst) == -SYS_ENFILE);
	CHECK(fddup2(&src, oldfd, &dst, newfd) == -SYS_ENFILE);
	CHECK(fddup2(&src, oldfd, &dst, NOFILE - 1) == -SYS_ENFILE);
	CHECK(memcmp(saved, dst.p_ofile, sizeof(saved)) == 0);
	CHECK(old->f_count == 1);
	CHECK(fddup2(&src, oldfd, &src, oldfd) == oldfd);
	CHECK(fddup2(&src, oldfd, &src, 1) == 1);
	CHECK(src.p_ofile[oldfd] == fp && src.p_ofile[1] == fp);
	CHECK(fp->f_count == UCHAR_MAX);
	fp->f_count = 2;
	fdclean(&src);
	fdclean(&dst);
	sys_unlock();
	CHECK(file_in_use() == 0);
}

/* Select current process and clear previous error on successful request */
static void test_sys_dup(void)
{
	struct proc proc = {0};
	struct proc other = {0};
	struct file *fp = NULL;
	int fd = 0;
	int i = 0;

	init_bufc();
	init_fsys();
	sched_stub_reset(&proc);
	sys_lock();
	fp = falloc(&proc, &fd);
	CHECK(fp != NULL);
	sys_unlock();
	proc.p_error = -SYS_EBADF;
	CHECK(sys_dup(fd) == 1);
	CHECK(proc.p_error == 0);
	CHECK(sys_lock_held() == 0);
	CHECK(sys_dup(-1) == -SYS_EBADF);
	CHECK(proc.p_error == -SYS_EBADF);
	CHECK(sys_lock_held() == 0);
	CHECK(sys_dup2(fd, NOFILE) == -SYS_EBADF);
	CHECK(proc.p_error == -SYS_EBADF);
	CHECK(sys_lock_held() == 0);
	CHECK(sys_dup2(fd, 1) == 1);
	CHECK(proc.p_error == 0);
	CHECK(fp->f_count == 2);
	for (i = 2; i < NOFILE; i++)
		CHECK(sys_dup(fd) == i);
	CHECK(sys_dup(fd) == -SYS_EMFILE);
	CHECK(proc.p_error == -SYS_EMFILE);
	CHECK(sys_lock_held() == 0);
	CHECK(sys_dup2(fd, NOFILE - 1) == NOFILE - 1);
	CHECK(proc.p_error == 0);
	CHECK(fp->f_count == NOFILE);

	/* Other process cannot resolve first process descriptor */
	sched_stub_reset(&other);
	CHECK(sys_dup(fd) == -SYS_EBADF);
	CHECK(other.p_error == -SYS_EBADF);
	CHECK(proc.p_error == 0);
	CHECK(other.p_ofile[fd] == NULL);
	CHECK(sys_lock_held() == 0);
	sys_lock();
	fdclean(&proc);
	sys_unlock();
	CHECK(file_in_use() == 0);
}
