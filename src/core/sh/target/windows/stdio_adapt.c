
/* stdio_adapt.c */

#include "machdep.h"

#if DEBUG_STDIO_ADAPT
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt "\n",	\
	       __FILE__,__func__,__LINE__,	\
	       ##__VA_ARGS__			\
		)
#else
#define DP(fmt, ...)
#endif

#if USE_PIPE
#include "pipe.h"
#endif

int win_getchar(void)
{
	int ch = _getch();

	DP("%c (%3d)", ch, ch);

	if (ch == 0 || ch == 224)
		ch = 256 + _getch();

	DP("%c ($3d)", ch, ch);

	return ch;
}

int win_putchar(int c)
{
	DP("%c", c);

	_putch(c);
}

int win_printf(const char *fmt, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, fmt);
#if USE_PIPE
	if (pipe.p_num > 0)
	{
		vsprintf(pipe.p_buf, fmt, arg_ptr);
	}
	else
	{
		vprintf(fmt, arg_ptr);
	}
#else
	vprintf(fmt, arg_ptr);
#endif
	va_end(arg_ptr);
}

int win_scanf(char *fmt, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, fmt);
#if USE_PIPE
	if (pipe.p_num > 0)
	{
		vsscanf(exe.e_pipebuf, fmt, arg_ptr);
	}
	else
	{
		vscanf(fmt, arg_ptr);
	}
#else
	vscanf(fmt, arg_ptr);
#endif
	va_end(arg_ptr);
}

/* EOF */

