/*****************************************************************************
 * @file        task.h
 * @brief       任务管理配置文件
 * @author      zyf
 * @version     1.0
 *****************************************************************************/

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "list.h"

/*---------------------------------------------------------------------------
 *  宏定义
 *---------------------------------------------------------------------------*/
/* PendSV 触发宏 */
#define portNVIC_INT_CTRL_REG (*((volatile uint32_t *)0xE000ED04)) /*把这个地址当成一个 32 位寄存器来访问。（ICSR 寄存器）*/
#define portNVIC_PENDSVSET_BIT (1UL << 28UL)                       /*ICSR 里的第 28 位，写 1 会把 PendSV 置为 pending。*/

/*任务配置宏*/
#define MAX_PRIORITIES 8   /* 优先级 0~7，数字越大优先级越高 */
#define TASK_STACK_MIN 128 /* 最小栈大小（单位：uint32_t = 字） */
#define TASK_NAME_LEN 16

/*系统节拍配置宏*/
#define configTICK_RATE_HZ 1000 /* 系统节拍频率1ms 一次 */

/* 将临界区函数配置为快捷宏 */
#define taskENTER_CRITICAL() vPortEnterCritical()
#define taskEXIT_CRITICAL() vPortExitCritical()

/* 中断级临界区（在中断中使用），不计数没有嵌套保护 */
#define taskDISABLE_INTERRUPTS() __asm volatile("CPSID I")
#define taskENABLE_INTERRUPTS() __asm volatile("CPSIE I")

/*---------------------------------------------------------------------------
 *  数据结构
 *---------------------------------------------------------------------------*/

/*任务函数类型*/
typedef void (*TaskFunction_t)(void *param);

/*任务控制块*/
typedef struct TCB
{
    volatile uint32_t *pxTopOfStack; /* 必须是第一个字段！ */

    ListItem_t xStateListItem; /* 挂在就绪/延时链表上 */

    uint32_t uxPriority; /* 优先级 */

    uint32_t *pxStack;    /* 栈底地址（用于检测溢出） */
    uint32_t ulStackSize; /* 栈大小 */

    char pcTaskName[TASK_NAME_LEN];
} TCB_t;

typedef TCB_t *TaskHandle_t;

/*---------------------------------------------------------------------------
 *  函数声明
 *---------------------------------------------------------------------------*/
int32_t xTaskCreate(TaskFunction_t pxTaskCode,
                    const char *pcName,
                    uint32_t ulStackSize,
                    void *pvParam,
                    uint32_t uxPriority,
                    TaskHandle_t *pxHandle);
void vTaskStartScheduler(void);
void taskYIELD(void);
void vTaskSwitchContext(void);
uint32_t xTaskGetTickCount(void);
void vPortEnterCritical(void);
void vPortExitCritical(void);
void vApplicationIdleHook(void);
void vTaskSuspend(TaskHandle_t xTaskToSuspend);
void vTaskResume(TaskHandle_t xTaskToResume);
void vTaskDelete(TaskHandle_t xTaskToDelete);
void vTaskDelay(uint32_t xTicksToDelay);
void vSafePrintf(const char *fmt, ...);
#endif