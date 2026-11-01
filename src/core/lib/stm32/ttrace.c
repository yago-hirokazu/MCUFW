/* ttrace.c */

#include "ttrace.h"
#include "ttrace_adapt.h"

#if TTRACE_DEBUG == 1
#include "debugout.h"
#include "delay.h"
#include "io.h"
#endif
#if TTRACE_DEBUG == 1
#define TTRACE_DEBUG_ENTER() DEBUGOUT1_H()
#define TTRACE_DEBUG_LEAVE() DEBUGOUT1_L()
#else
#define TTRACE_DEBUG_ENTER()
#define TTRACE_DEBUG_LEAVE()
#endif

uint32_t t_ms;
uint32_t t_count;

void ttrinit(void)
{
	uint32_t fCK_PSC = TTRACE_TIMx_fPCLK();

	TTRACE_TIMx_CLK_EN();

	TTRACE_TIMx->PSC = (fCK_PSC/1000000) - 1; // generate 1 MHz fCK_CNT
	TTRACE_TIMx->ARR = 0x3E8-1; // generate interrupt at every 1000 us = 1 ms
	TTRACE_TIMx->CNT = 0;
	TTRACE_TIMx->EGR = TIM_EGR_UG;
	while (!(TTRACE_TIMx->SR & TIM_SR_UIF))
	{}
	TTRACE_TIMx->SR &= ~TIM_SR_UIF;

	NVIC_SetPriority(TTRACE_TIMx_IRQn, TTRACE_TIMx_NVIC_PRI);
	NVIC_EnableIRQ(TTRACE_TIMx_IRQn);
	TTRACE_TIMx->DIER = TIM_DIER_UIE;

	t_count = 0;
}

void ttrterm(void)
{
	TTRACE_TIMx->CR1 &= ~TIM_CR1_CEN;
	TTRACE_TIMx->DIER &= ~TIM_DIER_UIE;
	NVIC_ClearPendingIRQ(TTRACE_TIMx_IRQn);
	TTRACE_TIMx->SR &= ~TIM_SR_UIF;
	NVIC_DisableIRQ(TTRACE_TIMx_IRQn);
	TTRACE_TIMx_CLK_DI();
}

void ttrstart(void)
{
	if (t_count++ == 0)
	{
		t_ms = 0;
		TTRACE_TIMx->CNT  = 0;
		TTRACE_TIMx->CR1 |= TIM_CR1_CEN;
	}
}

uint32_t ttrstop(void)
{
	if (--t_count == 0)
	{
		uint32_t t_us, us;
		TTRACE_TIMx->CR1 &= ~TIM_CR1_CEN;
		us = TTRACE_TIMx->CNT;
		t_us = t_ms * 1000 + us;
		return t_us;
	}
	return 0xFFFFFFFF;
}

void ttraceint(void)
{
	TTRACE_DEBUG_ENTER();

	NVIC_ClearPendingIRQ(TTRACE_TIMx_IRQn);
	TTRACE_TIMx->SR &= ~TIM_SR_UIF;

	t_ms++;

	TTRACE_DEBUG_LEAVE();
}

/* EOF */

