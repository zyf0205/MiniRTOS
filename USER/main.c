#include "stm32f4xx.h"
#include <stdio.h>
#include "led.h"
#include "usart.h"
#include "task.h"

/* 保存任务句柄，用于挂起/恢复/删除 */
TaskHandle_t hTask1 = NULL;
TaskHandle_t hTask2 = NULL;
TaskHandle_t hTask3 = NULL;

void Task1_Red(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[R] count=%d tick=%d\r\n",
                    (int)count, (int)xTaskGetTickCount());
        for (volatile uint32_t i = 0; i < 500000; i++);

        count++;

        if (count == 3) {
            vSafePrintf(">>> Suspend Task2\r\n");
            vTaskSuspend(hTask2);
        }
        if (count == 6) {
            vSafePrintf(">>> Resume Task2\r\n");
            vTaskResume(hTask2);
        }
        if (count == 8) {
            vSafePrintf(">>> Suspend myself\r\n");
            vTaskSuspend(NULL);
            vSafePrintf(">>> I'm back!\r\n");
        }
    }
}

void Task2_Blue(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[B] count=%d tick=%d\r\n",
                    (int)count++, (int)xTaskGetTickCount());
        for (volatile uint32_t i = 0; i < 500000; i++);
    }
}

void Task3_Green(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[G] count=%d tick=%d\r\n",
                    (int)count, (int)xTaskGetTickCount());
        for (volatile uint32_t i = 0; i < 500000; i++);

        count++;

        if (count == 12) {
            vSafePrintf(">>> Resume Task1\r\n");
            vTaskResume(hTask1);
        }
        if (count == 14) {
            vSafePrintf(">>> delete Task1\r\n");
            vTaskDelete(hTask1);
        }
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
    printf("  MiniRTOS Phase 3 Test\r\n");
    printf("===========================\r\n\r\n");

    /* 三个任务同优先级 */
    xTaskCreate(Task1_Red, "Task1", 128, NULL, 1, &hTask1);
    xTaskCreate(Task2_Blue, "Task2", 128, NULL, 1, &hTask2);
    xTaskCreate(Task3_Green, "Task3", 128, NULL, 1, &hTask3);

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}