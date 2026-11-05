/* cat.c */

/**************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 **************************************************************************/
#include "custom_assert.h"
#include "code_file.h"
#include "esh.h"

/**************************************************************************
 * FUNCTION DEFINITIONS
 **************************************************************************/
int cat(int argc, char **argv)
{
	int ch;

	(void)argv;
	assert_warm(argc == 1, FILE_CLI_CAT, EXIT_FAILURE);
	while ((ch = getchar()) != EOF)
		if (putchar(ch) == EOF)
			return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
