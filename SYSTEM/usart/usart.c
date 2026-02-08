#include "usart.h"
#include "stm32f4xx.h"
#include <stdio.h>

/*
 * USART1: TX=PA9  RX=PA10
 * 挂在 APB2 上
 */

void UART_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;

	/* 开时钟 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	/* PA9 PA10 复用为 USART1 */
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	/* PA9=TX  PA10=RX */
	gpio.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &gpio);

	/* USART 配置 */
	usart.USART_BaudRate = baudrate;
	usart.USART_WordLength = USART_WordLength_8b;
	usart.USART_StopBits = USART_StopBits_1;
	usart.USART_Parity = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &usart);

	USART_Cmd(USART1, ENABLE);
}

void UART_SendChar(char c)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
		;
	USART_SendData(USART1, (uint16_t)c);
}

void UART_SendString(const char *str)
{
	while (*str)
	{
		UART_SendChar(*str++);
	}
}

/* printf 重定向 */
int fputc(int ch, FILE *f)
{
	(void)f;
	UART_SendChar((char)ch);
	return ch;
}