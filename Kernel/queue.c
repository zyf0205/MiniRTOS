#include "queue.h"
#include "task.h"
#include <string.h>

/*---------------------------------------------------------------------------
 *  静态分配（和 task.c 一样，先用静态数组）
 *---------------------------------------------------------------------------*/
#define MAX_QUEUES 4
#define QUEUE_POOL_SIZE 512 /* 所有队列共享的缓冲区（字节） */

static Queue_t xQueuePool[MAX_QUEUES];
static uint32_t uxQueueCount = 0;

static int8_t ucQueueStoragePool[QUEUE_POOL_SIZE];
static uint32_t uxStorageUsed = 0;

/*---------------------------------------------------------------------------
 *  创建队列
 *---------------------------------------------------------------------------*/
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize)
{
    Queue_t *pxNewQueue;
    uint32_t uxBufSize;

    if (uxQueueCount >= MAX_QUEUES)
        return NULL;

    /* 计算需要的缓冲区大小 */
    uxBufSize = uxQueueLength * uxItemSize;

    if ((uxStorageUsed + uxBufSize) > QUEUE_POOL_SIZE)
        return NULL;

    pxNewQueue = &xQueuePool[uxQueueCount];
    uxQueueCount++;

    /* 分配缓冲区 */
    pxNewQueue->pcHead = &ucQueueStoragePool[uxStorageUsed];
    uxStorageUsed += uxBufSize;
    pxNewQueue->pcTail = pxNewQueue->pcHead + uxBufSize;

    /* 初始化读写指针 */
    pxNewQueue->pcWriteTo = pxNewQueue->pcHead;
    pxNewQueue->pcReadFrom = pxNewQueue->pcHead + ((uxQueueLength - 1) * uxItemSize);

    /* 初始化参数 */
    pxNewQueue->uxLength = uxQueueLength;
    pxNewQueue->uxItemSize = uxItemSize;
    pxNewQueue->uxMessagesWaiting = 0;

    /* 初始化等待链表 */
    vListInit(&(pxNewQueue->xTasksWaitingToSend));
    vListInit(&(pxNewQueue->xTasksWaitingToReceive));

    return pxNewQueue;
}

/*---------------------------------------------------------------------------
 *  拷贝数据到队列（内部函数）
 *---------------------------------------------------------------------------*/
static void prvCopyDataToQueue(Queue_t *pxQueue, const void *pvItemToQueue)
{
    /* 拷贝数据到写入位置 */
    memcpy((void *)pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize);

    /* 写指针往后移 */
    pxQueue->pcWriteTo += pxQueue->uxItemSize;

    /* 到末尾就绕回开头 */
    if (pxQueue->pcWriteTo >= pxQueue->pcTail)
    {
        pxQueue->pcWriteTo = pxQueue->pcHead;
    }

    pxQueue->uxMessagesWaiting++;
}

/*---------------------------------------------------------------------------
 *  从队列拷贝数据出来（内部函数）
 *---------------------------------------------------------------------------*/
static void prvCopyDataFromQueue(Queue_t *pxQueue, void *pvBuffer)
{
    /* 读指针先往后移 */
    pxQueue->pcReadFrom += pxQueue->uxItemSize;

    /* 到末尾就绕回开头 */
    if (pxQueue->pcReadFrom >= pxQueue->pcTail)
    {
        pxQueue->pcReadFrom = pxQueue->pcHead;
    }

    /* 从读取位置拷贝数据 */
    memcpy(pvBuffer, (void *)pxQueue->pcReadFrom, pxQueue->uxItemSize);

    pxQueue->uxMessagesWaiting--;
}

/*---------------------------------------------------------------------------
 *  发送数据到队列（带阻塞）
 *---------------------------------------------------------------------------*/
int32_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, uint32_t xTicksToWait)
{
    Queue_t *pxQueue = (Queue_t *)xQueue;

    for (;;)
    {
        taskENTER_CRITICAL();

        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            /* 队列没满，写入 */
            prvCopyDataToQueue(pxQueue, pvItemToQueue);

            /* 唤醒等待接收的任务 */
            if (pxQueue->xTasksWaitingToReceive.uxNumberOfItems > 0)
            {
                ListItem_t *pxItem = pxQueue->xTasksWaitingToReceive.xListEnd.pxNext; /*拿到头结点*/
                TCB_t *pxTCB = (TCB_t *)pxItem->pvOwner;                              /*拿到对应pcb*/

                /* 从队列等待链表移除 */
                uxListRemove(pxItem);

                /* 从延时链表移除（如果在的话） */
                if (pxTCB->xStateListItem.pvContainer != NULL)
                {
                    uxListRemove(&(pxTCB->xStateListItem));
                }

                /* 放回就绪链表 */
                prvAddTaskToReadyList(pxTCB);

                /*如果大于当前优先级触发pendsv中断切换任务*/
                if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
                {
                    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
                }
            }

            taskEXIT_CRITICAL();
            return 0;
        }
        else
        {
            /* 队列满了 */
            if (xTicksToWait == 0)
            {
                /* 不等待，直接失败 */
                taskEXIT_CRITICAL();
                return -1;
            }

            /* 阻塞：从就绪链表移除 */
            prvRemoveTaskFromReadyList(pxCurrentTCB);

            /* 加入队列等待发送链表（用 xEventListItem） */
            vListInsertEnd(&(pxQueue->xTasksWaitingToSend),
                           &(pxCurrentTCB->xEventListItem));

            /* 同时加入延时链表（超时机制） */
            if (xTicksToWait < 0xFFFFFFFFUL)
            {
                prvAddCurrentTaskToDelayedList(xTicksToWait);
            }

            taskEXIT_CRITICAL();

            /* 触发切换 */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;

            /*
             * 被唤醒后回到 for 循环顶部
             * 重新检查队列，如果还是满的就返回 -1（超时）
             */
            xTicksToWait = 0; /*为下一次进入循环判断做准备，下一次要是还是满的直接超时啦，直接返回*/
        }
    }
}

/*---------------------------------------------------------------------------
 *  从队列接收数据（带阻塞）
 *---------------------------------------------------------------------------*/
int32_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait)
{
    Queue_t *pxQueue = (Queue_t *)xQueue;

    for (;;)
    {
        taskENTER_CRITICAL();

        if (pxQueue->uxMessagesWaiting > 0)
        {
            /* 队列有数据，读出来 */
            prvCopyDataFromQueue(pxQueue, pvBuffer);

            /* 唤醒等待发送的任务 */
            if (pxQueue->xTasksWaitingToSend.uxNumberOfItems > 0)
            {
                ListItem_t *pxItem = pxQueue->xTasksWaitingToSend.xListEnd.pxNext;
                TCB_t *pxTCB = (TCB_t *)pxItem->pvOwner;

                uxListRemove(pxItem);

                if (pxTCB->xStateListItem.pvContainer != NULL)
                {
                    uxListRemove(&(pxTCB->xStateListItem));
                }

                prvAddTaskToReadyList(pxTCB);

                if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
                {
                    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
                }
            }

            taskEXIT_CRITICAL();
            return 0;
        }
        else
        {
            /* 队列空 */
            if (xTicksToWait == 0)
            {
                taskEXIT_CRITICAL();
                return -1;
            }

            /* 阻塞：从就绪链表移除 */
            prvRemoveTaskFromReadyList(pxCurrentTCB);

            /* 加入队列等待接收链表 */
            vListInsertEnd(&(pxQueue->xTasksWaitingToReceive),
                           &(pxCurrentTCB->xEventListItem));

            /* 加入延时链表（超时） */
            if (xTicksToWait < 0xFFFFFFFFUL)
            {
                prvAddCurrentTaskToDelayedList(xTicksToWait);
            }

            taskEXIT_CRITICAL();

            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;

            xTicksToWait = 0; /*为下一次进入循环判断做准备，下一次要是还是满的直接超时啦，直接返回*/
        }
    }
}

/*---------------------------------------------------------------------------
 *  查询队列元素个数
 *---------------------------------------------------------------------------*/
uint32_t uxQueueMessagesWaiting(QueueHandle_t xQueue)
{
    return ((Queue_t *)xQueue)->uxMessagesWaiting;
}