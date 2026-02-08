#ifndef LED_H
#define LED_H

#include <stdint.h>

/* 颜色定义：用 bit0=R  bit1=G  bit2=B 组合 */
#define LED_COLOR_OFF 0x00
#define LED_COLOR_RED 0x01
#define LED_COLOR_GREEN 0x02
#define LED_COLOR_BLUE 0x04
#define LED_COLOR_YELLOW 0x03 /* R+G */
#define LED_COLOR_CYAN 0x06   /* G+B */
#define LED_COLOR_PURPLE 0x05 /* R+B */
#define LED_COLOR_WHITE 0x07  /* R+G+B */

void LED_Init(void);
void LED_SetColor(uint8_t color);
void LED_Off(void);
void LED_Toggle(void);

#endif