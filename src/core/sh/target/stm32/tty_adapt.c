
/* tty_adapt.c */

#include "machdep.h"
#include "keymap.h"
#include "prompt.h"
#include "tty.h"
#include "tty_adapt.h"

#if DEBUG_TTY_ADAPT
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt "\n",	\
	       __FILE__,__func__,__LINE__,	\
	       ##__VA_ARGS__			\
		)
#else
#define DP(fmt, ...)
#endif

/* scan state */
enum scan_state {
	SCAN_NORMAL = 0,
	SCAN_ESCSEQ1,
	SCAN_ESCSEQ2
} scan_state;

/* functions imported from tty.c */
extern int scan_normal(int c, struct qbyte *line_p);
extern void proc_del(void);
extern void proc_up(void);
extern void proc_down(void);
extern void proc_right(void);
extern void proc_left(void);

/* local functions only for win32 */
static void scan_escseq1(char c);
static void scan_escseq2(char c);


/* exported function implementation */

void EOT(void) { NVIC_SystemReset(); }

/* CLear Screen */
void cls(void)
{
	/* Clear screen */
	putchar(ASCII_ESC);
	putchar('[');
	putchar('2');
	putchar('J');

	/* Reset terminal */
	putchar(ASCII_ESC);
	putchar('c');
}

/* Clear Line */
void cll(void)
{
	putchar(ASCII_ESC);
	putchar('[');
	putchar('2');
	putchar('K');

	putchar('\r');
}

int scan_core(int c, struct qbyte *line_p)
{
	int delim = 0;

	switch (scan_state) {
	case SCAN_NORMAL:
		delim = scan_normal(c, line_p);
		break;
	case SCAN_ESCSEQ1:
		scan_escseq1(c);
		break;
	case SCAN_ESCSEQ2:
		scan_escseq2(c);
		break;
	default:
		break;
	}

	DP("delim=%d", delim);
	return delim;
}

int scan_normal_adapt(int *c, int *delim, struct qbyte *line_p)
{
	if (*c == '\r') {
		*delim |= DELIM_LF;
		if (line_p->wp >= 2 &&
		    line_p->byte[line_p->wp-2] != '`')
			qbput(' ', line_p);
		qbput('\n', line_p);
		*c = '\n';
		return DO_ECHO;
	}

	if (*c == ASCII_DEL) {
		proc_del();
	}

	if (*c == ASCII_ESC) {
		scan_state = SCAN_ESCSEQ1;
	}

	return DONT_ECHO;
}

void prompt(void)
{
	/* Print prompt to operator */
	printf("%s ", PROMPT);
}

void echo_right(void)
{
	putchar(ASCII_ESC);
	putchar('[');
	putchar('C');
}

void echo_left(void)
{
	putchar(ASCII_ESC);
	putchar('[');
	putchar('D');
}
#if TBD
int isdigit(int c)
{
	if ('0' <= c && c <= '9')
		return 1;

	return 0;
}

int isalpha(int c)
{
	if ('A' <= c && c <= 'Z')
		return 1;
	if ('a' <= c && c <= 'z')
		return 1;

	return 0;
}

int tolower(int c)
{
	if ('A' <= c && c <= 'Z')
		c += 'a' - 'A';

	return c;
}
#endif

/* local function implementation */

static void scan_escseq1(char c)
{
	if (c == '[')
		scan_state = SCAN_ESCSEQ2;
	else
		scan_state = SCAN_NORMAL;

	return;

}

static void scan_escseq2(char c)
{
	if ('A' <= c && c <= 'D') {
		switch (c) {
		case 'A':
			proc_up();
			break;
		case 'B':
			proc_down();
			break;
		case 'C':
			proc_right();
			break;
		case 'D':
			proc_left();
			break;
		default:
			break;
		}

		scan_state = SCAN_NORMAL;

		return;
	}
	return;
}

/* EOF */

