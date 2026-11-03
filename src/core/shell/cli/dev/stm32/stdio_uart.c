
/* stdio_uart.c */

#include "machdep.h"
#include "qbyte.h"
#include "stdio_uart.h"
#include "stdio_uart_adapt.h"

#if DEBUG_STDIO_UART
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt "\n",	\
	       __FNAME__,__func__,__LINE__,	\
	       ##__VA_ARGS__			\
		)
#else
#define DP(fmt, ...)
#endif


/**
  Get a character from the stdio
  \return     The next character from the input, or -1 on read error.
*/
int stdio_uart_stdout_putchar(int ch)
{
	if (stdio_uart_run == 0)
		return -1;

	char c;
	u32 active_isr_num = SCB->ICSR & 0x0000001f;

	if (ch == '\n')
		stdio_uart_stdout_putchar('\r');

	ch &= 0x7f;
#if 1
	if (active_isr_num == 3     /* HardFault */
	    || active_isr_num == 4  /* MemManage */
	    || active_isr_num == 5  /* BusFault */
	    || active_isr_num == 6  /* UsageFault */
		) {
		while (stdio_uart_adapt_Tx_buffer_empty() == false) {}
		stdio_uart_adapt_Tx_char(ch);
		while (stdio_uart_adapt_Tx_char_complete() == false) {}
	}
	else
#endif
	{
		register int count = 0;
		c = qbput(ch, &stdio_uart_txq);
		while (c == 0 && count < 0xFFFF) {

			/* Don't stick to output all
			 * characters if this is being
			 * executed within an IRQ handler.
			 */
			if (active_isr_num != 0)
				break;

			stdio_uart_adapt_enable_Tx_IRQ();
			c = qbput(ch, &stdio_uart_txq);
			count++;
		}
		stdio_uart_adapt_enable_Tx_IRQ();
	}

	return ch;
}

/**
  Put a character to the stdout
  \param[in] ch Character to output
  \return       The character written, or -1 on write error.
*/
int stdio_uart_stdin_getchar(void)
{
	return (qbget(&stdio_uart_rxq));
}


#if STDIO_UART_RX_BY_DMA
static bool not_yet_rx_started(u32 rp, u32 ndtr)
{
	if (rp == 0 && ndtr == 0)
		return true;
	return false;
}
#endif



void stdio_uart_start_rx(void)
{
#if STDIO_UART_RX_BY_DMA
	stdio_uart_rx_fifo_rp = 0;
	stdio_uart_adapt_start_Rx_DMA(&stdio_uart_rx_fifo[0],
				      STDIO_UART_RX_FIFO_SIZE);
	stdio_uart_adapt_enable_Rx_IRQ();
#else
	stdio_uart_adapt_start_Rx_IRQ(&stdio_uart_rx_fifo[0], 1);
#endif
}

void stdio_uart_tx_int(void)
{
	char c = qbget(&stdio_uart_txq);
	if (c) {
		stdio_uart_adapt_Tx_char(c & 0x7f);

// UART_EndTransmit_IT() disable TCIE interrupt,
// re-enable it until qbget(&outq) return 0
		stdio_uart_adapt_enable_Tx_IRQ();
	} else {
		stdio_uart_adapt_Tx_char(0);
// UART_EndTransmit_IT() disable TCIE interrupt,
// so don't need to do it here.
//		USARTx->CR1 &= ~USART_CR1_TCIE;
	}
}

void stdio_uart_rx_int(void)
{
#if !STDIO_UART_RX_BY_DMA
// RXNE status has already been cleared in HAL_UART_IRQHandler()
//	if (stdio_uart_adapt_Rx_buffer_not_empty() == true) {
		char c = (char)stdio_uart_adapt_get_Rx_char();
		qbput(c, &rawq); /* cache key code */
//	}
	stdio_uart_adapt_start_Rx_IRQ(&rxfifo[0], 1);
#endif
}

void stdio_uart_open(void)
{
	stdio_uart_run = 1;
	qbinit(&stdio_uart_rxq, &stdio_uart_rxbuf[0], STDIO_UART_RX_BUFS);
	qbinit(&stdio_uart_txq, &stdio_uart_txbuf[0], STDIO_UART_TX_BUFS);
	stdio_uart_start_rx();
}

void stdio_uart_close(void)
{
	stdio_uart_run = 0;
}

void stdio_uart_term(void)
{
	stdio_uart_adapt_term();
}

int stdio_uart_empty(void)
{
	return qbempty(&stdio_uart_txq);
}

void stdio_uart_flush(void)
{
	stdio_uart_adapt_enable_Tx_IRQ();
}

void stdio_uart_isr(void)
{
#if STDIO_UART_RX_BY_DMA

	uint32_t *NDTR = stdio_uart_adapt_get_pointer_to_NDTR();
	uint32_t rx_fifo_wp = STDIO_UART_RX_FIFO_SIZE - *NDTR;

	if (not_yet_rx_started(stdio_uart_rx_fifo_rp, *NDTR) == true)
		return;

	// Read data from Rx FIFO if it is not empty and
	// put the data to raw queue
	while (stdio_uart_rx_fifo_rp != rx_fifo_wp) {
		char c = stdio_uart_rx_fifo[stdio_uart_rx_fifo_rp];
		qbput(c, &stdio_uart_rxq);

		stdio_uart_rx_fifo_rp += 1;
		stdio_uart_rx_fifo_rp &= STDIO_UART_RX_FIFO_ROUND;

		rx_fifo_wp = STDIO_UART_RX_FIFO_SIZE - *NDTR;
	}

#endif
}


/* EOF */

