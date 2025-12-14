
/* stdio_uart.h */

#ifndef __STDIO_UART_H__
#define __STDIO_UART_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

int	stdio_uart_stdout_putchar(int ch);
int	stdio_uart_stdin_getchar(void);
void	stdio_uart_start_rx(void);
void	stdio_uart_tx_int(void);
void	stdio_uart_rx_int(void);
void	stdio_uart_open(void);
void	stdio_uart_close(void);
int	stdio_uart_empty(void);
void	stdio_uart_flush(void);
void	stdio_uart_isr(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
