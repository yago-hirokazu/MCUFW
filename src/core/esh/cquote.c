
/* cquote.c */

#include "cquote.h"
#include "esh.h"


#define DEBUG 1
#if DEBUG
#define DP(fmt, ...)\
do {\
	fprintf(stdout, "debug @ [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)
#else
#define DP(...)
#endif


int is_quotestart(char *str, char quote)
{
	char *c=str;
	if (*c == quote)
		return 1;
	return 0;
}

int is_quoteend(char *str, char quote, int *ctrace)
{
	int k=0;
	char *c=str;
	do {
		c++;
		k++;
		if (k >= SYMCHARS)
			return 0;
	} while (*c != quote);

	if (ctrace != NULL)
		*ctrace = k;

	return 1;
}

int is_closequote(char *str, char quote, int *ctrace)
{
	int k=0;
	char *c=str;

	if (!is_quotestart(str, quote))
		return 0;

	if (!is_quoteend(str, quote, ctrace))
		return 0;

	return 1;
}

int qcatargv(char quote, int *argc, char **argv)
{
	int k=0, l=0, ctp=0, quoted=0, count=(*argc-1);
	int last=count;
	char c=0, str[SYMCHARS], tmp[SYMCHARS];

	/* Find start of quotation */
	for (k=1; k<=last; k++) {
		c = argv[k][0];
		if (c == quote) {
			quoted = 1;
			break;
		}
	}

	/* No quotation found */
	if (quoted == 0)
		return 0;

	/* comcatinate argument vectors */
	ctp = 0; /* Character Test Pointer */

	strncpy(tmp, argv[k], SYMCHARS);
	for (l=k+1; l<=last; l++) {
		DP("tmp=%s",tmp);
		snprintf(str, SYMCHARS, "%s %s", tmp, argv[l]);
		DP("str=%s",str);
		strncpy(tmp, str, SYMCHARS);
		if (is_closequote(&str[ctp], quote, &ctp))
			break;
		count--;
	}

	DP("Copy str to argv[%d]",k);
	*argc = count;
	strncpy(argv[k], str, SYMCHARS);
	return 1;
}

/* EOF */

