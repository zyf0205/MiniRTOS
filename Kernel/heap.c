#include "heap.h"
#include "task.h"
#include <string.h>

/*---------------------------------------------------------------------------
 *  对齐宏
 *---------------------------------------------------------------------------*/
#define portBYTE_ALIGNMENT_MASK (portBYTE_ALIGNMENT - 1) /*对齐掩码：0b111*/

/*---------------------------------------------------------------------------
 *  空闲块头部
 *---------------------------------------------------------------------------*/
typedef struct BlockLink
{
    struct BlockLink *pxNextFreeBlock; /*下一个空闲块的首地址*/
    uint32_t xBlockSize;               /*下一个空闲块的大小*/
} BlockLink_t;

/* 头部大小（对齐后） */
static const uint32_t xHeapStructSize =
    (sizeof(BlockLink_t) + portBYTE_ALIGNMENT_MASK) & ~portBYTE_ALIGNMENT_MASK; /*把BlockLink_t 向上取整到8字节的倍数*/

/*---------------------------------------------------------------------------
 *  内存池
 *---------------------------------------------------------------------------*/
static uint8_t ucHeap[configTOTAL_HEAP_SIZE];

/*---------------------------------------------------------------------------
 *  空闲链表哨兵
 *---------------------------------------------------------------------------*/
static BlockLink_t xStart; /* 链表头 */
static BlockLink_t *pxEnd = NULL; /* 链表尾（位于堆内末尾的哨兵） */

/*---------------------------------------------------------------------------
 *  统计信息
 *---------------------------------------------------------------------------*/
static size_t xFreeBytesRemaining = 0;            /*当前剩余可用堆空间（字节数）*/
static size_t xMinimumEverFreeBytesRemaining = 0; /*运行过程中出现过的最小剩余堆空间*/
static uint32_t xHeapInitialised = 0;             /*堆是否已经初始化的标志位*/

/* 已分配块的最高位标记（用来区分已分配和空闲） */
static uint32_t xBlockAllocatedBit = 0;

/*---------------------------------------------------------------------------
 *  把空闲块插回链表（按地址排序 + 合并相邻块）
 *---------------------------------------------------------------------------*/
static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert)
{
    BlockLink_t *pxIterator;
    uintptr_t uxBlockToInsert;

    /* 按地址排序，找到插入位置 */
    uxBlockToInsert = (uintptr_t)pxBlockToInsert;
    for (pxIterator = &xStart;
         (pxIterator->pxNextFreeBlock != NULL) &&
             ((uintptr_t)pxIterator->pxNextFreeBlock < uxBlockToInsert);
         pxIterator = pxIterator->pxNextFreeBlock)
    {
        /* 空循环，找到前一个块 */
    }

    /* --- 尝试和前面的块合并 --- */
    if (((uint8_t *)pxIterator + pxIterator->xBlockSize) == (uint8_t *)pxBlockToInsert)
    {
        /* 前一个块的末尾 == 当前块的起始 → 合并 */
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;
    }

    /* --- 尝试和后面的块合并 --- */
    if (((uint8_t *)pxBlockToInsert + pxBlockToInsert->xBlockSize) ==
        (uint8_t *)pxIterator->pxNextFreeBlock)
    {
        if (pxIterator->pxNextFreeBlock != pxEnd)
        {
            /* 当前块的末尾 == 后一个块的起始 → 合并 */
            pxBlockToInsert->xBlockSize +=
                pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock =
                pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
        else
        {
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    }
    else
    {
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 如果没和前面合并，需要让前面指向自己 */
    if (pxIterator != pxBlockToInsert)
    {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

/*---------------------------------------------------------------------------
 *  初始化堆
 *---------------------------------------------------------------------------*/
void vPortHeapInit(void)
{
    BlockLink_t *pxFirstFreeBlock; /*指向“第一个空闲块”（初始化时就是整个堆）*/
    uint8_t *pxAlignedHeap;        /*对齐后的堆起始地址，用八位便于计算*/
    uintptr_t uxAddress;           /*用于做地址运算的整数形式地址*/
    uint32_t xTotalHeapSize;       /*对齐后实际可用的堆总大小。*/

    /* 对齐内存池起始地址 */
    uxAddress = (uintptr_t)ucHeap;
    if (uxAddress & portBYTE_ALIGNMENT_MASK)
    {
        uxAddress += portBYTE_ALIGNMENT;
        uxAddress &= ~portBYTE_ALIGNMENT_MASK;
    }
    pxAlignedHeap = (uint8_t *)uxAddress; /*向高地址跳动到八字结对齐后的地址*/

    /* 可用大小 */
    xTotalHeapSize = configTOTAL_HEAP_SIZE - (uint32_t)(pxAlignedHeap - ucHeap); /*总堆大小减去这部分，得到真正能用的堆大小。*/

    /* 在堆末尾放置链表尾哨兵（heap_4 风格） */
    xTotalHeapSize -= xHeapStructSize;
    xTotalHeapSize &= ~((uint32_t)portBYTE_ALIGNMENT_MASK);
    pxEnd = (BlockLink_t *)(pxAlignedHeap + xTotalHeapSize);
    pxEnd->xBlockSize = 0;
    pxEnd->pxNextFreeBlock = NULL;

    /* 初始化 xStart */
    xStart.pxNextFreeBlock = (BlockLink_t *)pxAlignedHeap;
    xStart.xBlockSize = 0;

    /* 整个内存池是一个大空闲块 */
    pxFirstFreeBlock = (BlockLink_t *)pxAlignedHeap;
    pxFirstFreeBlock->xBlockSize = (uint32_t)((uint8_t *)pxEnd - (uint8_t *)pxFirstFreeBlock);
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

    /* 统计 */
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;            /*初始化时：总可用堆*/
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize; /*最小等于当前空余*/

    /* 最高位用来标记已分配 */
    xBlockAllocatedBit = ((uint32_t)1) << 31;

    xHeapInitialised = 1; /*已初始化*/
}

/*---------------------------------------------------------------------------
 *  分配内存
 *---------------------------------------------------------------------------*/
void *pvPortMalloc(size_t xWantedSize)
{
    BlockLink_t *pxBlock;         /*用于链表操作的中间变量*/
    BlockLink_t *pxPreviousBlock; /*用于链表操作的中间变量*/
    BlockLink_t *pxNewBlock;      /*分配遗留下来的新块（空闲块）*/
    void *pvReturn = NULL;

    if (xHeapInitialised == 0)
    {
        vPortHeapInit();
    }

    taskENTER_CRITICAL();

    if (xWantedSize > 0)
    {
        /* 实际分配大小要加上头部大小 */
        xWantedSize += xHeapStructSize;

        /* 8字节对齐 */
        if (xWantedSize & portBYTE_ALIGNMENT_MASK)
        {
            xWantedSize += portBYTE_ALIGNMENT;
            xWantedSize &= ~portBYTE_ALIGNMENT_MASK;
        }
    }

    /* 大小合理且有足够空间（不超过最高位，且有足够空间） */
    if (xWantedSize > 0 && (xWantedSize & xBlockAllocatedBit) == 0 && xWantedSize <= xFreeBytesRemaining)
    {
        /* 遍历空闲链表，找第一个够大的块 */
        pxPreviousBlock = &xStart;
        pxBlock = xStart.pxNextFreeBlock;

        /*寻找符合条件的区块*/
        while ((pxBlock->xBlockSize < xWantedSize) &&
               (pxBlock->pxNextFreeBlock != NULL))
        {
            pxPreviousBlock = pxBlock;
            pxBlock = pxBlock->pxNextFreeBlock;
        }

        /*找到了*/
        if (pxBlock != pxEnd)
        {
            /*返回头部后面的地址给用户 */
            pvReturn = (void *)((uint8_t *)pxBlock + xHeapStructSize);

            /* 从空闲链表移除 */
            pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

            /* 剩余空间够不够切出一个新空闲块？ */
            if ((pxBlock->xBlockSize - xWantedSize) > (xHeapStructSize * 2))
            {
                /* 切一刀：后半部分变成新的空闲块 */
                pxNewBlock = (BlockLink_t *)((uint8_t *)pxBlock + xWantedSize); /*分配后新块的地址*/
                pxNewBlock->xBlockSize = pxBlock->xBlockSize - xWantedSize;     /*得到分配后新块的大小*/

                /* 当前块（分配出来的块）缩小 */
                pxBlock->xBlockSize = xWantedSize;

                /* 新空闲块插回链表 */
                prvInsertBlockIntoFreeList(pxNewBlock);
            }

            /* 更新统计 */
            xFreeBytesRemaining -= pxBlock->xBlockSize;

            if (xFreeBytesRemaining < xMinimumEverFreeBytesRemaining)
            {
                xMinimumEverFreeBytesRemaining = xFreeBytesRemaining; /*更新最小堆空间*/
            }

            /* 标记为已分配（最高位置1） */
            pxBlock->xBlockSize |= xBlockAllocatedBit;
            pxBlock->pxNextFreeBlock = NULL;
        }
    }

    taskEXIT_CRITICAL();

    return pvReturn; /*返回分配的块的用户地址*/
}

/*---------------------------------------------------------------------------
 *  释放内存
 *---------------------------------------------------------------------------*/
void vPortFree(void *pv)
{
    BlockLink_t *pxBlock;

    if (pv == NULL)
        return;

    /* 从用户指针反推块头部 */
    pxBlock = (BlockLink_t *)((uint8_t *)pv - xHeapStructSize);

    /* 确认是已分配的块 */
    if ((pxBlock->xBlockSize & xBlockAllocatedBit) == 0)
        return;
    if (pxBlock->pxNextFreeBlock != NULL)
        return;

    taskENTER_CRITICAL();

    /* 清除已分配标记 */
    pxBlock->xBlockSize &= ~xBlockAllocatedBit;

    /* 更新统计 */
    xFreeBytesRemaining += pxBlock->xBlockSize;

    /* 插回空闲链表（自动合并相邻块） */
    prvInsertBlockIntoFreeList(pxBlock);

    taskEXIT_CRITICAL();
}

/*---------------------------------------------------------------------------
 *  查询信息
 *---------------------------------------------------------------------------*/
size_t xPortGetFreeHeapSize(void)
{
    return xFreeBytesRemaining;
}

size_t xPortGetMinimumEverFreeHeapSize(void)
{
    return xMinimumEverFreeBytesRemaining;
}