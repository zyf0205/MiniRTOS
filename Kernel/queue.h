#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include "list.h"
#include "task.h"

/* 供 queue.c 使用 */
extern TCB_t *volatile pxCurrentTCB;
void prvAddTaskToReadyList(TCB_t *pxTCB);
void prvRemoveTaskFromReadyList(TCB_t *pxTCB);
void prvAddCurrentTaskToDelayedList(uint32_t xTicksToDelay);
extern volatile uint32_t xTickCount;

/*---------------------------------------------------------------------------
 *  队列结构
 *---------------------------------------------------------------------------*/
typedef struct QueueDefinition
{
    int8_t *pcHead;     /* 缓冲区起始地址 */
    int8_t *pcTail;     /* 缓冲区末尾（最后一个字节的下一个位置） */
    int8_t *pcWriteTo;  /* 下次写入位置 */
    int8_t *pcReadFrom; /* 下次读取位置 */

    uint32_t uxLength;                   /* 队列容量（能存多少个元素） */
    uint32_t uxItemSize;                 /* 每个元素的大小（字节） */
    volatile uint32_t uxMessagesWaiting; /* 当前队列中的元素个数 */

    List_t xTasksWaitingToSend;    /* 等待发送的任务链表 */
    List_t xTasksWaitingToReceive; /* 等待接收的任务链表 */
} Queue_t;

/*---------------------------------------------------------------------------
 *  队列句柄
 *---------------------------------------------------------------------------*/
typedef Queue_t *QueueHandle_t;

/*---------------------------------------------------------------------------
 *  API
 *---------------------------------------------------------------------------*/

/*
 * 创建队列
 *   uxQueueLength : 容量（能存多少个元素）
 *   uxItemSize    : 每个元素的大小（字节）
 *   返回          : 队列句柄，失败返回 NULL
 */
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);

/*
 * 发送数据到队列
 *   xQueue        : 队列句柄
 *   pvItemToQueue : 要发送的数据指针
 *   xTicksToWait  : 队列满时最多等多少 tick（0 = 不等）
 *   返回          : 0 成功，-1 失败（超时）
 */
int32_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, uint32_t xTicksToWait);

/*
 * 从队列接收数据
 *   xQueue        : 队列句柄
 *   pvBuffer      : 接收缓冲区
 *   xTicksToWait  : 队列空时最多等多少 tick（0 = 不等）
 *   返回          : 0 成功，-1 失败（超时）
 */
int32_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait);

/*
 * 查询队列中当前有多少元素
 */
uint32_t uxQueueMessagesWaiting(QueueHandle_t xQueue);

#endif