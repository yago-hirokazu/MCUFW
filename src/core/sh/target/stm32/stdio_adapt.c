
/* stdio_adapt.c */

#include "stdio_uart.h"


/**
   Writes the character specified by c (converted to an unsigned
   char) to the output stream pointed to by stream, at the position
   indicated by the associated file position indicator (if defined),
   and advances the indicator appropriately. If the file position
   indicator is not defined, the character is appended to the output
   stream.
   \param[in] c       Character
   \param[in] stream  Stream handle
   \return    The character written. If a write error occurs,
              the error indicator is set and fputc returns EOF.
*/
int fputc(int ch, FILE *stream)
{
#if defined(__CC_ARM) || (defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
	if (stream == &__stdout)
		return stdio_uart_stdout_putchar(ch);
	return -1;
#elif defined(__ICCARM__)
	return stdio_uart_stdout_putchar(ch);
#else
#error "build error at fputc()"
#endif
}

/**
   Obtains the next character (if present) as an unsigned char
   converted to an int, from the input stream pointed to by stream,
   and advances the associated file position indicator (if defined).
   \param[in] stream  Stream handle
   \return    The next character from the input stream pointed to by
              stream.
              If the stream is at end-of-file, the end-of-file
              indicator is set and fgetc returns EOF. If a read error
              occurs, the error indicator is set and fgetc returns
              EOF.
*/

int fgetc(FILE *stream)
{
#if defined(__CC_ARM) || (defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
	if (stream == &__stdin) {
		return stdio_uart_stdin_getchar();
	}
	return -1;
#elif defined(__ICCARM__)
	return stdio_uart_stdin_getchar();
#else
#error "build error at fgetc()"
#endif
}


/* EOF */

