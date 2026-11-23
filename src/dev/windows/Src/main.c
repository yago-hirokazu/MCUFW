
/* main.c */

#include "main.h"
#include "tty_config.h"
#include "sh.h"
#include "log_error.h"

int main(int argc, char *argv[])
{
	if (init_error() != 0)
		return EXIT_FAILURE;
	shinit();

	while (1) {
		sh();
	}

	return EXIT_SUCCESS;
}

/* EOF */

