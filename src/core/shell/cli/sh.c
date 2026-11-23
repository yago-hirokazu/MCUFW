
/* sh.c */

#include "sh.h"

void sh(void)
{
	int c = getchar();
	if (c != EOF)
		ttytask(c);
	run_tasks();
}

/* EOF */

