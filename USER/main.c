#include "stm32f4xx.h"
#include <stdio.h>
#include "led.h"
#include "usart.h"
#include "task.h"
#include "queue.h"

/* 队列句柄 */
QueueHandle_t xQueue1 = NULL;

/*===========================================================
 *  测试 1：生产者-消费者
 *
 *  Producer 每 500ms 发一个数
 *  Consumer 一直等待接收
 *===========================================================*/

void TaskProducer(void *param)
{
    (void)param;
    uint32_t txData = 0;

    while (1)
    {
        txData++;

        if (xQueueSend(xQueue1, &txData, 1000) == 0)
        {
            vSafePrintf("[P] Sent: %d  tick=%d\r\n",
                        (int)txData, (int)xTaskGetTickCount());
        }
        else
        {
            vSafePrintf("[P] Send FAILED (queue full)\r\n");
        }

        vTaskDelay(500);
    }
}

void TaskConsumer(void *param)
{
    (void)param;
    uint32_t rxData = 0;

    while (1)
    {
        /* 最多等 2000ms */
        if (xQueueReceive(xQueue1, &rxData, 2000) == 0)
        {
            vSafePrintf("[C] Recv: %d  tick=%d\r\n",
                        (int)rxData, (int)xTaskGetTickCount());
        }
        else
        {
            vSafePrintf("[C] Recv TIMEOUT\r\n");
        }
    }
}

/*===========================================================
 *  测试 2：队列满阻塞
 *
 *  FastProducer 每 200ms 发一个数
 *  SlowConsumer 每 1000ms 收一个数
 *  队列容量 = 3，会被塞满
 *===========================================================*/


void TaskFastProducer(void *param)
{
    (void)param;
    uint32_t txData = 0;

    while (1)
    {
        txData++;

        vSafePrintf("[FP] Trying to send %d...\r\n", (int)txData);

        if (xQueueSend(xQueue1, &txData, 5000) == 0)
        {
            vSafePrintf("[FP] Sent: %d  tick=%d  q=%d\r\n",
                        (int)txData, (int)xTaskGetTickCount(),
                        (int)uxQueueMessagesWaiting(xQueue1));
        }
        else
        {
            vSafePrintf("[FP] Send TIMEOUT\r\n");
        }

        vTaskDelay(200);
    }
}

void TaskSlowConsumer(void *param)
{
    (void)param;
    uint32_t rxData = 0;

    while (1)
    {
        vTaskDelay(1000);

        if (xQueueReceive(xQueue1, &rxData, 0) == 0)
        {
            vSafePrintf("[SC] Recv: %d  tick=%d  q=%d\r\n",
                        (int)rxData, (int)xTaskGetTickCount(),
                        (int)uxQueueMessagesWaiting(xQueue1));
        }
        else
        {
            vSafePrintf("[SC] Queue empty\r\n");
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
    printf("  MiniRTOS Phase 5 Test\r\n");
    printf("===========================\r\n\r\n");

    /* 创建队列：容量 5，每个元素 4 字节 */
    xQueue1 = xQueueCreate(5, sizeof(uint32_t));

    if (xQueue1 == NULL)
    {
        printf("Queue create FAILED!\r\n");
        while (1);
    }
    printf("Queue created OK\r\n\r\n");

    /* 测试 1：生产者消费者 */
    // xTaskCreate(TaskProducer, "Prod", 128, NULL, 1, NULL);
    // xTaskCreate(TaskConsumer, "Cons", 128, NULL, 2, NULL);

    /* 测试 2：先注释掉，测试 1 通过后再打开 */
    xTaskCreate(TaskFastProducer, "FProd", 128, NULL, 1, NULL);
    xTaskCreate(TaskSlowConsumer, "SCons", 128, NULL, 2, NULL);

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}