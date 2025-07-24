
/**
 * @file tty.c
 * @brief
 */

/** @addtogroup
 *  @{
 */

/**************************************************************************
 * STANDARD INCLUDE FILES
 **************************************************************************/


/**************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 **************************************************************************/
#include "esh.h"
#include "keymap.h"
#include "prompt.h"
#include "qbyte.h"
#include "tty.h"
#include "tty_adapt.h"


/**************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 **************************************************************************/
struct qbyte line;


/**************************************************************************
 * LOCAL DEFINITIONS
 **************************************************************************/
enum {  TTY_BHIST = TTY_NHIST-1};
enum {  TTY_BBUF  = TTY_NBUF-1};
#define TTY_NCHAR   16


/**************************************************************************
 * LOCAL TYPES
 **************************************************************************/
struct tty_history {
	struct {
		char ch[TTY_NBUF];	/* Characters in a line */
		int eol;		/* EOL pointer */
	} ll[TTY_NHIST];		/* Line List */
	int lw;				/* List Write pointer */
	int lr;				/* List Read pointer */
	int ln;				/* List Number */
};

struct tty_context {
	char t_indent;			/* is in indent */
	char t_prevc;			/* Previously scanned character */
};

/**************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 **************************************************************************/
char linebuf[TTY_NBUF];
struct tty_arg arg;
struct tty_history history;
struct tty_context tty;


/**************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 **************************************************************************/
static void proc_bs(void);
static void echo_del(void);
static void puth(struct qbyte *line_p, struct tty_history *history_p);
static void geth(struct tty_history *history_p, struct qbyte *line_p);
static void reprompt(void);
static int is_indent(int c, struct qbyte *line_p);
#if DEBUG_TTY
static void outc(char *str);
static void outh(char *str);
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt,	\
	       __FNAME__,__func__,__LINE__,	\
	       ##__VA_ARGS__			\
		)
#define OC(str) outc(str)
#define OH(str) outh(str)
#else
#define DP(...)
#define OC(str)
#define OH(str)
#endif


/**************************************************************************
 * EXPORT FUNCTION PROTOTYPES
 **************************************************************************/
extern int scan(int c, struct qbyte *line_p);
extern struct tty_arg *tokenize(char *line, struct tty_arg *arg);
extern struct tty_arg *make_clarg(char *line, struct tty_arg *arg);
extern void proc_del(void);
extern void proc_up(void);
extern void proc_down(void);
extern void proc_right(void);
extern void proc_left(void);
extern char *find_space(char *cp, int endaddr);
extern int is_endofline(char c);
extern int is_before_space(struct qbyte *line_p);
extern void nul_to_space(char *cp);


/**************************************************************************
 * IMPORT FUNCTION PROTOTYPES
 **************************************************************************/
extern void EOT(void);
extern void cls(void);
extern void cll(void);
extern int scan_core(int c, struct qbyte *line_p);
extern int scan_normal_adapt(int *c, int *delim, struct qbyte *line_p);
extern void prompt(void);
extern void echo_right(void);
extern void echo_left(void);


/**************************************************************************
 * FUNCTION DEFINITIONS
 **************************************************************************/
void ttyinit(void)
{
	qbinit(&line, (uint8_t *)&linebuf[0], TTY_NBUF);
}

void ttytask(int c)
{
	struct tty_arg *arg_p = &arg;
	char found_applet;

	if (!(DELIM_LF & scan(c, &line)))
		return;

	arg_p = make_clarg(linebuf, arg_p);
	if (arg_p == NULL)
		goto repro;

	found_applet = run_applet(arg_p->c, arg_p->v);
	if (found_applet)
		puth(&line, &history);

repro:
	DP("goto repro\n");
	qbreset(&line, 0);
	memset(&arg, 0, sizeof(struct tty_arg));
	prompt();
	return;
}

int scan(int c, struct qbyte *line_p)
{
	if (c == ASCII_NUL) /* no char to be processed */
		return 0;
	if (c == ASCII_EOT) /* ^C (Ctrl-D) */
		EOT();
	if (c == ASCII_BS)
		proc_bs();

	/* Don't capture indent */
	if (is_indent(c, line_p))
		return 0;

	return scan_core(c, line_p);
}

struct tty_arg *tokenize(char *line, struct tty_arg *arg)
{
	char *cp = line;
	int k = 0;
	int eobaddr = (int)(line + TTY_BBUF);

	if (cp == NULL)
		return NULL;

	arg->v[0] = cp;
	arg->c = 1;

	if (is_endofline(arg->v[0][0])) /* To reduce search cycle */
		goto out;

	for (k = 1; k < TTY_NARG-1; k++) {
		cp = find_space(arg->v[k-1], eobaddr);
		if (cp == NULL)
			break;
		*cp = '\0'; /* replace space by nul */
		cp++; /* skip delimiter character */
		if (cp == NULL || *cp == '\0')
			break;

		arg->v[k] = cp;
		arg->c++;

		if (is_endofline(arg->v[k][0])) /* To reduce search cycle */
			break;
	}

#if DEBUG_TTY
	for (k = 0; k < arg->c; k++)
		DP("argv[%d]=%s\n",k,arg->v[k]);
#endif

	/* don't put history if it is blank line */
	if (arg->c == 0)
		return NULL;

out:
	return (struct tty_arg *)arg;
}

struct tty_arg *make_clarg(char *line, struct tty_arg *arg)
{
	struct tty_arg *arg_p=NULL;
	arg_p = tokenize(line, arg);
	if (arg_p == NULL)
		return NULL;
	if (arg_p->v[0][0] == '\n')
		return NULL;
	if (is_endofline(arg_p->v[arg_p->c-1][0]))
		arg_p->c--;

#if DEBUG_TTY
	for (int k = 0; k < arg->c; k++)
		DP("argv[%d]=%s\n",k,arg->v[k]);
#endif

	return (struct tty_arg *)arg_p;
}

int scan_normal(int c, struct qbyte *line_p)
{
	int delim=0, echo=0;
	char prevc = tty.t_prevc;

	if (prevc == '=' ||
	    prevc == ';' ||
	    prevc == '`') {
		if (c != ' ')
			qbput(' ', line_p);
	}

	if (c == '=' ||
	    c == ';' ||
	    c == '`') {
		if (!is_before_space(line_p))
			qbput(' ', line_p);
		qbput(c, line_p);
		goto ECHOBACK;
	}

	if (isdigit(c)) {
		qbput(c, line_p); /* store into line buffer */
		goto ECHOBACK;
	}

	if (isalpha(c)) {
#if ONLY_LOWER
		c = tolower(c);
#endif
		qbput(c, line_p);
		goto ECHOBACK;
	}

	if (c == '#' ||
	    c == '!' ||
	    c == '[' ||
	    c == ']' ||
	    c == '_' ||
	    c == '$' ||
	    c == '+' ||
	    c == '-' ||
	    c == '*' ||
	    c == '/' ||
	    c == '|' ||
	    c == '&') {
		qbput(c, line_p);
		goto ECHOBACK;
	}

	if (c == ' ') {
		delim |= DELIM_SP;
		qbput(c, line_p);
		goto ECHOBACK;
	}

	if (c == '\n') {
		DP("LF=0x%02X\n", c);
		delim |= DELIM_LF;
		if (line_p->wp >= 1 && !is_before_space(line_p))
			qbput(' ', line_p);
		qbput(c, line_p);
		goto ECHOBACK;
	}

	echo = scan_normal_adapt(&c, &delim, line_p);
	if (echo)
		goto ECHOBACK;

	goto SCANEND;

ECHOBACK:
	fputc(c, stdout);
	fflush(stdout);

SCANEND:
	tty.t_prevc = c;
	return delim;
}

static void proc_bs(void)
{
	if (!line.wp)
		return;

	line.wp--;
	line.byte[line.wp] = ASCII_NUL;

	/* echo back */
	echo_left();
	echo_del();

	return;
}

static void echo_del(void)
{
	putchar(' ');
	echo_left();
}

char *find_keyc(char *cp, int (*delim)(char c), int endaddr)
{
	if (cp == NULL)
		return NULL;
	while (!delim(*cp)) {
#if DEBUG_TTY
		DP("cp=0x%08X endaddr=0x%08X\n",(int)cp,endaddr);
		if (*cp == '\n') {
			DP("*cp=0x%02X\n",*cp);
		} else {
			DP("*cp=%c\n",*cp);
		}
#endif
		if ((int)cp >= endaddr)
			return NULL;
		cp++;
	}
	DP("done searching\n",0);
	cp++; /* skip delimiter character */
	return cp;
}

char count_arg(char *cp, int (*delim)(char c))
{
	DP("*cp=%02X\n", *cp);
	if (cp == NULL)
		return 0;
	if (delim(*cp))
		return 0;
	return 1;
}

/* Put command to history */
static void puth(struct qbyte *line_p, struct tty_history *history_p)
{
	int k=0, lw=history_p->lw, wp=line_p->wp, eol=wp, found_eol=0;
	char *cp=NULL;


	/* Find & Remove CR with spaces around it */
	cp = (char *)&line_p->byte[0];
	for (k=eol; k>0; k--) {
		if (is_endofline(cp[k])) {
			found_eol = 1;
			break;
		}
	}
	if (found_eol) {
		wp = k;
		for (k=wp-1; k<=eol; k++)
			cp[k] = 0;
	}

	cp = (char *)&line_p->byte[0];
	DP("lw=%d <= %s: wp=%d\n",lw, cp, wp);
	nul_to_space(cp);
	DP("lw=%d <= %s: wp=%d\n",lw, cp, wp);
	for (k=0; k<lw; k++) {
		if (!strncmp(cp, history_p->ll[k].ch, TTY_NBUF))
			return;
	}

	memcpy(&history_p->ll[lw].ch[0], cp, TTY_NBUF);
	history_p->ll[lw].eol = wp - 1;
	lw++;
	lw &= TTY_BHIST;
	if (history_p->ln < TTY_NHIST)
		history_p->ln++;
	history_p->lw = lw;
}

/* Get command from history */
static void geth(struct tty_history *history_p, struct qbyte *line_p)
{
	int k;
	int lr = history_p->lr;
	int eol = history_p->ll[lr].eol;
	char *c = &history_p->ll[lr].ch[0];

	qbreset(line_p, 0);
	for (k = 0; k < eol; k++) {
		line_p->byte[k] = *c;
		putchar(*c);
		c++;
	}
	line_p->wp = eol;
	DP("eol=%d\n", eol);
}

static void reprompt(void)
{
	cll();
	prompt();
}

static int is_indent(int c, struct qbyte *line_p)
{
	if (line_p->wp == 0) {
		if (c == ' ' || c == '\t') {
			tty.t_indent = 1;
			return 1;
		}
	}
	if (tty.t_indent) {
		if (c == ' ' || c == '\t')
			return 1;
		tty.t_indent = 0;
	}
	return 0;
}

#if DEBUG_TTY
static void outc(char *str)
{
	char *c = str, k=0;
	while (!is_endofline(*c)) {
		if (k >= 16)
			break;
		fputc(*c, stdout);
		fflush(stdout);
		c++;
		k++;
	}
	fputc('\n', stdout);
	fflush(stdout);
}

static void outh(char *str)
{
	char *c = str, k=0;
	while (!is_endofline(*c)) {
		if (k >= TTY_NCHAR)
			break;
		fprintf(stdout, "%02X ", *c);
		fflush(stdout);
		c++;
		k++;
	}
	fprintf(stdout, "%02X\n", *c);
	fputc('\n', stdout);
	fflush(stdout);
}
#endif

void proc_del(void)
{
	line.byte[line.wp] = ASCII_NUL;

	/* echo back */
	echo_del();

	return;
}

void proc_up(void)
{
	register int ln = history.ln;

	reprompt();

	if (ln) {
		if (history.lr < ln - 1)
			history.lr++;
		else
			history.lr = 0;
	}

	DP("lr,ln=%02x,%02x\n",history.lr,history.ln);

	geth(&history, &line);

	return;
}

void proc_down(void)
{
	register int ln = history.ln;

	reprompt();

	if (ln) {
		if (history.lr > 0)
			history.lr--;
		else
			history.lr = ln - 1;
	}

	DP("lr,ln=%02x,%02x\n",history.lr,history.ln);

	geth(&history, &line);

	return;
}

void proc_right(void)
{
	if (line.wp >= TTY_NBUF)
		return;

	line.wp++;

	echo_right();

	return;
}

void proc_left(void)
{
	if (!line.wp)
		return;

	line.wp--;

	echo_left();

	return;
}

char *find_space(char *cp, int endaddr)
{
	if (cp == NULL)
		return NULL;
	while (*cp != ' ') {
#if DEBUG_TTY
		DP("cp=0x%08X endaddr=0x%08X\n",(int)cp,endaddr);
		if (*cp == '\n') {
			DP("*cp=0x%02X\n",*cp);
		} else {
			DP("*cp=%c\n",*cp);
		}
#endif
		if ((int)cp >= endaddr)
			return NULL;
		cp++;
	}
	DP("done find_space()\n");
	return cp;
}

int is_endofline(char c)
{
	if (c == '\r' || c == '\n')
		return 1;
	return 0;
}

int is_before_space(struct qbyte *line_p)
{
	if (line_p->wp >= 1 &&
	    line_p->byte[line_p->wp-1] == ' ')
		return 1;
	return 0;
}

void nul_to_space(char *cp)
{
	int k=0;
	if (cp == NULL)
		return;
	while (k < TTY_BBUF) {
		if (*cp == '\0') {
			*cp = ' ';

			if (*(cp+1) == '\0')
				break;
		}
		cp++;
	}

}


/* End of doxygen group */
/** @} */

/* EOF */

