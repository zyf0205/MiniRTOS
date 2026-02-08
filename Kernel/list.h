#ifndef MINI_LIST_H
#define MINI_LIST_H

#include <stdint.h>
#include <stddef.h>

/*---------------------------------------------------------------------------
 *  链表节点
 *---------------------------------------------------------------------------*/
struct xLIST;

typedef struct xLIST_ITEM
{
  uint32_t xItemValue;           /* 排序用的值 */
  struct xLIST_ITEM *pxNext;     /* 后一个节点 */
  struct xLIST_ITEM *pxPrevious; /* 前一个节点 */
  void *pvOwner;                 /* 谁拥有这个节点（指向 TCB） */
  struct xLIST *pvContainer;     /* 这个节点在哪个链表里 */
} ListItem_t;

/*---------------------------------------------------------------------------
 *  迷你节点（哨兵节点专用，省内存，不需要 owner 和 container）
 *---------------------------------------------------------------------------*/
typedef struct xMINI_LIST_ITEM
{
  uint32_t xItemValue;
  struct xLIST_ITEM *pxNext;
  struct xLIST_ITEM *pxPrevious;
} MiniListItem_t;

/*---------------------------------------------------------------------------
 *  链表
 *---------------------------------------------------------------------------*/
typedef struct xLIST
{
  uint32_t uxNumberOfItems; /* 链表中节点个数（不含哨兵） */
  ListItem_t *pxIndex;      /* 遍历用的游标指针 */
  MiniListItem_t xListEnd;  /* 哨兵节点 */
} List_t;

/*---------------------------------------------------------------------------
 *  API
 *---------------------------------------------------------------------------*/
void vListInit(List_t *pxList);
void vListInitItem(ListItem_t *pxItem);
void vListInsertEnd(List_t *pxList, ListItem_t *pxNewItem);
void vListInsert(List_t *pxList, ListItem_t *pxNewItem);
uint32_t uxListRemove(ListItem_t *pxItemToRemove);

#endif