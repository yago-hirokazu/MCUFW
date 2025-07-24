
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
/* win32 adaptation doesn't have scan state */

/* functions imported from tty.c */
extern int scan_normal(int c, struct qbyte *line_p);
extern void proc_del(void);
extern void proc_up(void);
extern void proc_down(void);
extern void proc_right(void);
extern void proc_left(void);

/* local functions only for win32 */
static void scan_vkcode(int c);
static COORD cursor(HANDLE h);


/* exported function implementation */

void EOT(void) { exit(EXIT_SUCCESS); }

/* CLear Screen */
void cls(void)
{
	system("cls");
}

/* CLear Line */
void cll(void)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coordScreen = {0, 0};
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */
	DWORD dwConSize;                 /* number of character cells in
					    the current buffer */

	/* get the number of character cells in the current buffer */
	GetConsoleScreenBufferInfo( hConsole, &csbi );
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	coordScreen.Y = csbi.dwCursorPosition.Y;
	coordScreen.X = 0;

	/* fill the entire screen with blanks */
	FillConsoleOutputCharacter( hConsole, (TCHAR) ' ',
				    csbi.dwSize.X, coordScreen,
				    &cCharsWritten );

	/* get the current text attribute */
	GetConsoleScreenBufferInfo( hConsole, &csbi );

	/* now set the buffer's attributes accordingly */
	FillConsoleOutputAttribute( hConsole, csbi.wAttributes,
				    dwConSize, coordScreen, &cCharsWritten );

	/* put the cursor at the left most of the current line */
	SetConsoleCursorPosition( hConsole, coordScreen );
}

int scan_core(int c, struct qbyte *line_p)
{
	int delim = 0;

	if (c <= 256) {
		DP("scan_normal(%c)", c);
		delim = scan_normal(c, line_p);
	} else {
		DP("scan_vkcode(%c)", c);
		c -= 256;
		scan_vkcode(c);
		delim = 0;
	}

	DP("delim=%d", delim);
	return delim;
}

int scan_normal_adapt(int *c, int *delim, struct qbyte *line_p)
{
	if (*c == '\r') {
		DP("CR=0x%02X\n", *c);
		*delim |= DELIM_LF;
		if (line_p->wp >= 1 && !is_before_space(line_p))
			qbput(' ', line_p);
		qbput('\n', line_p);
		*c = '\n';
		return DO_ECHO;
	}
	return DONT_ECHO;
}

void prompt(void)
{
	printf("%s ", PROMPT);
}

void echo_right(void)
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD c = cursor(h);
	c.X++;
	SetConsoleCursorPosition(h, c);
}

void echo_left(void)
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD c = cursor(h);
	c.X--;
	SetConsoleCursorPosition(h, c);
}


/* local function implementation */

static void scan_vkcode(int c)
{
	switch (c) {
	case 0x53:
		proc_del();
		break;
	case 0x48:
		proc_up();
		break;
	case 0x4b:
		proc_left();
		break;
	case 0x4d:
		proc_right();
		break;
	case 0x50:
		proc_down();
		break;
	default:
		break;
	}
}


static COORD cursor(HANDLE h)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi = {0,};
	COORD coord = {0,};

	GetConsoleScreenBufferInfo(h, &csbi);
	coord.X = csbi.dwCursorPosition.X;
	coord.Y = csbi.dwCursorPosition.Y;

	return coord;
}

/* EOF */

