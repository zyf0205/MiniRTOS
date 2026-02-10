#include "sem.h"

/*---------------------------------------------------------------------------
 *  创建二值信号量
 *
 *  容量=1，元素大小=0
 *  初始为空（要先 Give 才能 Take）
 *---------------------------------------------------------------------------*/
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    return xQueueCreate(1, 0);
}

/*---------------------------------------------------------------------------
 *  创建计数信号量
 *
 *  容量=uxMaxCount，元素大小=0
 *  初始计数=uxInitialCount（预先 Give 若干次）
 *---------------------------------------------------------------------------*/
SemaphoreHandle_t xSemaphoreCreateCounting(uint32_t uxMaxCount,
                                           uint32_t uxInitialCount)
{
    SemaphoreHandle_t xSem;
    uint32_t i;

    xSem = xQueueCreate(uxMaxCount, 0);

    if (xSem != NULL)
    {
        /* 预先填充 uxInitialCount 个信号 */
        for (i = 0; i < uxInitialCount; i++)
        {
            xQueueSend(xSem, NULL, 0);
        }
    }

    return xSem;
}