/* ctrace.c (Clock TRACE) */

#include "ctrace.h"

#if CTRACE_DEBUG == 1
#include "debugout.h"
#include "delay.h"
#include "io.h"
#endif
#if CTRACE_DEBUG == 1
#define CTRACE_DEBUG_ENTER() DEBUGOUT1_H()
#define CTRACE_DEBUG_LEAVE() DEBUGOUT1_L()
#else
#define CTRACE_DEBUG_ENTER()
#define CTRACE_DEBUG_LEAVE()
#endif

uint32_t c_count, c_overflows;

void ctrinit(void)
{
	uint32_t fCK_PSC = CTRACE_TIMx_fPCLK();

	CTRACE_TIMx_CLK_EN();

	CTRACE_TIMx->PSC = 0; // Run with system clock frequency
	CTRACE_TIMx->ARR = 0xFFFFFFFF; // We can measure clocks for 53 sec
	CTRACE_TIMx->CNT = 0;
	CTRACE_TIMx->EGR = TIM_EGR_UG;
	while (!(CTRACE_TIMx->SR & TIM_SR_UIF))
	{}
	CTRACE_TIMx->SR &= ~TIM_SR_UIF;

	NVIC_SetPriority(CTRACE_TIMx_IRQn, CTRACE_TIMx_NVIC_PRI);
	NVIC_EnableIRQ(CTRACE_TIMx_IRQn);
	CTRACE_TIMx->DIER = TIM_DIER_UIE;

	c_count = 0;
}

void ctrterm(void)
{
	CTRACE_TIMx->CR1 &= ~TIM_CR1_CEN;
	CTRACE_TIMx->DIER &= ~TIM_DIER_UIE;
	NVIC_ClearPendingIRQ(CTRACE_TIMx_IRQn);
	CTRACE_TIMx->SR &= ~TIM_SR_UIF;
	NVIC_DisableIRQ(CTRACE_TIMx_IRQn);
	CTRACE_TIMx_CLK_DI();
}

void ctrstart(void)
{
	if (c_count++ == 0)
	{
		CTRACE_TIMx->CNT  = 0;
		CTRACE_TIMx->CR1 |= TIM_CR1_CEN;
	}
}

uint32_t ctrstop(void)
{
	if (--c_count == 0)
	{
		uint32_t clks=0;
		CTRACE_TIMx->CR1 &= ~TIM_CR1_CEN;
		clks = CTRACE_TIMx->CNT;
		return clks;
	}
	return 0xFFFFFFFF;
}

void ctraceint(void)
{
	CTRACE_DEBUG_ENTER();

	NVIC_ClearPendingIRQ(CTRACE_TIMx_IRQn);
	CTRACE_TIMx->SR &= ~TIM_SR_UIF;

	c_overflows++;

	CTRACE_DEBUG_LEAVE();
}

/* EOF */

