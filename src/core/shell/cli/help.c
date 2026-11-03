
/* help.c */

#include "esh.h"
#include "esh_adapt.h"

#if USE_STACKMACH
char * const usage_builtins[] = {
	"test\n",
	"expr\n",
	"echo\n",
	NULL
};

void help_builtins(void)
{
	int k=0;
	while (usage_builtins[k]!=NULL)
		printf("%s",usage_builtins[k++]);
}
#endif /* #if USE_STACKMACH */

void usage(void)
{
	int k;
	for (k = 0; helps[k] != NULL; k++)
		helps[k]();
	return;
}

int help(int argc, char **argv)
{
#if USE_STACKMACH
	help_builtins();
#endif
	usage();
	return EXIT_SUCCESS;
}

/* EOF */

