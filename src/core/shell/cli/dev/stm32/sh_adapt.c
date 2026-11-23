
/* sh_adapt.c */

#include "sh.h"
#include "sh_adapt.h"
#include "stdio_uart.h"
#include "tty_adapt.h"


void shinit(void)
{
	init_tty();
	stdio_uart_open();
	prompt();
}

/* EOF */

