/* test_pipe.c */

#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "pipe.h"
#include "sched.h"
#include "sched_stub.h"
#include "user.h"
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
static struct proc *hook_proc;
static struct file *hook_file;
static struct inode *hook_inode;
static ptrdiff_t hook_result;

static void init_proc(struct proc *proc, struct user *user);
static ptrdiff_t call_read(struct proc *proc, int fd, void *buf,
			   size_t count);
static ptrdiff_t call_write(struct proc *proc, int fd, const void *buf,
			    size_t count);
static int call_close(struct proc *proc, int fd);
static void call_close_all(struct proc *proc);
static void supply_read_hook(struct wait_chan *chan,
			     enum wait_reason reason);
static void drain_write_hook(struct wait_chan *chan,
			     enum wait_reason reason);
static void unlock_hook(struct wait_chan *chan, enum wait_reason reason);
static void test_zero_length_and_validation(void);
static void test_create_and_data_path(void);
static void test_full_and_offset_reset(void);
static void test_eof_epipe_and_close_wakeup(void);
static void test_wait_rechecks(void);
static void test_pipe_lock(void);
static void test_rdwri_bounds(void);
static void test_resource_rollback(void);
static void test_pipe_user_results(void);

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
	test_zero_length_and_validation();
	test_create_and_data_path();
	test_full_and_offset_reset();
	test_eof_epipe_and_close_wakeup();
	test_wait_rechecks();
	test_pipe_lock();
	test_rdwri_bounds();
	test_resource_rollback();
	test_pipe_user_results();
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
	proc->p_state = PROC_NEW;
}

static ptrdiff_t call_read(struct proc *proc, int fd, void *buf,
			   size_t count)
{
	ptrdiff_t result;

	kernel_lock();
	result = readp(proc, getf_for_proc(proc, fd), buf, count);
	kernel_unlock();
	return result;
}

static ptrdiff_t call_write(struct proc *proc, int fd, const void *buf,
			    size_t count)
{
	ptrdiff_t result;

	kernel_lock();
	result = writep(proc, getf_for_proc(proc, fd), buf, count);
	kernel_unlock();
	return result;
}

static int call_close(struct proc *proc, int fd)
{
	int result;

	kernel_lock();
	result = fd_close(proc, fd);
	kernel_unlock();
	return result;
}

static void call_close_all(struct proc *proc)
{
	kernel_lock();
	close_all(proc);
	kernel_unlock();
}

static void supply_read_hook(struct wait_chan *chan,
			     enum wait_reason reason)
{
	static const char data = 'Q';
	size_t offset;

	(void)chan;
	(void)reason;
	offset = hook_inode->i_size;
	hook_result = writei(hook_inode, &offset, &data, 1);
}

static void drain_write_hook(struct wait_chan *chan,
			     enum wait_reason reason)
{
	unsigned char data[PIPSIZ];

	(void)chan;
	(void)reason;
	hook_result = readp(hook_proc, hook_file, data, sizeof(data));
}

static void unlock_hook(struct wait_chan *chan, enum wait_reason reason)
{
	(void)chan;
	(void)reason;
	prele(hook_inode);
}

static void test_zero_length_and_validation(void)
{
	struct proc proc;
	struct user proc_user;
	char data;
	int fd[2];

	binit();
	finit();
	init_proc(&proc, &proc_user);
	sched_stub_reset(&proc);
	CHECK(readp(NULL, NULL, NULL, 0) == 0);
	CHECK(writep(NULL, NULL, NULL, 0) == 0);
	sched_stub_reset(NULL);
	CHECK(sys_pipe(fd) == -SYS_EINVAL);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == 0);
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	CHECK(call_read(&proc, fd[0], NULL, 1) == -SYS_EINVAL);
	CHECK(call_write(&proc, fd[1], NULL, 1) == -SYS_EINVAL);
	CHECK(call_read(&proc, fd[1], &data, 1) == -SYS_EBADF);
	CHECK(call_write(&proc, fd[0], &data, 1) == -SYS_EBADF);
	CHECK(call_read(&proc, NOFILE, &data, 1) == -SYS_EINVAL);
	call_close_all(&proc);
}

static void test_create_and_data_path(void)
{
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	struct file *rf;
	struct file *wf;
	char data[4];
	int fd[2];

	binit();
	finit();
	init_proc(&proc, &proc_user);
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	CHECK(fd[0] == 0);
	CHECK(fd[1] == 1);
	rf = getf_for_proc(&proc, fd[0]);
	wf = getf_for_proc(&proc, fd[1]);
	CHECK(rf != NULL);
	CHECK(wf != NULL);
	CHECK(rf->f_flag == (FREAD | FPIPE));
	CHECK(wf->f_flag == (FWRITE | FPIPE));
	CHECK(rf->f_inode == wf->f_inode);
	ip = rf->f_inode;
	CHECK(ip->i_count == 2);
	CHECK(pipe_in_use() == 1);

	CHECK(call_write(&proc, fd[1], "abc", 3) == 3);
	CHECK(ip->i_size == 3);
	CHECK(wf->f_offset == 0);
	memset(data, 0, sizeof(data));
	CHECK(call_read(&proc, fd[0], data, 2) == 2);
	CHECK(memcmp(data, "ab", 2) == 0);
	CHECK(rf->f_offset == 2);
	CHECK(ip->i_size == 3);
	CHECK(call_write(&proc, fd[1], "d", 1) == 1);
	CHECK(ip->i_size == 4);
	CHECK(call_read(&proc, fd[0], data, 2) == 2);
	CHECK(memcmp(data, "cd", 2) == 0);
	CHECK(rf->f_offset == 0);
	CHECK(ip->i_size == 0);
	CHECK(sched_stub_wakeup_count(&ip->i_read_chan) >= 2);
	CHECK(sched_stub_wakeup_count(&ip->i_write_chan) >= 1);
	call_close_all(&proc);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == 0);
}

static void test_full_and_offset_reset(void)
{
	unsigned char input[PIPSIZ + 1];
	unsigned char output[PIPSIZ];
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	struct file *rf;
	int fd[2];
	int i;

	binit();
	finit();
	init_proc(&proc, &proc_user);
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	rf = getf_for_proc(&proc, fd[0]);
	ip = rf->f_inode;
	for (i = 0; i < PIPSIZ + 1; i++)
		input[i] = (unsigned char)i;
	CHECK(call_write(&proc, fd[1], input, sizeof(input)) == PIPSIZ);
	CHECK(ip->i_size == PIPSIZ);
	CHECK(call_read(&proc, fd[0], output, 1) == 1);
	CHECK(rf->f_offset == 1);
	CHECK(ip->i_size == PIPSIZ);
	CHECK(call_write(&proc, fd[1], input, 1) == -SYS_EAGAIN);
	CHECK(call_read(&proc, fd[0], output + 1, PIPSIZ - 1) ==
	      PIPSIZ - 1);
	CHECK(memcmp(input, output, PIPSIZ) == 0);
	CHECK(rf->f_offset == 0);
	CHECK(ip->i_size == 0);
	CHECK(call_write(&proc, fd[1], input, 1) == 1);
	call_close_all(&proc);
}

static void test_eof_epipe_and_close_wakeup(void)
{
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	char data;
	int fd[2];

	binit();
	finit();
	init_proc(&proc, &proc_user);
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	ip = getf_for_proc(&proc, fd[0])->f_inode;
	CHECK(call_close(&proc, fd[1]) == 0);
	CHECK(sched_stub_wakeup_count(&ip->i_read_chan) == 1);
	CHECK(sched_stub_wakeup_count(&ip->i_write_chan) == 1);
	CHECK(call_read(&proc, fd[0], &data, 1) == 0);
	CHECK(call_close(&proc, fd[0]) == 0);
	CHECK(pipe_in_use() == 0);

	binit();
	finit();
	init_proc(&proc, &proc_user);
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	CHECK(call_close(&proc, fd[0]) == 0);
	CHECK(call_write(&proc, fd[1], &data, 1) == -SYS_EPIPE);
	CHECK(call_close(&proc, fd[1]) == 0);
	CHECK(pipe_in_use() == 0);
}

static void test_wait_rechecks(void)
{
	unsigned char data[PIPSIZ];
	struct proc proc;
	struct user proc_user;
	struct file *rf;
	char byte;
	int fd[2];

	binit();
	finit();
	init_proc(&proc, &proc_user);
	proc.p_backend = &proc;
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	rf = getf_for_proc(&proc, fd[0]);
	hook_inode = rf->f_inode;
	hook_result = -1;
	sched_stub_set_sleep_hook(supply_read_hook);
	CHECK(call_read(&proc, fd[0], &byte, 1) == 1);
	CHECK(byte == 'Q');
	CHECK(hook_result == 1);
	CHECK(sched_stub_sleep_count() == 1);
	CHECK(sched_stub_last_reason() == WAIT_READ);

	memset(data, 0x5a, sizeof(data));
	CHECK(call_write(&proc, fd[1], data, sizeof(data)) == PIPSIZ);
	hook_proc = &proc;
	hook_file = rf;
	hook_result = -1;
	sched_stub_set_sleep_hook(drain_write_hook);
	CHECK(call_write(&proc, fd[1], "W", 1) == 1);
	CHECK(hook_result == PIPSIZ);
	CHECK(sched_stub_sleep_count() == 2);
	CHECK(sched_stub_last_reason() == WAIT_WRITE);
	call_close_all(&proc);
}

static void test_pipe_lock(void)
{
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	int fd[2];

	binit();
	finit();
	init_proc(&proc, &proc_user);
	proc.p_backend = &proc;
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	ip = getf_for_proc(&proc, fd[0])->f_inode;
	hook_inode = ip;
	ip->i_flag |= ILOCK;
	sched_stub_set_sleep_hook(unlock_hook);
	kernel_lock();
	CHECK(plock(ip) == 0);
	CHECK(ip->i_flag & ILOCK);
	CHECK(!(ip->i_flag & IWANT));
	CHECK(sched_stub_last_reason() == WAIT_LOCK);
	prele(ip);
	kernel_unlock();
	call_close_all(&proc);
}

static void test_rdwri_bounds(void)
{
	unsigned char input[PIPSIZ + 1];
	unsigned char output[PIPSIZ];
	struct inode *ip;
	size_t offset;

	binit();
	finit();
	sched_stub_reset(NULL);
	memset(input, 0xa5, sizeof(input));
	ip = ialloc();
	CHECK(ip != NULL);
	offset = 0;
	CHECK(writei(ip, &offset, input, sizeof(input)) == PIPSIZ);
	CHECK(offset == PIPSIZ);
	CHECK(ip->i_size == PIPSIZ);
	CHECK(writei(ip, &offset, input, 1) == 0);
	offset = 0;
	CHECK(readi(ip, &offset, output, sizeof(output)) == PIPSIZ);
	CHECK(memcmp(input, output, PIPSIZ) == 0);
	CHECK(readi(ip, &offset, output, 1) == 0);
	CHECK(readi(NULL, NULL, NULL, 0) == 0);
	CHECK(writei(NULL, NULL, NULL, 0) == 0);
	iput(ip);
}

static void test_resource_rollback(void)
{
	struct inode *ips[NINODE];
	struct proc caller;
	struct user caller_user;
	struct proc owner;
	struct user owner_user;
	int pipes[NINODE][2];
	int fd[2];
	int item;
	int i;

	binit();
	finit();
	init_proc(&caller, &caller_user);
	sched_stub_reset(&caller);
	for (i = 0; i < NOFILE - 1; i++)
		CHECK(allocate_file(&caller, &item) != NULL);
	CHECK(sys_pipe(fd) == -SYS_EMFILE);
	CHECK(file_in_use() == NOFILE - 1);
	CHECK(inode_in_use() == 0);
	call_close_all(&caller);

	binit();
	finit();
	init_proc(&caller, &caller_user);
	sched_stub_reset(&caller);
	for (i = 0; i < NOFILE; i++)
		CHECK(allocate_file(&caller, &item) != NULL);
	CHECK(sys_pipe(fd) == -SYS_EMFILE);
	CHECK(file_in_use() == NOFILE);
	CHECK(inode_in_use() == 0);
	call_close_all(&caller);

	binit();
	finit();
	init_proc(&caller, &caller_user);
	init_proc(&owner, &owner_user);
	sched_stub_reset(&caller);
	for (i = 0; i < NFILE; i++)
		CHECK(allocate_file(&owner, &item) != NULL);
	CHECK(sys_pipe(fd) == -SYS_ENFILE);
	CHECK(file_in_use() == NFILE);
	CHECK(inode_in_use() == 0);
	call_close_all(&owner);

	binit();
	finit();
	init_proc(&caller, &caller_user);
	init_proc(&owner, &owner_user);
	sched_stub_reset(&caller);
	for (i = 0; i < NFILE - 1; i++)
		CHECK(allocate_file(&owner, &item) != NULL);
	CHECK(sys_pipe(fd) == -SYS_ENFILE);
	CHECK(file_in_use() == NFILE - 1);
	CHECK(inode_in_use() == 0);
	CHECK(caller.p_addr->u_ofile[0] == NULL);
	call_close_all(&owner);

	binit();
	finit();
	init_proc(&caller, &caller_user);
	sched_stub_reset(&caller);
	for (i = 0; i < NINODE; i++) {
		ips[i] = ialloc();
		CHECK(ips[i] != NULL);
	}
	CHECK(sys_pipe(fd) == -SYS_ENFILE);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == NINODE);
	for (i = 0; i < NINODE; i++)
		iput(ips[i]);

	binit();
	finit();
	init_proc(&caller, &caller_user);
	sched_stub_reset(&caller);
	for (i = 0; i < NINODE; i++)
		CHECK(sys_pipe(pipes[i]) == 0);
	CHECK(sys_pipe(fd) == -SYS_ENFILE);
	CHECK(pipe_in_use() == NINODE);
	call_close_all(&caller);
	CHECK(file_in_use() == 0);
	CHECK(inode_in_use() == 0);
}

/* Check explicit target preparation and current pipe result conversion */
static void test_pipe_user_results(void)
{
	struct inode *ips[NINODE];
	struct proc unbound = { 0 };
	struct proc caller;
	struct proc owner;
	struct user caller_user;
	struct user owner_user;
	int fd[2] = { -10, -20 };
	int error = 0;
	int i;

	binit();
	finit();
	proc_init(&caller, &caller_user);
	proc_init(&owner, &owner_user);
	sched_stub_reset(&caller);
	u.u_error = SYS_EIO;
	u.u_r.r_val1 = 111;
	u.u_r.r_val2 = 222;
	CHECK(pipe_for_proc(&unbound, fd) == -SYS_EINVAL);
	CHECK(fd[0] == -10 && fd[1] == -20);
	CHECK(pipe_for_proc(&owner, fd) == 0);
	CHECK(fd[0] == 0 && fd[1] == 1);
	CHECK(owner_user.u_error == 0);
	CHECK(owner_user.u_r.r_val1 == 0 && owner_user.u_r.r_val2 == 1);
	CHECK(current_user() == &caller_user);
	CHECK(u.u_error == SYS_EIO);
	CHECK(u.u_r.r_val1 == 111 && u.u_r.r_val2 == 222);
	close_all(&owner);
	for (i = 0; i < NOFILE - 1; i++)
		CHECK(falloc_for_user(&owner_user) != NULL);
	fd[0] = -10;
	fd[1] = -20;
	CHECK(pipe_for_proc(&owner, fd) == -SYS_EMFILE);
	CHECK(owner_user.u_error == SYS_EMFILE);
	CHECK(fd[0] == -10 && fd[1] == -20);
	CHECK(owner_user.u_ofile[NOFILE - 1] == NULL);
	CHECK(file_in_use() == NOFILE - 1 && inode_in_use() == 0);
	CHECK(u.u_error == SYS_EIO);
	CHECK(u.u_r.r_val1 == 111 && u.u_r.r_val2 == 222);
	close_all(&owner);
	for (i = 0; i < NINODE; i++) {
		ips[i] = ialloc_error(&error);
		CHECK(ips[i] != NULL && error == 0);
	}
	CHECK(pipe_for_proc(&owner, fd) == -SYS_ENFILE);
	CHECK(owner_user.u_error == SYS_ENFILE);
	CHECK(fd[0] == -10 && fd[1] == -20);
	CHECK(u.u_error == SYS_EIO);
	CHECK(u.u_r.r_val1 == 111 && u.u_r.r_val2 == 222);
	for (i = 0; i < NINODE; i++)
		iput(ips[i]);
	CHECK(sys_pipe(NULL) == -SYS_EINVAL);
	CHECK(u.u_error == SYS_EINVAL);
	CHECK(sys_pipe(fd) == 0);
	CHECK(u.u_error == 0 && u.u_r.r_val1 == fd[0]);
	CHECK(u.u_r.r_val2 == fd[1]);
	close_all(&caller);
	u.u_error = SYS_EIO;
	pipe_create();
	CHECK(u.u_error == SYS_EIO);
	CHECK(getf((int)u.u_r.r_val1)->f_flag == (FREAD | FPIPE));
	CHECK(getf((int)u.u_r.r_val2)->f_flag == (FWRITE | FPIPE));
	close_all(&caller);
	sched_stub_reset(NULL);
	fd[0] = -10;
	fd[1] = -20;
	CHECK(sys_pipe(fd) == -SYS_EINVAL);
	CHECK(fd[0] == -10 && fd[1] == -20);
	u.u_error = 0;
}
