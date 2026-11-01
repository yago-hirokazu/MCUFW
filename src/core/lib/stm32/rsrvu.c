
/* rsrvu.c */

#include "rsrvu.h"
#include "rsrvu_adapt.h"

void (*rsrvu_proc)(void);

void rsrvuinit(void)
{
	uint32_t prescaler;
	prescaler = (u32)(RSRVU_TIMx_fCKPSC() / RSRVU_TIMx_fCKCNT) - 1;

	RSRVU_TIMx_CLK_EN();

	RSRVU_TIMx->PSC = prescaler;
	RSRVU_TIMx->CNT = 0;
	RSRVU_TIMx->ARR = 0;
	RSRVU_TIMx->EGR = TIM_EGR_UG;
	while (!(RSRVU_TIMx->SR & TIM_SR_UIF))
	{}
	RSRVU_TIMx->SR &= (uint32_t)~TIM_SR_UIF;

	NVIC_SetPriority(RSRVU_TIMx_IRQn, RSRVU_TIMx_NVIC_PRI);
	NVIC_EnableIRQ(RSRVU_TIMx_IRQn);
	RSRVU_TIMx->DIER = TIM_DIER_UIE;
}

void rsrvu(uint32_t us, void (*proc)(void))
{
	RSRVU_TIMx->SR &= (uint32_t)~TIM_SR_UIF;

	rsrvu_proc = proc;

	if (us == 0)
		us = 1;
	RSRVU_TIMx->CNT = 0;
	RSRVU_TIMx->ARR = us;

	RSRVU_TIMx->CR1 |= TIM_CR1_CEN;
}

void rsrvustop(void)
{
	RSRVU_TIMx->CR1 &= (uint32_t)~TIM_CR1_CEN;
	RSRVU_TIMx->SR &= (uint32_t)~TIM_SR_UIF;
}

void rsrvuint(void)
{
	RSRVU_TIMx->CR1 &= (uint32_t)~TIM_CR1_CEN;
	RSRVU_TIMx->SR &= (uint32_t)~TIM_SR_UIF;
	if (rsrvu_proc != NULL)
		rsrvu_proc();
}
/* EOF */

