/* test_sched.c */

#include <stdio.h>
#include <string.h>

#include "cortn.h"
#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"
#include "esh_sched.h"

#define TEST_DATA_SIZE	(ESH_PIPE_BUFFER_SIZE * 4 + 37)
#define TEST_PROCS	3
#define SCHED_LIMIT	1000

static int failures;

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
	esh_ssize_t result;
};

struct reader_arg {
	unsigned char *data;
	size_t capacity;
	size_t count;
	int fd;
	esh_ssize_t last_result;
};

static unsigned char stacks[TEST_PROCS][CORTN_WINDOWS_STACK_SIZE]
	__attribute__((aligned(16)));

static void writer_entry(struct cortn *self, void *arg)
{
	struct writer_arg *writer;

	(void)self;
	writer = arg;
	writer->result = esh_write(writer->fd, writer->data, writer->count);
	CHECK(esh_close(writer->fd) == 0);
}

static void reader_entry(struct cortn *self, void *arg)
{
	struct reader_arg *reader;
	esh_ssize_t count;

	(void)self;
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

static void init_proc(struct esh_proc *proc, struct cortn *cortn,
		      unsigned char *stack, cortn_entry_t entry, void *arg)
{
	CHECK(cortn_init(cortn, stack, CORTN_WINDOWS_STACK_SIZE,
			 entry, arg) == CORTN_SUCCESS);
	CHECK(esh_proc_bind_backend(proc, cortn) == 0);
}

static void test_blocking_transfer(void)
{
	struct writer_arg writer_arg;
	struct reader_arg reader_arg;
	struct esh_proc procs[2];
	struct cortn cortns[2];
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
	CHECK(cortn_init(&cortns[0], stacks[0], sizeof(stacks[0]),
			 writer_entry, &writer_arg) == CORTN_SUCCESS);
	CHECK(cortn_init(&cortns[1], stacks[1], sizeof(stacks[1]),
			 reader_entry, &reader_arg) == CORTN_SUCCESS);
	CHECK(esh_proc_bind_backend(&procs[0], &cortns[0]) == 0);
	CHECK(esh_proc_bind_backend(&procs[1], &cortns[1]) == 0);

	/* Reader starts first and waits for producer */
	CHECK(esh_sched_resume(&procs[1]) == 0);
	CHECK(procs[1].ep_state == ESH_PROC_WAIT_READ);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(writer_arg.result == TEST_DATA_SIZE);
	CHECK(reader_arg.last_result == 0);
	CHECK(reader_arg.count == TEST_DATA_SIZE);
	CHECK(memcmp(source, result, sizeof(source)) == 0);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void close_reader_entry(struct cortn *self, void *arg)
{
	struct reader_arg *reader;

	(void)self;
	reader = arg;
	reader->last_result = esh_close(reader->fd);
}

static void test_broken_pipe_wakeup(void)
{
	struct writer_arg writer_arg;
	struct reader_arg reader_arg;
	struct esh_proc procs[2];
	struct cortn cortns[2];
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
	init_proc(&procs[0], &cortns[0], stacks[0], writer_entry,
		  &writer_arg);
	init_proc(&procs[1], &cortns[1], stacks[1], close_reader_entry,
		  &reader_arg);

	CHECK(esh_sched_resume(&procs[0]) == 0);
	CHECK(procs[0].ep_state == ESH_PROC_WAIT_WRITE);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(reader_arg.last_result == 0);
	CHECK(writer_arg.result == -ESH_EPIPE);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void single_read_entry(struct cortn *self, void *arg)
{
	struct reader_arg *reader;

	(void)self;
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
	struct cortn cortns[TEST_PROCS];
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
	init_proc(&procs[0], &cortns[0], stacks[0], writer_entry,
		  &writer_arg);
	for (i = 0; i < 2; i++)
		init_proc(&procs[i + 1], &cortns[i + 1], stacks[i + 1],
			  single_read_entry, &reader_arg[i]);

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
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

int main(void)
{
	test_blocking_transfer();
	test_broken_pipe_wakeup();
	test_multiple_waiters();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
