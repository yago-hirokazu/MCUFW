
/* cmd_sam.c */

#include "machdep.h"
#include "getopt.h"
#include "timing.h"

char * const applets_sam[] = {
	"sam1\n",
	"sam2\n",
	"sam3\n",
	NULL
};

void help_sam(void)
{
	int k=0;
	while (applets_sam[k]!=NULL)
		printf("%s",applets_sam[k++]);
}

int sam1_main(int argc, char *argv[])
{
	static char * const help[] = {
		"sam1\tApplet sample1\n",
		NULL
	};
	int opt;
	optind = 1; /* initialize index to parent argv vector */
	opterr = 0; /* disable error messages of getopt() */
	while ((opt = getopt(argc, argv, "h")) != -1) {
		if (opt == 'h') {
			for (int i=0; help[i]!=NULL; i++)
				printf("%s", help[i]);
			goto out;
		}
	}
	printf("sample1 applet\n");
out:
	return EXIT_SUCCESS;
}

int sam2_main(int argc, char *argv[])
{
	static char * const help[] = {
		"sam2\tApplet sample2\n",
		"\targ: [arg1] [arg2] [arg3]\n",
		"\targ1 output as string\n",
		"\targ2 output as decimal\n",
		"\targ3 output as hex\n",
		NULL
	};
#if 1
	printf("argc=%d\n",argc);
	for (int k=0; k<argc; k++)
		printf("argv[%d]=%s\n",k,argv[k]);
#endif
	if (argc <= 1)
		goto out;
	if (!strcmp(argv[1], "-h")) {
		for (int i=0; help[i]!=NULL; i++)
			printf("%s", help[i]);
		goto out;
	}
	printf("arg1=%s\n",argv[1]);
	printf("arg2=%d\n",(int)strtol(argv[2],NULL,0));
	printf("arg3=0x%08X\n",(int)strtol(argv[3],NULL,0));
out:
	return EXIT_SUCCESS;
}

int sam3_main(int argc, char *argv[])
{
	static char * const help[] = {
		"sam3\tApplet sample3\n",
		"\t-a\toption without argument\n",
		"\t-b\toption with    argument\n",
		"\t\targ: a string\n",
		NULL
	};
	int opt;
	optind = 1; /* initialize index to parent argv vector */
	opterr = 0; /* disable error messages of getopt() */
	while ((opt = getopt(argc, argv, "ab:h")) != -1) {
		if (opt == 'h') {
			for (int i=0; help[i]!=NULL; i++)
				printf("%s", help[i]);
			goto out;
		} else if (opt == 'a') {
			printf("-a option\n");
		} else if (opt == 'b') {
			printf("-b option with %s\n",optarg);
		} else {
			printf("invalid option: -%c\n", opt);
			return EXIT_FAILURE;
		}
	}
out:
	return EXIT_SUCCESS;
}

void task_sam(void)
{
}

/* EOF */

