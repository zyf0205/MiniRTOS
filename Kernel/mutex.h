#ifndef MUTEX_H
#define MUTEX_H

#include "queue.h"
#include "task.h"

/*---------------------------------------------------------------------------
 *  互斥量结构（扩展队列）
 *---------------------------------------------------------------------------*/
typedef struct Mutex
{
    Queue_t xQueue;              /* 底层还是队列（容量1，大小0） */
    TCB_t *pxOwner;              /* 谁持有这个互斥量 */
    uint32_t uxOriginalPriority; /* 持有者的原始优先级 */
} Mutex_t;

typedef Mutex_t *MutexHandle_t;

/*---------------------------------------------------------------------------
 *  API
 *---------------------------------------------------------------------------*/
MutexHandle_t xMutexCreate(void);
int32_t xMutexTake(MutexHandle_t xMutex, uint32_t xTicksToWait);
int32_t xMutexGive(MutexHandle_t xMutex);

#endif