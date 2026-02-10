#include "stm32f4xx.h"
#include <stdio.h>
#include "led.h"
#include "usart.h"
#include "task.h"

TaskHandle_t hTask1 = NULL;
TaskHandle_t hTask2 = NULL;
TaskHandle_t hTask3 = NULL;

/*===========================================================
 *  Task1：每 500ms 打印一次（优先级 1）
 *===========================================================*/
void Task1_Red(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[R] count=%d tick=%d\r\n",
                    (int)count++, (int)xTaskGetTickCount());

        vTaskDelay(500); /* 阻塞 500ms，让出 CPU */
    }
}

/*===========================================================
 *  Task2：每 1000ms 打印一次（优先级 1）
 *===========================================================*/
void Task2_Blue(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[B] count=%d tick=%d\r\n",
                    (int)count++, (int)xTaskGetTickCount());

        vTaskDelay(1000); /* 阻塞 1000ms */
    }
}

/*===========================================================
 *  Task3：每 2000ms 打印一次（优先级 2，更高）
 *===========================================================*/
void Task3_Green(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[G] count=%d tick=%d\r\n",
                    (int)count++, (int)xTaskGetTickCount());

        vTaskDelay(2000); /* 阻塞 2000ms */
    }
}

/*===========================================================
 *  主函数
 *===========================================================*/
int main(void)
{
    UART_Init(115200);
    LED_Init();

    printf("\r\n\r\n");
    printf("===========================\r\n");
    printf("  MiniRTOS Phase 4 Test\r\n");
    printf("===========================\r\n\r\n");

    xTaskCreate(Task1_Red, "Task1", 128, NULL, 1, &hTask1);
    xTaskCreate(Task2_Blue, "Task2", 128, NULL, 1, &hTask2);
    xTaskCreate(Task3_Green, "Task3", 128, NULL, 2, &hTask3);

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}