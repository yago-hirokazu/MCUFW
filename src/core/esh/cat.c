/* cat.c */

/**************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 **************************************************************************/
#include "esh.h"

/**************************************************************************
 * FUNCTION DEFINITIONS
 **************************************************************************/
int cat(int argc, char **argv)
{
	int ch;

	(void)argv;
	if (argc != 1)
		return EXIT_FAILURE;
	while ((ch = getchar()) != EOF)
		if (putchar(ch) == EOF)
			return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
