

# MiniRTOS

参照FreeRTOS写的实时操作系统，运行在 STM32F411CEU6（Cortex-M4）上。

## 功能特性

| 模块 | 功能 |
|------|------|
| 任务管理 | 创建、删除、挂起、恢复 |
| 调度器 | 抢占式调度、时间片轮转、优先级位图 |
| 时间管理 | vTaskDelay、延时链表、tick 溢出处理 |
| 队列 | 阻塞发送/接收、超时、死等 |
| 信号量 | 二值信号量、计数信号量 |
| 互斥量 | 优先级继承 |
| 内存管理 | Heap4（动态分配 + 释放 + 碎片合并） |
| 移植层 | PendSV/SVC 汇编上下文切换 |

## 工程结构

```
MiniRTOS/
├── Kernel/
│   ├── list.c/h        # 双向循环链表
│   ├── task.c/h        # 任务管理 + 调度器 + SysTick
│   ├── queue.c/h       # 消息队列
│   ├── sem.c/h         # 二值/计数信号量
│   ├── mutex.c/h       # 互斥量（优先级继承）
│   ├── heap.c/h        # Heap4 内存管理
│   └── portasm.s       # Cortex-M4 汇编移植层
├── Drivers/
│   ├── led.c/h         # RGB LED 驱动
│   ├── uart.c/h        # USART1 串口驱动
│   └── delay.c/h       # 延时（裸机用）
└── Core/
    └── main.c
```

## 硬件环境

- MCU：STM32F411CEU6（Cortex-M4，96MHz）
- 晶振：12MHz HSE
- LED：RGB（PA0/PA1/PA2，低电平点亮）
- 串口：USART1（PA9/PA10，115200）
- 开发环境：EIDE + ARMCC + ST-Link

## 快速开始

```c
#include "task.h"
#include "queue.h"
#include "sem.h"
#include "mutex.h"
#include "heap.h"

void Task1(void *param)
{
    while (1)
    {
        printf("Task1\r\n");
        vTaskDelay(1000);
    }
}

void Task2(void *param)
{
    while (1)
    {
        printf("Task2\r\n");
        vTaskDelay(500);
    }
}

int main(void)
{
    UART_Init(115200);

    xTaskCreate(Task1, "T1", 256, NULL, 1, NULL);
    xTaskCreate(Task2, "T2", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1);
}
```

## API 一览

### 任务

```c
int32_t xTaskCreate(TaskFunction_t pxTaskCode, const char *pcName,
                    uint32_t ulStackSize, void *pvParam,
                    uint32_t uxPriority, TaskHandle_t *pxHandle);
void vTaskDelete(TaskHandle_t xTask);
void vTaskSuspend(TaskHandle_t xTask);
void vTaskResume(TaskHandle_t xTask);
void vTaskDelay(uint32_t xTicksToDelay);
void taskYIELD(void);
void vTaskStartScheduler(void);
uint32_t xTaskGetTickCount(void);
```

### 队列

```c
QueueHandle_t xQueueCreate(uint32_t uxLength, uint32_t uxItemSize);
int32_t xQueueSend(QueueHandle_t xQueue, const void *pvItem, uint32_t xTicksToWait);
int32_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait);
uint32_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
```

### 信号量

```c
SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(uint32_t uxMaxCount, uint32_t uxInitialCount);
#define xSemaphoreTake(xSem, xTicksToWait)    xQueueReceive(xSem, NULL, xTicksToWait)
#define xSemaphoreGive(xSem)                  xQueueSend(xSem, NULL, 0)
```

### 互斥量

```c
MutexHandle_t xMutexCreate(void);
int32_t xMutexTake(MutexHandle_t xMutex, uint32_t xTicksToWait);
int32_t xMutexGive(MutexHandle_t xMutex);
```

### 内存管理

```c
void  vPortHeapInit(void);
void *pvPortMalloc(size_t xWantedSize);
void  vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
```

## 内核原理

### 上下文切换

```
异常触发时硬件自动保存:  R0-R3, R12, LR, PC, xPSR （8个）
PendSV 中手动保存:      R4-R11                     （8个）

任务切换 = 保存当前任务的 16 个寄存器到它的栈
         + 从下一个任务的栈恢复 16 个寄存器
```

### 调度策略

```
① 优先级位图 + __CLZ 快速查找最高就绪优先级
② 同优先级时间片轮转（SysTick 1ms 驱动）
③ 高优先级任务抢占低优先级任务
```

### 内存管理（Heap4）

```
空闲块单向链表 + 首次适配 + 相邻空闲块合并
分配: 找到够大的空闲块 → 切割 → 返回用户区指针
释放: 插回空闲链表 → 检查前后相邻块 → 合并
```


## License

MIT