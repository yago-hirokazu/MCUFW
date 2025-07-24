
/* main.c */

#include "main.h"
#include "tty_config.h"
#include "esh.h"

int main(int argc, char *argv[])
{
	eshinit();

	while (1) {
		esh();
	}

	return EXIT_SUCCESS;
}

/* EOF */

