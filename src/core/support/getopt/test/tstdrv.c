
/* tstdrv.c */

#include "machdep.h"
#include "getopt.h"

#define	STRCHARS	16
#define NARGV		16

enum test_type_t {
	ORIGINAL = 0,
	GOOGLE
} test_type;

struct arg_t {
	int a_argc;
	char a_argv[NARGV][STRCHARS];
};

struct arg_t arg1 = {
	8,
	"test\0",
	"-a\0",
	"-b\0",
	"1\0",
	"-c2\0",
	"--\0",
	"::\0",
	"-dd\0"
};

char opt1[] = ":ab:c:d\0";
char opt2[] = "ab:c:d\0";

#if 1
#define DP(fmt, ...) do {					\
		fprintf(stdout, fmt, ##__VA_ARGS__);		\
		fflush(stdout);					\
	} while (0);
#else
#define DP(fmt, ...)
#endif

static void test_original(int argc, char argv[NARGV][STRCHARS], char *opts);

int main(int argc, char *argv[])
{
	if (argc <= 1)
		test_type = ORIGINAL;
	else {
		if (strncmp(argv[1], "google", STRCHARS) == 0)
			test_type = GOOGLE;
	}
	if (test_type == ORIGINAL) {
		test_original(arg1.a_argc, arg1.a_argv, opt1);
		test_original(arg1.a_argc, arg1.a_argv, opt2);
	}

	exit(EXIT_SUCCESS);
}

static void test_original(int argc, char argv[NARGV][STRCHARS], char *opts)
{
	uint32_t k;
	DP("argc = %d\n", argc);
	for (k=0; k<argc; k++)
		DP("%s ", argv[k]);
	DP("\n");
	DP("opts = %s\n", opts);

	int opt;
	char *av[NARGV];
	for (k=0; k<NARGV; k++)
		av[k] = NULL;
	for (k=0; k<argc; k++)
		av[k] = &argv[k][0];
	optind = 1;
	opterr = 0;
	while ((opt = getopt(argc, av, opts)) != -1) {
		switch (opt) {
		case ':':
		case '?':
			DP("opt=\'%c\'\n",opt);
			break;
		default:
			DP("opt=\'%c\'\n",opt);
			if (optarg != NULL)
				DP("arg=\"%s\"\n",optarg);
			break;
		}
	}

}


/* EOF */

