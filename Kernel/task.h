#ifndef MINI_TASK_H
#define MINI_TASK_H

#include <stdint.h>
#include "list.h"

/*---------------------------------------------------------------------------
 *  配置
 *---------------------------------------------------------------------------*/
#define MAX_PRIORITIES 8   /* 优先级 0~7，数字越大优先级越高 */
#define TASK_STACK_MIN 128 /* 最小栈大小（单位：uint32_t = 字） */
#define TASK_NAME_LEN 16
#define configTICK_RATE_HZ 1000 /* 系统节拍频率1ms 一次 */

/* 快捷宏 */
#define taskENTER_CRITICAL() vPortEnterCritical()
#define taskEXIT_CRITICAL() vPortExitCritical()

/* 中断级临界区（在中断中使用），不计数没有嵌套保护 */
#define taskDISABLE_INTERRUPTS() __asm volatile("CPSID I")
#define taskENABLE_INTERRUPTS() __asm volatile("CPSIE I")
/*---------------------------------------------------------------------------
 *  任务函数类型
 *---------------------------------------------------------------------------*/
typedef void (*TaskFunction_t)(void *param);

/*---------------------------------------------------------------------------
 *  任务控制块 TCB
 *---------------------------------------------------------------------------*/
typedef struct TCB
{
    volatile uint32_t *pxTopOfStack; /* 必须是第一个字段！ */

    ListItem_t xStateListItem; /* 挂在就绪/延时链表上 */

    uint32_t uxPriority; /* 优先级 */

    uint32_t *pxStack;    /* 栈底地址（用于检测溢出） */
    uint32_t ulStackSize; /* 栈大小 */

    char pcTaskName[TASK_NAME_LEN];
} TCB_t;

/*---------------------------------------------------------------------------
 *  任务句柄 = TCB 指针
 *---------------------------------------------------------------------------*/
typedef TCB_t *TaskHandle_t;

/*---------------------------------------------------------------------------
 *  API
 *---------------------------------------------------------------------------*/

/*
 * 创建任务
 *   pxTaskCode  : 任务函数
 *   pcName      : 任务名
 *   ulStackSize : 栈大小（单位 uint32_t）
 *   pvParam     : 传给任务函数的参数
 *   uxPriority  : 优先级 (0 ~ MAX_PRIORITIES-1)
 *   pxHandle    : 输出任务句柄（可以传 NULL）
 */
int32_t xTaskCreate(TaskFunction_t pxTaskCode,
                    const char *pcName,
                    uint32_t ulStackSize,
                    void *pvParam,
                    uint32_t uxPriority,
                    TaskHandle_t *pxHandle);

/*
 * 启动调度器
 */
void vTaskStartScheduler(void);

/*
 * 主动让出 CPU（触发 PendSV）
 */
void taskYIELD(void);

void vTaskSwitchContext(void);

/* 获取 tick 计数 */
uint32_t xTaskGetTickCount(void);

/* ---- 临界区 ---- */
void vPortEnterCritical(void);
void vPortExitCritical(void);

/*空闲任务钩子函数*/
void vApplicationIdleHook(void);

/* 挂起/恢复/删除 */
void vTaskSuspend(TaskHandle_t xTaskToSuspend);
void vTaskResume(TaskHandle_t xTaskToResume);
void vTaskDelete(TaskHandle_t xTaskToDelete);

/* 任务延时 */
void vTaskDelay(uint32_t xTicksToDelay);

/*安全打印*/
void vSafePrintf(const char *fmt, ...);
#endif