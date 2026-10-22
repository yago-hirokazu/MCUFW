/* test_sched.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SCHED_THREAD
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <process.h>
#include <windows.h>
#endif

#include "buf.h"
#include "conf.h"
#include "file.h"
#include "inode.h"
#include "param.h"
#include "pipe.h"
#include "ramdisk.h"
#include "sched.h"
#include "sys_error.h"
#include "user.h"

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

struct io_stage {
	const unsigned char *source;
	unsigned char *output;
	size_t total;
	size_t transferred;
	unsigned int updates;
	int read_fd;
	int write_fd;
	int error;
};

#ifdef SCHED_THREAD
struct native_arg {
	HANDLE start;
	int error;
};
#endif

struct user_wait_arg {
	struct wait_chan *chan;
	int before;
	int after;
};

#ifdef SCHED_THREAD
struct native_user_arg {
	HANDLE ready;
	HANDLE resume;
	int expected;
	int actual;
};
#endif

static int failures;
static void (*saved_ram_strategy)(struct buf *bp);
static unsigned int max_pipe_io;

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
static void check_buffer_lists(void);
static void ram_test_begin(void);
static void ram_test_end(void);
static void locked_ram_strategy(struct buf *bp);
static void make_full_pipes(struct proc *owner, int pipes[NINODE][2]);
static void drain_full_pipes(struct proc *owner, int pipes[NINODE][2]);
static void test_ram_capacity(void);
#if NBUF >= NINODE + 2
static int ram_increment(void);
static unsigned int ram_counter(void);
static void io_stage_entry(void *arg);
static void test_pipeline_ram(void);
#ifdef SCHED_THREAD
static unsigned __stdcall native_ram_entry(void *arg);
static void test_parallel_ram(void);
#endif
#endif

static void user_wait_entry(void *arg);
static void user_peer_entry(void *arg);
static void test_user_isolation(void);
#ifdef SCHED_THREAD
static unsigned __stdcall native_user_entry(void *arg);
static void test_native_user_isolation(void);
#endif

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
	test_user_isolation();
#ifdef SCHED_THREAD
	test_native_user_isolation();
#endif
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
	test_ram_capacity();
#if NBUF >= NINODE + 2
	test_pipeline_ram();
#ifdef SCHED_THREAD
	test_parallel_ram();
#endif
#endif
	printf("resources NBUF=%d BSIZE=%d buf=%zu payload=%zu headers=%zu\n",
	       NBUF, BSIZE, sizeof(struct buf), sizeof(buffers), sizeof(buf));
	printf("resources sentinels=%zu queue=%zu switch=%zu count=%zu\n",
	       (nblkdev + 1) * sizeof(struct buf), sizeof(ramqueue),
	       nblkdev * sizeof(bdevsw[0]), sizeof(nblkdev));
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
	result = readp(proc, getf_for_proc(proc, fd), buf, count);
	kernel_unlock();
	return result;
}

static ptrdiff_t proc_write(int fd, const void *buf, size_t count)
{
	struct proc *proc;
	ptrdiff_t result;

	kernel_lock();
	proc = current_proc();
	result = writep(proc, getf_for_proc(proc, fd), buf, count);
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
	read_file = getf_for_proc(writer, fd[0]);
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
	struct user procs_user[2];
	unsigned char source[TEST_DATA_SIZE];
	unsigned char result[TEST_DATA_SIZE];
	int write_fd;
	int read_fd;
	int i;

	for (i = 0; i < TEST_DATA_SIZE; i++)
		source[i] = (unsigned char)(i * 37);
	memset(result, 0, sizeof(result));
	binit();
	finit();
	proc_init(&procs[0], &procs_user[0]);
	proc_init(&procs[1], &procs_user[1]);
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
	struct user owner_user;
	struct proc sleeper;
	struct user sleeper_user;
	struct inode *ip;
	unsigned char input;
	unsigned char output;
	size_t offset;
	int write_fd;
	int read_fd;

	input = 0x6d;
	output = 0;
	binit();
	finit();
	proc_init(&owner, &owner_user);
	proc_init(&sleeper, &sleeper_user);
	CHECK(connect_pipe(&owner, &sleeper, &write_fd, &read_fd) == 0);
	ip = getf_for_proc(&sleeper, read_fd)->f_inode;
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
	struct user procs_user[2];
	int i;

	memset(&chan, 0, sizeof(chan));
	for (i = 0; i < 2; i++) {
		proc_init(&procs[i], &procs_user[i]);
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
	struct user proc_user;

	memset(&chan, 0, sizeof(chan));
	proc_init(&proc, &proc_user);
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
	struct user proc_user;

	memset(&chan, 0, sizeof(chan));
	proc_init(&proc, &proc_user);
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
	struct user proc_user;

	memset(&chan, 0, sizeof(chan));
	proc_init(&proc, &proc_user);
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
	struct user procs_user[PIPELINE_MAX + 1];
	int called;
	int i;

	called = 0;
	for (i = 0; i < PIPELINE_MAX + 1; i++)
		proc_init(&procs[i], &procs_user[i]);
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
	check_buffer_lists();
}

static void check_buffer_lists(void)
{
	struct buf *head;
	struct buf *bp;
	int seen[NBUF] = { 0 };
	int dev;
	int count;
	int i;

	for (dev = -1; dev < nblkdev; dev++) {
		head = dev < 0 ? &bfreelist : bdevsw[dev].d_tab;
		if (!head)
			continue;
		bp = head->b_forw;
		for (count = 0; bp != head && count < NBUF; count++) {
			CHECK(bp->b_forw->b_back == bp);
			CHECK(bp->b_back->b_forw == bp);
			for (i = 0; i < NBUF && bp != &buf[i]; i++)
				;
			CHECK(i < NBUF);
			if (i < NBUF)
				seen[i]++;
			if (dev < 0)
				CHECK(bp->b_dev == NODEV);
			else if (bp->b_dev != NODEV)
				CHECK(major(bp->b_dev) == (unsigned int)dev);
			bp = bp->b_forw;
		}
		CHECK(bp == head);
		if (dev >= 0 && bdevsw[dev].d_queue) {
			CHECK(bdevsw[dev].d_queue->dq_head == NULL);
			CHECK(bdevsw[dev].d_queue->dq_tail == NULL);
			CHECK(bdevsw[dev].d_queue->dq_active == NULL);
		}
	}
	bp = bfreelist.av_forw;
	for (count = 0; bp != &bfreelist && count < NBUF; count++) {
		CHECK(bp->av_forw->av_back == bp);
		CHECK(bp->av_back->av_forw == bp);
		CHECK(!(bp->b_flags & B_BUSY));
		bp = bp->av_forw;
	}
	CHECK(bp == &bfreelist && count == NBUF);
	for (i = 0; i < NBUF; i++)
		CHECK(seen[i] == 1);
}

static void ram_test_begin(void)
{
	CHECK(current_proc() == NULL);
	CHECK(!kernel_is_locked());
	u.u_error = 0;
	raminit();
	binit();
	finit();
	saved_ram_strategy = bdevsw[RAM_MAJOR].d_strategy;
	bdevsw[RAM_MAJOR].d_strategy = locked_ram_strategy;
	max_pipe_io = 0;
}

static void ram_test_end(void)
{
	CHECK(!kernel_is_locked());
	CHECK(current_proc() == NULL);
	bdevsw[RAM_MAJOR].d_strategy = saved_ram_strategy;
	check_resources();
}

static void locked_ram_strategy(struct buf *bp)
{
	CHECK(kernel_is_locked());
	if (pipe_in_use() == NINODE)
		max_pipe_io++;
	ramstrategy(bp);
}

static void make_full_pipes(struct proc *owner, int pipes[NINODE][2])
{
	unsigned char marker;
	int i;

	for (i = 0; i < NINODE; i++) {
		CHECK(pipe_for_proc(owner, pipes[i]) == 0);
		marker = (unsigned char)(0x30 + i);
		CHECK(writep(owner, getf_for_proc(owner, pipes[i][1]),
			     &marker, 1) == 1);
	}
	CHECK(pipe_in_use() == NINODE);
	CHECK(file_in_use() == NINODE * 2);
}

static void drain_full_pipes(struct proc *owner, int pipes[NINODE][2])
{
	unsigned char marker;
	int i;

	for (i = 0; i < NINODE; i++) {
		marker = 0;
		CHECK(readp(owner, getf_for_proc(owner, pipes[i][0]),
			    &marker, 1) == 1);
		CHECK(marker == (unsigned char)(0x30 + i));
	}
	close_all(owner);
}

static void test_ram_capacity(void)
{
	struct proc owner;
	struct user owner_user;
	struct buf *held[NBUF];
	struct buf *bp;
	int pipes[NINODE][2];
	int fd[2];
	int extra;
	int i;

	ram_test_begin();
	proc_init(&owner, &owner_user);
	kernel_lock();
	make_full_pipes(&owner, pipes);
	extra = NBUF - NINODE;
	for (i = 0; i < extra; i++) {
		held[i] = bread(makedev(RAM_MAJOR, 0), (unsigned long)i);
		CHECK(held[i] != NULL && u.u_error == 0);
		held[i]->b_addr[0] = (unsigned char)(0x70 + i);
		CHECK(kernel_is_locked());
	}
	CHECK(bread(makedev(RAM_MAJOR, 0), NBUF) == NULL);
	CHECK(u.u_error == SYS_EAGAIN);
	if (extra) {
		CHECK(bread(makedev(RAM_MAJOR, 0), 0) == NULL);
		CHECK(u.u_error == SYS_EAGAIN);
	}
	CHECK(pipe_for_proc(&owner, fd) == -SYS_ENFILE);
	CHECK(file_in_use() == NINODE * 2);
	u.u_error = 0;
	for (i = 0; i < extra; i++)
		bdwrite(held[i]);
	if (extra) {
		for (i = 0; i < extra * 2 + 1; i++) {
			bp = bread(makedev(RAM_MAJOR, 1), (unsigned long)i);
			CHECK(bp != NULL && u.u_error == 0);
			bp->b_addr[0] = (unsigned char)i;
			bdwrite(bp);
		}
		bp = getblk(makedev(RAM_MAJOR, 0), RAM_NBLK);
		CHECK(bp != NULL);
		u.u_error = 0;
		bwrite(bp);
		CHECK(u.u_error == SYS_EIO);
		u.u_error = 0;
		bp = bread(makedev(RAM_MAJOR, 0), 0);
		CHECK(bp != NULL && u.u_error == 0 && bp->b_addr[0] == 0x70);
		brelse(bp);
		CHECK(max_pipe_io > 0);
	}
	bflush(NODEV);
	drain_full_pipes(&owner, pipes);
	CHECK(kernel_is_locked());
	for (i = 0; i < NBUF; i++) {
		held[i] = getblk(makedev(RAM_MAJOR, 0), (unsigned long)i);
		CHECK(held[i] != NULL);
	}
	CHECK(pipe_for_proc(&owner, fd) == -SYS_ENFILE);
	CHECK(file_in_use() == 0 && inode_in_use() == 0);
	for (i = 0; i < NBUF; i++)
		brelse(held[i]);
	CHECK(pipe_for_proc(&owner, fd) == 0);
	close_all(&owner);
	kernel_unlock();
	ram_test_end();
}

#if NBUF >= NINODE + 2
static int ram_increment(void)
{
	struct buf *bp;
	unsigned int value;
	int error;

	kernel_lock();
	u.u_error = 0;
	bp = bread(makedev(RAM_MAJOR, 0), 0);
	if (bp) {
		value = bp->b_addr[0] | ((unsigned int)bp->b_addr[1] << 8);
		value++;
		bp->b_addr[0] = (unsigned char)value;
		bp->b_addr[1] = (unsigned char)(value >> 8);
#ifdef SCHED_THREAD
		/* Encourage native preemption while update remains protected */
		SwitchToThread();
#endif
		bwrite(bp);
	}
	error = -u.u_error;
	kernel_unlock();
	return error;
}

static unsigned int ram_counter(void)
{
	struct buf *bp;
	unsigned int value;

	value = 0;
	kernel_lock();
	u.u_error = 0;
	bp = bread(makedev(RAM_MAJOR, 0), 0);
	CHECK(bp != NULL && u.u_error == 0);
	if (bp) {
		value = bp->b_addr[0] | ((unsigned int)bp->b_addr[1] << 8);
		brelse(bp);
	}
	kernel_unlock();
	return value;
}

static void io_stage_entry(void *arg)
{
	struct io_stage *stage;
	unsigned char data[37];
	ptrdiff_t count;
	ptrdiff_t written;
	size_t remaining;

	stage = arg;
	for (;;) {
		if (stage->read_fd < 0) {
			remaining = stage->total - stage->transferred;
			if (!remaining)
				break;
			if (remaining > sizeof(data))
				remaining = sizeof(data);
			memcpy(data, stage->source + stage->transferred,
			       remaining);
			count = (ptrdiff_t)remaining;
		} else {
			count = proc_read(stage->read_fd, data, sizeof(data));
			if (count <= 0) {
				stage->error = (int)count;
				break;
			}
		}
		stage->error = ram_increment();
		if (stage->error)
			break;
		stage->updates++;
		if (stage->write_fd >= 0) {
			written = proc_write(stage->write_fd, data,
					     (size_t)count);
			if (written != count) {
				stage->error = -SYS_EIO;
				break;
			}
		} else {
			if (stage->transferred + (size_t)count > stage->total) {
				stage->error = -SYS_EIO;
				break;
			}
			memcpy(stage->output + stage->transferred, data,
			       (size_t)count);
		}
		stage->transferred += (size_t)count;
	}
	if (stage->read_fd >= 0)
		CHECK(proc_close(stage->read_fd) == 0);
	if (stage->write_fd >= 0)
		CHECK(proc_close(stage->write_fd) == 0);
}

static void test_pipeline_ram(void)
{
	struct proc procs[PIPELINE_MAX];
	struct user procs_user[PIPELINE_MAX];
	struct io_stage stages[PIPELINE_MAX];
	unsigned char source[TEST_DATA_SIZE];
	unsigned char output[TEST_DATA_SIZE];
	unsigned int updates;
	int prepared;
	int i;

	ram_test_begin();
	memset(stages, 0, sizeof(stages));
	memset(output, 0, sizeof(output));
	for (i = 0; i < TEST_DATA_SIZE; i++)
		source[i] = (unsigned char)(i * 37);
	for (i = 0; i < PIPELINE_MAX; i++) {
		proc_init(&procs[i], &procs_user[i]);
		stages[i].source = source;
		stages[i].output = output;
		stages[i].total = sizeof(source);
		stages[i].read_fd = -1;
		stages[i].write_fd = -1;
	}
	prepared = 0;
	for (i = 0; i < PIPELINE_MAX - 1; i++) {
		if (connect_pipe(&procs[i], &procs[i + 1],
				 &stages[i].write_fd, &stages[i + 1].read_fd)) {
			CHECK(0);
			goto out;
		}
	}
	CHECK(pipe_in_use() == NINODE);
	for (i = 0; i < PIPELINE_MAX; i++) {
		if (sched_prepare(&procs[i], io_stage_entry, &stages[i])) {
			CHECK(0);
			goto out;
		}
		prepared++;
	}
	CHECK(run_scheduler(procs, PIPELINE_MAX) == 0);
	updates = 0;
	for (i = 0; i < PIPELINE_MAX; i++) {
		CHECK(stages[i].error == 0);
		CHECK(stages[i].transferred == sizeof(source));
		updates += stages[i].updates;
	}
	CHECK(max_pipe_io > 0);
	CHECK(ram_counter() == updates);
	CHECK(memcmp(source, output, sizeof(source)) == 0);
out:
	for (i = 0; i < PIPELINE_MAX; i++)
		close_all_locked(&procs[i]);
	for (i = 0; i < prepared; i++)
		sched_release(&procs[i]);
	ram_test_end();
}

#ifdef SCHED_THREAD
static unsigned __stdcall native_ram_entry(void *arg)
{
	struct native_arg *worker;
	int i;

	worker = arg;
	if (current_proc() ||
	    WaitForSingleObject(worker->start, 20000) != WAIT_OBJECT_0) {
		worker->error = -SYS_EINVAL;
		return 1;
	}
	for (i = 0; i < 500; i++) {
		worker->error = ram_increment();
		if (worker->error)
			return 1;
	}
	return 0;
}

static void test_parallel_ram(void)
{
	struct proc owner;
	struct user owner_user;
	struct native_arg workers[2];
	HANDLE threads[2];
	HANDLE start;
	DWORD result;
	int pipes[NINODE][2];
	int created;
	int i;

	ram_test_begin();
	proc_init(&owner, &owner_user);
	kernel_lock();
	make_full_pipes(&owner, pipes);
	kernel_unlock();
	start = CreateEvent(NULL, TRUE, FALSE, NULL);
	CHECK(start != NULL);
	created = 0;
	if (start) {
		for (i = 0; i < 2; i++) {
			workers[i].start = start;
			workers[i].error = 0;
			threads[i] = (HANDLE)_beginthreadex(NULL, 0,
					native_ram_entry, &workers[i], 0, NULL);
			if (!threads[i])
				break;
			created++;
		}
		CHECK(SetEvent(start));
		if (created) {
			result = WaitForMultipleObjects((DWORD)created, threads,
						      TRUE, 20000);
			CHECK(result == WAIT_OBJECT_0);
			if (result != WAIT_OBJECT_0)
				exit(1);
		}
		for (i = 0; i < created; i++) {
			CHECK(workers[i].error == 0);
			CHECK(CloseHandle(threads[i]));
		}
		CHECK(CloseHandle(start));
	}
	CHECK(created == 2);
	CHECK(ram_counter() == (unsigned int)(created * 500));
	CHECK(max_pipe_io > 0);
	kernel_lock();
	drain_full_pipes(&owner, pipes);
	kernel_unlock();
	ram_test_end();
}
#endif
#endif

/* Retain originating user error across scheduler wait and resume */
static void user_wait_entry(void *arg)
{
	struct user_wait_arg *waiter = arg;
	struct buf *bp;

	kernel_lock();
	CHECK(current_user() == current_proc()->p_addr);
	CHECK(u.u_procp == current_proc());
	CHECK(u.u_ofile[0] != NULL && u.u_ofile[0]->f_offset == 1);
	u.u_error = 0;
	bp = bread(makedev(RAM_MAJOR, 0), RAM_NBLK);
	CHECK(bp != NULL);
	waiter->before = u.u_error;
	u.u_r.r_val1 = 101;
	u.u_r.r_val2 = 202;
	brelse(bp);
	CHECK(sched_sleep(waiter->chan, WAIT_READ) == 0);
	CHECK(current_user() == current_proc()->p_addr);
	CHECK(u.u_procp == current_proc());
	CHECK(u.u_ofile[0] != NULL && u.u_ofile[0]->f_offset == 1);
	CHECK(u.u_r.r_val1 == 101 && u.u_r.r_val2 == 202);
	waiter->after = u.u_error;
	kernel_unlock();
}

/* Successful peer operation must not clear sleeping user's error */
static void user_peer_entry(void *arg)
{
	struct wait_chan *chan = arg;
	struct buf *bp;

	kernel_lock();
	CHECK(u.u_error == 0);
	CHECK(u.u_procp == current_proc());
	CHECK(u.u_ofile[0] != NULL && u.u_ofile[0]->f_offset == 2);
	CHECK(ufalloc() == 1 && u.u_r.r_val1 == 1);
	u.u_r.r_val2 = 303;
	bp = bread(makedev(RAM_MAJOR, 0), 0);
	CHECK(bp != NULL && u.u_error == 0);
	brelse(bp);
	sched_wakeup(chan);
	kernel_unlock();
}

/* Host and worker users remain distinct through wait, join and reuse */
static void test_user_isolation(void)
{
	struct proc procs[2];
	struct user users[2];
	struct file *files[2];
	struct user *host;
	struct wait_chan chan = { 0 };
	struct user_wait_arg waiter = { &chan, 0, 0 };
	int fd;
	int i;
	int j;

	raminit();
	binit();
	finit();
	host = current_user();
	CHECK(host->u_procp == NULL);
	for (j = 0; j < NOFILE; j++)
		CHECK(host->u_ofile[j] == NULL);
	host->u_error = SYS_EINVAL;
	for (i = 0; i < 2; i++) {
		users[i].u_error = SYS_EPIPE;
		users[i].u_ofile[NOFILE - 1] = &file[0];
		proc_init(&procs[i], &users[i]);
		CHECK(procs[i].p_addr == &users[i]);
		CHECK(users[i].u_procp == &procs[i]);
		CHECK(users[i].u_error == 0);
		CHECK(procs[i].p_addr != host);
		for (j = 0; j < NOFILE; j++)
			CHECK(users[i].u_ofile[j] == NULL);
		kernel_lock();
		files[i] = allocate_file(&procs[i], &fd);
		CHECK(files[i] != NULL && fd == 0);
		if (files[i])
			files[i]->f_offset = (size_t)i + 1;
		kernel_unlock();
	}
	CHECK(files[0] != files[1]);
	CHECK(sched_prepare(&procs[0], user_wait_entry, &waiter) == 0);
	CHECK(sched_prepare(&procs[1], user_peer_entry, &chan) == 0);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(waiter.before == SYS_EIO && waiter.after == SYS_EIO);
	CHECK(users[0].u_error == SYS_EIO && users[1].u_error == 0);
	CHECK(current_user() == host && host->u_error == SYS_EINVAL);
	kernel_lock();
	for (i = 0; i < 2; i++)
		close_all(&procs[i]);
	kernel_unlock();
	for (i = 0; i < 2; i++)
		sched_release(&procs[i]);
	CHECK(users[0].u_error == SYS_EIO);
	CHECK(users[0].u_procp == &procs[0]);
	proc_init(&procs[0], &users[0]);
	CHECK(users[0].u_error == 0);
	CHECK(users[0].u_procp == &procs[0]);
	for (j = 0; j < NOFILE; j++)
		CHECK(users[0].u_ofile[j] == NULL);
	host->u_error = 0;
	check_resources();
}

#ifdef SCHED_THREAD
/* Leave error live across host thread synchronization */
static unsigned __stdcall native_user_entry(void *arg)
{
	struct native_user_arg *worker = arg;

	kernel_lock();
	if (current_proc() || u.u_error || u.u_procp) {
		kernel_unlock();
		return 1;
	}
	u.u_error = worker->expected;
	kernel_unlock();
	SetEvent(worker->ready);
	if (WaitForSingleObject(worker->resume, 20000) != WAIT_OBJECT_0)
		return 1;
	kernel_lock();
	worker->actual = u.u_error;
	kernel_unlock();
	return 0;
}

/* Unbound native threads and host each retain dedicated base user */
static void test_native_user_isolation(void)
{
	struct native_user_arg workers[2];
	HANDLE threads[2];
	HANDLE ready[2];
	HANDLE resume;
	DWORD result;
	int i;

	u.u_error = SYS_EINVAL;
	resume = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!resume)
		exit(1);
	for (i = 0; i < 2; i++) {
		ready[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!ready[i])
			exit(1);
		workers[i].ready = ready[i];
		workers[i].resume = resume;
		workers[i].expected = i ? SYS_EPIPE : SYS_EIO;
		workers[i].actual = 0;
		threads[i] = (HANDLE)_beginthreadex(NULL, 0, native_user_entry,
						 &workers[i], 0, NULL);
		if (!threads[i])
			exit(1);
	}
	result = WaitForMultipleObjects(2, ready, TRUE, 20000);
	if (result != WAIT_OBJECT_0)
		exit(1);
	CHECK(u.u_error == SYS_EINVAL);
	CHECK(SetEvent(resume));
	result = WaitForMultipleObjects(2, threads, TRUE, 20000);
	if (result != WAIT_OBJECT_0)
		exit(1);
	for (i = 0; i < 2; i++) {
		CHECK(workers[i].actual == workers[i].expected);
		CHECK(CloseHandle(threads[i]));
		CHECK(CloseHandle(ready[i]));
	}
	CHECK(CloseHandle(resume));
	CHECK(u.u_error == SYS_EINVAL);
	u.u_error = 0;
}
#endif
