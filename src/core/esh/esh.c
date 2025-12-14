
/* esh.c */

#include "esh.h"

void esh(void)
{
	int c = getchar();
	if (c != EOF)
		ttytask(c);
	run_tasks();
}

/* EOF */

