/*****************************************************************************
 * @file        task.c
 * @brief       任务管理
 * @author      zyf
 * @version     1.0
 *****************************************************************************/

#include "task.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stm32f4xx.h>

/*---------------------------------------------------------------------------
 *  全局变量
 *---------------------------------------------------------------------------*/
/*当前正在运行任务的句柄*/
TCB_t *volatile pxCurrentTCB = NULL;

/* 空闲任务的栈和 TCB */
static uint32_t xIdleTaskStack[128];
static TCB_t xIdleTaskTCB;

/* 系统节拍计数 */
volatile uint32_t xTickCount = 0;

/* 临界区嵌套计数 */
static volatile uint32_t uxCriticalNesting = 0;

/*就绪链表数组和优先级位图*/
List_t pxReadyTasksLists[MAX_PRIORITIES]; /* 就绪链表数组：每个优先级一条链表 */
static uint32_t uxTopReadyPriority = 0;   /* 优先级位图：bit N = 1 表示优先级 N 有任务就绪 */

/* 挂起链表 */
static List_t xSuspendedTaskList;

/* 删除等待链表（等空闲任务来回收） */
static List_t xTasksWaitingTermination;

/*延时链表，使用两个链表，应对xNextTaskUnblockTime溢出情况*/
static List_t xDelayedTaskList1;
static List_t xDelayedTaskList2;
static List_t *pxDelayedTaskList;                             /* 当前延时链表 */
static List_t *pxOverflowDelayedTaskList;                     /* 溢出延时链表 */
static volatile uint32_t xNextTaskUnblockTime = 0xFFFFFFFFUL; /* 下一个需要唤醒的时间点（优化：不用每次遍历链表） */

/*---------------------------------------------------------------------------
 *  内部函数声明
 *---------------------------------------------------------------------------*/
static void prvTaskExitError(void);
static uint32_t *prvInitialiseStack(uint32_t *pxTopOfStack,
                                    TaskFunction_t pxCode,
                                    void *pvParam);
static void prvSelectHighestPriorityTask(void);
static void prvSwitchDelayedLists(void);
static void prvIdleTask(void *param);

/*---------------------------------------------------------------------------
 *  三个汇编函数，在portasm.s中编写
 *---------------------------------------------------------------------------*/
extern void vPortSVCHandler(void);
extern void vPortPendSVHandler(void);
extern void vPortStartFirstTask(void);

/*任务退出错误处理*/
static void prvTaskExitError(void)
{
    while (1)
        ;
}

/*初始化栈帧*/
static uint32_t *prvInitialiseStack(uint32_t *pxTopOfStack,
                                    TaskFunction_t pxCode,
                                    void *pvParam)
{
    /*
     * 栈是满递减的：先减再存
     * 从高地址往低地址排列
     */

    /* --- 硬件自动弹出的 8 个寄存器 --- */
    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x01000000UL; /* xPSR: Thumb 位 = 1,让thumb指令能够执行 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)pxCode; /* PC: 任务入口(跳转到任务函数) */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)prvTaskExitError; /* LR: 任务退出去哪(任务函数返回,按理说不应该返回) */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x12121212UL; /* R12: 填辨识值方便调试 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x03030303UL; /* R3 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x02020202UL; /* R2 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x01010101UL; /* R1 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)pvParam; /* R0: 任务参数 */

    /* --- 我们手动保存/恢复的 8 个寄存器 --- */
    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x11111111UL; /* R11 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x10101010UL; /* R10 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x09090909UL; /* R9 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x08080808UL; /* R8 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x07070707UL; /* R7 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x06060606UL; /* R6 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x05050505UL; /* R5 */

    pxTopOfStack--;
    *pxTopOfStack = (uint32_t)0x04040404UL; /* R4 */

    /* 返回栈顶指针，存到 TCB 的 pxTopOfStack */
    return pxTopOfStack;
}

/*添加任务到就绪链表*/
void prvAddTaskToReadyList(TCB_t *pxTCB)
{
    /* 在位图中标记该优先级有任务 */
    uxTopReadyPriority |= (1UL << pxTCB->uxPriority);

    /* 把任务的节点插到对应优先级的就绪链表尾部 */
    vListInsertEnd(&(pxReadyTasksLists[pxTCB->uxPriority]),
                   &(pxTCB->xStateListItem));
}

/*找到最高就绪优先级，从中取出任务设为 pxCurrentTCB*/
static void prvSelectHighestPriorityTask(void)
{
    uint32_t uxTopPriority;
    List_t *pxList;

    /* 位图找最高优先级 */
    uxTopPriority = (31UL - (uint32_t)__CLZ(uxTopReadyPriority));

    pxList = &pxReadyTasksLists[uxTopPriority];

    /* 移动 pxIndex 到下一个节点（实现同优先级轮转） */
    pxList->pxIndex = pxList->pxIndex->pxNext;

    /* 如果 pxIndex 指向了哨兵，再跳一个 */
    if ((void *)pxList->pxIndex == (void *)&(pxList->xListEnd))
    {
        pxList->pxIndex = pxList->pxIndex->pxNext;
    }

    pxCurrentTCB = (TCB_t *)pxList->pxIndex->pvOwner;
}

/*交换延时链表（tick 溢出时调用）*/
static void prvSwitchDelayedLists(void)
{
    List_t *pxTemp;

    /* 交换两个链表指针 */
    pxTemp = pxDelayedTaskList;
    pxDelayedTaskList = pxOverflowDelayedTaskList;
    pxOverflowDelayedTaskList = pxTemp;

    /* 更新最近唤醒时间 */
    if (pxDelayedTaskList->uxNumberOfItems > 0)
    {
        ListItem_t *pxHead = pxDelayedTaskList->xListEnd.pxNext;
        xNextTaskUnblockTime = pxHead->xItemValue; /*更新为头节点的value值*/
    }
    else
    {
        xNextTaskUnblockTime = 0xFFFFFFFFUL;
    }
}

/*空闲任务函数*/
static void prvIdleTask(void *param)
{
    (void)param;

    while (1)
    {
        /* 可以在这里调用用户的钩子函数 */
        /* vApplicationIdleHook(); */
    }
}

/*创建空闲任务，调度器启动时调用*/
static void prvCreateIdleTask(void)
{
    xIdleTaskTCB.pxStack = xIdleTaskStack;
    xIdleTaskTCB.ulStackSize = 128;
    xIdleTaskTCB.pxTopOfStack = prvInitialiseStack(
        xIdleTaskStack + 128,
        prvIdleTask,
        NULL);
    xIdleTaskTCB.uxPriority = 0; /* 最低优先级 */

    strncpy(xIdleTaskTCB.pcTaskName, "IDLE", TASK_NAME_LEN - 1);
    xIdleTaskTCB.pcTaskName[TASK_NAME_LEN - 1] = '\0';

    /*初始化链表项，包含队列阻塞链表项*/
    vListInitItem(&(xIdleTaskTCB.xStateListItem));
    xIdleTaskTCB.xStateListItem.pvOwner = &xIdleTaskTCB;
    vListInitItem(&(xIdleTaskTCB.xEventListItem));
    xIdleTaskTCB.xEventListItem.pvOwner = &xIdleTaskTCB;

    prvAddTaskToReadyList(&xIdleTaskTCB);
}

/*创建任务*/
int32_t xTaskCreate(TaskFunction_t pxTaskCode,
                    const char *pcName,
                    uint32_t ulStackSize,
                    void *pvParam,
                    uint32_t uxPriority,
                    TaskHandle_t *pxHandle)
{
    TCB_t *pxNewTCB;   /*新tcb*/
    uint32_t *pxStack; /*栈底地址*/

    /* ---- 首次调用时初始化所有就绪链表 ---- */
    static uint32_t ulFirstCall = 1;
    if (ulFirstCall)
    {
        uint32_t i;
        for (i = 0; i < MAX_PRIORITIES; i++)
        {
            vListInit(&pxReadyTasksLists[i]);
        }
        ulFirstCall = 0;
    }

    /* 1. 分配栈空间（静态分配，用 static 数组模拟） */
    /*    正式版应该用 malloc，这里为了简单先用 static */
    static uint32_t ulTaskCount = 0;     /*任务数量*/
    static uint32_t xTaskStacks[4][256]; /* 最多 4 个任务，每个任务 256 字 = 1KB */
    static TCB_t xTCBs[4];               /*存放tcb的数组*/

    if (ulTaskCount >= 4) /*超过四个任务，直接返回*/
        return -1;

    pxStack = xTaskStacks[ulTaskCount]; /*得到栈底地址*/
    pxNewTCB = &xTCBs[ulTaskCount];     /*把tcb放到数组里*/
    ulTaskCount++;

    /* 2. 记录栈信息 */
    pxNewTCB->pxStack = pxStack;         /*更新tcb栈底地址*/
    pxNewTCB->ulStackSize = ulStackSize; /*更新栈大小*/

    /* 3. 初始化任务栈（伪造栈帧） */
    pxNewTCB->pxTopOfStack = prvInitialiseStack(pxStack + ulStackSize, pxTaskCode, pvParam);

    /* 4. 设置优先级 */
    if (uxPriority >= MAX_PRIORITIES)
        uxPriority = MAX_PRIORITIES - 1;
    pxNewTCB->uxPriority = uxPriority;

    /* 5. 复制任务名 */
    strncpy(pxNewTCB->pcTaskName, pcName, TASK_NAME_LEN - 1);
    pxNewTCB->pcTaskName[TASK_NAME_LEN - 1] = '\0';

    /* 6. 初始化链表节点 */
    vListInitItem(&(pxNewTCB->xStateListItem));
    pxNewTCB->xStateListItem.pvOwner = pxNewTCB; /* 节点指向自己的 TCB */
    vListInitItem(&(pxNewTCB->xEventListItem));  /*队列阻塞链表箱*/
    pxNewTCB->xEventListItem.pvOwner = pxNewTCB;

    /* 7. 加入就绪链表 */
    prvAddTaskToReadyList(pxNewTCB);

    /* 8. 输出句柄 */
    if (pxHandle != NULL)
        *pxHandle = pxNewTCB;

    return 0;
}

/*触发pendsv，切换上下文*/
void taskYIELD(void)
{
    /* 触发 PendSV */
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
}

/*找到最高就绪优先级，从中取出任务设为 pxCurrentTCB，供pendsv调用*/
void vTaskSwitchContext(void)
{
    prvSelectHighestPriorityTask();
}

/* 获取当前 tick 值*/
uint32_t xTaskGetTickCount(void)
{
    return xTickCount;
}

/*SysTick 初始化*/
static void prvStartSysTick(void)
{
/*
 * SysTick 寄存器:
 *   LOAD = 重装载值
 *   VAL  = 当前值
 *   CTRL = 控制
 *
 * 时钟源 = HCLK = 96MHz
 * 1ms = 96000000 / 1000 = 96000 个周期
 * LOAD = 96000 - 1
 */
#define portNVIC_SYSTICK_CTRL (*((volatile uint32_t *)0xE000E010))
#define portNVIC_SYSTICK_LOAD (*((volatile uint32_t *)0xE000E014))
#define portNVIC_SYSTICK_VAL (*((volatile uint32_t *)0xE000E018))

#define portNVIC_SYSTICK_CLK (1UL << 2) /* 使用处理器时钟 */
#define portNVIC_SYSTICK_INT (1UL << 1) /* 使能中断 */
#define portNVIC_SYSTICK_EN (1UL << 0)  /* 使能计数器 */

    /* 停止 SysTick */
    portNVIC_SYSTICK_CTRL = 0;
    portNVIC_SYSTICK_VAL = 0;

    /* 重装载值 */
    portNVIC_SYSTICK_LOAD = (SystemCoreClock / configTICK_RATE_HZ) - 1;

    /* 启动: 使用内核时钟 + 开中断 + 使能 */
    portNVIC_SYSTICK_CTRL = portNVIC_SYSTICK_CLK | portNVIC_SYSTICK_INT | portNVIC_SYSTICK_EN;
}

/*SysTick 中断处理,
  检查延时链表，唤醒到时间的任务，
  触发pendsv切换任务*/
void SysTick_Handler(void)
{
    TCB_t *pxTCB;
    uint32_t xConstTickCount;

    taskDISABLE_INTERRUPTS();

    /* tick +1 */
    xTickCount++;
    xConstTickCount = xTickCount;

    /* 检查是否溢出（tick 从 0xFFFFFFFF 变成 0） */
    if (xConstTickCount == 0)
    {
        prvSwitchDelayedLists(); /*交换两个延时链表的指向*/
    }

    /* 检查延时链表中是否有任务该醒了 */
    if (xConstTickCount >= xNextTaskUnblockTime)
    {
        while (1)
        {
            /* 延时链表为空，没有任务在等 */
            if (pxDelayedTaskList->uxNumberOfItems == 0)
            {
                xNextTaskUnblockTime = 0xFFFFFFFFUL;
                break;
            }

            /* 看链表头部（最早该醒的任务） */
            ListItem_t *pxHead = pxDelayedTaskList->xListEnd.pxNext;

            if (xConstTickCount < pxHead->xItemValue)
            {
                /* 还没到时间，更新下次唤醒时间，退出 */
                xNextTaskUnblockTime = pxHead->xItemValue;
                break;
            }

            /* 到时间了！从延时链表移除 */
            pxTCB = (TCB_t *)pxHead->pvOwner;
            uxListRemove(pxHead);

            /* 如果任务还在队列等待链表上，也移除 ← 新增 */
            if (pxTCB->xEventListItem.pvContainer != NULL)
            {
                uxListRemove(&(pxTCB->xEventListItem));
            }

            /* 放回就绪链表 */
            prvAddTaskToReadyList(pxTCB);
        }
    }

    /* 触发 PendSV 做任务切换 */
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;

    taskENABLE_INTERRUPTS();
}

/*进入临界区，支持嵌套*/
void vPortEnterCritical(void)
{
    taskDISABLE_INTERRUPTS();
    uxCriticalNesting++;
}

/*退出临界区，支持嵌套*/
void vPortExitCritical(void)
{
    uxCriticalNesting--;
    if (uxCriticalNesting == 0)
    {
        taskENABLE_INTERRUPTS();
    }
}

/*从就绪链表中移除任务 同时更新优先级位图*/
void prvRemoveTaskFromReadyList(TCB_t *pxTCB)
{
    /* 从链表中移除，返回剩余节点数 */
    if (uxListRemove(&(pxTCB->xStateListItem)) == 0)
    {
        /* 这个优先级没有任务了，清除位图中对应的位 */
        uxTopReadyPriority &= ~(1UL << pxTCB->uxPriority);
    }
}

/*挂起任务,
  把任务从就绪链表移到挂起链表
  如果挂起的是当前任务，立刻切换*/
void vTaskSuspend(TaskHandle_t xTaskToSuspend)
{
    TCB_t *pxTCB;

    taskENTER_CRITICAL();

    /* NULL 表示挂起自己 */
    if (xTaskToSuspend == NULL)
    {
        pxTCB = pxCurrentTCB;
    }
    else
    {
        pxTCB = xTaskToSuspend;
    }

    /* 从就绪链表移除 */
    prvRemoveTaskFromReadyList(pxTCB);

    /* 加入挂起链表 */
    vListInsertEnd(&xSuspendedTaskList, &(pxTCB->xStateListItem));

    taskEXIT_CRITICAL();

    /* 如果挂起的是自己，立刻触发切换 */
    if (pxTCB == pxCurrentTCB)
    {
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
    }
}

/*恢复任务
  把任务从挂起链表移回就绪链表
  如果恢复的任务优先级比当前任务高，触发切换*/
void vTaskResume(TaskHandle_t xTaskToResume)
{
    TCB_t *pxTCB = xTaskToResume;

    /* 不能恢复 NULL，不能恢复自己（自己在运行，没被挂起） */
    if (pxTCB == NULL || pxTCB == pxCurrentTCB)
    {
        return;
    }

    taskENTER_CRITICAL();

    /* 确认任务确实在挂起链表中 */
    if (pxTCB->xStateListItem.pvContainer == &xSuspendedTaskList)
    {
        /* 从挂起链表移除 */
        uxListRemove(&(pxTCB->xStateListItem));

        /* 加回就绪链表 */
        prvAddTaskToReadyList(pxTCB);

        /* 如果恢复的任务优先级更高，触发切换 */
        if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
        {
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
    }

    taskEXIT_CRITICAL();
}

/*删除任务
  从就绪/挂起链表移除
  如果删除的是自己，切换到其他任务
  Phase 1 用的是静态分配，所以这里只是从链表移除
  不需要释放内存。后续改成动态分配时再加内存释放。*/
void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    TCB_t *pxTCB;

    taskENTER_CRITICAL();

    /* NULL 表示删除自己 */
    if (xTaskToDelete == NULL)
    {
        pxTCB = pxCurrentTCB;
    }
    else
    {
        pxTCB = xTaskToDelete;
    }

    /* 从当前所在链表移除（可能在就绪或挂起链表） */
    if (pxTCB->xStateListItem.pvContainer != NULL)
    {
        uxListRemove(&(pxTCB->xStateListItem));

        /* 如果是从就绪链表移除的，更新位图 */
        if (pxReadyTasksLists[pxTCB->uxPriority].uxNumberOfItems == 0)
        {
            uxTopReadyPriority &= ~(1UL << pxTCB->uxPriority);
        }
    }

    taskEXIT_CRITICAL();

    /* 如果删除的是自己，切换到其他任务 */
    if (pxTCB == pxCurrentTCB)
    {
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
    }
}

/*线程安全打印*/
void vSafePrintf(const char *fmt, ...)
{
    va_list args;

    taskENTER_CRITICAL();

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    taskEXIT_CRITICAL();
}

/*初始化两个延时链表*/
static void prvInitialiseDelayLists(void)
{
    vListInit(&xDelayedTaskList1);
    vListInit(&xDelayedTaskList2);

    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}

/*把任务加入延时链表,根据唤醒时间是否溢出，放入不同的链表*/
void prvAddCurrentTaskToDelayedList(uint32_t xTicksToDelay)
{
    uint32_t xTimeToWake;

    /* 计算唤醒时间 */
    xTimeToWake = xTickCount + xTicksToDelay;

    /* 设置节点排序值为唤醒时间 */
    pxCurrentTCB->xStateListItem.xItemValue = xTimeToWake;

    if (xTimeToWake < xTickCount)
    {
        /* 溢出了，放入溢出链表 */
        vListInsert(pxOverflowDelayedTaskList, &(pxCurrentTCB->xStateListItem));
    }
    else
    {
        /* 没溢出，放入当前延时链表 */
        vListInsert(pxDelayedTaskList, &(pxCurrentTCB->xStateListItem));

        /* 更新最近唤醒时间 */
        if (xTimeToWake < xNextTaskUnblockTime)
        {
            xNextTaskUnblockTime = xTimeToWake;
        }
    }
}

/*任务延时，让当前任务进入阻塞态，到底唤醒时间后自动唤醒*/
void vTaskDelay(uint32_t xTicksToDelay)
{
    if (xTicksToDelay == 0)
    {
        /* 延时 0 等于只让出一次 CPU */
        taskYIELD();
        return;
    }

    taskENTER_CRITICAL();

    /* 从就绪链表移除 */
    prvRemoveTaskFromReadyList(pxCurrentTCB);

    /* 加入延时链表 */
    prvAddCurrentTaskToDelayedList(xTicksToDelay);

    taskEXIT_CRITICAL();

    /* 触发任务切换 */
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
}

/*启动调度器*/
void vTaskStartScheduler(void)
{
    /* 初始化挂起链表和删除等待链表 */
    vListInit(&xSuspendedTaskList);
    vListInit(&xTasksWaitingTermination);

    /* 初始化延时链表 */
    prvInitialiseDelayLists();

    /* 创建空闲任务 */
    prvCreateIdleTask();

    /* 选出最高优先级任务 */
    prvSelectHighestPriorityTask();

    /* PendSV 和 SysTick 设为最低优先级 */
    (*((volatile uint32_t *)0xE000ED20)) |= (0xFFUL << 16);
    (*((volatile uint32_t *)0xE000ED20)) |= (0xFFUL << 24);

    /* 启动 SysTick */
    prvStartSysTick();

    /* 启动第一个任务 */
    vPortStartFirstTask();

    while (1)
        ;
}