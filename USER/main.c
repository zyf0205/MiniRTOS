#include "stm32f4xx.h"
#include <stdio.h>
#include "led.h"
#include "usart.h"
#include "task.h"
#include "mutex.h"

MutexHandle_t xMutex = NULL;

/*===========================================================
 *  测试 1：基本互斥
 *
 *  两个任务抢同一个互斥量
 *  持有锁期间模拟"干活"，释放后让出
 *===========================================================*/

void TaskA(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[A] Waiting for mutex...\r\n");

        if (xMutexTake(xMutex, portMAX_DELAY) == 0) {
            count++;
            vSafePrintf("[A] Got mutex #%d  tick=%d\r\n",
                        (int)count, (int)xTaskGetTickCount());

            /* 模拟干活 */
            vTaskDelay(500);

            vSafePrintf("[A] Releasing mutex  tick=%d\r\n",
                        (int)xTaskGetTickCount());
            xMutexGive(xMutex);
        }

        vTaskDelay(100);
    }
}

void TaskB(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1) {
        vSafePrintf("[B] Waiting for mutex...\r\n");

        if (xMutexTake(xMutex, portMAX_DELAY) == 0) {
            count++;
            vSafePrintf("[B] Got mutex #%d  tick=%d\r\n",
                        (int)count, (int)xTaskGetTickCount());

            vTaskDelay(300);

            vSafePrintf("[B] Releasing mutex  tick=%d\r\n",
                        (int)xTaskGetTickCount());
            xMutexGive(xMutex);
        }

        vTaskDelay(100);
    }
}

/*===========================================================
 *  测试 2：优先级继承
 *
 *  TaskH（高优先级）和 TaskL（低优先级）抢互斥量
 *  TaskM（中优先级）不用互斥量，只管跑
 *
 *  如果优先级继承生效：
 *    TaskL 持有锁时被提升到高优先级
 *    TaskM 抢不了 TaskL
 *    TaskH 等待时间短
 *
 *  先注释掉，测试 1 通过后再打开
 *===========================================================*/


void TaskH_High(void *param)
{
    (void)param;

    vTaskDelay(200);

    while (1)
    {
        vSafePrintf("[H] Waiting for mutex... tick=%d\r\n",
                    (int)xTaskGetTickCount());

        if (xMutexTake(xMutex, portMAX_DELAY) == 0)
        {
            vSafePrintf("[H] Got mutex!  tick=%d\r\n",
                        (int)xTaskGetTickCount());

            vTaskDelay(200);

            vSafePrintf("[H] Releasing mutex  tick=%d\r\n",
                        (int)xTaskGetTickCount());
            xMutexGive(xMutex);
        }

        vTaskDelay(1000);
    }
}

void TaskM_Mid(void *param)
{
    (void)param;
    uint32_t count = 0;

    while (1)
    {
        count++;
        vSafePrintf("[M] Running #%d  tick=%d\r\n",
                    (int)count, (int)xTaskGetTickCount());
        vTaskDelay(100);
    }
}

void TaskL_Low(void *param)
{
    (void)param;

    while (1)
    {
        vSafePrintf("[L] Taking mutex... tick=%d\r\n",
                    (int)xTaskGetTickCount());

        if (xMutexTake(xMutex, portMAX_DELAY) == 0)
        {
            vSafePrintf("[L] Got mutex, working... tick=%d\r\n",
                        (int)xTaskGetTickCount());

            vTaskDelay(500);

            vSafePrintf("[L] Releasing mutex  tick=%d\r\n",
                        (int)xTaskGetTickCount());
            xMutexGive(xMutex);
        }

        vTaskDelay(200);
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
    printf("  MiniRTOS Phase 8 Test\r\n");
    printf("===========================\r\n\r\n");

    xMutex = xMutexCreate();
    if (xMutex == NULL) {
        printf("Mutex create FAILED!\r\n");
        while (1);
    }
    printf("Mutex created OK\r\n\r\n");

    /* 测试 1：基本互斥 */
    // xTaskCreate(TaskA, "TaskA", 128, NULL, 1, NULL);
    // xTaskCreate(TaskB, "TaskB", 128, NULL, 1, NULL);

    /* 测试 2：优先级继承（先注释） */
    
    xTaskCreate(TaskH_High, "TaskH", 128, NULL, 3, NULL);
    xTaskCreate(TaskM_Mid,  "TaskM", 128, NULL, 2, NULL);
    xTaskCreate(TaskL_Low,  "TaskL", 128, NULL, 1, NULL);
    

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}