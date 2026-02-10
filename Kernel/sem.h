#ifndef SEM_H
#define SEM_H

#include "queue.h"

/*---------------------------------------------------------------------------
 *  信号量句柄（本质就是队列句柄）
 *---------------------------------------------------------------------------*/
typedef QueueHandle_t SemaphoreHandle_t;

/*---------------------------------------------------------------------------
 *  二值信号量
 *---------------------------------------------------------------------------*/

/* 创建（初始为空，需要先 Give 一次才能 Take） */
SemaphoreHandle_t xSemaphoreCreateBinary(void);

/*---------------------------------------------------------------------------
 *  计数信号量
 *---------------------------------------------------------------------------*/

/*
 * uxMaxCount    : 最大计数值（容量）
 * uxInitialCount: 初始计数值
 */
SemaphoreHandle_t xSemaphoreCreateCounting(uint32_t uxMaxCount,
                                           uint32_t uxInitialCount);

/*---------------------------------------------------------------------------
 *  通用操作（二值和计数都用这两个）
 *---------------------------------------------------------------------------*/

/* 获取信号量（计数-1，空时阻塞） */
#define xSemaphoreTake(xSem, xTicksToWait) \
    xQueueReceive((xSem), NULL, (xTicksToWait))

/* 释放信号量（计数+1） */
#define xSemaphoreGive(xSem) \
    xQueueSend((xSem), NULL, 0)

/* 查询当前计数值 */
#define uxSemaphoreGetCount(xSem) \
    uxQueueMessagesWaiting((xSem))

#endif