/*
 * Prepared test entries => sys_read() / sys_write() => pipe sleep/wakeup
 * Controller => sched_resume() => verify wait state and transferred data
 */

#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "file.h"
#include "inode.h"
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

struct write_arg {
	const unsigned char *data;
	size_t count;
	int fd;
	ptrdiff_t result;
};

struct read_arg {
	unsigned char *data;
	size_t capacity;
	size_t count;
	int fd;
	ptrdiff_t result;
};

static int failures;

static size_t buffer_in_use(void);
static void close_all_locked(struct proc *proc);
static int connect_pipe(struct proc *writer, struct proc *reader,
			int *write_fd, int *read_fd);
static int run_scheduler(struct proc *procs, int count);
static void run_prepared(void (*entry)(void *arg));
static void write_entry(void *arg);
static void read_entry(void *arg);
static void offset_read_entry(void *arg);
static void public_api_entry(void *arg);
static void test_v7_offset(void);
static void peer_close_entry(void *arg);
static void test_blocking_transfer(void);
static void test_reader_close_wakeup(void);
static void test_writer_close_wakeup(void);

int main(void)
{
	run_prepared(public_api_entry);
	test_v7_offset();
	run_prepared(peer_close_entry);
	test_blocking_transfer();
	test_reader_close_wakeup();
	test_writer_close_wakeup();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static size_t buffer_in_use(void)
{
	size_t count;
	int i;

	count = 0;
	for (i = 0; i < NBUF; i++) {
		if (buf[i].b_flags & B_BUSY)
			count++;
	}
	return count;
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

static void write_entry(void *arg)
{
	struct write_arg *writer;

	writer = arg;
	writer->result = sys_write(writer->fd, writer->data, writer->count);
	CHECK(sys_close(writer->fd) == 0);
}

static void read_entry(void *arg)
{
	struct read_arg *reader;
	ptrdiff_t count;

	reader = arg;
	for (;;) {
		count = sys_read(reader->fd, reader->data + reader->count,
				 reader->capacity - reader->count);
		reader->result = count;
		if (count <= 0)
			break;
		reader->count += (size_t)count;
	}
	CHECK(sys_close(reader->fd) == 0);
}

/* Run synchronous API checks from prepared execution context */
static void run_prepared(void (*entry)(void *arg))
{
	struct proc proc;

	binit();
	finit();
	pinit(&proc);
	CHECK(sched_prepare(&proc, entry, NULL) == 0);
	CHECK(sched_resume(&proc) == 0);
	CHECK(proc.p_state == PROC_FINISHED);
	close_all_locked(&proc);
	sched_release(&proc);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
	CHECK(buffer_in_use() == 0);
}

/* Check descriptor mapping and partial reads within active process */
static void public_api_entry(void *arg)
{
	struct proc *proc;
	struct file *rf;
	struct file *wf;
	struct inode *ip;
	char data[4];
	int fd[2];

	(void)arg;
	proc = current_proc();
	CHECK(sys_pipe(fd) == 0);
	rf = getf(proc, fd[0]);
	wf = getf(proc, fd[1]);
	CHECK(rf && rf->f_flag == (FREAD | FPIPE));
	CHECK(wf && wf->f_flag == (FWRITE | FPIPE));
	CHECK(rf->f_inode == wf->f_inode);
	ip = rf->f_inode;
	CHECK(sys_write(fd[1], "abc", 3) == 3);
	CHECK(ip->i_size == 3);
	memset(data, 0, sizeof(data));
	CHECK(sys_read(fd[0], data, 2) == 2);
	CHECK(!memcmp(data, "ab", 2));
	CHECK(rf->f_offset == 2);
	CHECK(ip->i_size == 3);
	CHECK(sys_read(fd[0], data + 2, 1) == 1);
	CHECK(!memcmp(data, "abc", 3));
	CHECK(rf->f_offset == 0);
	CHECK(ip->i_size == 0);
	CHECK(sys_read(NOFILE, data, 1) == -SYS_EBADF);
	CHECK(sys_write(NOFILE, data, 1) == -SYS_EBADF);
	CHECK(sys_close(NOFILE) == -SYS_EBADF);
	close_all_locked(proc);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
	CHECK(buffer_in_use() == 0);
}

/* Pause after partial read so controller can inspect blocked writer */
static void offset_read_entry(void *arg)
{
	struct read_arg *reader;

	reader = arg;
	CHECK(sys_read(reader->fd, reader->data, 1) == 1);
	kernel_lock();
	CHECK(sched_sleep((char *)reader, WAIT_READ) == 0);
	kernel_unlock();
	CHECK(sys_read(reader->fd, reader->data + 1, PIPSIZ - 1) ==
	      PIPSIZ - 1);
	/* Empty pipe now blocks until writer supplies final byte */
	CHECK(sys_read(reader->fd, reader->data + PIPSIZ, 1) == 1);
	CHECK(sys_read(reader->fd, reader->data, 1) == 0);
	CHECK(sys_close(reader->fd) == 0);
}

/* Require full drain before writer can finish oversized request */
static void test_v7_offset(void)
{
	struct write_arg writer;
	struct read_arg reader;
	struct proc procs[2];
	struct file *rf;
	struct inode *ip;
	unsigned char source[PIPSIZ + 1];
	unsigned char output[PIPSIZ + 1];
	int write_fd;
	int read_fd;
	int i;

	for (i = 0; i < PIPSIZ + 1; i++)
		source[i] = (unsigned char)(i * 37 + 11);
	memset(output, 0, sizeof(output));
	binit();
	finit();
	pinit(&procs[0]);
	pinit(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	rf = getf(&procs[1], read_fd);
	ip = rf->f_inode;
	writer.data = source;
	writer.count = sizeof(source);
	writer.fd = write_fd;
	writer.result = -1;
	memset(&reader, 0, sizeof(reader));
	reader.data = output;
	reader.fd = read_fd;
	CHECK(sched_prepare(&procs[0], write_entry, &writer) == 0);
	CHECK(sched_prepare(&procs[1], offset_read_entry, &reader) == 0);
	CHECK(sched_resume(&procs[0]) == 0);
	CHECK(procs[0].p_state == PROC_WAIT_WRITE);
	CHECK(writer.result == -1);
	CHECK(ip->i_size == PIPSIZ);
	CHECK(sched_resume(&procs[1]) == 0);
	CHECK(procs[1].p_state == PROC_WAIT_READ);
	CHECK(rf->f_offset == 1);
	CHECK(ip->i_size == PIPSIZ);
	CHECK(procs[0].p_state == PROC_WAIT_WRITE);
	CHECK(procs[0].p_wchan == (char *)ip + 1);
	kernel_lock();
	sched_wakeup((char *)&reader);
	kernel_unlock();
	CHECK(sched_resume(&procs[1]) == 0);
	CHECK(rf->f_offset == 0);
	CHECK(ip->i_size == 0);
	CHECK(procs[0].p_state == PROC_RUNNABLE);
	CHECK(procs[1].p_state == PROC_WAIT_READ);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(writer.result == PIPSIZ + 1);
	CHECK(memcmp(source, output, sizeof(source)) == 0);
	sched_release(&procs[0]);
	sched_release(&procs[1]);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
	CHECK(buffer_in_use() == 0);
}

/* Check EOF and broken pipe from prepared execution context */
static void peer_close_entry(void *arg)
{
	unsigned char byte;
	int fd[2];

	(void)arg;
	CHECK(sys_pipe(fd) == 0);
	CHECK(sys_close(fd[1]) == 0);
	CHECK(sys_read(fd[0], &byte, 1) == 0);
	CHECK(sys_close(fd[0]) == 0);
	CHECK(sys_pipe(fd) == 0);
	CHECK(sys_close(fd[0]) == 0);
	CHECK(sys_write(fd[1], &byte, 1) == -SYS_EPIPE);
	CHECK(sys_close(fd[1]) == 0);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}

static void test_blocking_transfer(void)
{
	struct write_arg writer;
	struct read_arg reader;
	struct proc procs[2];
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
	pinit(&procs[0]);
	pinit(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	writer.data = source;
	writer.count = sizeof(source);
	writer.fd = write_fd;
	writer.result = -1;
	reader.data = result;
	reader.capacity = sizeof(result);
	reader.count = 0;
	reader.fd = read_fd;
	reader.result = -1;
	CHECK(sched_prepare(&procs[0], write_entry, &writer) == 0);
	CHECK(sched_prepare(&procs[1], read_entry, &reader) == 0);
	CHECK(sched_resume(&procs[1]) == 0);
	CHECK(procs[1].p_state == PROC_WAIT_READ);
	CHECK(run_scheduler(procs, 2) == 0);
	CHECK(writer.result == TEST_DATA_SIZE);
	CHECK(reader.result == 0);
	CHECK(reader.count == TEST_DATA_SIZE);
	CHECK(!memcmp(source, result, sizeof(source)));
	sched_release(&procs[0]);
	sched_release(&procs[1]);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}

static void test_reader_close_wakeup(void)
{
	struct read_arg reader;
	struct proc procs[2];
	unsigned char byte;
	int write_fd;
	int read_fd;

	binit();
	finit();
	pinit(&procs[0]);
	pinit(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	reader.data = &byte;
	reader.capacity = 1;
	reader.count = 0;
	reader.fd = read_fd;
	reader.result = -1;
	CHECK(sched_prepare(&procs[1], read_entry, &reader) == 0);
	CHECK(sched_resume(&procs[1]) == 0);
	CHECK(procs[1].p_state == PROC_WAIT_READ);
	kernel_lock();
	CHECK(fd_close(&procs[0], write_fd) == 0);
	kernel_unlock();
	CHECK(run_scheduler(&procs[1], 1) == 0);
	CHECK(reader.result == 0);
	sched_release(&procs[1]);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}

static void test_writer_close_wakeup(void)
{
	struct write_arg writer;
	struct proc procs[2];
	unsigned char data[PIPSIZ + 1];
	int write_fd;
	int read_fd;

	memset(data, 0x3c, sizeof(data));
	binit();
	finit();
	pinit(&procs[0]);
	pinit(&procs[1]);
	CHECK(connect_pipe(&procs[0], &procs[1], &write_fd, &read_fd) == 0);
	writer.data = data;
	writer.count = sizeof(data);
	writer.fd = write_fd;
	writer.result = -1;
	CHECK(sched_prepare(&procs[0], write_entry, &writer) == 0);
	CHECK(sched_resume(&procs[0]) == 0);
	CHECK(procs[0].p_state == PROC_WAIT_WRITE);
	kernel_lock();
	CHECK(fd_close(&procs[1], read_fd) == 0);
	kernel_unlock();
	CHECK(run_scheduler(&procs[0], 1) == 0);
	CHECK(writer.result == -SYS_EPIPE);
	sched_release(&procs[0]);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}
