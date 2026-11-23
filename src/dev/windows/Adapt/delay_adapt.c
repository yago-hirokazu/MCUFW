
/* delay_adapt.c */

#include "delay.h"

void dlyinit(void)
{
	return; /* do nothing */
}

void msec(DELAY_UWORD ms)
{
	Sleep(ms);
}

void usec(DELAY_UWORD us)
{
	LARGE_INTEGER lFrequency;
	LARGE_INTEGER lEndTime;
	LARGE_INTEGER lCurTime;

	lFrequency.QuadPart = 0;
	lEndTime.QuadPart = 0;
	lCurTime.QuadPart = 0;

	QueryPerformanceFrequency(&lFrequency);
	lFrequency.QuadPart /= 1000000;
	if (lFrequency.QuadPart) {
		QueryPerformanceCounter (&lEndTime);
		lEndTime.QuadPart += (LONGLONG)us * lFrequency.QuadPart;
		do {
			QueryPerformanceCounter (&lCurTime);
			/* Run other threads being ready to run */
			/* Sleep(0); */
		} while (lCurTime.QuadPart < lEndTime.QuadPart);
	}
}

/* EOF */

