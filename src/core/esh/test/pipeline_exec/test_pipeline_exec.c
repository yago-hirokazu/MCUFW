#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"
#include "exe.h"
#include "stdio_adapt.h"
#include "script.h"
#include "par.h"
#include "lex.h"

extern int echo(int argc, char **argv);

#define TEST_DATA_SIZE	(ESH_PIPE_BUFFER_SIZE * 8 + 41)

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
static size_t source_count = TEST_DATA_SIZE;

static int producer_main(int argc, char **argv)
{
	esh_ssize_t count;

	(void)argc;
	(void)argv;
	CHECK(esh_write(ESH_FD_STDOUT, NULL, 0) == 0);
	count = esh_write(ESH_FD_STDOUT, source, source_count);
	if (count < 0)
		return (int)count;
	return count == (esh_ssize_t)source_count ? 11 : -ESH_EINVAL;
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
	CHECK(esh_read(ESH_FD_STDIN, NULL, 0) == 0);
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

static int error_main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	return -ESH_EINVAL;
}

static struct applet error_applet = {"error", error_main};
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

static void test_transfer_matrix(void)
{
	const size_t sizes[] = {0, 1, ESH_PIPE_BUFFER_SIZE,
		ESH_PIPE_BUFFER_SIZE + 1, TEST_DATA_SIZE};
	struct proc proc[PROCS];
	size_t s;
	size_t i;
	int stages;
	int repeat;
	int p;

	for (i = 0; i < sizeof(source); i++)
		source[i] = (unsigned char)i;
	for (stages = 2; stages <= PROCS; stages++) {
		for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
			for (repeat = 0; repeat < 10; repeat++) {
				source_count = sizes[s];
				output_count = 0;
				init_command(&proc[0], &producer);
				for (p = 1; p < stages - 1; p++)
					init_command(&proc[p], &filter);
				init_command(&proc[stages - 1], &consumer);
				CHECK(exe_run_pipeline(proc, stages) == 37);
				CHECK(output_count == source_count);
				for (i = 0; i < source_count; i++)
					CHECK(output[i] == (stages == 2 ?
					      source[i] : toupper(source[i])));
				CHECK(esh_file_in_use() == 0);
				CHECK(esh_pipe_in_use() == 0);
			}
		}
	}
	source_count = sizeof(source);
	init_command(&proc[0], &producer);
	init_command(&proc[1], &error_applet);
	CHECK(exe_run_pipeline(proc, 2) == -ESH_EINVAL);
	CHECK(proc[0].p_exec.ep_status == -ESH_EPIPE);
	init_command(&proc[0], &producer);
	init_command(&proc[1], &error_applet);
	init_command(&proc[2], &consumer);
	output_count = 0;
	CHECK(exe_run_pipeline(proc, 3) == 37);
	CHECK(proc[0].p_exec.ep_status == -ESH_EPIPE);
	CHECK(output_count == 0);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_executor_bounds(void)
{
	struct code code;
	struct exe executor;
	int i;

	code_init(&code);
	memset(&executor, 0, sizeof(executor));
	CHECK(exe_run_codes(NULL, &executor) == EXIT_FAILURE);
	CHECK(exe_run_codes(&code, NULL) == EXIT_FAILURE);
	code.c_codes = CODES;
	for (i = 0; i < CODES; i++)
		code.c_code[i] = OP_NOP;
	for (i = 0; i < CODES; i++)
		CHECK(exe_run_codes(&code, &executor) == EXIT_SUCCESS);
	CHECK(executor.e_pc == CODES);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	executor.e_pc = 0;
	code.c_codes = 1;
	code.c_code[0] = OP_BLTN_CMD;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_codes = 2;
	code.c_code[1] = 999;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_code[0] = OP_CMD_EXE;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_code[0] = OP_JUMP_IF_FALSE;
	code.c_code[1] = 0;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_code[0] = OP_PUSH_SYMBOL;
	code.c_stab.s_num = 1;
	executor.e_sp = STACKS;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	executor.e_sp = 0;
	code.c_code[0] = 0xff;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
}

static int script_line(const char *text)
{
	char line[TTY_NBUF];

	CHECK(strlen(text) < sizeof(line));
	memset(line, 0, sizeof(line));
	strcpy(line, text);
	return esh_script_line(line);
}

static void test_console_script(void)
{
	esh_script_init();
	CHECK(script_line("echo hello|cat|cat|cat \n") == 0);
	CHECK(script_line("k = 1 \n") == 0);
	CHECK(script_line("while [ $k -le 2 ] \n") == 1);
	CHECK(script_line("do \n") == 1);
	CHECK(script_line("k = ` expr $k + 1 ` \n") == 1);
	CHECK(script_line("done \n") == 1);
	CHECK(script_line("test $k -eq 3 \n") == 0);
	CHECK(script_line("test \" $k \" -eq 3 \n") == 0);
	CHECK(script_line("if [ $k -eq 3 ] \n") == 1);
	CHECK(script_line("then \n") == 1);
	CHECK(script_line("echo \" value $k \"|cat \n") == 1);
	CHECK(script_line("fi \n") == 0);
	CHECK(script_line("echo | \n") == -ESH_EINVAL);
	CHECK(script_line("test $k -eq 3 \n") == 0);
	CHECK(script_line("while [ 1 -eq 1 ] \n") == 1);
	CHECK(script_line("do \n") == 1);
	CHECK(script_line("while [ 1 -eq 1 ] \n") == 1);
	CHECK(script_line("do \n") == 1);
	CHECK(script_line("k = 7 \n") == 1);
	CHECK(script_line("break 2 \n") == 1);
	CHECK(script_line("done \n") == 1);
	CHECK(script_line("done \n") >= 0);
	CHECK(script_line("test $k -eq 7 \n") == 0);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
}

static void test_parser_bounds(void)
{
	struct code code;
	struct token token;
	struct tty_arg args;
	char line[TTY_NBUF];
	char long_symbol[SYMCHARS + 1];
	int i;

	code_init(&code);
	par_init();
	memset(&token, 0, sizeof(token));
	token.t_type = T_BLTN_CMD;
	strcpy(token.t_symbol, "echo");
	CHECK(par_make_code(NULL, &code) == PARINVLD);
	CHECK(par_make_code(&token, NULL) == PARINVLD);
	code.c_codes = CODES;
	CHECK(par_make_code(&token, &code) == PARINVLD);
	CHECK(code.c_codes == CODES);
	for (i = 0; i < CODES; i++)
		CHECK(code.c_code[i] == 0);
	code_init(&code);
	par_init();
	CHECK(par_make_code(&token, &code) == PARONGO);
	token.t_type = T_CMD_ARG;
	strcpy(token.t_symbol, "overflow");
	code.c_stab.s_num = SYMBOLS;
	CHECK(par_make_code(&token, &code) == PARINVLD);
	CHECK(code.c_stab.s_num == SYMBOLS);
	code_init(&code);
	par_init();
	token.t_type = T_VARIABLE;
	code.c_vtab.v_num = SYMBOLS;
	CHECK(par_make_code(&token, &code) == PARINVLD);
	CHECK(code.c_vtab.v_num == SYMBOLS);
	code_init(&code);
	par_init();
	token.t_type = T_IF;
	strcpy(token.t_symbol, "if");
	code.c_ltab.l_num = LABELS;
	CHECK(par_make_code(&token, &code) == PARINVLD);
	CHECK(code.c_ltab.l_num == LABELS);
	memset(line, 'x', sizeof(line));
	CHECK(tokenize(line, &args) == NULL);
	CHECK(tokenize(NULL, &args) == NULL);
	CHECK(tokenize(line, NULL) == NULL);
	CHECK(lex_analyze_token(NULL, &token) == LEXERROR);
	CHECK(lex_analyze_token("echo", NULL) == LEXERROR);
	memset(long_symbol, 'x', sizeof(long_symbol));
	long_symbol[SYMCHARS] = 0;
	CHECK(lex_analyze_token(long_symbol, &token) == LEXERROR);
	esh_script_init();
	for (i = 0; i < MAXNESTS - 1; i++) {
		CHECK(script_line("while [ 1 -eq 1 ] \n") == 1);
		CHECK(script_line("do \n") == 1);
	}
	CHECK(script_line("while [ 1 -eq 1 ] \n") == 1);
	CHECK(script_line("do \n") == -ESH_EINVAL);
	CHECK(script_line("echo recovered|cat \n") == 0);
	esh_script_init();
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

static void blocker_entry(void *arg)
{
	(void)arg;
	CHECK(0);
}

static void test_backend_exhaustion_cleanup(void)
{
	struct esh_proc blockers[ESH_PIPELINE_MAX - 1];
	struct proc proc[2];
	int i;

	for (i = 0; i < ESH_PIPELINE_MAX - 1; i++) {
		esh_proc_init(&blockers[i]);
		CHECK(esh_sched_prepare(&blockers[i], blocker_entry,
					NULL) == 0);
	}
	init_command(&proc[0], &producer);
	init_command(&proc[1], &consumer);
	CHECK(exe_run_pipeline(proc, 2) == -ESH_ENOMEM);
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
	for (i = 0; i < ESH_PIPELINE_MAX - 1; i++)
		esh_sched_release(&blockers[i]);

	output_count = 0;
	CHECK(exe_run_pipeline(proc, 2) == 37);
	CHECK(output_count == sizeof(source));
	CHECK(!memcmp(source, output, sizeof(source)));
	CHECK(esh_file_in_use() == 0);
	CHECK(esh_pipe_in_use() == 0);
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
	test_backend_exhaustion_cleanup();
	test_pipeline_bytecode();
	test_transfer_matrix();
	test_executor_bounds();
	test_console_script();
	test_parser_bounds();
	printf("all tests passed\n");
	return EXIT_SUCCESS;
}
