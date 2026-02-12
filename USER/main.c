#include "stm32f4xx.h"
#include <stdio.h>
#include "usart.h"
#include "task.h"
#include "heap.h"

void Task1(void *param)
{
    (void)param;
    while (1) {
        vSafePrintf("[T1] heap_free=%d tick=%d\r\n",
                    (int)xPortGetFreeHeapSize(),
                    (int)xTaskGetTickCount());
        vTaskDelay(1000);
    }
}

void Task2(void *param)
{
    (void)param;

    vSafePrintf("[T2] alive! heap_free=%d\r\n",
                (int)xPortGetFreeHeapSize());
    vTaskDelay(3000);

    vSafePrintf("[T2] deleting myself! heap_free=%d\r\n",
                (int)xPortGetFreeHeapSize());
    vTaskDelete(NULL);
}

int main(void)
{
    UART_Init(115200);

    printf("\r\n\r\n");
    printf("=========================\r\n");
    printf("  MiniRTOS Phase C Test\r\n");
    printf("=========================\r\n\r\n");

    printf("Heap free: %d\r\n", (int)xPortGetFreeHeapSize());

    xTaskCreate(Task1, "Task1", 256, NULL, 1, NULL);
    xTaskCreate(Task2, "Task2", 256, NULL, 1, NULL);

    printf("After create, Heap free: %d\r\n\r\n", (int)xPortGetFreeHeapSize());

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}