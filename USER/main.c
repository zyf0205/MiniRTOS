#include "stm32f4xx.h"
#include <stdio.h>
#include "usart.h"
#include "task.h"
#include "heap.h"

void TaskMalloc(void *param)
{
    (void)param;
    uint32_t *p1, *p2, *p3;

    while (1) {
        vSafePrintf("--- Heap Test ---\r\n");
        vSafePrintf("Free: %d\r\n", (int)xPortGetFreeHeapSize());

        /* 分配三块 */
        p1 = pvPortMalloc(100);
        p2 = pvPortMalloc(200);
        p3 = pvPortMalloc(300);

        vSafePrintf("Alloc 100+200+300, Free: %d\r\n",
                    (int)xPortGetFreeHeapSize());

        if (p1) *p1 = 0xAAAAAAAA;
        if (p2) *p2 = 0xBBBBBBBB;
        if (p3) *p3 = 0xCCCCCCCC;

        if (p1)
            vSafePrintf("p1=0x%08lX val=0x%08lX\r\n", (unsigned long)p1, (unsigned long)*p1);
        else
            vSafePrintf("p1=NULL\r\n");

        if (p2)
            vSafePrintf("p2=0x%08lX val=0x%08lX\r\n", (unsigned long)p2, (unsigned long)*p2);
        else
            vSafePrintf("p2=NULL\r\n");

        if (p3)
            vSafePrintf("p3=0x%08lX val=0x%08lX\r\n", (unsigned long)p3, (unsigned long)*p3);
        else
            vSafePrintf("p3=NULL\r\n");

        /* 释放中间那块，制造碎片 */
        vPortFree(p2);
        vSafePrintf("Free p2, Free: %d\r\n", (int)xPortGetFreeHeapSize());

        /* 释放第一块，应该和 p2 的空间合并 */
        vPortFree(p1);
        vSafePrintf("Free p1, Free: %d (should merge)\r\n",
                    (int)xPortGetFreeHeapSize());

        /* 释放第三块，全部合并回一大块 */
        vPortFree(p3);
        vSafePrintf("Free p3, Free: %d (all merged)\r\n",
                    (int)xPortGetFreeHeapSize());

        vSafePrintf("Min ever free: %d\r\n\r\n",
                    (int)xPortGetMinimumEverFreeHeapSize());

        vTaskDelay(3000);
    }
}

int main(void)
{
    UART_Init(115200);

    printf("\r\n\r\n");
    printf("===========================\r\n");
    printf("  MiniRTOS Heap4 Test\r\n");
    printf("===========================\r\n\r\n");

    /* 先初始化堆 */
    vPortHeapInit();
    printf("Heap init done, Free: %d\r\n\r\n", (int)xPortGetFreeHeapSize());

    xTaskCreate(TaskMalloc, "Malloc", 128, NULL, 1, NULL);

    printf("Starting scheduler...\r\n\r\n");
    vTaskStartScheduler();

    while (1);
}