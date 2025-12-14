
/* tty_adapt.c */

#include "keymap.h"
#include "prompt.h"
#include "tty.h"
#include "tty_adapt.h"
#include "qbyte.h"

/* scan state */
enum cygwin_scan_state {
	SCAN_NORMAL = 0,
	SCAN_ESCSEQ1,
	SCAN_ESCSEQ2,
	SCAN_ESCSEQ3
} scan_state;

extern int scan_normal(int c);
extern void proc_del(void);
extern void proc_up(void);
extern void proc_down(void);
extern void proc_right(void);
extern void proc_left(void);

static void EOT(void) { exit(0); }

void ttycls(void)
{
	system("cls");
}

/* Clear Line */
void cll(void)
{
	putchar(ASCII_ESC);
	putchar('[');
	putchar('2');
	putchar('K');
}

int scan_core(int c, struct qbyte *line_p)
{
	int cr_found = 0;

	switch (scan_state) {
	case SCAN_NORMAL:
		cr_found = scan_normal(c);
		break;
	case SCAN_ESCSEQ1:
		scan_escseq1(c);
		break;
	case SCAN_ESCSEQ2:
		scan_escseq2(c);
		break;
	case SCAN_ESCSEQ3:
		scan_escseq3(c);
		break;
	default:
		break;
	}

	return cr_found;
}

void prompt(void)
{
	char prompt[32];
	memset(prompt, 0, sizeof(prompt));
	strcpy(prompt, PROMPT);
	printf("%s ", prompt);
}

int scan_vkcode(int c)
{
	switch (c) {
	case 0x53:
		proc_del();
		break;
	case 0x48:
		proc_up();
		return GETHIST;
	case 0x4b:
		proc_left();
		break;
	case 0x4d:
		proc_right();
		break;
	case 0x50:
		proc_down();
		return GETHIST;
	default:
		break;
	}
	return 0;
}


COORD cursor(HANDLE h)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi = {0,};
	COORD coord = {0,};

	GetConsoleScreenBufferInfo(h, &csbi);
	coord.X = csbi.dwCursorPosition.X;
	coord.Y = csbi.dwCursorPosition.Y;

	return coord;
}

void echo_right(void)
{
	putchar(ASCII_ESC);
	putchar('[');
	putchar('C');
	return;
}


void echo_left(void)
{
	putchar(ASCII_ESC);
	putchar('[');
	putchar('D');
	return;
}


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
#ifdef __CYGWIN__
	else {
		if (c == '3')
			scan_state = SCAN_ESCSEQ3;
		else
			scan_state = SCAN_NORMAL;
	}
#endif
	return;
}

#if defined(__CYGWIN__)
static void scan_escseq3(char c)
{
	if (c == '~')
		proc_del();

	scan_state = SCAN_NORMAL;

	return;
}
#endif

/* EOF */

