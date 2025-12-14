
/* delay.c */

#include "delay.h"

__weak void dlyinit(void)
{
}

void msec(DELAY_UWORD ms)
{
	DELAY_UWORD ms_ = ms;
	if (ms_ == 0)
		return;
	else if (ms_ == 1)
		usec(1000);
	else {
		while (ms_ > 0) {
			usec(1000);
			ms_--;
		}
	}
}

__weak void usec(DELAY_UWORD us)
{
}

/* EOF */

