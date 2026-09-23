
/* break.c */

#include "machdep.h"
#include "esh.h"
#include "par.h"

#define EP(fmt, ...)\
do {\
	fprintf(stdout,"error [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)

#if DEBUG_TEST
#define DP(fmt, ...)\
do {\
	fprintf(stdout,"debug [%s|%s()|L%d] " fmt "\n",\
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
extern int loops;

static int break_label(struct parse *par, int *loop_at, int loop_p);

int break_command(int argc, char *argv[])
{
	int loop_p=0, label=0;

	DP("break");
	DP("argc = %d",argc);
#if DEBUG_BREAK
	for (int k=0; k<argc; k++)
		DP("argv[%d] = %s",k,argv[k]);
#endif

	// Use 1st argument
	loop_p = strtol(argv[1], NULL, 0);
	label = break_label(parse, loop_at, loop_p);

	DP("loop_p = %d\n", loop_p);
	DP("label  = %d\n", label);

	return label;
}

static int break_label(struct parse *par, int *loop_at, int loop_p)
{
	int label=0, loopat=0;

	if (loop_p >= loops)
		loop_p = loops - 1;
	loopat = loop_at[loop_p];
	label = par[loopat].p_label.l_end;
	DP("label = parse[loop_at[loop_p=%d]=%d].p_label.l_end = %d",
	   loop_p,loop_at[loop_p],label);

	return label;
}

/* EOF */

