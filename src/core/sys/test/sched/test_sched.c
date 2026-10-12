/* test_sched.c */

#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "pipe.h"
#include "sched.h"
#include "sys_error.h"

#define TEST_DATA_SIZE	(PIPSIZ * 2 + 37)
#define SCHED_LIMIT	1000

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

struct writer_arg {
	const unsigned char *data;
	size_t count;
	int fd;
	ptrdiff_t result;
};

struct reader_arg {
	unsigned char *data;
	size_t capacity;
	size_t count;
	int fd;
	ptrdiff_t last_result;
};

struct wait_arg {
	struct wait_chan *chan;
	enum wait_reason reason;
	int result;
	int lock_held;
};

static int failures;

static ptrdiff_t proc_read(int fd, void *buf, size_t count);
static ptrdiff_t proc_write(int fd, const void *buf, size_t count);
static int proc_close(int fd);
static void close_all_locked(struct proc *proc);
static int connect_pipe(struct proc *writer, struct proc *reader,
			int *write_fd, int *read_fd);
static int run_scheduler(struct proc *procs, int count);
static void writer_entry(void *arg);
static void reader_entry(void *arg);
static void wait_entry(void *arg);
static void unused_entry(void *arg);
static void test_blocking_transfer(void);
static void test_wait_lock(void);
static void test_multiple_waiters(void);
static void test_release_waiter(void);
static void test_wake_then_release(void);
static void test_yield_failure_cleanup(void);
static void test_prepare_exhaustion(void);
static void check_resources(void);

int main(void)
{
	test_blocking_transfer();
	check_resources();
	test_wait_lock();
	check_resources();
	test_multiple_waiters();
	test_release_waiter();
	test_wake_then_release();
	test_yield_failure_cleanup();
	test_prepare_exhaustion();
	check_resources();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static ptrdiff_t proc_read(int fd, void *buf, size_t count)
{
	struct proc *proc;
	ptrdiff_t result;

	kernel_lock();
	proc = current_proc();
	result = readp(proc, getf(proc, fd), buf, count);
	kernel_unlock();
	return result;
}

static ptrdiff_t proc_write(int fd, const void *buf, size_t count)
{
	struct proc *proc;
	ptrdiff_t result;

	kernel_lock();
	proc = current_proc();
	result = writep(proc, getf(proc, fd), buf, count);
	kernel_unlock();
	return result;
}

static int proc_close(int fd)
{
	struct proc *proc;
	int result;

	kernel_lock();
	proc = current_proc();
	result = fd_close(proc, fd);
	kernel_unlock();
	return result;
}

static void close_all_locked(struct proc *proc)
{
	kernel_lock();
	close_all(proc);
	kernel_unlock();
}

static int connect_pipe(struct proc *writer, struct proc *reader,
			int *write_fd, int *read_fd)
{
	struct file *read_file;
	int fd[2];
	int result;

	kernel_lock();
	result = pipe_for_proc(writer, fd);
	if (result < 0)
		goto out;
	read_file = getf(writer, fd[0]);
	*read_fd = fd_install(reader, read_file);
	if (*read_fd < 0) {
		result = *read_fd;
		goto out;
	}
	result = fd_close(writer, fd[0]);
	if (result < 0)
		goto out;
	*write_fd = fd[1];
out:
	kernel_unlock();
	return result;
}

static int run_scheduler(struct proc *procs, int count)
{
	int finished;
	int progress;
	int iteration;
	int i;

	for (iteration = 0; iteration < SCHED_LIMIT; iteration++) {
		finished = 0;
		progress = 0;
		for (i = 0; i < count; i++) {
			if (procs[i].p_state == PROC_FINISHED) {
				finished++;
				continue;
			}
			if (procs[i].p_state == PROC_ERROR)
				return -1;
			if (procs[i].p_state != PROC_RUNNABLE)
				continue;
			if (sched_resume(&procs[i]) < 0)
				return -1;
			progress = 1;
		}
		if (finished == count)
			return 0;
		if (!progress)
			return -1;
	}
	return -1;
}

static void writer_entry(void *arg)
{
	struct writer_arg *writer;

	writer = arg;
	writer->result = proc_write(writer->fd, writer->data, writer->count);
	CHECK(proc_close(writer->fd) == 0);
}

static void reader_entry(void *arg)
{
	struct reader_arg *reader;
	ptrdiff_t count;

	reader = arg;
	for (;;) {
		count = proc_read(reader->fd, reader->data + reader->count,
				  reader->capacity - reader->count);
		reader->last_result = count;
		if (count <= 0)
			break;
		reader->count += (size_t)count;
	}
	CHECK(proc_close(reader->fd) == 0);
}

static void wait_entry(void *arg)
{
	struct wait_arg *wait;

	wait = arg;
	kernel_lock();
	wait->result = sched_sleep(wait->chan, wait->reason);
	wait->lock_held = kernel_is_locked();
	kernel_unlock();
}

static void unused_entry(void *arg)
{
	int *called;

	called = arg;
	(*called)++;
}

static void test_blocking_transfer(void)
{
	struct writer_arg writer;
	struct reader_arg reader;
	struct proc procs[2];
	unsigned char source[TEST_DATA_SIZE];
	unsigned char result[TEST_DATA_SIZE];
	int write_fd;
	int read_fd;
	int i;

	for (i = 0; i < TEST_DATA_SIZE; i++)
		source[i] = (unsigned char)(i * 37);
	memset(result, 0, sizeof(result));
	file_system_init();
	proc_init(&procs[0]);
	proc_init(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	writer.data = source;
	writer.count = sizeof(source);
	writer.fd = write_fd;
	writer.result = -1;
	reader.data = result;
	reader.capacity = sizeof(result);
	reader.count = 0;
	reader.fd = read_fd;
	reader.last_result = -1;
	CHECK(sched_prepare(&procs[0], writer_entry, &writer) == 0);
	CHECK(sched_prepare(&procs[1], reader_entry, &reader) == 0);
	CHECK(sched_resume(&procs[1]) == 0);
	CHECK(procs[1].p_state == PROC_WAIT_READ);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(writer.result == TEST_DATA_SIZE);
	CHECK(reader.last_result == 0);
	CHECK(reader.count == TEST_DATA_SIZE);
	CHECK(memcmp(source, result, sizeof(source)) == 0);
	sched_release(&procs[0]);
	sched_release(&procs[1]);
}

static void test_wait_lock(void)
{
	struct reader_arg reader;
	struct proc owner;
	struct proc sleeper;
	struct inode *ip;
	unsigned char input;
	unsigned char output;
	size_t offset;
	int write_fd;
	int read_fd;

	input = 0x6d;
	output = 0;
	file_system_init();
	proc_init(&owner);
	proc_init(&sleeper);
	CHECK(connect_pipe(&owner, &sleeper, &write_fd, &read_fd) == 0);
	ip = getf(&sleeper, read_fd)->f_inode;
	kernel_lock();
	ip->i_flag |= ILOCK;
	offset = 0;
	CHECK(writei(ip, &offset, &input, 1) == 1);
	kernel_unlock();
	reader.data = &output;
	reader.capacity = 1;
	reader.count = 0;
	reader.fd = read_fd;
	reader.last_result = -1;
	CHECK(sched_prepare(&sleeper, reader_entry, &reader) == 0);
	CHECK(sched_resume(&sleeper) == 0);
	CHECK(sleeper.p_state == PROC_WAIT_LOCK);
	kernel_lock();
	prele(ip);
	kernel_unlock();
	CHECK(run_scheduler(&sleeper, 1) == 0);
	CHECK(reader.count == 1);
	CHECK(output == input);
	close_all_locked(&owner);
	sched_release(&sleeper);
}

static void test_multiple_waiters(void)
{
	struct wait_arg wait[2];
	struct wait_chan chan;
	struct proc procs[2];
	int i;

	memset(&chan, 0, sizeof(chan));
	for (i = 0; i < 2; i++) {
		proc_init(&procs[i]);
		wait[i].chan = &chan;
		wait[i].reason = WAIT_READ;
		wait[i].result = -1;
		wait[i].lock_held = 0;
		CHECK(sched_prepare(&procs[i], wait_entry, &wait[i]) == 0);
		CHECK(sched_resume(&procs[i]) == 0);
		CHECK(procs[i].p_state == PROC_WAIT_READ);
	}
	kernel_lock();
	sched_wakeup(&chan);
	kernel_unlock();
	CHECK(chan.wc_head == NULL);
	CHECK(chan.wc_tail == NULL);
	CHECK(run_scheduler(procs, 2) == 0);
	for (i = 0; i < 2; i++) {
		CHECK(wait[i].result == 0);
		CHECK(wait[i].lock_held);
		sched_release(&procs[i]);
	}
}

static void test_release_waiter(void)
{
	struct wait_arg wait;
	struct wait_chan chan;
	struct proc proc;

	memset(&chan, 0, sizeof(chan));
	proc_init(&proc);
	wait.chan = &chan;
	wait.reason = WAIT_WRITE;
	CHECK(sched_prepare(&proc, wait_entry, &wait) == 0);
	CHECK(sched_resume(&proc) == 0);
	CHECK(proc.p_state == PROC_WAIT_WRITE);
	sched_release(&proc);
	CHECK(proc.p_backend == NULL);
	CHECK(chan.wc_head == NULL);
	CHECK(chan.wc_tail == NULL);
}

static void test_wake_then_release(void)
{
	struct wait_arg wait;
	struct wait_chan chan;
	struct proc proc;

	memset(&chan, 0, sizeof(chan));
	proc_init(&proc);
	wait.chan = &chan;
	wait.reason = WAIT_READ;
	CHECK(sched_prepare(&proc, wait_entry, &wait) == 0);
	CHECK(sched_resume(&proc) == 0);
	kernel_lock();
	sched_wakeup(&chan);
	kernel_unlock();
	CHECK(proc.p_state == PROC_RUNNABLE);
	sched_release(&proc);
	CHECK(proc.p_backend == NULL);
	CHECK(chan.wc_head == NULL);
}

static void test_yield_failure_cleanup(void)
{
	struct wait_arg wait;
	struct wait_chan chan;
	struct proc proc;

	memset(&chan, 0, sizeof(chan));
	proc_init(&proc);
	wait.chan = &chan;
	wait.reason = WAIT_READ;
	wait.result = 0;
	wait.lock_held = 0;
	CHECK(sched_prepare(&proc, wait_entry, &wait) == 0);
	sched_test_fail_next_yield();
	CHECK(sched_resume(&proc) == 0);
	CHECK(wait.result == -SYS_EINVAL);
	CHECK(wait.lock_held);
	CHECK(proc.p_state == PROC_FINISHED);
	CHECK(proc.p_wait_chan == NULL);
	CHECK(proc.p_wait_next == NULL);
	CHECK(chan.wc_head == NULL);
	CHECK(chan.wc_tail == NULL);
	sched_release(&proc);
}

static void test_prepare_exhaustion(void)
{
	struct proc procs[PIPELINE_MAX + 1];
	int called;
	int i;

	called = 0;
	for (i = 0; i < PIPELINE_MAX + 1; i++)
		proc_init(&procs[i]);
	for (i = 0; i < PIPELINE_MAX; i++)
		CHECK(sched_prepare(&procs[i], unused_entry, &called) == 0);
	CHECK(sched_prepare(&procs[i], unused_entry, &called) ==
	      -SYS_ENOMEM);
	for (i = 0; i < PIPELINE_MAX; i++)
		sched_release(&procs[i]);
	CHECK(called == 0);
	CHECK(sched_prepare(&procs[i], unused_entry, &called) == 0);
	sched_release(&procs[i]);
	CHECK(called == 0);
}

static void check_resources(void)
{
	size_t count;
	int i;

	count = 0;
	for (i = 0; i < NBUF; i++) {
		if (buf[i].b_flags & B_BUSY)
			count++;
	}
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
	CHECK(count == 0);
}
