/* test_pipeline.c */

#include "tty_config.h"

#include "code.h"
#include "esh.h"
#include "lex.h"
#include "machdep.h"
#include "par.h"
#include "qbyte.h"
#include "tok.h"
#include "tty.h"

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "check failed: %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

extern struct parse parse[MAXNESTS];
static struct qbyte input_line;
static char input_buffer[TTY_NBUF];

static void parser_reset(void);
static int parse_line(char *line, struct code *code);
static int parse_script(const char *script, struct code *code);
static int count_opcode(const struct code *code, enum opcode opcode);
static void test_tokenizer(void);
static void test_two_stage(void);
static void test_three_stage_without_spaces(void);
static void test_quoted_pipe(void);
static void test_variable_argument(void);
static void test_nested_pipeline(void);
static void test_invalid_pipeline(void);

int main(void)
{
	test_tokenizer();
	test_two_stage();
	test_three_stage_without_spaces();
	test_quoted_pipe();
	test_variable_argument();
	test_nested_pipeline();
	test_invalid_pipeline();
	printf("all tests passed\n");
	return EXIT_SUCCESS;
}

static void parser_reset(void)
{
	memset(parse, 0, sizeof(struct parse) * MAXNESTS);
}

static int parse_line(char *line, struct code *code)
{
	struct tty_arg arg;
	struct token token;
	int lex_result;
	int par_result;
	int i;

	memset(&arg, 0, sizeof(arg));
	memset(&token, 0, sizeof(token));
	if (!tokenize(line, &arg))
		return 0;
	lex_init();
	for (i = 0; i < arg.c; i++) {
		lex_result = lex_analyze_token(arg.v[i], &token);
		if (lex_result == LEXERROR)
			return 0;
		if (lex_result == LEXQUOTE)
			continue;
		par_result = par_make_code(&token, code);
		if (par_result == PARINVLD)
			return 0;
	}
	return 1;
}

static int parse_script(const char *script, struct code *code)
{
	size_t i;

	parser_reset();
	code_init(code);
	memset(input_buffer, 0, sizeof(input_buffer));
	qbinit(&input_line, (uint8_t *)input_buffer,
	       sizeof(input_buffer));
	for (i = 0; script[i] != '\0'; i++) {
		if (!(scan(script[i], &input_line) & DELIM_LF))
			continue;
		if (!parse_line(input_buffer, code))
			return 0;
		qbreset(&input_line, '\0');
	}
	return 1;
}

static int count_opcode(const struct code *code, enum opcode opcode)
{
	int count;
	int i;

	count = 0;
	for (i = 0; i < code->c_codes; i++) {
		if (code->c_code[i] == (int)opcode)
			count++;
	}
	return count;
}

static void test_tokenizer(void)
{
	static char line[TTY_NBUF] = "echo one|echo two \n";
	struct tty_arg arg;

	memset(&arg, 0, sizeof(arg));
	CHECK(tokenize(line, &arg) != NULL);
	CHECK(arg.c == 6);
	CHECK(strcmp(arg.v[0], "echo") == 0);
	CHECK(strcmp(arg.v[1], "one") == 0);
	CHECK(strcmp(arg.v[2], "|") == 0);
	CHECK(strcmp(arg.v[3], "echo") == 0);
}

static void test_two_stage(void)
{
	struct code code;

	CHECK(parse_script("echo one | echo two\n", &code));
	CHECK(count_opcode(&code, OP_PIPE_NEXT) == 1);
	CHECK(count_opcode(&code, OP_BLTN_CMD) == 2);
	CHECK(count_opcode(&code, OP_CMD_EXE) == 1);
}

static void test_three_stage_without_spaces(void)
{
	struct code code;

	CHECK(parse_script("echo one|echo two|echo three\n", &code));
	CHECK(count_opcode(&code, OP_PIPE_NEXT) == 2);
	CHECK(count_opcode(&code, OP_BLTN_CMD) == 3);
	CHECK(count_opcode(&code, OP_CMD_EXE) == 1);
}

static void test_quoted_pipe(void)
{
	struct code code;

	CHECK(parse_script("echo \"a|b\" | echo done\n", &code));
	CHECK(count_opcode(&code, OP_PIPE_NEXT) == 1);
	CHECK(code.c_stab.s_num > 0);
	CHECK(strcmp(code.c_stab.s_str, "a|b") == 0);

	CHECK(parse_script("echo 'c|d' | echo done\n", &code));
	CHECK(count_opcode(&code, OP_PIPE_NEXT) == 1);
	CHECK(code.c_stab.s_num > 0);
	CHECK(strcmp(code.c_stab.s_str, "c|d") == 0);
}

static void test_variable_argument(void)
{
	struct code code;

	CHECK(parse_script("v = 1\necho $v|echo done\n", &code));
	CHECK(count_opcode(&code, OP_PIPE_NEXT) == 1);
	CHECK(count_opcode(&code, OP_CMD_VARG) == 1);
}

static void test_nested_pipeline(void)
{
	struct code code;
	const char *script =
		"if [ 1 -eq 1 ]; then\n"
		"echo if|echo body\n"
		"fi\n"
		"while [ 1 -eq 0 ]; do\n"
		"echo while|echo body\n"
		"done\n";

	CHECK(parse_script(script, &code));
	CHECK(count_opcode(&code, OP_PIPE_NEXT) == 2);
}

static void test_invalid_pipeline(void)
{
	struct code code;

	CHECK(!parse_script("| echo one\n", &code));
	CHECK(!parse_script("echo one |\n", &code));
	CHECK(!parse_script("echo one || echo two\n", &code));
}
