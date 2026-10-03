/* stdio_adapt.c */

#include "machdep.h"
#include <stdarg.h>
#include "esh_debug.h"

#ifdef ESH_STDIO_PIPE
#include "esh_sched.h"

static int win_pipe_fd(int fd)
{
	struct esh_proc *proc = esh_current_proc();

	return proc && esh_fd_get(&proc->ep_fd_table, fd);
}
#else
static int win_pipe_fd(int fd)
{
	(void)fd;
	return 0;
}
#endif

int win_getchar(void)
{
	int ch;

#ifdef ESH_STDIO_PIPE
	unsigned char byte;

	if (win_pipe_fd(0)) {
#if DEBUG_STDIO
		fprintf(stderr, "stdio: pipe read\n");
#endif
		return esh_read(0, &byte, 1) == 1 ? byte : EOF;
	}
#endif
	ch = _getch();
	if (ch == 0 || ch == 224)
		ch = 256 + _getch();
	return ch;
}

int win_putchar(int c)
{
#ifdef ESH_STDIO_PIPE
	unsigned char byte = (unsigned char)c;

	if (win_pipe_fd(1))
		return esh_write(1, &byte, 1) == 1 ? byte : EOF;
#endif
	return _putch(c);
}

static int win_vfprintf(FILE *stream, const char *fmt, va_list args)
{
#ifdef ESH_STDIO_PIPE
	va_list copy;
	char *buffer;
	int length;
	ptrdiff_t count;

	if (stream == stdout && win_pipe_fd(1)) {
#if DEBUG_STDIO
		fprintf(stderr, "stdio: pipe format\n");
#endif
		va_copy(copy, args);
		length = vsnprintf(NULL, 0, fmt, copy);
		va_end(copy);
		if (length < 0)
			return EOF;
		buffer = malloc((size_t)length + 1);
		if (!buffer)
			return EOF;
		vsnprintf(buffer, (size_t)length + 1, fmt, args);
		count = esh_write(1, buffer, (size_t)length);
		free(buffer);
		return count == length ? length : EOF;
	}
#endif
	return vfprintf(stream, fmt, args);
}

int win_printf(const char *fmt, ...)
{
	va_list args;
	int result;

	va_start(args, fmt);
	result = win_vfprintf(stdout, fmt, args);
	va_end(args);
	return result;
}

int win_fprintf(FILE *stream, const char *fmt, ...)
{
	va_list args;
	int result;

	va_start(args, fmt);
	result = win_vfprintf(stream, fmt, args);
	va_end(args);
	return result;
}

int win_scanf(const char *fmt, ...)
{
	va_list args;
	int result;

	/* Pipe input requires read or getchar; leave stream untouched */
	if (win_pipe_fd(0))
		return EOF;
	va_start(args, fmt);
	result = vscanf(fmt, args);
	va_end(args);
	return result;
}
