#include "stm32f4xx.h"
#include <stdio.h>
#include "led.h"
#include "usart.h"
#include "task.h"
#include "queue.h"
#include "sem.h"

/*===========================================================
 *  测试 1：二值信号量
 *
 *  TaskGiver 每 1000ms Give 一次
 *  TaskTaker 一直 Take 等待
 *===========================================================*/

SemaphoreHandle_t xBinarySem = NULL;

void TaskGiver(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vTaskDelay(1000);
        count++;

        xSemaphoreGive(xBinarySem);
        vSafePrintf("[Giver] Give #%d  tick=%d\r\n",
                    (int)count, (int)xTaskGetTickCount());
    }
}

void TaskTaker(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        if (xSemaphoreTake(xBinarySem, portMAX_DELAY) == 0) {
            count++;
            vSafePrintf("[Taker] Take #%d  tick=%d\r\n",
                        (int)count, (int)xTaskGetTickCount());
        }
    }
}

/*===========================================================
 *  测试 2：计数信号量（模拟 3 个停车位）
 *  先注释掉，测试 1 通过后再打开
 *===========================================================*/

SemaphoreHandle_t xParkingSem = NULL;

void TaskCar(void *param)
{
    uint32_t id = (uint32_t)param;

    while (1) {
        vSafePrintf("[Car%d] Waiting for parking...\r\n", (int)id);

        if (xSemaphoreTake(xParkingSem, 5000) == 0) {
            vSafePrintf("[Car%d] Parked!  free=%d  tick=%d\r\n",
                        (int)id,
                        (int)uxSemaphoreGetCount(xParkingSem),
                        (int)xTaskGetTickCount());

            vTaskDelay(2000 + id * 500);

            xSemaphoreGive(xParkingSem);
            vSafePrintf("[Car%d] Left!    free=%d  tick=%d\r\n",
                        (int)id,
                        (int)uxSemaphoreGetCount(xParkingSem),
                        (int)xTaskGetTickCount());
        } else {
            vSafePrintf("[Car%d] TIMEOUT! No space!\r\n", (int)id);
        }

        vTaskDelay(500);
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
    printf("  MiniRTOS Phase 6+7 Test\r\n");
    printf("===========================\r\n\r\n");

    /* 测试 1：二值信号量 */
    // xBinarySem = xSemaphoreCreateBinary();
    // xTaskCreate(TaskGiver, "Giver", 128, NULL, 1, NULL);
    // xTaskCreate(TaskTaker, "Taker", 128, NULL, 2, NULL);

    /* 测试 2：计数信号量（先注释） */

    xParkingSem = xSemaphoreCreateCounting(3, 3);
    xTaskCreate(TaskCar, "Car1", 128, (void *)1, 1, NULL);
    xTaskCreate(TaskCar, "Car2", 128, (void *)2, 1, NULL);
    xTaskCreate(TaskCar, "Car3", 128, (void *)3, 1, NULL);
    xTaskCreate(TaskCar, "Car4", 128, (void *)4, 1, NULL);

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}