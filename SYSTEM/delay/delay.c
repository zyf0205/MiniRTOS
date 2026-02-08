#include "delay.h"
#include "stm32f4xx.h"

/*
 * 使用 SysTick 轮询方式实现精确延时
 * 注意：后续 RTOS 阶段会接管 SysTick，届时这个延时就不用了
 */

static uint32_t fac_us; /* 每微秒的计数值 */

void Delay_Init(void)
{
	/* SysTick 时钟 = HCLK/8 = 100MHz/8 = 12.5MHz（STM32F411 主频100MHz）*/
	/* 如果你的主频是其他值，这里会自动适配 */
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
	fac_us = SystemCoreClock / 8000000; /* 每us的计数次数 */
}

void delay_us(uint32_t us)
{
	uint32_t temp;

	SysTick->LOAD = (uint32_t)(us * fac_us - 1);
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

	do
	{
		temp = SysTick->CTRL;
	} while ((temp & SysTick_CTRL_ENABLE_Msk) &&
					 !(temp & SysTick_CTRL_COUNTFLAG_Msk));

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->VAL = 0;
}

void delay_ms(uint32_t ms)
{
	while (ms--)
	{
		delay_us(1000);
	}
}