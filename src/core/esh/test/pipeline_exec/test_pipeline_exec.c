/* test_pipeline_exec.c */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "esh_errno.h"
#include "exe.h"
#include "file.h"
#include "lex.h"
#include "param.h"
#include "par.h"
#include "pipe.h"
#include "sched.h"
#include "script.h"
#include "stdio_adapt.h"
#include "sys_error.h"

#define TEST_DATA_SIZE	(PIPSIZ * 8 + 41)

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

static size_t busy_buffer_count(void);
static void check_resources(void);
static int producer_main(int argc, char **argv);
static int filter_main(int argc, char **argv);
static int consumer_main(int argc, char **argv);
static int early_filter_main(int argc, char **argv);
static int error_main(int argc, char **argv);
static int stdio_producer_main(int argc, char **argv);
static int getchar_consumer_main(int argc, char **argv);
static void init_command(struct exe_proc *proc, struct applet *applet);
static void init_executor_instruction(struct code *code, struct exe *executor,
				      int opcode, int operand);
static void test_three_stage_pipeline(void);
static void test_early_filter_exit(void);
static void test_stdio_pipeline(void);
static void test_invalid_count(void);
static void test_transfer_matrix(void);
static void test_executor_bounds(void);
static int script_line(const char *text);
static void test_console_script(void);
static void test_parser_bounds(void);
static void test_pipe_exhaustion_cleanup(void);
static void blocker_entry(void *arg);
static void test_backend_exhaustion_cleanup(void);
static void test_pipeline_bytecode(void);

extern int echo(int argc, char **argv);

int main(void)
{
	binit();
	finit();
	test_stdio_pipeline();
	check_resources();
	test_three_stage_pipeline();
	check_resources();
	test_early_filter_exit();
	check_resources();
	test_invalid_count();
	check_resources();
	test_pipe_exhaustion_cleanup();
	check_resources();
	test_backend_exhaustion_cleanup();
	check_resources();
	test_pipeline_bytecode();
	check_resources();
	test_transfer_matrix();
	check_resources();
	test_executor_bounds();
	check_resources();
	test_console_script();
	check_resources();
	test_parser_bounds();
	check_resources();
	printf("all tests passed\n");
	return EXIT_SUCCESS;
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
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
	CHECK(busy_buffer_count() == 0);
}

static int producer_main(int argc, char **argv)
{
	ptrdiff_t count;

	(void)argc;
	(void)argv;
	CHECK(sys_write(FD_STDOUT, NULL, 0) == 0);
	count = sys_write(FD_STDOUT, source, source_count);
	if (count < 0)
		return (int)count;
	return count == (ptrdiff_t)source_count ? 11 : -ESH_EINVAL;
}

static int filter_main(int argc, char **argv)
{
	unsigned char buffer[73];
	ptrdiff_t count;
	ptrdiff_t written;
	int i;

	(void)argc;
	(void)argv;
	for (;;) {
		count = sys_read(FD_STDIN, buffer, sizeof(buffer));
		if (count <= 0)
			break;
		for (i = 0; i < count; i++)
			buffer[i] = (unsigned char)toupper(buffer[i]);
		written = sys_write(FD_STDOUT, buffer, (size_t)count);
		if (written != count)
			return written < 0 ? (int)written : -ESH_EINVAL;
	}
	return count < 0 ? (int)count : 22;
}

static int consumer_main(int argc, char **argv)
{
	ptrdiff_t count;

	(void)argc;
	(void)argv;
	CHECK(sys_read(FD_STDIN, NULL, 0) == 0);
	for (;;) {
		count = sys_read(FD_STDIN, output + output_count,
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
	if (sys_read(FD_STDIN, &byte, 1) != 1)
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

static void init_command(struct exe_proc *proc, struct applet *applet)
{
	memset(proc, 0, sizeof(*proc));
	proc->p_cmd = applet;
	proc->p_argc = 1;
	proc->p_argv[0] = (char *)applet->name;
}

static void init_executor_instruction(struct code *code, struct exe *executor,
				      int opcode, int operand)
{
	code_init(code);
	memset(executor, 0, sizeof(*executor));
	code->c_code[0] = opcode;
	code->c_code[1] = operand;
	code->c_codes = 2;
}

static void test_three_stage_pipeline(void)
{
	struct exe_proc proc[3];
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
		CHECK(proc[0].p_exec.p_status == 11);
		CHECK(proc[1].p_exec.p_status == 22);
		CHECK(proc[2].p_exec.p_status == 37);
		CHECK(output_count == sizeof(source));
		for (i = 0; i < TEST_DATA_SIZE; i++)
			CHECK(output[i] == (unsigned char)toupper(source[i]));
		CHECK(file_in_use() == 0);
		CHECK(pipe_in_use() == 0);
	}
}

static void test_early_filter_exit(void)
{
	struct exe_proc proc[3];

	output_count = 0;
	init_command(&proc[0], &producer);
	init_command(&proc[1], &early_filter);
	init_command(&proc[2], &consumer);
	CHECK(exe_run_pipeline(proc, 3) == 37);
	CHECK(proc[0].p_exec.p_status == -SYS_EPIPE);
	CHECK(proc[1].p_exec.p_status == 23);
	CHECK(proc[2].p_exec.p_status == 37);
	CHECK(output_count == 0);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}

static void test_stdio_pipeline(void)
{
	struct exe_proc proc[2];
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
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}

static void test_invalid_count(void)
{
	struct exe_proc proc[PROCS];

	CHECK(exe_run_pipeline(proc, 1) == -ESH_EINVAL);
	CHECK(exe_run_pipeline(proc, PROCS + 1) == -ESH_EINVAL);
}

static void test_transfer_matrix(void)
{
	const size_t sizes[] = {0, 1, PIPSIZ, PIPSIZ + 1,
		TEST_DATA_SIZE};
	struct exe_proc proc[PROCS];
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
				CHECK(file_in_use() == 0);
				CHECK(pipe_in_use() == 0);
			}
		}
	}
	source_count = sizeof(source);
	init_command(&proc[0], &producer);
	init_command(&proc[1], &error_applet);
	CHECK(exe_run_pipeline(proc, 2) == -ESH_EINVAL);
	CHECK(proc[0].p_exec.p_status == -SYS_EPIPE);
	init_command(&proc[0], &producer);
	init_command(&proc[1], &error_applet);
	init_command(&proc[2], &consumer);
	output_count = 0;
	CHECK(exe_run_pipeline(proc, 3) == 37);
	CHECK(proc[0].p_exec.p_status == -SYS_EPIPE);
	CHECK(output_count == 0);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
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

	init_executor_instruction(&code, &executor, OP_BLTN_CMD, 0);
	code.c_codes = 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_executor_instruction(&code, &executor, OP_BLTN_CMD, 999);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_executor_instruction(&code, &executor, OP_USER_CMD, 999);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_executor_instruction(&code, &executor, OP_BLTN_CMD, C_ECHO);
	executor.e_procs = PROCS;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, OP_PUSH_SYMBOL, -1);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_executor_instruction(&code, &executor, OP_PUSH_SYMBOL, 0);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_stab.s_num = 1;
	memset(code.c_stab.s_str, 'x', SYMCHARS);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, OP_CMD_VARG, 0);
	init_command(&executor.e_proc[0], &echo_applet);
	executor.e_procs = 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_vtab.v_num = 1;
	memset(code.c_vtab.v_val, 'x', SYMCHARS);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, OP_JUMP, 0);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_ltab.l_num = 1;
	code.c_ltab.l_addr[0] = code.c_codes + 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, OP_JUMP_IF_FALSE, 0);
	code.c_ltab.l_num = 1;
	code.c_ltab.l_addr[0] = 0;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_executor_instruction(&code, &executor, OP_PUSH_SYMBOL, 0);
	code.c_stab.s_num = 1;
	code.c_stab.s_str[0] = 0;
	executor.e_sp = STACKS;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, OP_CMD_ARG, 0);
	code.c_stab.s_num = 1;
	code.c_stab.s_str[0] = 0;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_command(&executor.e_proc[0], &echo_applet);
	executor.e_proc[0].p_argc = TTY_NARG;
	executor.e_procs = 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	init_executor_instruction(&code, &executor, OP_CMD_EXE, 0);
	code.c_codes = 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, OP_PIPE_NEXT, 0);
	code.c_codes = 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	executor.e_procs = PROCS;
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);

	init_executor_instruction(&code, &executor, 0xff, 0);
	CHECK(exe_run_codes(&code, &executor) == EXIT_FAILURE);
	code.c_code[0] = OP_NOP;
	code.c_codes = 1;
	CHECK(exe_run_codes(&code, &executor) == EXIT_SUCCESS);
	CHECK(executor.e_pc == 1);
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
	/* Incomplete block retains parser state and returns one */
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
	/* Error reset preserves variable table */
	CHECK(script_line("echo | \n") == -ESH_EINVAL);
	CHECK(script_line("test $k -eq 3 \n") == 0);
	/* Break execution uses loop metadata before script reset */
	CHECK(script_line("while [ 1 -eq 1 ] \n") == 1);
	CHECK(script_line("do \n") == 1);
	CHECK(script_line("while [ 1 -eq 1 ] \n") == 1);
	CHECK(script_line("do \n") == 1);
	CHECK(script_line("k = 7 \n") == 1);
	CHECK(script_line("break 2 \n") == 1);
	CHECK(script_line("done \n") == 1);
	CHECK(script_line("done \n") >= 0);
	CHECK(script_line("test $k -eq 7 \n") == 0);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
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
	struct proc blockers[NINODE - 1];
	struct exe_proc proc[3];
	size_t j;
	int fd[2];
	int i;

	for (i = 0; i < NINODE - 1; i++) {
		proc_init(&blockers[i]);
		CHECK(pipe_for_proc(&blockers[i], fd) == 0);
	}
	init_command(&proc[0], &producer);
	init_command(&proc[1], &filter);
	init_command(&proc[2], &consumer);
	CHECK(exe_run_pipeline(proc, 3) == -SYS_ENFILE);
	CHECK(pipe_in_use() == NINODE - 1);
	CHECK(file_in_use() == (NINODE - 1) * 2);
	for (i = 0; i < NINODE - 1; i++)
		close_all(&blockers[i]);
	CHECK(pipe_in_use() == 0);
	CHECK(file_in_use() == 0);

	output_count = 0;
	CHECK(exe_run_pipeline(proc, 3) == 37);
	CHECK(output_count == source_count);
	for (j = 0; j < source_count; j++)
		CHECK(output[j] == (unsigned char)toupper(source[j]));
	CHECK(pipe_in_use() == 0);
	CHECK(file_in_use() == 0);
}

static void blocker_entry(void *arg)
{
	(void)arg;
	CHECK(0);
}

static void test_backend_exhaustion_cleanup(void)
{
	struct proc blockers[PIPELINE_MAX - 1];
	struct exe_proc proc[2];
	int i;

	for (i = 0; i < PIPELINE_MAX - 1; i++) {
		proc_init(&blockers[i]);
		CHECK(sched_prepare(&blockers[i], blocker_entry, NULL) == 0);
	}
	init_command(&proc[0], &producer);
	init_command(&proc[1], &consumer);
	CHECK(exe_run_pipeline(proc, 2) == -SYS_ENOMEM);
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
	for (i = 0; i < PIPELINE_MAX - 1; i++)
		sched_release(&blockers[i]);

	output_count = 0;
	CHECK(exe_run_pipeline(proc, 2) == 37);
	CHECK(output_count == sizeof(source));
	CHECK(!memcmp(source, output, sizeof(source)));
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
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
	CHECK(file_in_use() == 0);
	CHECK(pipe_in_use() == 0);
}
