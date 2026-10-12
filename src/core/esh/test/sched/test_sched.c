/* test_sched.c */

#include <stdio.h>
#include <string.h>

#ifdef ESH_SCHED_THREAD
#include <process.h>
#include <stdint.h>
#include <windows.h>
#endif

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"
#include "esh_sched.h"

#define TEST_DATA_SIZE	(ESH_PIPE_BUFFER_SIZE * 4 + 37)
#define TEST_PROCS	3
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

struct yield_failure_arg {
	struct esh_wait_chan *chan;
	int result;
	int lock_held;
};

static int failures;

static size_t busy_buffer_count(void);
static void check_resources(void);
static void writer_entry(void *arg);
static void reader_entry(void *arg);
static int run_scheduler(struct esh_proc *procs, int count);
static int connect_pipe(struct esh_proc *writer, struct esh_proc *reader,
			int *write_fd, int *read_fd);
static void init_proc(struct esh_proc *proc, void (*entry)(void *arg),
		      void *arg);
static void test_blocking_transfer(void);
static void close_reader_entry(void *arg);
static void test_broken_pipe_wakeup(void);
static void single_read_entry(void *arg);
static void test_multiple_waiters(void);
static void unused_entry(void *arg);
static void test_prepare_exhaustion(void);
static void test_release_waiter(void);
static void test_wake_then_release(void);
static void yield_failure_entry(void *arg);
static void test_yield_failure_cleanup(void);
#ifdef ESH_SCHED_THREAD
static void race_reader_entry(void *arg);
static unsigned __stdcall race_writer_entry(void *arg);
static void test_native_race_and_handles(void);
#endif

extern void esh_sched_test_fail_next_yield(void);

int main(void)
{
	test_blocking_transfer();
	check_resources();
	test_broken_pipe_wakeup();
	check_resources();
	test_multiple_waiters();
	check_resources();
	test_prepare_exhaustion();
	check_resources();
	test_release_waiter();
	check_resources();
	test_wake_then_release();
	check_resources();
	test_yield_failure_cleanup();
	check_resources();
#ifdef ESH_SCHED_THREAD
	test_native_race_and_handles();
	check_resources();
#endif
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static size_t busy_buffer_count(void)
{
	size_t count;
	size_t i;

	count = 0;
	for (i = 0; i < NBUF; i++) {
		if (buf[i].b_flags & B_BUSY)
			count++;
	}
	return count;
}

static void check_resources(void)
{
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
	CHECK(busy_buffer_count() == 0);
}

static void writer_entry(void *arg)
{
	struct writer_arg *writer;

	writer = arg;
	writer->result = esh_write(writer->fd, writer->data, writer->count);
	CHECK(esh_close(writer->fd) == 0);
}

static void reader_entry(void *arg)
{
	struct reader_arg *reader;
	ptrdiff_t count;

	reader = arg;
	for (;;) {
		count = esh_read(reader->fd, reader->data + reader->count,
				 reader->capacity - reader->count);
		reader->last_result = count;
		if (count <= 0)
			break;
		reader->count += (size_t)count;
	}
	CHECK(esh_close(reader->fd) == 0);
}

static int run_scheduler(struct esh_proc *procs, int count)
{
	int finished;
	int progress;
	int iteration;
	int i;

	for (iteration = 0; iteration < SCHED_LIMIT; iteration++) {
		finished = 0;
		progress = 0;
		for (i = 0; i < count; i++) {
			if (procs[i].ep_state == ESH_PROC_FINISHED) {
				finished++;
				continue;
			}
			if (procs[i].ep_state == ESH_PROC_ERROR)
				return -1;
			if (procs[i].ep_state != ESH_PROC_RUNNABLE)
				continue;
			if (esh_sched_resume(&procs[i]) < 0)
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

static int connect_pipe(struct esh_proc *writer, struct esh_proc *reader,
			int *write_fd, int *read_fd)
{
	struct esh_file *read_file;
	int fd[2];

	if (esh_pipe_for_proc(writer, fd) < 0)
		return -1;
	read_file = esh_fd_get(&writer->ep_fd_table, fd[0]);
	*read_fd = esh_fd_install(&reader->ep_fd_table, read_file);
	if (*read_fd < 0)
		return -1;
	if (esh_fd_close(&writer->ep_fd_table, fd[0]) < 0)
		return -1;
	*write_fd = fd[1];
	return 0;
}

static void init_proc(struct esh_proc *proc, void (*entry)(void *arg),
		      void *arg)
{
	CHECK(esh_sched_prepare(proc, entry, arg) == 0);
}

static void test_blocking_transfer(void)
{
	struct writer_arg writer_arg;
	struct reader_arg reader_arg;
	struct esh_proc procs[2];
	unsigned char source[TEST_DATA_SIZE];
	unsigned char result[TEST_DATA_SIZE];
	int write_fd;
	int read_fd;
	int i;

	for (i = 0; i < TEST_DATA_SIZE; i++)
		source[i] = (unsigned char)(i * 37);
	memset(result, 0, sizeof(result));
	esh_file_system_init();
	esh_proc_init(&procs[0]);
	esh_proc_init(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);

	writer_arg.data = source;
	writer_arg.count = sizeof(source);
	writer_arg.fd = write_fd;
	writer_arg.result = -1;
	reader_arg.data = result;
	reader_arg.capacity = sizeof(result);
	reader_arg.count = 0;
	reader_arg.fd = read_fd;
	reader_arg.last_result = -1;
	init_proc(&procs[0], writer_entry, &writer_arg);
	init_proc(&procs[1], reader_entry, &reader_arg);

	/* Reader starts first and waits for producer */
	CHECK(esh_sched_resume(&procs[1]) == 0);
	CHECK(procs[1].ep_state == ESH_PROC_WAIT_READ);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(writer_arg.result == TEST_DATA_SIZE);
	CHECK(reader_arg.last_result == 0);
	CHECK(reader_arg.count == TEST_DATA_SIZE);
	CHECK(memcmp(source, result, sizeof(source)) == 0);
	esh_sched_release(&procs[0]);
	esh_sched_release(&procs[1]);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void close_reader_entry(void *arg)
{
	struct reader_arg *reader;

	reader = arg;
	reader->last_result = esh_close(reader->fd);
}

static void test_broken_pipe_wakeup(void)
{
	struct writer_arg writer_arg;
	struct reader_arg reader_arg;
	struct esh_proc procs[2];
	unsigned char source[TEST_DATA_SIZE];
	int write_fd;
	int read_fd;

	memset(source, 0x6d, sizeof(source));
	esh_file_system_init();
	esh_proc_init(&procs[0]);
	esh_proc_init(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	writer_arg.data = source;
	writer_arg.count = sizeof(source);
	writer_arg.fd = write_fd;
	writer_arg.result = 0;
	reader_arg.fd = read_fd;
	reader_arg.last_result = -1;
	init_proc(&procs[0], writer_entry, &writer_arg);
	init_proc(&procs[1], close_reader_entry, &reader_arg);

	CHECK(esh_sched_resume(&procs[0]) == 0);
	CHECK(procs[0].ep_state == ESH_PROC_WAIT_WRITE);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(reader_arg.last_result == 0);
	CHECK(writer_arg.result == -ESH_EPIPE);
	esh_sched_release(&procs[0]);
	esh_sched_release(&procs[1]);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void single_read_entry(void *arg)
{
	struct reader_arg *reader;

	reader = arg;
	reader->last_result = esh_read(reader->fd, reader->data, 1);
	if (reader->last_result > 0)
		reader->count = (size_t)reader->last_result;
	CHECK(esh_close(reader->fd) == 0);
}

static void test_multiple_waiters(void)
{
	struct writer_arg writer_arg;
	struct reader_arg reader_arg[2];
	struct esh_pipe_inode *inode;
	struct esh_file *read_file;
	struct esh_file *write_file;
	struct esh_proc procs[TEST_PROCS];
	unsigned char input;
	unsigned char output[2];
	int fd[2];
	int i;

	input = 0x7b;
	memset(output, 0, sizeof(output));
	esh_file_system_init();
	for (i = 0; i < TEST_PROCS; i++)
		esh_proc_init(&procs[i]);
	CHECK(esh_pipe_for_proc(&procs[0], fd) == 0);
	read_file = esh_fd_get(&procs[0].ep_fd_table, fd[0]);
	write_file = esh_fd_get(&procs[0].ep_fd_table, fd[1]);
	inode = write_file->f_inode;
	for (i = 0; i < 2; i++) {
		reader_arg[i].data = &output[i];
		reader_arg[i].capacity = 1;
		reader_arg[i].count = 0;
		reader_arg[i].fd = esh_fd_install(&procs[i + 1].ep_fd_table,
						       read_file);
		reader_arg[i].last_result = -1;
		CHECK(reader_arg[i].fd >= 0);
	}
	CHECK(esh_fd_close(&procs[0].ep_fd_table, fd[0]) == 0);
	writer_arg.data = &input;
	writer_arg.count = 1;
	writer_arg.fd = fd[1];
	writer_arg.result = -1;
	init_proc(&procs[0], writer_entry, &writer_arg);
	for (i = 0; i < 2; i++)
		init_proc(&procs[i + 1], single_read_entry, &reader_arg[i]);

	CHECK(esh_sched_resume(&procs[1]) == 0);
	CHECK(esh_sched_resume(&procs[2]) == 0);
	CHECK(procs[1].ep_state == ESH_PROC_WAIT_READ);
	CHECK(procs[2].ep_state == ESH_PROC_WAIT_READ);

	/* Spurious wakeup must recheck empty condition */
	esh_kernel_lock();
	esh_wakeup(&inode->pi_read_chan);
	esh_kernel_unlock();
	CHECK(esh_sched_resume(&procs[1]) == 0);
	CHECK(procs[1].ep_state == ESH_PROC_WAIT_READ);
	CHECK(esh_sched_resume(&procs[2]) == 0);
	CHECK(procs[2].ep_state == ESH_PROC_WAIT_READ);

	CHECK(run_scheduler(procs, TEST_PROCS) == 0);
	CHECK(writer_arg.result == 1);
	CHECK(reader_arg[0].last_result + reader_arg[1].last_result == 1);
	CHECK(reader_arg[0].count + reader_arg[1].count == 1);
	CHECK(output[0] == input || output[1] == input);
	for (i = 0; i < TEST_PROCS; i++)
		esh_sched_release(&procs[i]);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void unused_entry(void *arg)
{
	int *called = arg;

	(*called)++;
}

static void test_prepare_exhaustion(void)
{
	struct esh_proc procs[ESH_PIPELINE_MAX + 1];
	int called = 0;
	int i;

	for (i = 0; i < ESH_PIPELINE_MAX + 1; i++)
		esh_proc_init(&procs[i]);
	for (i = 0; i < ESH_PIPELINE_MAX; i++)
		CHECK(esh_sched_prepare(&procs[i], unused_entry, &called) == 0);
	CHECK(esh_sched_prepare(&procs[i], unused_entry, &called) ==
	      -ESH_ENOMEM);
	for (i = 0; i < ESH_PIPELINE_MAX; i++)
		esh_sched_release(&procs[i]);
	CHECK(called == 0);
	CHECK(esh_sched_prepare(&procs[i], unused_entry, &called) == 0);
	esh_sched_release(&procs[i]);
	CHECK(called == 0);
}

static void test_release_waiter(void)
{
	struct esh_proc procs[2];
	struct reader_arg reader;
	struct esh_pipe_inode *inode;
	unsigned char byte;
	int write_fd;
	int read_fd;

	esh_file_system_init();
	esh_proc_init(&procs[0]);
	esh_proc_init(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	inode = esh_fd_get(&procs[1].ep_fd_table, read_fd)->f_inode;
	memset(&reader, 0, sizeof(reader));
	reader.data = &byte;
	reader.capacity = 1;
	reader.fd = read_fd;
	init_proc(&procs[1], reader_entry, &reader);
	CHECK(esh_sched_resume(&procs[1]) == 0);
	CHECK(procs[1].ep_state == ESH_PROC_WAIT_READ);
	CHECK(run_scheduler(&procs[1], 1) == -1);
	esh_sched_release(&procs[1]);
	CHECK(inode->pi_read_chan.wc_head == NULL);
	CHECK(inode->pi_read_chan.wc_tail == NULL);
	esh_kernel_lock();
	esh_close_all(&procs[0].ep_fd_table);
	esh_close_all(&procs[1].ep_fd_table);
	esh_kernel_unlock();
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_wake_then_release(void)
{
	struct esh_proc procs[2];
	struct reader_arg reader;
	struct esh_pipe_inode *inode;
	unsigned char byte;
	int write_fd;
	int read_fd;

	esh_file_system_init();
	esh_proc_init(&procs[0]);
	esh_proc_init(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	inode = esh_fd_get(&procs[1].ep_fd_table, read_fd)->f_inode;
	memset(&reader, 0, sizeof(reader));
	reader.data = &byte;
	reader.capacity = 1;
	reader.fd = read_fd;
	init_proc(&procs[1], reader_entry, &reader);
	CHECK(esh_sched_resume(&procs[1]) == 0);
	CHECK(procs[1].ep_state == ESH_PROC_WAIT_READ);
	esh_kernel_lock();
	esh_wakeup(&inode->pi_read_chan);
	esh_kernel_unlock();
	CHECK(procs[1].ep_state == ESH_PROC_RUNNABLE);
	CHECK(inode->pi_read_chan.wc_head == NULL);
	CHECK(inode->pi_read_chan.wc_tail == NULL);
	esh_sched_release(&procs[1]);
	CHECK(procs[1].ep_backend == NULL);
	CHECK(inode->pi_read_chan.wc_head == NULL);
	CHECK(inode->pi_read_chan.wc_tail == NULL);
	esh_kernel_lock();
	esh_close_all(&procs[0].ep_fd_table);
	esh_close_all(&procs[1].ep_fd_table);
	esh_kernel_unlock();
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void yield_failure_entry(void *arg)
{
	struct yield_failure_arg *failure;

	failure = arg;
	esh_kernel_lock();
	failure->result = esh_sleep(failure->chan, ESH_WAIT_READ);
	failure->lock_held = esh_kernel_is_locked();
	esh_kernel_unlock();
}

static void test_yield_failure_cleanup(void)
{
	struct yield_failure_arg failure;
	struct esh_wait_chan chan;
	struct esh_proc proc;

	memset(&chan, 0, sizeof(chan));
	esh_proc_init(&proc);
	failure.chan = &chan;
	failure.result = 0;
	failure.lock_held = 0;
	init_proc(&proc, yield_failure_entry, &failure);
	esh_sched_test_fail_next_yield();
	CHECK(esh_sched_resume(&proc) == 0);
	CHECK(failure.result == -ESH_EINVAL);
	CHECK(failure.lock_held);
	CHECK(chan.wc_head == NULL);
	CHECK(chan.wc_tail == NULL);
	CHECK(proc.ep_wait_chan == NULL);
	CHECK(proc.ep_wait_next == NULL);
	esh_sched_release(&proc);
}

#ifdef ESH_SCHED_THREAD
struct thread_race {
	struct esh_proc *tr_reader;
	struct esh_proc *tr_writer;
	struct esh_pipe_inode *tr_inode;
	HANDLE tr_checked;
	int tr_read_fd;
	int tr_write_fd;
	int tr_reader_ok;
	int tr_writer_ok;
	DWORD tr_reader_id;
};

static void race_reader_entry(void *arg)
{
	struct thread_race *race = arg;
	struct esh_io_status status;
	unsigned char byte = 0;
	int result = 0;

	race->tr_reader_id = GetCurrentThreadId();
	esh_kernel_lock();
	status = esh_pipe_read_nonblock(race->tr_reader, race->tr_read_fd,
					&byte, 1);
	/* Waker contends for lock exactly after empty condition check */
	SetEvent(race->tr_checked);
	while (status.io_result == ESH_IO_WAIT_READ && result == 0) {
		result = esh_sleep(&race->tr_inode->pi_read_chan,
				   ESH_WAIT_READ);
		status = esh_pipe_read_nonblock(race->tr_reader,
						race->tr_read_fd, &byte, 1);
	}
	race->tr_reader_ok = result == 0 && esh_kernel_is_locked() &&
		esh_current_proc() == race->tr_reader &&
		GetCurrentThreadId() == race->tr_reader_id &&
		status.io_count == 1 && byte == 0x6d;
	esh_kernel_unlock();
	esh_close(race->tr_read_fd);
}

static unsigned __stdcall race_writer_entry(void *arg)
{
	struct thread_race *race = arg;
	struct esh_io_status status;
	unsigned char byte = 0x6d;

	if (WaitForSingleObject(race->tr_checked, 5000) != WAIT_OBJECT_0)
		return 1;
	race->tr_writer_ok = esh_current_proc() == NULL &&
		!esh_kernel_is_locked();
	esh_sched_set_current_proc(race->tr_writer);
	esh_kernel_lock();
	status = esh_pipe_write_nonblock(race->tr_writer,
					 race->tr_write_fd, &byte, 1);
	race->tr_writer_ok = race->tr_writer_ok && status.io_count == 1 &&
		esh_current_proc() == race->tr_writer && esh_kernel_is_locked();
	esh_wakeup(&race->tr_inode->pi_read_chan);
	esh_fd_close(&race->tr_writer->ep_fd_table, race->tr_write_fd);
	esh_kernel_unlock();
	esh_sched_set_current_proc(NULL);
	return 0;
}

static void test_native_race_and_handles(void)
{
	struct esh_proc procs[2];
	struct thread_race race;
	DWORD handles_before;
	DWORD handles_after;
	HANDLE writer;
	uintptr_t thread;
	int iteration;

	CHECK(GetProcessHandleCount(GetCurrentProcess(), &handles_before));
	for (iteration = 0; iteration < 100; iteration++) {
		esh_file_system_init();
		esh_proc_init(&procs[0]);
		esh_proc_init(&procs[1]);
		memset(&race, 0, sizeof(race));
		CHECK(connect_pipe(&procs[0], &procs[1], &race.tr_write_fd,
				   &race.tr_read_fd) == 0);
		race.tr_reader = &procs[1];
		race.tr_writer = &procs[0];
		race.tr_inode = esh_fd_get(&procs[1].ep_fd_table,
					   race.tr_read_fd)->f_inode;
		race.tr_checked = CreateEvent(NULL, TRUE, FALSE, NULL);
		CHECK(race.tr_checked != NULL);
		init_proc(&procs[1], race_reader_entry, &race);
		thread = _beginthreadex(NULL, 0, race_writer_entry, &race,
				       0, NULL);
		CHECK(thread != 0);
		writer = (HANDLE)thread;
		CHECK(esh_sched_resume(&procs[1]) == 0);
		CHECK(WaitForSingleObject(writer, 5000) == WAIT_OBJECT_0);
		CloseHandle(writer);
		CHECK(esh_current_proc() == NULL);
		CHECK(run_scheduler(&procs[1], 1) == 0);
		esh_sched_release(&procs[1]);
		CloseHandle(race.tr_checked);
		CHECK(race.tr_reader_ok);
		CHECK(race.tr_writer_ok);
		CHECK(race.tr_reader_id != GetCurrentThreadId());
		CHECK(esh_file_in_use() == 0);
		CHECK(esh_pipe_in_use() == 0);
	}
	CHECK(GetProcessHandleCount(GetCurrentProcess(), &handles_after));
	CHECK(handles_after == handles_before);
}
#endif
