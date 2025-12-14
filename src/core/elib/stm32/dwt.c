
/* dwt.c */

#include "dwt.h"

void dwtstart(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void dwtstop(void)
{
	DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
}

void dwtreset(void)
{
	DWT->CYCCNT = 0;
}

uint32_t dwtcount(void)
{
	return DWT->CYCCNT;
}

double dwtms(uint32_t scycnt, uint32_t ecycnt)
{
  double time = ((double)ecycnt-(double)scycnt)/(double)SystemCoreClock;
  time *= 1000; // ms
  return time;
}

double dwtus(uint32_t scycnt, uint32_t ecycnt)
{
  double time = ((double)ecycnt-(double)scycnt)/(double)SystemCoreClock;
  time *= 1000000; // ms
  return time;
}

/* EOF */

