#include "task.h"
#include <string.h>
#include <stm32f4xx.h>

/*---------------------------------------------------------------------------
 *  全局变量
 *---------------------------------------------------------------------------*/

/* 当前正在运行的任务 TCB 的指针(句柄)*/
TCB_t *volatile pxCurrentTCB = NULL;

/* 就绪链表数组：每个优先级一条链表 */
List_t pxReadyTasksLists[MAX_PRIORITIES];

/* 优先级位图：bit N = 1 表示优先级 N 有任务就绪 */
static uint32_t uxTopReadyPriority = 0;

/* 系统节拍计数 */
static volatile uint32_t xTickCount = 0;

/* 临界区嵌套计数 */
static volatile uint32_t uxCriticalNesting = 0;
/*---------------------------------------------------------------------------
 *  内部函数声明
 *---------------------------------------------------------------------------*/
static void prvTaskExitError(void);
static uint32_t *prvInitialiseStack(uint32_t *pxTopOfStack,
                                    TaskFunction_t pxCode,
                                    void *pvParam);

/* 汇编函数，Step 3 再写 */
extern void vPortSVCHandler(void);
extern void vPortPendSVHandler(void);
extern void vPortStartFirstTask(void);

/*---------------------------------------------------------------------------
 *  任务退出错误处理
 *
 *  如果任务函数 return 了会跳到这里（任务不应该 return）
 *---------------------------------------------------------------------------*/
static void prvTaskExitError(void)
{
    while (1)
        ;
}

/*---------------------------------------------------------------------------
 *  初始化任务栈
 *
 *  在栈上伪造一个异常栈帧，让 PendSV 能"恢复"这个从未运行过的任务
 *---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------
 *  添加任务到就绪链表
 *---------------------------------------------------------------------------*/
static void prvAddTaskToReadyList(TCB_t *pxTCB)
{
    /* 在位图中标记该优先级有任务 */
    uxTopReadyPriority |= (1UL << pxTCB->uxPriority);

    /* 把任务的节点插到对应优先级的就绪链表尾部 */
    vListInsertEnd(&(pxReadyTasksLists[pxTCB->uxPriority]),
                   &(pxTCB->xStateListItem));
}

/*---------------------------------------------------------------------------
 *  找到最高就绪优先级，从中取出任务设为 pxCurrentTCB
 *---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------
 *  创建任务
 *---------------------------------------------------------------------------*/
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

    /* 7. 加入就绪链表 */
    prvAddTaskToReadyList(pxNewTCB);

    /* 8. 输出句柄 */
    if (pxHandle != NULL)
        *pxHandle = pxNewTCB;

    return 0;
}

/*---------------------------------------------------------------------------
 *  主动让出 CPU
 *---------------------------------------------------------------------------*/

/* PendSV 触发宏 */
#define portNVIC_INT_CTRL_REG (*((volatile uint32_t *)0xE000ED04)) /*把这个地址当成一个 32 位寄存器来访问。（ICSR 寄存器）*/
#define portNVIC_PENDSVSET_BIT (1UL << 28UL)                       /*ICSR 里的第 28 位，写 1 会把 PendSV 置为 pending。*/

void taskYIELD(void)
{
    /* 触发 PendSV */
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
}

/*---------------------------------------------------------------------------
 *  上下文切换（PendSV 中调用）
 *---------------------------------------------------------------------------*/
void vTaskSwitchContext(void)
{
    prvSelectHighestPriorityTask();
}

/*---------------------------------------------------------------------------
 *  获取当前 tick 值
 *---------------------------------------------------------------------------*/
uint32_t xTaskGetTickCount(void)
{
    return xTickCount;
}

/*---------------------------------------------------------------------------
 *  SysTick 初始化
 *
 *  每 1ms 触发一次 SysTick 中断
 *  SysTick 是 24 位递减计数器
 *  计数到 0 时触发中断并自动重装载
 *---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------
 *  SysTick 中断处理
 *
 *  每 1ms 执行一次：
 *    tick +1，然后触发 PendSV 做任务切换
 *---------------------------------------------------------------------------*/
void SysTick_Handler(void)
{
    /*
     * 关中断防止嵌套问题
     * SysTick 虽然已经是中断上下文，但关中断可以防止
     * 更高优先级的中断打断链表操作
     */
    taskDISABLE_INTERRUPTS();

    xTickCount++;

    /* 触发 PendSV */
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;

    taskENABLE_INTERRUPTS();
}

/*---------------------------------------------------------------------------
 *  进入临界区
 *
 *  支持嵌套：
 *    进入临界区 A        uxCriticalNesting = 1, 关中断
 *      进入临界区 B      uxCriticalNesting = 2, 中断已经关了
 *      退出临界区 B      uxCriticalNesting = 1, 不开中断（还在A里）
 *    退出临界区 A        uxCriticalNesting = 0, 开中断
 *---------------------------------------------------------------------------*/
void vPortEnterCritical(void)
{
    taskDISABLE_INTERRUPTS();
    uxCriticalNesting++;
}

void vPortExitCritical(void)
{
    uxCriticalNesting--;
    if (uxCriticalNesting == 0)
    {
        taskENABLE_INTERRUPTS();
    }
}

/*---------------------------------------------------------------------------
 *  启动调度器
 *---------------------------------------------------------------------------*/
void vTaskStartScheduler(void)
{

    /* --- 注意：此时任务应该已经创建好了 --- */

    /* 2. 选出最高优先级任务 */
    prvSelectHighestPriorityTask();

    /* 3. 配置 PendSV 和 SysTick 为最低优先级 */
    /*    SHPR3 寄存器: PendSV = [23:16], SysTick = [31:24] */
    (*((volatile uint32_t *)0xE000ED20)) |= (0xFFUL << 16); /* PendSV */
    (*((volatile uint32_t *)0xE000ED20)) |= (0xFFUL << 24); /* SysTick */

    /* 启动 SysTick */
    prvStartSysTick();

    /* 4. 启动第一个任务（触发 SVC） */
    vPortStartFirstTask();

    /* 不应该到这里 */
    while (1)
        ;
}
