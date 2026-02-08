#include "list.h"

/*---------------------------------------------------------------------------
 *  初始化链表
 *
 *  初始化后的状态:
 *    pxIndex → xListEnd
 *    xListEnd.pxNext → xListEnd（自己指自己，空链表）
 *    xListEnd.pxPrevious → xListEnd
 *---------------------------------------------------------------------------*/
void vListInit(List_t *pxList)
{
    /* 游标指向哨兵 */
    pxList->pxIndex = (ListItem_t *)&(pxList->xListEnd);

    /* 哨兵的值设为最大，排序插入时新节点一定在它前面 */
    pxList->xListEnd.xItemValue = 0xFFFFFFFFUL;

    /* 空链表：哨兵自己指自己 */
    pxList->xListEnd.pxNext = (ListItem_t *)&(pxList->xListEnd);
    pxList->xListEnd.pxPrevious = (ListItem_t *)&(pxList->xListEnd);

    /* 节点计数 = 0 */
    pxList->uxNumberOfItems = 0;
}

/*---------------------------------------------------------------------------
 *  初始化节点
 *---------------------------------------------------------------------------*/
void vListInitItem(ListItem_t *pxItem)
{
    /* 标记：不属于任何链表 */
    pxItem->pvContainer = NULL;
}

/*---------------------------------------------------------------------------
 *  插入到链表末尾（pxIndex 的前面）
 *
 *  用途：就绪链表中，同优先级的任务按创建顺序排列
 *
 *  插入前:
 *    ... ←→ [Prev] ←→ [pxIndex] ←→ ...
 *
 *  插入后:
 *    ... ←→ [Prev] ←→ [New] ←→ [pxIndex] ←→ ...
 *---------------------------------------------------------------------------*/
void vListInsertEnd(List_t *pxList, ListItem_t *pxNewItem)
{
    ListItem_t *pxIndex = pxList->pxIndex;

    /* 新节点插到 pxIndex 前面 */
    pxNewItem->pxNext = pxIndex;
    pxNewItem->pxPrevious = pxIndex->pxPrevious;
    pxIndex->pxPrevious->pxNext = pxNewItem;
    pxIndex->pxPrevious = pxNewItem;

    /* 记录所属链表 */
    pxNewItem->pvContainer = pxList;

    pxList->uxNumberOfItems++;
}

/*---------------------------------------------------------------------------
 *  按 xItemValue 排序插入（升序）
 *
 *  用途：延时链表中，唤醒时间早的排前面
 *
 *  遍历链表找到第一个 value 比 newItem 大的节点，插在它前面
 *  哨兵 value = MAX，所以新节点一定插在哨兵前面
 *---------------------------------------------------------------------------*/
void vListInsert(List_t *pxList, ListItem_t *pxNewItem)
{
    ListItem_t *pxIterator;
    const uint32_t xValueToInsert = pxNewItem->xItemValue;

    /* 从哨兵开始往后找 */
    if (xValueToInsert == 0xFFFFFFFFUL)
    {
        /* 值等于最大，插在哨兵前面 */
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        for (pxIterator = (ListItem_t *)&(pxList->xListEnd);
             pxIterator->pxNext->xItemValue <= xValueToInsert;
             pxIterator = pxIterator->pxNext)
        {
            /* 空循环体，找到插入位置 */
        }
    }

    /* 插在 pxIterator 后面 */
    pxNewItem->pxNext = pxIterator->pxNext;
    pxNewItem->pxPrevious = pxIterator;
    pxIterator->pxNext->pxPrevious = pxNewItem;
    pxIterator->pxNext = pxNewItem;

    pxNewItem->pvContainer = pxList;

    pxList->uxNumberOfItems++;
}

/*---------------------------------------------------------------------------
 *  移除节点
 *
 *  返回链表中剩余节点个数
 *---------------------------------------------------------------------------*/
uint32_t uxListRemove(ListItem_t *pxItemToRemove)
{
    List_t *pxList = pxItemToRemove->pvContainer;

    /* 前后节点互相指 */
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;

    /* 如果游标正好指向被删的节点，退回到前一个 */
    if (pxList->pxIndex == pxItemToRemove)
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }

    /* 标记：不再属于任何链表 */
    pxItemToRemove->pvContainer = NULL;

    pxList->uxNumberOfItems--;

    return pxList->uxNumberOfItems;
}