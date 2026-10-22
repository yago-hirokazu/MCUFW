/* test_file.c */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "sched.h"
#include "sched_stub.h"
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
static void test_current_file_api(void);
static void test_file_allocation(void);
static void test_descriptor_helpers(void);
static void test_user_descriptors(void);
static void test_fd_exhaustion(void);
static void test_file_exhaustion(void);
static void test_inode_lifecycle(void);
static void test_inode_exhaustion(void);
static void test_buffer_failure(void);
static void test_reuse_after_close_all(void);

/* Prepare file for explicit test owner */
static struct file *allocate_file(struct proc *proc, int *fd)
{
	struct file *fp;

	if (!fd)
		return NULL;
	if (!proc || !proc->p_addr) {
		*fd = -SYS_EINVAL;
		return NULL;
	}
	fp = falloc_for_user(proc->p_addr);
	*fd = fp ? (int)proc->p_addr->u_r.r_val1 : -proc->p_addr->u_error;
	return fp;
}

int main(void)
{
	test_current_file_api();
	test_file_allocation();
	test_descriptor_helpers();
	test_user_descriptors();
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
	proc_init(proc, user);
}

static void test_file_allocation(void)
{
	struct proc proc;
	struct proc unbound = { 0 };
	struct user proc_user;
	struct file *fp;
	int fd;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	CHECK(file_in_use() == 0);
	CHECK(getf_for_proc(&proc, -1) == NULL);
	CHECK(getf_for_proc(&proc, NOFILE) == NULL);
	CHECK(getf_for_proc(NULL, 0) == NULL);
	fp = allocate_file(&proc, &fd);
	CHECK(fp == &file[0]);
	CHECK(fd == 0);
	CHECK(proc.p_addr->u_ofile[0] == fp);
	CHECK(fp->f_count == 1);
	CHECK(fp->f_flag == 0);
	CHECK(fp->f_inode == NULL);
	CHECK(fp->f_offset == 0);
	CHECK(getf_for_proc(&proc, fd) == fp);
	CHECK(file_in_use() == 1);
	CHECK(fd_close(&proc, fd) == 0);
	CHECK(fd_close(&proc, fd) == -SYS_EBADF);
	CHECK(file_in_use() == 0);
	CHECK(allocate_file(NULL, &fd) == NULL);
	CHECK(fd == -SYS_EINVAL);
	CHECK(allocate_file(&proc, NULL) == NULL);
	CHECK(getf_for_proc(&unbound, 0) == NULL);
	CHECK(allocate_file(&unbound, &fd) == NULL);
	CHECK(fd == -SYS_EINVAL);
	CHECK(fd_install(&unbound, &file[0]) == -SYS_EINVAL);
	CHECK(fd_install_at(&unbound, 0, &file[0]) == -SYS_EINVAL);
	CHECK(fd_close(&unbound, 0) == -SYS_EBADF);
	close_all(&unbound);
	CHECK(file_in_use() == 0);
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
	fp = allocate_file(&proc, &fd);
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

/* Keep same fd independent while transferring shared file reference */
static void test_user_descriptors(void)
{
	struct proc procs[2];
	struct user users[2];
	struct file *files[2];
	int fd;
	int i;

	binit();
	finit();
	for (i = 0; i < 2; i++) {
		init_proc(&procs[i], &users[i]);
		CHECK(users[i].u_procp == &procs[i]);
		files[i] = allocate_file(&procs[i], &fd);
		CHECK(files[i] != NULL && fd == 0);
	}
	CHECK(files[0] != files[1]);
	sched_stub_reset(&procs[0]);
	users[0].u_error = SYS_EIO;
	users[1].u_error = SYS_EPIPE;
	CHECK(fd_install_at(&procs[1], 0, files[0]) == -SYS_EMFILE);
	CHECK(fd_install(&procs[1], files[0]) == 1);
	CHECK(files[0]->f_count == 2);
	CHECK(fd_close(&procs[0], 0) == 0);
	CHECK(getf_for_proc(&procs[0], 0) == NULL);
	CHECK(getf_for_proc(&procs[1], 0) == files[1]);
	CHECK(getf_for_proc(&procs[1], 1) == files[0]);
	CHECK(files[0]->f_count == 1);
	CHECK(current_user() == &users[0]);
	CHECK(users[0].u_error == SYS_EIO && users[1].u_error == SYS_EPIPE);
	close_all(&procs[1]);
	CHECK(procs[1].p_addr->u_ofile[0] == NULL);
	CHECK(file_in_use() == 0);
	CHECK(users[0].u_error == SYS_EIO && users[1].u_error == SYS_EPIPE);
	sched_stub_reset(NULL);
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
		fp = allocate_file(&proc, &fd);
		CHECK(fp != NULL);
		CHECK(fd == i);
	}
	used = file_in_use();
	sched_stub_reset(&proc);
	CHECK(ufalloc() == -1);
	CHECK(u.u_error == SYS_EMFILE);
	sched_stub_reset(NULL);
	CHECK(allocate_file(&proc, &fd) == NULL);
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
		CHECK(allocate_file(&owner, &fd) != NULL);
	sched_stub_reset(&waiter);
	CHECK(ufalloc() == 0);
	sched_stub_reset(NULL);
	CHECK(allocate_file(&waiter, &fd) == NULL);
	CHECK(fd == -SYS_ENFILE);
	CHECK(waiter.p_addr->u_ofile[0] == NULL);
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
	fp = allocate_file(&proc, &fd);
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
	CHECK(u.u_error == SYS_ENFILE);
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
	CHECK(u.u_error == SYS_ENFILE);
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
	fp = allocate_file(&proc, &fd);
	CHECK(ip != NULL);
	CHECK(fp != NULL);
	bp = ip->i_buf;
	fp->f_inode = ip;
	close_all(&proc);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == 0);
	CHECK(allocate_file(&proc, &fd) == &file[0]);
	CHECK(fd == 0);
	ip = ialloc();
	CHECK(ip == &inode[0]);
	CHECK(ip->i_buf == bp);
	close_all(&proc);
	iput(ip);
}

/* Check V7 results, sticky errors and current-user selection */
static void test_current_file_api(void)
{
	struct proc procs[2];
	struct user users[2];
	struct file *fp;
	int i;

	binit();
	finit();
	for (i = 0; i < 2; i++) {
		memset(&users[i], 0xa5, sizeof(users[i]));
		proc_init(&procs[i], &users[i]);
		CHECK(users[i].u_r.r_val1 == 0);
		CHECK(users[i].u_r.r_val2 == 0);
	}
	sched_stub_reset(&procs[0]);
	u.u_error = SYS_EIO;
	u.u_r.r_val2 = 123;
	CHECK(ufalloc() == 0 && u.u_r.r_val1 == 0);
	CHECK(u.u_error == SYS_EIO && u.u_r.r_val2 == 123);
	fp = falloc();
	CHECK(fp != NULL && u.u_r.r_val1 == 0);
	CHECK(getf(0) == fp && u.u_error == SYS_EIO);
	CHECK(getf(-1) == NULL && u.u_error == SYS_EBADF);
	CHECK(getf(NOFILE) == NULL && u.u_error == SYS_EBADF);
	users[1].u_error = SYS_EPIPE;
	users[1].u_r.r_val1 = 456;
	users[1].u_r.r_val2 = 789;
	CHECK(fd_install(&procs[1], fp) == 0);
	CHECK(getf_for_proc(&procs[1], NOFILE) == NULL);
	CHECK(users[1].u_error == SYS_EPIPE);
	CHECK(users[1].u_r.r_val1 == 456);
	CHECK(users[1].u_r.r_val2 == 789);
	CHECK(u.u_error == SYS_EBADF && u.u_r.r_val2 == 123);
	sched_stub_reset(&procs[1]);
	CHECK(getf(0) == fp && u.u_error == SYS_EPIPE);
	close_all(&procs[0]);
	close_all(&procs[1]);
	CHECK(u.u_error == SYS_EPIPE && u.u_r.r_val1 == 456);
	sched_stub_reset(NULL);
	u.u_error = 0;
	CHECK(falloc() == NULL && u.u_error == SYS_EINVAL);
	CHECK(file_in_use() == 0);
	u.u_error = 0;
}
