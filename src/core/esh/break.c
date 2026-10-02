
/* break.c */

#include "machdep.h"
#include "esh.h"
#include "par.h"

#define EP(fmt, ...)\
do {\
	fprintf(stderr,"error [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)

#if DEBUG_BREAK
#define DP(fmt, ...)\
do {\
	fprintf(stderr,"debug [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)
#else
#define DP(fmt, ...)
#endif


extern struct parse parse[MAXNESTS];
extern int loop_at[MAXNESTS];

static int break_label(struct parse *par, int *loop_at, int loop_p);

int break_command(int argc, char *argv[])
{
	char *end;
	long depth = 1;
	long loop_p;

	DP("break");
	DP("argc = %d",argc);
#if DEBUG_BREAK
	for (int k=0; k<argc; k++)
		DP("argv[%d] = %s",k,argv[k]);
#endif

	/*
	  argc    = 4 => invalid

	  argc    = 3
	  argv[0] = "break"
	  argv[1] = user input = loop depth to break
	  argv[2] = current loop pointer (loop depth - 1)

	  argc    = 2
	  argv[0] = "break"
	  argv[1] = current loop pointer (loop depth - 1)

	  argc    = 1 => invalid
	*/

	if (argc < 2 || argc > 3)
		goto invalid_argument;

	loop_p = strtol(argv[argc - 1], &end, 10);
	if (*argv[argc - 1] == '\0' || *end != '\0' || loop_p < 0)
		goto invalid_argument;

	if (argc == 3) {
		depth = strtol(argv[1], &end, 10);
		if (*argv[1] == '\0' || *end != '\0' || depth <= 0)
			goto invalid_argument;
	}

	if (depth > loop_p + 1)
		loop_p = 0;
	else
		loop_p -= depth - 1;

	return break_label(parse, loop_at, loop_p);

invalid_argument:
	EP("break: numeric argument required");
	return -1;
}

static int break_label(struct parse *par, int *loop_at, int loop_p)
{
	int loopat;
	int label;

	loopat = loop_at[loop_p];
	label = par[loopat].p_label.l_end;
	DP("label = parse[loop_at[loop_p=%d]=%d].p_label.l_end = %d",
	   loop_p, loop_at[loop_p], label);

	return label;
}

/* EOF */

