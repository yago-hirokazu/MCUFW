
/* test_quote.c */

#include "machdep.h"
#include "cquote.h"


#define NARG	5
#define NARGV	8

#if 1
#define DP(fmt, ...)\
do {\
	fprintf(stdout, "debug @ [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)
#else
#define DP(...)
#endif

struct arg
{
	int a_argc;
	char *a_argv[NARGV];
};

struct arg arg[NARG] = {
	// 0
	{
		5,
		"cmd",
		"'argv1",
		"argv2",
		"argv3",
		"argv4'",
		"\0",
		"\0",
		"\0"
	},
	// 1
	{
		5,
		"cmd",
		"argv1",
		"'argv2",
		"argv3",
		"argv4'",
		"\0",
		"\0",
		"\0"
	},
	// 2
	{
		5,
		"cmd",
		"argv1",
		"argv2",
		"'argv3",
		"argv4'",
		"\0",
		"\0",
		"\0"
	},
	// 3
	{
		5,
		"cmd",
		"argv1",
		"argv2",
		"argv3",
		"'argv4'",
		"\0",
		"\0",
		"\0"
	},
	// 4
	{
		5,
		"cmd",
		"argv1",
		"argv2",
		"'argv3'",
		"argv4",
		"\0",
		"\0",
		"\0"
	},
};


static void dumpargv(int c, char *v[]);

int main(int argc, char *argv[])
{
	int c=0, k=0, l=0;
	char *v[NARGV];
	char str[NARGV][SYMCHARS];

	if (argc > 2) {
		c = argc;
		for (k=0; k<argc; k++)
			v[k] = argv[k];
		dumpargv(c, v);
		qcatargv('\'', &c, v);
		dumpargv(c, v);
		return EXIT_SUCCESS;
	}


	for (k=0; k<NARG; k++) {
		for (l=0; l<NARGV; l++) {
			strncpy(str[l], arg[k].a_argv[l], SYMCHARS);
			v[l] = &str[l][0];
		}
		c = arg[k].a_argc;
		dumpargv(c, v);
		DP("BEFORE qcatargv: c=%d", c);
		qcatargv('\'', &c, v);
		DP("AFTER  qcatargv: c=%d", c);
		dumpargv(c, v);
	}

	return EXIT_SUCCESS;
}

static void dumpargv(int c, char *v[])
{
	for (int k=1; k<c; k++) {
		fprintf(stdout,"argv[%d]=%s\n",k,v[k]);
	}
}

/* EOF */

