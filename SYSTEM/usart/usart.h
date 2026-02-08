#ifndef USART_H
#define USART_H

#include <stdint.h>

void UART_Init(uint32_t baudrate);
void UART_SendChar(char c);
void UART_SendString(const char *str);

#endif