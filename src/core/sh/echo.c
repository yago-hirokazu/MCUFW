
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
		printf("%s",argv[k]);
		if (k < argc-1)
			printf(" ");
	}
	if (opt_n == 0)
		printf("\n");

	return EXIT_SUCCESS;
}

/* EOF */

