
/* test_par.c */

#include "machdep.h"
#include "tty_config.h"
#include "code.h"
#include "esh.h"
#include "exe.h"
#include "lex.h"
#include "par.h"
#include "qbyte.h"
#include "symchar.h"
#include "tok.h"
#include "tty.h"


#define NSTR		64

FILE *filein;
FILE *fileout;
char linebuf[TTY_NBUF];
char splited[TTY_NBUF];
struct qbyte aline;
struct token token[TOKENS];
struct code code;
struct exe exe;

#define OUTPUT_TO_FILE	1
#if OUTPUT_TO_FILE

#define DP(fmt, ...) do {					\
		fprintf(fileout, fmt, ##__VA_ARGS__);		\
		fflush(fileout);				\
	} while (0);

static void outs(char *str)
{
	char *c = str;
	while (!is_endofline(*c)) {
		fputc(*c++, fileout);
		fflush(fileout);
	}
	fputc('\n', fileout);
	fflush(fileout);
}

#else

#define DP(fmt, ...) do {					\
		fprintf(stdout, fmt, ##__VA_ARGS__);		\
		fflush(stdout);					\
	} while (0);

static void outs(char *str)
{
	char *c = str;
	while (!is_endofline(*c)) {
		fputc(*c++, stdout);
		fflush(stdout);
	}
	fputc('\n', stdout);
	fflush(stdout);
}

#endif

static void replace_dot_to_underbar(char *str)
{
	char *s=str;
	int k=0;
	while (s[k] != '\0' && k < NSTR) {
		if (s[k] == '.')
			s[k] = '_';
		k++;
	}

	return;
}

static void add_file_extension(char *str)
{
	char *s=str;
	int k=0;
	while (s[k] != '\0') {
		if (k >= NSTR) {
			fprintf(stderr,"%f(): file name too long\n",__func__);
			exit(EXIT_FAILURE);
		}
		k++;
	}
	s[k++] = '.';
	s[k++] = 't';
	s[k++] = 'x';
	s[k++] = 't';

	return;
}


int main(int argc, void *argv[])
{
	struct tty_arg arg;
	struct tty_arg *arg_p=&arg;
	struct token tok;
	char ret=0, c=0, k=0, l=0, chars=0, tokens=0;

	if (argc < 2) {
		fprintf(stdout,"few arguments\n");
		exit(EXIT_FAILURE);
	}

	filein = fopen(argv[1], "r");
	if (filein == NULL) {
		fprintf(stdout,"cannt open %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

#if OUTPUT_TO_FILE
	char fileout_name[NSTR]={0,};
	memset(&fileout_name[0], 0, sizeof(fileout_name));
	fileout_name[0] = 'r';
	fileout_name[1] = 'e';
	fileout_name[2] = 's';
	fileout_name[3] = 'u';
	fileout_name[4] = 'l';
	fileout_name[5] = 't';
	fileout_name[6] = '_';
	strncpy(&fileout_name[7], argv[1], 64-7);
	replace_dot_to_underbar(fileout_name);
	add_file_extension(fileout_name);
	fileout = fopen(fileout_name, "w");
#endif

	DP("Lexically analyzing characters to make tokens\n");
	DP("---------------------------------------------\n");
	tokens=0;
	slex_init();
	memset(&token, 0, sizeof(token));
	memset(&linebuf, 0, sizeof(linebuf));
	qbinit(&aline, &linebuf[0], TTY_NBUF);
	while (c = fgetc(filein)) {
		/*
		 * Capture characters on a line
		 */
		if (c == EOF)
			goto parse;
		if (c == '\0')
			continue;
		chars++;
		ret = scan(c, &aline);

		DP("scan(%c, &aline)=0x%04X\n",c,ret);

		if (!(ret & DELIM_LF))
			continue;

		DP("aline.wp = %d\n", aline.wp);
		DP("Raw    : "); outs(linebuf);

		/*
		 * Make tokens from words
		 */
		memset(&tok, 0, sizeof(tok));
		memset(&arg, 0, sizeof(arg));

		arg_p = tokenize(linebuf, arg_p);

		slex_init();

		for (k=0; k<arg.c; k++) {
			ret = slex_make_token(arg.v[k], &token[tokens]);
			DP("%2d = slex_make_token(%s, &token)\n",
			   ret, arg.v[k]);
			DP("token[%02d].t_type  =%s\n",
			   tokens, token_type_str[token[tokens].t_type]);
			DP("token[%02d].t_symbol=%s\n",
			   tokens, token[tokens].t_symbol);
			tokens++;
		}

		qbreset(&aline, '\0');
	}

parse:
	DP("\n");
	DP("%d tokens were made from %d characters\n",tokens,chars);
	DP("\n");
	DP("Result of lexical analysis\n");
	DP("--------------------------\n");
	for (k=0; k<tokens; k++) {
		DP("token[%02d]: %s,\t%s\n",
		   k, token_type_str[token[k].t_type],
		   symlf(token[k].t_symbol));
	}

	DP("\n");
	DP("Parsing tokens to make byte codes\n");
	DP("---------------------------------\n");

	memset(&code, 0, sizeof(code));

	k=0;
	while (k < tokens || token[k].t_type != T_UNDEF) {
		ret = par_make_code(&token[k], &code);
		k++;
	}
	code.c_code[code.c_codes] = OP_NOP;
	DP("# of codes = %3d\n", code.c_codes);
	k=0;
	while (k <= code.c_codes) {
		if (code.c_code[k] & 0x80) {
			char c;
			c = code.c_code[k];
			c &= ~0x80;
			DP("code[%3d]=%s\n", k, opcode_str[c]);
		} else {
			DP("code[%3d]=%02X\n", k, code.c_code[k]);
		}
		k++;
	}
	if (code.c_vtab.v_num > 0) {
		struct variable_table *vt = &code.c_vtab;
		DP("Variable:\n");
		for (k=0; k < vt->v_num; k++) {
			DP("v_var[%d]: %s = %s\n",
			   k,vt->v_var[k],vt->v_val[k]);
		}
	}
	if (code.c_ltab.l_num > 0) {
		struct label_table *lt = &code.c_ltab;
		DP("Label:\n");
		for (k=0; k < lt->l_num; k++) {
			DP("l_addr[%d] = %3d\n",k,lt->l_addr[k]);
		}
	}
	if (code.c_stab.s_num > 0) {
		struct string_table *st = &code.c_stab;
		DP("String:\n");
		for (k=0; k < st->s_num; k++) {
			DP("s_str[%d] = %s\n",k,st->s_str[k]);
		}
	}

	DP("\n");
	DP("Executing byte codes\n");
	DP("--------------------\n");

	memset(&exe, 0, sizeof(exe));

	while (exe.e_pc < code.c_codes) {
		k = exe.e_pc;
		if (code.c_code[k] & 0x80) {
			char c;
			c = code.c_code[k];
			c &= ~0x80;
			DP("Execute code[%3d]=%s\n", exe.e_pc, opcode_str[c]);
		} else {
			DP("Execute code[%3d]=%02X\n", k, code.c_code[k]);
		}
		exe_run_codes(&code, &exe);
		k = 0;
		DP("exe.e_procs = %d\n", exe.e_procs);
		while (k < exe.e_procs) {
			l = 0;
			DP("exe.e_proc[%d].p_argc = %d\n",
			   k, exe.e_proc[k].p_argc);
			while (l < exe.e_proc[k].p_argc) {
				DP("argv[%2d]=%s\n",
				   l, exe.e_proc[k].p_argv[l]);
				l++;
			}
			k++;
		}
		k = 0;
		while (k < exe.e_sp) {
			DP("stack[%3d]=%02X\n", k, exe.e_stack[k]);
			k++;
		}
		if (code.c_vtab.v_num > 0) {
			struct variable_table *vt = &code.c_vtab;
			for (k=0; k < vt->v_num; k++) {
				DP("variable[%3d]: %s = %s\n",
				   k,vt->v_var[k],vt->v_val[k]);
			}
		}
	}
	k = exe.e_pc;
	if (code.c_code[k] & 0x80) {
		char c;
		c = code.c_code[k];
		c &= ~0x80;
		DP("Execute code[%3d]=%s\n", exe.e_pc, opcode_str[c]);
	} else {
		DP("Execute code[%3d]=%02X\n", k, code.c_code[k]);
	}
	exe_run_codes(&code, &exe);
	k = 0;
	DP("exe.e_procs = %d\n", exe.e_procs);
	while (k < exe.e_procs) {
		l = 0;
		DP("exe.e_proc[%d].p_argc = %d\n",
		   k, exe.e_proc[k].p_argc);
		while (l < exe.e_proc[k].p_argc) {
			DP("argv[%2d]=%s\n",
			   l, exe.e_proc[k].p_argv[l]);
			l++;
		}
		k++;
	}
	k = 0;
	while (k < exe.e_sp) {
		DP("stack[%3d]=%02X\n", k, exe.e_stack[k]);
		k++;
	}
	if (code.c_vtab.v_num > 0) {
		struct variable_table *vt = &code.c_vtab;
		for (k=0; k < vt->v_num; k++) {
			DP("variable[%3d]: %s = %s\n",
			   k,vt->v_var[k],vt->v_val[k]);
		}
	}


	exit(EXIT_SUCCESS);
}

/* EOF */

