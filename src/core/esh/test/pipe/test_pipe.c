/* test_pipe.c */

#include <stdio.h>
#include <string.h>

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"
#include "esh_sched.h"

static int failures;

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

static void setup(struct esh_proc *proc)
{
	esh_file_system_init();
	esh_proc_init(proc);
	esh_sched_set_current_proc(proc);
}

static void teardown(struct esh_proc *proc)
{
	esh_close_all(&proc->ep_fd_table);
	esh_sched_set_current_proc(NULL);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_create_and_transfer(void)
{
	struct esh_proc proc;
	struct esh_file *file;
	char buf[4];
	int fd[2];

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	file = esh_fd_get(&proc.ep_fd_table, fd[0]);
	CHECK(file != NULL);
	CHECK(file->f_direction == ESH_FILE_READ);
	file = esh_fd_get(&proc.ep_fd_table, fd[1]);
	CHECK(file != NULL);
	CHECK(file->f_direction == ESH_FILE_WRITE);
	CHECK(esh_write(fd[1], "abc", 3) == 3);
	memset(buf, 0, sizeof(buf));
	CHECK(esh_read(fd[0], buf, 3) == 3);
	CHECK(memcmp(buf, "abc", 3) == 0);
	teardown(&proc);
}

static void test_wait_states(void)
{
	struct esh_io_status status;
	struct esh_proc proc;
	unsigned char data[ESH_PIPE_BUFFER_SIZE + 1];
	int fd[2];

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	status = esh_pipe_read_nonblock(&proc, fd[0], data, 1);
	CHECK(status.io_result == ESH_IO_WAIT_READ);
	CHECK(esh_read(fd[0], data, 1) == -ESH_EAGAIN);
	memset(data, 0x5a, sizeof(data));
	CHECK(esh_write(fd[1], data, sizeof(data)) ==
	      ESH_PIPE_BUFFER_SIZE);
	status = esh_pipe_write_nonblock(&proc, fd[1], data, 1);
	CHECK(status.io_result == ESH_IO_WAIT_WRITE);
	CHECK(esh_write(fd[1], data, 1) == -ESH_EAGAIN);
	teardown(&proc);
}

static void test_wrap_around(void)
{
	struct esh_proc proc;
	unsigned char first[200];
	unsigned char second[206];
	unsigned char discarded[150];
	unsigned char result[256];
	unsigned char expected[256];
	int fd[2];
	int i;

	for (i = 0; i < 200; i++)
		first[i] = (unsigned char)i;
	for (i = 0; i < 206; i++)
		second[i] = (unsigned char)(255 - i);
	memcpy(expected, first + 150, 50);
	memcpy(expected + 50, second, 206);

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	CHECK(esh_write(fd[1], first, sizeof(first)) == sizeof(first));
	CHECK(esh_read(fd[0], discarded, sizeof(discarded)) ==
	      sizeof(discarded));
	CHECK(esh_write(fd[1], second, sizeof(second)) == sizeof(second));
	CHECK(esh_read(fd[0], result, sizeof(result)) == sizeof(result));
	CHECK(memcmp(result, expected, sizeof(result)) == 0);
	teardown(&proc);
}

static void test_eof(void)
{
	struct esh_io_status status;
	struct esh_proc proc;
	char buf[4];
	int fd[2];

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	CHECK(esh_write(fd[1], "abc", 3) == 3);
	CHECK(esh_close(fd[1]) == 0);
	CHECK(esh_read(fd[0], buf, sizeof(buf)) == 3);
	status = esh_pipe_read_nonblock(&proc, fd[0], buf, 1);
	CHECK(status.io_result == ESH_IO_EOF);
	CHECK(esh_read(fd[0], buf, 1) == 0);
	CHECK(esh_close(fd[0]) == 0);
	teardown(&proc);
}

static void test_broken_pipe(void)
{
	struct esh_io_status status;
	struct esh_proc proc;
	int fd[2];

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	CHECK(esh_close(fd[0]) == 0);
	status = esh_pipe_write_nonblock(&proc, fd[1], "x", 1);
	CHECK(status.io_result == ESH_IO_BROKEN_PIPE);
	CHECK(status.io_error == -ESH_EPIPE);
	CHECK(esh_write(fd[1], "x", 1) == -ESH_EPIPE);
	teardown(&proc);
}

static void test_fd_reference(void)
{
	struct esh_file *read_file;
	struct esh_proc proc;
	int duplicate;
	int fd[2];

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	read_file = esh_fd_get(&proc.ep_fd_table, fd[0]);
	duplicate = esh_fd_install(&proc.ep_fd_table, read_file);
	CHECK(duplicate >= 0);
	CHECK(esh_close(fd[0]) == 0);
	CHECK(esh_write(fd[1], "x", 1) == 1);
	CHECK(esh_close(duplicate) == 0);
	CHECK(esh_write(fd[1], "x", 1) == -ESH_EPIPE);
	teardown(&proc);
}

static void test_invalid_operations(void)
{
	struct esh_proc proc;
	char byte;
	int fd[2];

	setup(&proc);
	CHECK(esh_pipe(fd) == 0);
	CHECK(esh_read(fd[1], &byte, 1) == -ESH_EBADF);
	CHECK(esh_write(fd[0], &byte, 1) == -ESH_EBADF);
	CHECK(esh_read(-1, &byte, 1) == -ESH_EBADF);
	CHECK(esh_close(ESH_OPEN_MAX) == -ESH_EBADF);
	CHECK(esh_read(-1, NULL, 0) == 0);
	CHECK(esh_write(-1, NULL, 0) == 0);
	teardown(&proc);
}

static void test_pipe_exhaustion(void)
{
	struct esh_proc proc;
	int fd[ESH_PIPE_MAX][2];
	int extra[2];
	int i;

	setup(&proc);
	for (i = 0; i < ESH_PIPE_MAX; i++)
		CHECK(esh_pipe(fd[i]) == 0);
	CHECK(esh_pipe(extra) == -ESH_ENFILE);
	CHECK(esh_pipe_in_use() == ESH_PIPE_MAX);
	CHECK(esh_file_in_use() == ESH_PIPE_MAX * 2);
	teardown(&proc);
}

static void test_file_exhaustion(void)
{
	struct esh_pipe_inode *inode;
	struct esh_file *files[ESH_FILE_MAX];
	int i;

	esh_file_system_init();
	inode = esh_pipe_inode_alloc();
	CHECK(inode != NULL);
	for (i = 0; i < ESH_FILE_MAX; i++) {
		files[i] = esh_file_alloc(ESH_FILE_READ, inode);
		CHECK(files[i] != NULL);
	}
	CHECK(esh_file_alloc(ESH_FILE_READ, inode) == NULL);
	for (i = 0; i < ESH_FILE_MAX; i++)
		esh_file_put(files[i]);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_fd_exhaustion_rollback(void)
{
	struct esh_pipe_inode *inode;
	struct esh_file *file;
	struct esh_proc proc;
	int fd[2];
	int i;

	setup(&proc);
	inode = esh_pipe_inode_alloc();
	CHECK(inode != NULL);
	file = esh_file_alloc(ESH_FILE_READ, inode);
	CHECK(file != NULL);
	for (i = 0; i < ESH_OPEN_MAX - 1; i++)
		CHECK(esh_fd_install(&proc.ep_fd_table, file) >= 0);
	esh_file_put(file);
	CHECK(esh_pipe(fd) == -ESH_EMFILE);
	CHECK(esh_file_in_use() == 1);
	CHECK(esh_pipe_in_use() == 1);
	teardown(&proc);
}

int main(void)
{
	test_create_and_transfer();
	test_wait_states();
	test_wrap_around();
	test_eof();
	test_broken_pipe();
	test_fd_reference();
	test_invalid_operations();
	test_pipe_exhaustion();
	test_file_exhaustion();
	test_fd_exhaustion_rollback();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
