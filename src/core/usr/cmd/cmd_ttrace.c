
/* cmd_ttrace.c */

#include "machdep.h"
#include "ttrace.h"

static char * const _usage[] = {
	"ttrt\n",
	"ttrp\n",
	NULL
};

void help_ttr(void)
{
	for (int i=0; _usage[i]!=NULL; i++)
		printf("%s", _usage[i]);
}

int ttrt_main(int argc, char **argv)
{
	static char * const help[] = {
		"ttrt\tTime TRace starT\n",
		"\tA command to check whether time trace works fine\n",
		"\tor not.\n",
		NULL
	};

	int ac, av, c = '\0';
	char *opt;

	// Parse option
	if (argc < 2)
		goto noopt;
	opt = argv[1];
	ac = argc-1;
	av = 1;
	while (ac > 0) {
		if (ac == 0)
			goto exec;
		opt = argv[av];
		while ((c = *opt++) != '-') {
			if (c == '\0')
				goto exec;
		}
		c = *opt;
		if (c == 'h') {
			for (int i=0; help[i]!=NULL; i++)
				printf("%s", help[i]);
			goto out;
		} else {
			printf("invalid option: -%c\n", c);
			goto out;
		}

	}
	goto exec;

noopt:

exec:

	ttrstart();

out:
	return EXIT_SUCCESS;
}

int ttrp_main(int argc, char **argv)
{
	static char * const help[] = {
		"ttrp\tTime TRace stoP\n",
		NULL
	};

	int ac, av, c = '\0';
	char *opt;

	// Parse option
	if (argc < 2)
		goto noopt;
	opt = argv[1];
	ac = argc-1;
	av = 1;
	while (ac > 0) {
		if (ac == 0)
			goto exec;
		opt = argv[av];
		while ((c = *opt++) != '-') {
			if (c == '\0')
				goto exec;
		}
		c = *opt;
		if (c == 'h') {
			for (int i=0; help[i]!=NULL; i++)
				printf("%s", help[i]);
			goto out;
		} else {
			printf("invalid option: -%c\n", c);
			goto out;
		}

	}
	goto exec;

noopt:

exec:
	printf("%d [us]\n", ttrstop());

out:
	return EXIT_SUCCESS;
}

/* EOF */

