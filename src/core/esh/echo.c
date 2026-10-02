
/* echo.c */

#include "machdep.h"

int echo(int argc, char *argv[])
{
	int k=1, opt_n=0;

	opt_n = 0;
	if (argc > 1) {
		if (!strcmp(argv[1], "-n")) {
			opt_n = 1;
			k++;
		}
	}
	for (; k<argc; k++) {
		if (printf("%s", argv[k]) < 0)
			return EXIT_FAILURE;
		if (k < argc-1)
			if (printf(" ") < 0)
				return EXIT_FAILURE;
	}
	if (opt_n == 0)
		if (printf("\n") < 0)
			return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* EOF */

