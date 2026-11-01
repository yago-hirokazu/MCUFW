
/* delay.c */

#include "delay.h"
#include "delay_adapt.h"

void dlyinit(void)
{
	u32 prescaler;
	prescaler = (u32)(DELAY_TIMx_fCKPSC / DELAY_TIMx_fCKCNT) - 1;

	DELAY_TIMx_CLK_ENABLE();
	DELAY_TIMx->PSC = prescaler;
	DELAY_TIMx->ARR = 0;
	DELAY_TIMx->EGR = TIM_EGR_UG;
	while (!(DELAY_TIMx->SR & TIM_SR_UIF))
	{}
	DELAY_TIMx->SR &= ~TIM_SR_UIF;
	DELAY_TIMx->CNT = 0;
}

void usec(DELAY_UWORD us)
{
	if (us == 0)
		us = 1;
	DELAY_TIMx->ARR = us;
	DELAY_TIMx->CR1 |= TIM_CR1_CEN;
	while (!(DELAY_TIMx->SR & TIM_SR_UIF))
	{}
	DELAY_TIMx->CR1 &= ~TIM_CR1_CEN;
	DELAY_TIMx->SR &= ~TIM_SR_UIF;
	DELAY_TIMx->CNT = 0;
}

/* EOF */

