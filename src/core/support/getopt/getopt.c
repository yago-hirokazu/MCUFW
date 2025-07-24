
/* getopt.c */

#include "machdep.h"

#if 0
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt,	\
	       __FNAME__,__func__,__LINE__,	\
	       ##__VA_ARGS__			\
		)
#else
#define DP(...)
#endif

int	opterr = 1;	/* determine whether it prints error log or not */
int	optind = 1;	/* index of argv to be processed */
int	optopt;		/* have a option character at detecting errors */
char	*optarg;	/* argument character associating with option */
int	optdel = '\0';	/* delimiter on argument vector */

int	argv_cp = 1;	/* pointer to characters of argument vector */

int getopt(int argc, char * const *argv, const char *opts)
{
	char *opts_cp=NULL; /* pointer to option string */
	int c='\0', ill=0, illp=0;

	DP("argc=%d optind=%d\n", argc, optind);

	/* To skip pointing to '-', initially argv_cp = 1. */
	if (argv_cp == 1) {
		if (argc <= optind || argv[optind][1] == '\0')
			return EOF;
		if (argv[optind][0] != '-' || strcmp(argv[optind], "--") == 0) {
			optind++;
			illp = __LINE__;
			goto illop;
		}
	}

	/*
	 * Check whether argv has characters matching with
	 * the ones of an option string or not.
	 * Although ':' will match with the characters of
	 * the option string, it is not a character to specify
	 * options. So, it is out of a matching candidate.
	 */
	c = argv[optind][argv_cp];
	optopt = c;
	opts_cp = strchr(opts, c);

	DP("optopt=%c\n", optopt);
	DP("opts_cp[0]=%c\n", opts_cp[0]);

	if (opts_cp == NULL || c == ':') {
		if (opterr)
			printf("illegal option -- %c\n", c);

		/* Check e/o argv[optind] */
		if (argv[optind][argv_cp+1] == '\0') {
			optind++;
			argv_cp = 1;
		}
		illp = __LINE__;
		goto illop;
	}

	/*
	 * opts = "a:bc..."
	 *         ^
	 *         |
	 *      opts_cp
	 */

	opts_cp++; /* see below */

	/*
	 * opts = "a:bc..."
	 *          ^
	 *          |
	 *       opts_cp
	 */

	/* Check whether this option has an argument or not */
	if (*opts_cp == ':') {
		/* Handle "-aarg" as "-a arg" */
		if (argv[optind][argv_cp+1] != optdel)
			optarg = &argv[optind][argv_cp+1];
		else {
			/*
			 * cmd -a arg
			 *      ^
			 *      |
			 * argv[optind]
			 */

			optind++; /* see below */

			/*
			 * cmd -a arg
			 *         ^
			 *         |
			 *    argv[optind]
			 */

			if (optind >= argc) {
				argv_cp = 1;
				illp = __LINE__;
				goto illop;
			}
			optarg = argv[optind];
		}
		optind++;
		argv_cp = 1;
	} else { /* this option has no argument */
		/* Check e/o argv[optind] */
		if (argv[optind][argv_cp+1] != '\0') {
			DP("argv[%d][%d]=%c\n",
			   optind,(argv_cp+1),argv[optind][argv_cp+1]);
			ill = 1; /* found illegal option like -aa */
		}
		optind++;
		argv_cp = 1;
		if (ill) {
			illp = __LINE__;
			goto illop;
		}
		optarg = NULL;
	}

	return (optopt);

illop:
	/*
	 * Return ':' if it is the 1st character
	 * of an option string. Otherwise, return
	 * '?'. Not return EOF to continue to parse
	 * later argv.
	 */
	DP("illp @ L%d\n", illp);
	if (*opts == ':')
		return ':';
	return '?';
}

/* EOF */

