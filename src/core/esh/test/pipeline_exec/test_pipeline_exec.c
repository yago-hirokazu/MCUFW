#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"
#include "exe.h"
#include "stdio_adapt.h"

extern int echo(int argc, char **argv);

#define TEST_DATA_SIZE	(ESH_PIPE_BUFFER_SIZE * 3 + 41)

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "check failed: %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static unsigned char source[TEST_DATA_SIZE];
static unsigned char output[TEST_DATA_SIZE + 2];
static size_t output_count;

static int producer_main(int argc, char **argv)
{
	esh_ssize_t count;

	(void)argc;
	(void)argv;
	count = esh_write(ESH_FD_STDOUT, source, sizeof(source));
	if (count < 0)
		return (int)count;
	return count == sizeof(source) ? 11 : -ESH_EINVAL;
}

static int filter_main(int argc, char **argv)
{
	unsigned char buffer[73];
	esh_ssize_t count;
	esh_ssize_t written;
	int i;

	(void)argc;
	(void)argv;
	for (;;) {
		count = esh_read(ESH_FD_STDIN, buffer, sizeof(buffer));
		if (count <= 0)
			break;
		for (i = 0; i < count; i++)
			buffer[i] = (unsigned char)toupper(buffer[i]);
		written = esh_write(ESH_FD_STDOUT, buffer, (size_t)count);
		if (written != count)
			return written < 0 ? (int)written : -ESH_EINVAL;
	}
	return count < 0 ? (int)count : 22;
}

static int consumer_main(int argc, char **argv)
{
	esh_ssize_t count;

	(void)argc;
	(void)argv;
	for (;;) {
		count = esh_read(ESH_FD_STDIN, output + output_count,
				 sizeof(output) - output_count);
		if (count <= 0)
			break;
		output_count += (size_t)count;
	}
	return count < 0 ? (int)count : 37;
}

static int early_filter_main(int argc, char **argv)
{
	unsigned char byte;

	(void)argc;
	(void)argv;
	if (esh_read(ESH_FD_STDIN, &byte, 1) != 1)
		return -ESH_EINVAL;
	return 23;
}

static struct applet producer = {"producer", producer_main};
static struct applet filter = {"filter", filter_main};
static struct applet consumer = {"consumer", consumer_main};
static struct applet early_filter = {"early", early_filter_main};
static struct applet echo_applet = {"echo", echo};

static int stdio_producer_main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	CHECK(printf("%s", source) == TEST_DATA_SIZE - 1);
	CHECK(putchar('!') == '!');
	CHECK(fprintf(stdout, "%s", "?") == 1);
	fprintf(stderr, "debug console only\n");
	return 0;
}

static int getchar_consumer_main(int argc, char **argv)
{
	int ch;
	int value;

	(void)argc;
	(void)argv;
	CHECK(scanf("%d", &value) == EOF);
	while ((ch = getchar()) != EOF) {
		CHECK(output_count < sizeof(output));
		output[output_count++] = (unsigned char)ch;
	}
	return 37;
}

static struct applet stdio_producer = {"stdio", stdio_producer_main};
static struct applet getchar_consumer = {"getchar", getchar_consumer_main};

static void init_command(struct proc *proc, struct applet *applet)
{
	memset(proc, 0, sizeof(*proc));
	proc->p_cmd = applet;
	proc->p_argc = 1;
	proc->p_argv[0] = (char *)applet->name;
}

static void test_three_stage_pipeline(void)
{
	struct proc proc[3];
	int iteration;
	int i;

	for (i = 0; i < TEST_DATA_SIZE; i++)
		source[i] = (unsigned char)('a' + i % 26);
	for (iteration = 0; iteration < 3; iteration++) {
		memset(output, 0, sizeof(output));
		output_count = 0;
		init_command(&proc[0], &producer);
		init_command(&proc[1], &filter);
		init_command(&proc[2], &consumer);
		CHECK(exe_run_pipeline(proc, 3) == 37);
		CHECK(proc[0].p_exec.ep_status == 11);
		CHECK(proc[1].p_exec.ep_status == 22);
		CHECK(proc[2].p_exec.ep_status == 37);
		CHECK(output_count == sizeof(source));
		for (i = 0; i < TEST_DATA_SIZE; i++)
			CHECK(output[i] == (unsigned char)toupper(source[i]));
		CHECK(esh_file_in_use() == 0);
		CHECK(esh_pipe_in_use() == 0);
	}
}

static void test_early_filter_exit(void)
{
	struct proc proc[3];

	output_count = 0;
	init_command(&proc[0], &producer);
	init_command(&proc[1], &early_filter);
	init_command(&proc[2], &consumer);
	CHECK(exe_run_pipeline(proc, 3) == 37);
	CHECK(proc[0].p_exec.ep_status == -ESH_EPIPE);
	CHECK(proc[1].p_exec.ep_status == 23);
	CHECK(proc[2].p_exec.ep_status == 37);
	CHECK(output_count == 0);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_stdio_pipeline(void)
{
	struct proc proc[2];
	char message[] = "hello pipe";

	output_count = 0;
	init_command(&proc[0], &echo_applet);
	proc[0].p_argc = 2;
	proc[0].p_argv[1] = message;
	init_command(&proc[1], &getchar_consumer);
	CHECK(exe_run_pipeline(proc, 2) == 37);
	CHECK(output_count == strlen(message) + 1);
	CHECK(!memcmp(output, "hello pipe\n", output_count));

	memset(source, 'x', sizeof(source));
	source[sizeof(source) - 1] = 0;
	output_count = 0;
	init_command(&proc[0], &stdio_producer);
	init_command(&proc[1], &getchar_consumer);
	CHECK(exe_run_pipeline(proc, 2) == 37);
	CHECK(output_count == TEST_DATA_SIZE + 1);
	CHECK(!memcmp(output, source, TEST_DATA_SIZE - 1));
	CHECK(output[TEST_DATA_SIZE - 1] == '!');
	CHECK(output[TEST_DATA_SIZE] == '?');
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_invalid_count(void)
{
	struct proc proc[PROCS];

	CHECK(exe_run_pipeline(proc, 1) == -ESH_EINVAL);
	CHECK(exe_run_pipeline(proc, PROCS + 1) == -ESH_EINVAL);
}

static void test_pipe_exhaustion_cleanup(void)
{
	struct esh_proc blocker;
	struct proc proc[PROCS];
	int fd[2];

	esh_proc_init(&blocker);
	CHECK(esh_pipe_for_proc(&blocker, fd) == 0);
	init_command(&proc[0], &producer);
	init_command(&proc[1], &filter);
	init_command(&proc[2], &filter);
	init_command(&proc[3], &consumer);
	CHECK(exe_run_pipeline(proc, PROCS) == -ESH_ENFILE);
	CHECK(esh_pipe_in_use() == 1);
	CHECK(esh_file_in_use() == 2);
	esh_close_all(&blocker.ep_fd_table);
	CHECK(esh_pipe_in_use() == 0);
	CHECK(esh_file_in_use() == 0);
}

static void test_pipeline_bytecode(void)
{
	struct code code;
	struct exe executor;

	code_init(&code);
	memset(&executor, 0, sizeof(executor));
	strcpy(code.c_stab.s_str, "left");
	strcpy(code.c_stab.s_str + SYMCHARS, "right");
	strcpy(code.c_stab.s_str + SYMCHARS * 2, "after");
	code.c_stab.s_num = 3;
	code.c_code[code.c_codes++] = OP_BLTN_CMD;
	code.c_code[code.c_codes++] = C_ECHO;
	code.c_code[code.c_codes++] = OP_CMD_ARG;
	code.c_code[code.c_codes++] = 0;
	code.c_code[code.c_codes++] = OP_PIPE_NEXT;
	code.c_code[code.c_codes++] = OP_BLTN_CMD;
	code.c_code[code.c_codes++] = C_ECHO;
	code.c_code[code.c_codes++] = OP_CMD_ARG;
	code.c_code[code.c_codes++] = 1;
	code.c_code[code.c_codes++] = OP_CMD_EXE;
	code.c_code[code.c_codes++] = OP_BLTN_CMD;
	code.c_code[code.c_codes++] = C_ECHO;
	code.c_code[code.c_codes++] = OP_CMD_ARG;
	code.c_code[code.c_codes++] = 2;
	code.c_code[code.c_codes++] = OP_CMD_EXE;
	while (executor.e_pc < code.c_codes)
		CHECK(exe_run_codes(&code, &executor) == EXIT_SUCCESS);
	CHECK(executor.e_status == EXIT_SUCCESS);
	CHECK(executor.e_procs == 0);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

int main(void)
{
	esh_file_system_init();
	test_stdio_pipeline();
	test_three_stage_pipeline();
	test_early_filter_exit();
	test_invalid_count();
	test_pipe_exhaustion_cleanup();
	test_pipeline_bytecode();
	printf("all tests passed\n");
	return EXIT_SUCCESS;
}
