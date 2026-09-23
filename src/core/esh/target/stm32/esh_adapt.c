
/* esh_adapt.c */

#include "esh.h"
#include "esh_adapt.h"
#include "stdio_uart.h"
#include "tty_adapt.h"


void eshinit(void)
{
	ttyinit();
	stdio_uart_open();
	prompt();
}

/* EOF */

