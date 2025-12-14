
/* run.c */

#include "esh.h"
#include "tty.h"

#undef UCMD
#undef PROTOTYPES
#include "ucmd.h"

#undef BLTN
#undef PROTOTYPES
#include "bltn.h"


#if DEBUG_SHELL
#define DP(fmt, ...) do {					\
		fprintf(stdout, "[%s|%s()|L%d] " fmt "\n",	\
			__FNAME__,__func__,__LINE__,		\
			##__VA_ARGS__				\
			);					\
		ffush(stdout);					\
	} while (0);
#else
#define DP(fmt, ...)
#endif


struct applet *find_applet(char *name, const struct applet *applet, int *k)
{
	struct applet *ap = (struct applet *)applet;

	if (ap == NULL)
		return NULL;

	for (; ap->name != NULL; ap++) {
		DP("%s", ap->name);
		if (!strncmp(name, ap->name, TTY_NBUF)) {
			DP("Find applet: %s", name);
			break;
		}

		if (k != NULL)
			(*k)++;
	}

	if (ap->name == NULL)
		return NULL;

	return ap;
}

int run_applet(int argc, char **argv)
{
	struct applet *ap;

	if (argv[0] == NULL || argv[0][0] == '\0' || argv[0][0] == '\n')
		return 0;

	ap = find_applet(argv[0], usercmds, NULL);
	if (ap) {
		ap->main(argc, argv);
		return 1;
	}
	ap = find_applet(argv[0], builtins, NULL);
	if (ap) {
		ap->main(argc, argv);
		return 1;
	}

	printf("%s not supported applet\n", argv[0]);

	return 0;
}

void run_tasks(void)
{
	int i;

	for (i = 0; tasks[i] != NULL; i++)
		tasks[i]();

	return;
}


/* EOF */

