
/* main.c */

#include "main.h"
#include "tty_config.h"
#include "esh.h"
#include "log_error.h"

int main(int argc, char *argv[])
{
	if (init_error() != 0)
		return EXIT_FAILURE;
	eshinit();

	while (1) {
		esh();
	}

	return EXIT_SUCCESS;
}

/* EOF */

