
/* tick_adaptc */

#include "tick.h"

uint32_t ticks = 0;
UINT tickid = 0;

static void CALLBACK tick(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser,
			  DWORD_PTR dw1, DWORD_PTR dw2)
{
	ticks++;
}

int tickinit(void)
{
	return EXIT_SUCCESS; /* do nothing */
}

int tickstart(void)
{
	timeBeginPeriod(1);
	tickid = timeSetEvent(1, 1, tick, 0, TIME_PERIODIC);
	if (tickid == 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

int tickstop(void)
{
	UINT ret = TIMERR_NOERROR;
	ret = timeKillEvent(tickid);
	if (ret != TIMERR_NOERROR)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

uint32_t tickget(void)
{
	return ticks;
}


/* EOF */

