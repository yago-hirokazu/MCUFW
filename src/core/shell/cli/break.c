
/* break.c */

#include "custom_assert.h"
#include "code_file.h"
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

/*
 * Report invalid numeric argument
 */
static int invalid_argument(void)
{
	EP("break: numeric argument required");
	return -1;
}

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

	assert_warm(argc >= 2, FILE_CLI_BREAK, invalid_argument());
	assert_warm(argc <= 3, FILE_CLI_BREAK, invalid_argument());
	assert_warm(argv != NULL, FILE_CLI_BREAK, invalid_argument());
	assert_warm(argv[argc - 1] != NULL, FILE_CLI_BREAK, invalid_argument());

	loop_p = strtol(argv[argc - 1], &end, 10);
	assert_warm(*argv[argc - 1] != '\0', FILE_CLI_BREAK,
		    invalid_argument());
	assert_warm(*end == '\0', FILE_CLI_BREAK, invalid_argument());
	assert_warm(loop_p >= 0, FILE_CLI_BREAK, invalid_argument());
	assert_warm(loop_p < MAXNESTS, FILE_CLI_BREAK, invalid_argument());

	if (argc == 3) {
		assert_warm(argv[1] != NULL, FILE_CLI_BREAK,
			    invalid_argument());
		depth = strtol(argv[1], &end, 10);
		assert_warm(*argv[1] != '\0', FILE_CLI_BREAK,
			    invalid_argument());
		assert_warm(*end == '\0', FILE_CLI_BREAK, invalid_argument());
		assert_warm(depth > 0, FILE_CLI_BREAK, invalid_argument());
	}

	if (depth > loop_p + 1)
		loop_p = 0;
	else
		loop_p -= depth - 1;

	return break_label(parse, loop_at, loop_p);

}

static int break_label(struct parse *par, int *loop_at, int loop_p)
{
	int loopat;
	int label;

	loopat = loop_at[loop_p];
	assert_warm(loopat >= 0, FILE_CLI_BREAK, -1);
	assert_warm(loopat < MAXNESTS, FILE_CLI_BREAK, -1);
	label = par[loopat].p_label.l_end;
	DP("label = parse[loop_at[loop_p=%d]=%d].p_label.l_end = %d",
	   loop_p, loop_at[loop_p], label);

	return label;
}

/* EOF */

