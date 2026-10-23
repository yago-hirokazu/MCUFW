/* test_pipe.c */

#include <stdint.h>
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
static void test_rdwri_validation(void);
static void test_iomove(void);
static void test_resource_rollback(void);
static void test_pipe_user_results(void);
static void test_public_transfer_state(void);
static void test_partial_failures(void);
static void fail_lock_hook(struct wait_chan *chan, enum wait_reason reason);

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
	test_rdwri_validation();
	test_iomove();
	test_resource_rollback();
	test_pipe_user_results();
	test_public_transfer_state();
	test_partial_failures();
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
	CHECK(current_proc() == proc);
	return sys_read(fd, buf, count);
}

static ptrdiff_t call_write(struct proc *proc, int fd, const void *buf,
			    size_t count)
{
	CHECK(current_proc() == proc);
	return sys_write(fd, buf, count);
}

static int call_close(struct proc *proc, int fd)
{
	CHECK(current_proc() == proc);
	return sys_close(fd);
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
	unsigned char *base = u.u_base;
	size_t count = u.u_count;
	ptrdiff_t offset = u.u_offset;
	int error = u.u_error;

	(void)chan;
	(void)reason;
	u.u_base = (unsigned char *)&data;
	u.u_count = 1;
	u.u_offset = (ptrdiff_t)hook_inode->i_size;
	u.u_error = 0;
	writei(hook_inode);
	hook_result = u.u_error ? -u.u_error : (ptrdiff_t)(1 - u.u_count);
	u.u_base = base;
	u.u_count = count;
	u.u_offset = offset;
	u.u_error = error;
}

/* Simulate peer read while retaining suspended writer state */
static void drain_write_hook(struct wait_chan *chan,
			     enum wait_reason reason)
{
	unsigned char data[PIPSIZ];
	unsigned char *base = u.u_base;
	size_t count = u.u_count;
	ptrdiff_t offset = u.u_offset;
	int error = u.u_error;

	(void)chan;
	(void)reason;
	u.u_base = data;
	u.u_count = sizeof(data);
	u.u_offset = 0;
	u.u_error = 0;
	readp(hook_file);
	hook_result = u.u_error ? -u.u_error :
		      (ptrdiff_t)(sizeof(data) - u.u_count);
	u.u_base = base;
	u.u_count = count;
	u.u_offset = offset;
	u.u_error = error;
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
	CHECK(sys_read(NOFILE, NULL, 0) == 0);
	CHECK(sys_write(NOFILE, NULL, 0) == 0);
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
	CHECK(call_read(&proc, NOFILE, &data, 1) == -SYS_EBADF);
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

	u.u_base = (unsigned char *)data;
	u.u_count = 123;
	u.u_offset = 77;
	u.u_error = SYS_EIO;
	u.u_r.r_val1 = 71;
	u.u_r.r_val2 = 72;
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
	CHECK(u.u_base == (unsigned char *)data + 2 && u.u_count == 0);
	CHECK(u.u_offset == 4 && u.u_error == 0);
	CHECK(u.u_r.r_val1 == 2 && u.u_r.r_val2 == 0);
	ip->i_size = PIPSIZ + 1;
	CHECK(call_read(&proc, fd[0], data, 1) == -SYS_EINVAL);
	CHECK(u.u_base == (unsigned char *)data && u.u_count == 1);
	CHECK(u.u_offset == 0 && u.u_error == SYS_EINVAL);
	CHECK(u.u_r.r_val1 == 0 && u.u_r.r_val2 == 0);
	ip->i_size = 0;
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
	plock(ip);
	CHECK(u.u_error == 0);
	CHECK(ip->i_flag & ILOCK);
	CHECK(!(ip->i_flag & IWANT));
	CHECK(sched_stub_last_reason() == WAIT_LOCK);
	prele(ip);
	kernel_unlock();
	call_close_all(&proc);
}

/* Check partial transfer, capacity, EOF and zero-length state */
static void test_rdwri_bounds(void)
{
	unsigned char input[PIPSIZ + 1];
	unsigned char output[PIPSIZ + 2];
	struct inode *ip;

	binit();
	finit();
	sched_stub_reset(NULL);
	memset(input, 0xa5, sizeof(input));
	memset(output, 0x5a, sizeof(output));
	ip = ialloc();
	CHECK(ip != NULL);
	u.u_base = input;
	u.u_count = sizeof(input);
	u.u_offset = 0;
	u.u_error = SYS_EIO;
	u.u_r.r_val1 = 77;
	u.u_r.r_val2 = 88;
	writei(ip);
	CHECK(u.u_count == 1 && u.u_base == input + PIPSIZ);
	CHECK(u.u_offset == PIPSIZ && ip->i_size == PIPSIZ);
	CHECK(u.u_error == SYS_EIO);
	writei(ip);
	CHECK(u.u_count == 1 && u.u_offset == PIPSIZ);
	u.u_base = output + 1;
	u.u_count = PIPSIZ + 1;
	u.u_offset = 0;
	readi(ip);
	CHECK(u.u_count == 1 && u.u_base == output + PIPSIZ + 1);
	CHECK(u.u_offset == PIPSIZ && u.u_error == SYS_EIO);
	CHECK(memcmp(input, output + 1, PIPSIZ) == 0);
	CHECK(output[0] == 0x5a && output[PIPSIZ + 1] == 0x5a);
	readi(ip);
	CHECK(u.u_count == 1 && u.u_offset == PIPSIZ);
	CHECK(u.u_r.r_val1 == 77 && u.u_r.r_val2 == 88);
	u.u_base = NULL;
	u.u_count = 0;
	u.u_offset = -1;
	u.u_error = 0;
	readi(NULL);
	writei(NULL);
	iomove(NULL, 0, -1);
	CHECK(u.u_base == NULL && u.u_count == 0 && u.u_offset == -1);
	CHECK(u.u_error == 0);
	iput(ip);
}

/* Invalid inode requests leave transfer state and payload untouched */
static void test_rdwri_validation(void)
{
	unsigned char bytes[2] = { 0x31, 0x32 };
	unsigned char *saved_base;
	unsigned char *payload;
	struct inode *ip;
	struct inode *target;
	struct buf *bp;
	size_t saved_count;
	ptrdiff_t saved_offset;
	int direction;
	int item;

	binit();
	finit();
	sched_stub_reset(NULL);
	ip = ialloc();
	CHECK(ip != NULL);
	bp = ip->i_buf;
	payload = bp->b_addr;
	memset(payload, 0x5a, PIPSIZ);
	for (direction = 0; direction < 2; direction++) {
		for (item = 0; item < 8; item++) {
			target = ip;
			ip->i_size = 1;
			u.u_base = bytes;
			u.u_count = 1;
			u.u_offset = 0;
			u.u_error = 0;
			switch (item) {
			case 0:
				u.u_offset = -1;
				break;
			case 1:
				u.u_offset = PIPSIZ + 1;
				break;
			case 2:
				ip->i_size = PIPSIZ + 1;
				break;
			case 3:
				u.u_base = NULL;
				break;
			case 4:
				target = NULL;
				break;
			case 5:
				ip->i_buf = NULL;
				break;
			case 6:
				bp->b_addr = NULL;
				break;
			case 7:
				u.u_count = (size_t)PTRDIFF_MAX + 1;
				break;
			}
			saved_base = u.u_base;
			saved_count = u.u_count;
			saved_offset = u.u_offset;
			if (direction == B_READ)
				readi(target);
			else
				writei(target);
			CHECK(u.u_error == SYS_EINVAL);
			CHECK(u.u_base == saved_base);
			CHECK(u.u_count == saved_count);
			CHECK(u.u_offset == saved_offset);
			CHECK(ip->i_size ==
			      (size_t)(item == 2 ? PIPSIZ + 1 : 1));
			CHECK(bytes[0] == 0x31 && bytes[1] == 0x32);
			CHECK(payload[0] == 0x5a &&
			      payload[PIPSIZ - 1] == 0x5a);
			ip->i_buf = bp;
			bp->b_addr = payload;
		}
	}
	ip->i_size = 1;
	u.u_base = bytes;
	u.u_count = 1;
	u.u_offset = 2;
	u.u_error = 0;
	readi(ip);
	CHECK(u.u_error == SYS_EINVAL && u.u_count == 1);
	CHECK(u.u_offset == 2 && u.u_base == bytes);
	iput(ip);
	u.u_base = NULL;
	u.u_count = 0;
	u.u_offset = 0;
	u.u_error = 0;
}

/* Check copy directions, immutable source and offset overflow */
static void test_iomove(void)
{
	static const unsigned char source[] = "immutable";
	unsigned char payload[sizeof(source) + 2];
	unsigned char output[sizeof(source)];
	unsigned char *base;
	void *cp;
	size_t count;
	size_t remaining;
	ptrdiff_t offset;
	int flag;
	int item;

	memset(payload, 0x5a, sizeof(payload));
	u.u_base = (unsigned char *)source;
	u.u_count = sizeof(source);
	u.u_offset = 3;
	u.u_error = SYS_EIO;
	iomove(payload + 1, sizeof(source), B_WRITE);
	CHECK(memcmp(payload + 1, source, sizeof(source)) == 0);
	CHECK(payload[0] == 0x5a && payload[sizeof(payload) - 1] == 0x5a);
	CHECK(u.u_count == 0 && u.u_base == source + sizeof(source));
	CHECK(u.u_offset == (ptrdiff_t)(3 + sizeof(source)) &&
	      u.u_error == SYS_EIO);
	u.u_base = output;
	u.u_count = sizeof(output);
	u.u_offset = 0;
	iomove(payload + 1, sizeof(output), B_READ);
	CHECK(memcmp(output, source, sizeof(output)) == 0);
	CHECK(u.u_count == 0 && u.u_base == output + sizeof(output));
	CHECK(u.u_offset == (ptrdiff_t)sizeof(output) && u.u_error == SYS_EIO);
	for (item = 0; item < 7; item++) {
		u.u_base = output;
		u.u_count = 1;
		u.u_offset = 0;
		u.u_error = 0;
		cp = payload;
		count = 1;
		flag = B_WRITE;
		switch (item) {
		case 0:
			flag = B_READ | B_DONE;
			break;
		case 1:
			cp = NULL;
			break;
		case 2:
			u.u_base = NULL;
			break;
		case 3:
			count = 2;
			break;
		case 4:
			u.u_offset = -1;
			break;
		case 5:
			u.u_offset = PTRDIFF_MAX;
			break;
		case 6:
			u.u_count = (size_t)PTRDIFF_MAX + 1;
			break;
		}
		base = u.u_base;
		remaining = u.u_count;
		offset = u.u_offset;
		iomove(cp, count, flag);
		CHECK(u.u_error == SYS_EINVAL);
		CHECK(u.u_base == base && u.u_count == remaining);
		CHECK(u.u_offset == offset && payload[0] == 0x5a);
	}
	u.u_base = output;
	u.u_count = 1;
	u.u_offset = PTRDIFF_MAX - 1;
	u.u_error = 0;
	iomove(payload, 1, B_READ);
	CHECK(u.u_offset == PTRDIFF_MAX && u.u_count == 0);
	CHECK(u.u_base == output + 1 && u.u_error == 0);
	u.u_base = NULL;
	u.u_count = 0;
	u.u_offset = 0;
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

/* Verify public entry validation and whole-request results */
static void test_public_transfer_state(void)
{
	static const unsigned char source[] = "read-only source";
	unsigned char input[PIPSIZ + 44] = { 0 };
	unsigned char output[PIPSIZ + 44];
	struct proc proc;
	struct user proc_user;
	int fd[2];

	binit();
	finit();
	sched_stub_reset(NULL);
	u.u_error = SYS_EIO;
	u.u_r.r_val1 = 123;
	u.u_r.r_val2 = 456;
	CHECK(sys_read(NOFILE, NULL, 0) == 0);
	CHECK(u.u_error == 0 && u.u_count == 0 && u.u_base == NULL);
	CHECK(u.u_r.r_val1 == 0 && u.u_r.r_val2 == 0 && u.u_offset == 0);
	CHECK(sys_write(NOFILE, NULL, 0) == 0);
	CHECK(sys_read(NOFILE, NULL, 1) == -SYS_EINVAL);
	CHECK(u.u_count == 1 && u.u_r.r_val1 == 0);
	CHECK(sys_write(NOFILE, input, (size_t)PTRDIFF_MAX + 1) ==
	      -SYS_EINVAL);
	CHECK(u.u_count == (size_t)PTRDIFF_MAX + 1 && u.u_r.r_val1 == 0);
	CHECK(sys_read(NOFILE, output, 1) == -SYS_EBADF);
	CHECK(sys_close(NOFILE) == -SYS_EINVAL);
	CHECK(u.u_error == SYS_EINVAL && u.u_r.r_val1 == 0);
	proc_init(&proc, &proc_user);
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	CHECK(sys_read(fd[0], output, 1) == -SYS_EAGAIN);
	CHECK(u.u_error == SYS_EAGAIN && u.u_count == 1);
	CHECK(u.u_base == output && u.u_r.r_val1 == 0);
	memset(input, 0xa5, sizeof(input));
	CHECK(sys_write(fd[1], input, sizeof(input)) == PIPSIZ);
	CHECK(u.u_error == 0 && u.u_count == 44 && u.u_offset == PIPSIZ);
	CHECK(u.u_base == input + PIPSIZ && u.u_r.r_val1 == PIPSIZ);
	CHECK(sys_write(fd[1], input + PIPSIZ, 44) == -SYS_EAGAIN);
	CHECK(u.u_count == 44 && u.u_r.r_val1 == 0);
	CHECK(sys_read(fd[0], output, sizeof(output)) == PIPSIZ);
	CHECK(u.u_count == 44 && u.u_offset == PIPSIZ);
	CHECK(u.u_base == output + PIPSIZ && u.u_r.r_val1 == PIPSIZ);
	CHECK(!memcmp(input, output, PIPSIZ));
	CHECK(sys_write(fd[1], source, sizeof(source)) ==
	      (ptrdiff_t)sizeof(source));
	CHECK(u.u_count == 0 && u.u_base == source + sizeof(source));
	CHECK(sys_read(fd[0], output, sizeof(output)) ==
	      (ptrdiff_t)sizeof(source));
	CHECK(u.u_count == sizeof(output) - sizeof(source));
	CHECK(!memcmp(output, source, sizeof(source)));
	CHECK(sys_close(fd[1]) == 0);
	CHECK(u.u_error == 0 && u.u_r.r_val1 == 0 && u.u_r.r_val2 == 0);
	CHECK(sys_read(fd[0], output, 1) == 0);
	CHECK(u.u_count == 1 && u.u_r.r_val1 == 0 && u.u_base == output);
	CHECK(sys_close(fd[0]) == 0);
	sched_stub_reset(NULL);
}

/* Simulate lock conflict after writer has already filled pipe */
static void fail_lock_hook(struct wait_chan *chan, enum wait_reason reason)
{
	CHECK(reason == WAIT_WRITE);
	drain_write_hook(chan, reason);
	hook_inode->i_flag |= ILOCK;
	current_proc()->p_backend = NULL;
}

/* Preserve partial progress on sleep failure and later lock failure */
static void test_partial_failures(void)
{
	unsigned char data[PIPSIZ + 44];
	struct proc proc;
	struct user proc_user;
	struct inode *ip;
	int fd[2];

	memset(data, 0x5a, sizeof(data));
	binit();
	finit();
	proc_init(&proc, &proc_user);
	proc.p_backend = &proc;
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	ip = getf_for_proc(&proc, fd[0])->f_inode;
	sched_stub_set_sleep_result(-SYS_EINVAL);
	CHECK(sys_write(fd[1], data, sizeof(data)) == -SYS_EINVAL);
	CHECK(u.u_count == 44 && u.u_r.r_val1 == PIPSIZ);
	CHECK(u.u_base == data + PIPSIZ && u.u_offset == PIPSIZ);
	CHECK(u.u_error == SYS_EINVAL && !(ip->i_flag & ILOCK));
	CHECK(ip->i_size == PIPSIZ);
	CHECK(!memcmp(ip->i_buf->b_addr, data, PIPSIZ));
	call_close_all(&proc);

	binit();
	finit();
	proc_init(&proc, &proc_user);
	proc.p_backend = &proc;
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	hook_file = getf_for_proc(&proc, fd[0]);
	hook_inode = hook_file->f_inode;
	sched_stub_set_sleep_hook(fail_lock_hook);
	CHECK(sys_write(fd[1], data, sizeof(data)) == -SYS_EAGAIN);
	CHECK(u.u_error == SYS_EAGAIN && u.u_count == 44);
	CHECK(u.u_r.r_val1 == PIPSIZ && u.u_base == data + PIPSIZ);
	CHECK(hook_inode->i_flag & ILOCK);
	kernel_lock();
	prele(hook_inode);
	kernel_unlock();
	call_close_all(&proc);

	binit();
	finit();
	proc_init(&proc, &proc_user);
	proc.p_backend = &proc;
	sched_stub_reset(&proc);
	CHECK(sys_pipe(fd) == 0);
	ip = getf_for_proc(&proc, fd[0])->f_inode;
	ip->i_flag |= ILOCK;
	sched_stub_set_sleep_result(-SYS_EINVAL);
	CHECK(sys_read(fd[0], data, 1) == -SYS_EINVAL);
	CHECK(u.u_error == SYS_EINVAL && u.u_count == 1);
	CHECK(u.u_r.r_val1 == 0 && (ip->i_flag & ILOCK));
	kernel_lock();
	prele(ip);
	kernel_unlock();
	call_close_all(&proc);
	sched_stub_reset(NULL);
}
