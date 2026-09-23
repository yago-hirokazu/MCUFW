
/* expr.c */

#include "machdep.h"

int expr_argp;

static int expr1(int argc, char *argv[], int *argp);
static int expr2(int argc, char *argv[], int *argp);
static int expr3(int argc, char *argv[], int *argp);
static int expr4(int argc, char *argv[], int *argp);
static int factor(int argc, char *argv[], int *argp);
static int find_closing_bracket(int argc, char *argv[], int argp);

#if DEBUG_EXPR
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt "\n",	\
	       __FNAME__,__func__,__LINE__,	\
	       ##__VA_ARGS__			\
		)
#define TR(cc, vv, pp) do {\
	int tr=0;\
	DP("argc=%d",cc);\
	DP("argp=%d",pp);\
	for (tr=0; tr<cc; tr++)\
		DP("argv[%d]=\"%s\"",tr,vv[tr]);\
	} while (0)
#else
#define DP(fmt, ...)
#define TR(cc, vv, pp)
#endif

int expr(int argc, char *argv[])
{
	TR(argc,argv,NULL);

	expr_argp = 1;
	return expr1(argc, argv, &expr_argp);
}

/*
 * expr1 := expr2 '|' expr2
 *        | expr2 '&' expr2
 *        | e
 *        ;
 */
static int expr1(int argc, char *argv[], int *argp)
{
	int left=0, right=0, result=0, p=*argp;
	char *s=NULL;

	TR(argc,argv,*argp);

	left = expr2(argc, argv, &p);
	result = left;
	DP("left=%d",left);
	while (p < argc) {
		s = argv[p];
		DP("argv[%d]=\"%s\"",p,s);
		if (!strcmp(s, "|")) {
			p++;
			right = expr2(argc, argv, &p);
			DP("right=%d",right);
		        result = (left | right);
			DP("%d | %d = %d",left,right,result);
			break;
		} else if (!strcmp(s, "&")) {
			p++;
			right = expr2(argc, argv, &p);
			DP("right=%d",right);
			result = (left & right);
			DP("%d & %d = %d",left,right,result);
			break;
		} else { /* empty */
			result = left;
			break;
		}
	}
	*argp = p;
	return result;
}

/*
 * expr2 := expr3 '<' expr3
 *        | expr3 '<=' expr3
 *        | expr3 '=' expr3
 *        | expr3 '!=' expr3
 *        | expr3 '>=' expr3
 *        | expr3 '>' expr3
 *        | e
 *        ;
 */
static int expr2(int argc, char *argv[], int *argp)
{
	int left=0, right=0, p=*argp, result=0;
	char *s=NULL;

	TR(argc,argv,p);

	left = expr3(argc, argv, &p);
	result = left;
	DP("left=%d",left);
	while (p < argc) {
		s = argv[p];
		DP("argv[%d]=\"%s\"",p,s);
		if (!strcmp(s, "<")) {
			p++;
			right = expr3(argc, argv, &p);
			DP("right=%d",right);
		        result = (left < right);
			DP("%d < %d = %d",left,right,result);
			break;
		} else if (!strcmp(s, "<=")) {
			p++;
			right = expr3(argc, argv, &p);
			DP("right=%d",right);
			result = (left <= right);
			DP("%d <= %d = %d",left,right,result);
			break;
		} else if (!strcmp(s, "=")) {
			p++;
			right = expr3(argc, argv, &p);
			DP("right=%d",right);
			result = (left == right);
			DP("%d == %d = %d",left,right,result);
			break;
		} else if (!strcmp(s, "!=")) {
			p++;
			right = expr3(argc, argv, &p);
			DP("right=%d",right);
			result = (left != right);
			DP("%d != %d = %d",left,right,result);
			break;
		} else if (!strcmp(s, ">")) {
			p++;
			right = expr3(argc, argv, &p);
			DP("right=%d",right);
			result = (left > right);
			DP("%d > %d = %d",left,right,result);
			break;
		} else if (!strcmp(s, ">=")) {
			p++;
			right = expr3(argc, argv, &p);
			DP("right=%d",right);
			result = (left >= right);
			DP("%d >= %d = %d",left,right,result);
			break;
		} else { /* empty */
			DP("%s: empty",s,0);
			result = left;
			break;
		}
	}
	*argp = p;
	return result;
}

/*
 * expr3 := expr4 '+' expr4
 *        | expr4 '-' expr4
 *        | e
 *        ;
 */
static int expr3(int argc, char *argv[], int *argp)
{
	int left=0, right=0, result=0, p=*argp, c=0;

	TR(argc,argv,p);

	left = expr4(argc, argv, &p);
	result = left;
	DP("left=%d",left);
	while (p < argc) {
		c = argv[p][0];
		DP("argv[%d][0]=%c",p,c);
		if (c == '+') {
			p++;
			right = expr4(argc, argv, &p);
			DP("right=%d",right);
			result = left + right;
			DP("%d + %d = %d",left,right,result);
			break;
		} else if (c == '-') {
			p++;
			right = expr4(argc, argv, &p);
			DP("right=%d",right);
			result = left - right;
			DP("%d - %d = %d",left,right,result);
			break;
		} else { /* empty */
			DP("%c: empty",c,0);
			result = left;
			break;
		}
	}
	*argp = p;
	return result;
}

/*
 * expr4 := factor '*' factor
 *        | factor '/' factor
 *        | factor '%' factor
 *        | e
 *        ;
 */
static int expr4(int argc, char *argv[], int *argp)
{
	int left=0, right=0, result=0, c=0, p=*argp;

	TR(argc,argv,*argp);

	left = factor(argc, argv, &p);
	result = left;
	DP("left=%d",left);
	while (p < argc) {
		c = argv[p][0];
		DP("argv[%d][0]='%c'",p,c);
		if (c == '*') {
			p++;
			right = factor(argc, argv, &p);
			DP("right=%d",right);
			result = left * right;
			DP("%d * %d = %d",left,right,result);
			break;
		} else if (c == '/') {
			p++;
			right = factor(argc, argv, &p);
			DP("right=%d",right);
			result = left / right;
			DP("%d / %d = %d",left,right,result);
			break;
		} else if (c == '%') {
			p++;
			right = factor(argc, argv, &p);
			DP("right=%d",right);
			result = left % right;
			DP("%d %% %d = %d",left,right,result);
			break;
		} else { /* empty */
			DP("%c: empty",c,0);
			result = left;
			break;
		}
	}
	*argp = p;
	return result;
}

/*
 * factor := NUMBER
 *         | '(' expr ')'
 *         | e
 *         ;
 */
static int factor(int argc, char *argv[], int *argp)
{
	int c=0, res=0, p=*argp;

	TR(argc,argv,p);

	while (p < argc) {
		c = argv[p][0];
		DP("argv[%d][0]='%c'",p,c);
		if (c == '(') {
			p++;
			if (find_closing_bracket(argc, argv, p)) {
				DP("( ... )",0);
				res = expr1(argc, argv, &p);
			}
		} else if (isdigit(c)) {
			res = strtol(argv[p],NULL,0);
			DP("argv[%d]=%s : res=%d",p,argv[p],res);
			p++;
			break;
		} else { /* empty */
			DP("%c: empty",c,0);
			break;
		}
	}
	*argp = p;
	return res;
}

static int find_closing_bracket(int argc, char *argv[], int argp)
{
	int p = argp, c=0;
	while (p < argc) {
		c = argv[p++][0];
		if (c == ')')
			return 1;
	}
	return 0;
}

/* EOF */

