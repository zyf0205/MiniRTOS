#include "led.h"
#include "stm32f4xx.h"

/* PA0=R  PA1=G  PA2=B  低电平点亮 */
#define LED_PINS (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2)

static uint8_t current_color = LED_COLOR_OFF;

void LED_Init(void)
{
  GPIO_InitTypeDef gpio;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

  gpio.GPIO_Pin = LED_PINS;
  gpio.GPIO_Mode = GPIO_Mode_OUT;
  gpio.GPIO_OType = GPIO_OType_PP;
  gpio.GPIO_Speed = GPIO_Speed_2MHz;
  gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOA, &gpio);

  LED_Off();
}

void LED_SetColor(uint8_t color)
{
  current_color = color;

  /* 先全灭（全部置高） */
  GPIO_SetBits(GPIOA, LED_PINS);

  /* 按位判断，低电平点亮 */
  if (color & 0x01)
    GPIO_ResetBits(GPIOA, GPIO_Pin_0); /* R */
  if (color & 0x02)
    GPIO_ResetBits(GPIOA, GPIO_Pin_1); /* G */
  if (color & 0x04)
    GPIO_ResetBits(GPIOA, GPIO_Pin_2); /* B */
}

void LED_Off(void)
{
  current_color = LED_COLOR_OFF;
  GPIO_SetBits(GPIOA, LED_PINS);
}

void LED_Toggle(void)
{
  if (current_color == LED_COLOR_OFF)
    return;

  /* 读当前输出，对使用中的引脚取反 */
  if (current_color & 0x01)
    GPIO_ToggleBits(GPIOA, GPIO_Pin_0);
  if (current_color & 0x02)
    GPIO_ToggleBits(GPIOA, GPIO_Pin_1);
  if (current_color & 0x04)
    GPIO_ToggleBits(GPIOA, GPIO_Pin_2);
}