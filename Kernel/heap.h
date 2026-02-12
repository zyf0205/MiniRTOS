#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

/* 内存池总大小（字节） */
#define configTOTAL_HEAP_SIZE    (10 * 1024)   /* 10KB */

/* 字节对齐 */
#define portBYTE_ALIGNMENT       8

void  vPortHeapInit(void);
void *pvPortMalloc(size_t xWantedSize);
void  vPortFree(void *pv);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);

#endif