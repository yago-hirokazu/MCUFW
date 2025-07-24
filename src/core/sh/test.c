
/* test.c */

#include "machdep.h"
#include "esh_adapt.h"
#include "test.h"

#if DEBUG_TEST
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt "\n",	\
	       __FNAME__,__func__,__LINE__,		\
	       ##__VA_ARGS__				\
		)
#else
#define DP(fmt, ...)
#endif

int	argp;	/* Argument Pointer */

static int equal(char *a, char *b);
static int expr0(int argc, char *argv[], int argp);
static int expr1(int argc, char *argv[], int argp);
static int expr2(int argc, char *argv[], int argp);
static int expr3(int argc, char *argv[], int argp);
static int length(char *str);

int test(int argc, char *argv[])
{
	argp = 1;
	argv[argc] = NULL;
	if (argc <= 1)
		goto error;
	if (expr0(argc, argv, argp))
		return TEST_TRUE;
error:
	return TEST_FALSE;
}

static int equal(char *a, char *b)
{
	DP("a=%s, b=%s",a,b);

	if (a == NULL || b == NULL)
		return 0;
	if (strcmp(a, b) == 0)
		return 1;
	return 0;
}

static int expr0(int argc, char *argv[], int argp)
{
	DP("argc=%d, argv=%s argp=%d",argc,argv[argp],argp);
	int p1;

	p1 = expr1(argc, argv, argp);
	DP("p1 = expr1(argc, argv, argp) = %d;",p1);
	if (equal(argv[argp++], "-o"))
		return (p1 | expr0(argc, argv, argp));
	DP("argp--",0);
	argp--;
	return (p1);
}

static int expr1(int argc, char *argv[], int argp)
{
	DP("argc=%d, argv=%s argp=%d",argc,argv[argp],argp);
	int p1;

	p1 = expr2(argc, argv, argp);
	DP("p1 = expr2(argc, argv, argp) = %d;",p1);
	if (equal(argv[argp++], "-a"))
		return (p1 & expr1(argc, argv, argp));
	DP("argp--",0);
	argp--;
	return (p1);
}

static int expr2(int argc, char *argv[], int argp)
{
	DP("argc=%d, arngv=%s argp=%d",argc,argv[argp],argp);

	if (equal(argv[argp++], "!"))
		return (!expr3(argc, argv, argp));
	DP("argp--",0);
	argp--;
	return (expr3(argc, argv, argp));
}

static int expr3(int argc, char *argv[], int argp)
{
	DP("argc=%d, argv=%s argp=%d",argc,argv[argp],argp);

	int p1;
	char *arg;
	char *p2;
	int int1, int2;

	if (argp >= argc)
		return 0;

	arg = argv[argp++];
	DP("arg=%s",arg);
	if (equal(arg, "(")) {
		p1 = expr0(argc, argv, argp);
		if (!equal(argv[argp++], ")")) {
			DP(") expected", 0);
			return 0;
		}
		return (p1);
	}

	p2 = argv[argp++];
	DP("p2=%s",p2);
	if (p2 == NULL)
		return (!equal(arg, ""));
	if (equal(p2, "="))
		return (equal(arg, argv[argp++]));
	if (equal(p2, "!="))
		return (!equal(arg, argv[argp++]));

	int1 = strtol(arg,NULL,0);
	int2 = strtol(argv[argp++],NULL,0);
	DP("int1=%d %s int2=%d",int1,p2,int2);
	if (equal(p2, "-eq"))
		return (int1 == int2);
	if (equal(p2, "-ne"))
		return (int1 != int2);
	if (equal(p2, "-gt"))
		return (int1 > int2);
	if (equal(p2, "-lt"))
		return (int1 < int2);
	if (equal(p2, "-ge"))
		return (int1 >= int2);
	if (equal(p2, "-le"))
		return (int1 <= int2);

	DP("unknown operator %s", p2);
	return 0;
}

static int length(char *str)
{
	char *s=str;
	int k=0;
	while (*s != '\0' || k < SYMCHARS) {
		s++;
		k++;
	}
	return (s - str - 1);
}

/* EOF */

