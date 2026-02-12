#include "mutex.h"
#include <string.h>

/*---------------------------------------------------------------------------
 *  静态分配
 *---------------------------------------------------------------------------*/
#define MAX_MUTEXES 4

static Mutex_t xMutexPool[MAX_MUTEXES]; /*静态分配数组池*/
static uint32_t uxMutexCount = 0;       /*当前分配了0个*/

/*---------------------------------------------------------------------------
 *  创建互斥量
 *---------------------------------------------------------------------------*/
MutexHandle_t xMutexCreate(void)
{
    Mutex_t *pxNewMutex;

    if (uxMutexCount >= MAX_MUTEXES) /*超过分配池数量了*/
        return NULL;

    pxNewMutex = &xMutexPool[uxMutexCount]; /*拿到互斥锁指针*/
    uxMutexCount++;                         /*数量增加*/

    /* 初始化底层队列：容量1，大小0 */
    pxNewMutex->xQueue.pcHead = NULL;
    pxNewMutex->xQueue.pcTail = NULL;
    pxNewMutex->xQueue.pcWriteTo = NULL;
    pxNewMutex->xQueue.pcReadFrom = NULL;
    pxNewMutex->xQueue.uxLength = 1;          /*容量1*/
    pxNewMutex->xQueue.uxItemSize = 0;        /*大小0*/
    pxNewMutex->xQueue.uxMessagesWaiting = 1; /* 初始可用（和信号量不同！） */

    /*初始化两个等待链表*/
    vListInit(&(pxNewMutex->xQueue.xTasksWaitingToSend));
    vListInit(&(pxNewMutex->xQueue.xTasksWaitingToReceive));

    /* 互斥量专属字段 */
    pxNewMutex->pxOwner = NULL;         /*互斥量被谁持有，当前为空*/
    pxNewMutex->uxOriginalPriority = 0; /*持有者的优先级*/

    return pxNewMutex; /*返回互斥量的地址*/
}

/*---------------------------------------------------------------------------
 *  获取互斥量（加锁）
 *
 *  如果锁可用 → 拿到锁，记录持有者
 *  如果被别人持有 → 优先级继承 + 阻塞等待
 *---------------------------------------------------------------------------*/
int32_t xMutexTake(MutexHandle_t xMutex, uint32_t xTicksToWait)
{
    Mutex_t *pxMutex = (Mutex_t *)xMutex; /*拿到互斥锁地址*/

    for (;;)
    {
        taskENTER_CRITICAL();
        /* 锁可用，拿到 */
        if (pxMutex->xQueue.uxMessagesWaiting > 0)
        {
            pxMutex->xQueue.uxMessagesWaiting = 0;                  /*拿到后容量变为0*/
            pxMutex->pxOwner = pxCurrentTCB;                        /*持有者变为当前任务*/
            pxMutex->uxOriginalPriority = pxCurrentTCB->uxPriority; /*更新持有者原始优先级*/

            taskEXIT_CRITICAL();
            return 0;
        }
        else /*锁不可用，正被其他任务持有*/
        {
            if (xTicksToWait == 0) /*如果等待之间为0，直接返回*/
            {
                taskEXIT_CRITICAL();
                return -1;
            }
            /*等待时间不为0*/
            /* ---- 优先级继承 ---- */
            if (pxMutex->pxOwner != NULL &&
                pxMutex->pxOwner->uxPriority < pxCurrentTCB->uxPriority)
            {
                /*
                 * 持有者优先级比我低
                 * 提升持有者到我的优先级
                 * 防止中优先级任务抢占持有者
                 */
                vTaskPrioritySet(pxMutex->pxOwner, pxCurrentTCB->uxPriority);
            }

            /* 从就绪链表移除自己 */
            prvRemoveTaskFromReadyList(pxCurrentTCB);

            /* 加入互斥量等待链表 */
            vListInsertEnd(&(pxMutex->xQueue.xTasksWaitingToReceive),
                           &(pxCurrentTCB->xEventListItem));

            /* 加入延时链表（超时），最大阻塞不用加入阻塞队列，只在等待队列里面 */
            if (xTicksToWait < portMAX_DELAY)
            {
                prvAddCurrentTaskToDelayedList(xTicksToWait);
            }

            taskEXIT_CRITICAL();

            /* 触发切换 */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;

            if (xTicksToWait != portMAX_DELAY) /*如果等待时间不是max，清零等待时间，下一次直接返回*/
            {
                xTicksToWait = 0;
            }
        }
    }
}

/*---------------------------------------------------------------------------
 *  释放互斥量（解锁）
 *
 *  只有持有者才能释放
 *  释放时恢复原始优先级
 *  唤醒等待的任务
 *---------------------------------------------------------------------------*/
int32_t xMutexGive(MutexHandle_t xMutex)
{
    Mutex_t *pxMutex = (Mutex_t *)xMutex;

    taskENTER_CRITICAL();

    /* 只有持有者才能释放 */
    if (pxMutex->pxOwner != pxCurrentTCB)
    {
        taskEXIT_CRITICAL();
        return -1;
    }

    /*持有者就是当前任务*/
    /* 恢复持有者的原始优先级 */
    if (pxCurrentTCB->uxPriority != pxMutex->uxOriginalPriority)
    {
        vTaskPrioritySet(pxCurrentTCB, pxMutex->uxOriginalPriority);
    }

    /* 释放锁 */
    pxMutex->pxOwner = NULL;
    pxMutex->xQueue.uxMessagesWaiting = 1; /* 锁可用 */

    /* 唤醒等待的任务（让它回到 for 循环自己去拿锁） */
    if (pxMutex->xQueue.xTasksWaitingToReceive.uxNumberOfItems > 0)
    {
        /*拿到等待队列中的第一个列表项*/
        ListItem_t *pxItem = pxMutex->xQueue.xTasksWaitingToReceive.xListEnd.pxNext;
        /*拿到他的tcb*/
        TCB_t *pxTCB = (TCB_t *)pxItem->pvOwner;

        /*将第一个列表项从列表中移除*/
        uxListRemove(pxItem);

        if (pxTCB->xStateListItem.pvContainer != NULL)
        {
            /*把任务管理列表项从对应列表上移除*/
            uxListRemove(&(pxTCB->xStateListItem));
        }

        /*加入就绪队列（唤醒）*/
        prvAddTaskToReadyList(pxTCB);

        /*如果优先级大于当前正在运行任务优先级，直接切换*/
        if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
        {
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
    }

    taskEXIT_CRITICAL();
    return 0;
}