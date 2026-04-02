/**
 * @file hash.c
 * @brief 哈希表实现，用于 Lua 表（数组/字典）的底层存储，包含插入、查找、垃圾收集等。
 *
*  * 哈希表节点 (Node) 包含两个重要成员：
 * - ref: 键 (key)
 * - val: 值 (value)
 */

char *rcs_hash="$Id: hash.c,v 2.1 1994/04/20 22:07:57 celes Exp $";

#include <string.h>
#include <stdlib.h>

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(strcmp(s1,s2)==0)
#define strneq(s1,s2)	(strcmp(s1,s2)!=0)

#define new(s)		((s *)malloc(sizeof(s)))
#define newvector(n,s)	((s *)calloc(n,sizeof(s)))

#define nhash(t)	((t)->nhash)
#define nodelist(t)	((t)->list)
#define list(t,i)	((t)->list[i])
#define markarray(t)    ((t)->mark)
#define ref_tag(n)	(tag(&(n)->ref))
#define ref_nvalue(n)	(nvalue(&(n)->ref))
#define ref_svalue(n)	(svalue(&(n)->ref))

#ifndef ARRAYBLOCK
#define ARRAYBLOCK 50
#endif

typedef struct ArrayList
{
 Hash *array;
 struct ArrayList *next;
} ArrayList;

/** 全局链表头，用于追踪所有已创建的哈希表，供垃圾回收使用。 */
static ArrayList *listhead = NULL;

/**
 * @brief 哈希函数，计算给定键 @p ref 在哈希表中的桶索引。
 *
 * 根据键的类型 (number, string) 将其映射到哈希表的索引内。
 * number 类型直接对 nhash 取模；string 类型逐字符移位累加并对 nhash 取模，防止溢出。
 *
 * @param t 指向哈希表结构体的指针。
 * @param ref 指向Object结构体的指针。
 * @return 类型支持则返回 [0, nhash -1] 范围中的桶索引；类型不支持则返回 -1。
 */
static int head (Hash *t, Object *ref)		/* hash function */
{
 if (tag(ref) == T_NUMBER) return (((int)nvalue(ref))%nhash(t));
 else if (tag(ref) == T_STRING)
 {
  int h;
  char *name = svalue(ref);
  for (h=0; *name!=0; name++)		/* interpret name as binary number */
  {
   h <<= 8;
   h  += (unsigned char) *name;		/* avoid sign extension */
   h  %= nhash(t);			/* make it a valid index */
  }
  return h;
 }
 else
 {
  lua_reportbug ("unexpected type to index table");
  return -1;
 }
}

/**
 * @brief 在给定桶索引中寻找键。
 *
 * 遍历哈希表 @p t 中桶索引为 @p h 的链表，查找与键 @p ref 匹配的节点。
 *
 * @param t 指向哈希表结构体的指针。
 * @param ref 指向 Object 结构体的指针。
 * @param h 通过哈希函数计算得到的桶索引。
 * @return 如果找到那么返回指向节点的指针；如果没找到返回 Null。
 * 
 * @note 更改成更有意义的函数名，注释掉原来的无用代码。
 */
static Node *hashFindNode(Hash *t, Object *ref, int h)
{
//  /**
//   * n 指向当前节点
//   * p 指向前一节点（根据被屏蔽的代码可看出p可能是为了去除节点而创建的）
//   */
//  Node *n=NULL, *p;
 Node* n = NULL;

 if (tag(ref) == T_NUMBER)
 {
  // for (p=NULL,n=list(t,h); n!=NULL; p=n, n=n->next)
  for (n=list(t,h); n != NULL; n=n->next)
   if (ref_tag(n) == T_NUMBER && nvalue(ref) == ref_nvalue(n)) break;
 }  
 else if (tag(ref) == T_STRING)
 {
  // for (p=NULL,n=list(t,h); n!=NULL; p=n, n=n->next)
  for (n = list(t,h); n != NULL; n = n->next)
   if (ref_tag(n) == T_STRING && streq(svalue(ref),ref_svalue(n))) break;
 }  
 if (n==NULL)				/* name not present */
  return NULL;
// #if 0
//  if (p!=NULL)				/* name present but not first */
//  {
//   p->next=n->next;			/* move-to-front self-organization */
//   n->next=list(t,h);
//   list(t,h)=n;
//  }
// #endif
 return n;
}

/**
 * @brief 释放链表函数。
 *
 * 接收一个指定节点 @p n ，遍历并释放从该节点开始的整个链表。
 *
 * @param n 指向需要释放节点的指针。
 */
static void freelist (Node *n)
{
 while (n)
 {
  Node *next = n->next;
  free (n);
  n = next;
 }
}

/**
 * @brief 哈希表创建函数。
 *
 * 根据 @p nhash 的容量创建哈希表并返回指向哈希表的指针。
 *
 * @param nhash 哈希表容量。
 * @return 创建成功返回指向哈希表的指针；创建失败返回空指针并报错。
 * @note 创建的哈希表需要手动释放空间;
 *       补充了失败时候的节点释放。
 */
static Hash *hashcreate (unsigned int nhash)
{
 Hash *t = new (Hash);
 if (t == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 nhash(t) = nhash;
 markarray(t) = 0;
 nodelist(t) = newvector (nhash, Node*);
 if (nodelist(t) == NULL)
 {
  free(t);
  lua_error ("not enough memory");
  return NULL;
 }
 return t;
}

/**
 * @brief 哈希表删除释放函数。
 *
 * 传入指向需要删除的哈希表指针；
 * 遍历哈希表的每个桶，通过 freelist 释放桶中所有节点；
 * 释放哈希表中所有桶，最后释放哈希表结构体。
 *
 * @param h 指向需要删除的哈希表的指针。
 */
static void hashdelete (Hash *h)
{
 int i;
 for (i=0; i<nhash(h); i++)
  freelist (list(h,i));
 free (nodelist(h));
 free(h);
}

/**
 * @brief 哈希标记函数，mark 用于垃圾回收的标记阶段。
 *
 * 如果哈希表没有被标记 (mark == 0) 那么标记成 1；
 * 遍历每个桶中的每个节点，调用 lua_markobject() 函数，
 * 对节点的 string, array(未被标记) 类型的 ref 和 val 进行标记。
 *
 * @param h 指向要被标记的哈希表的指针。
 */
void lua_hashmark (Hash *h)
{
 if (markarray(h) == 0)
 {
  int i;
  markarray(h) = 1;
  // 循环遍历哈希表的每个桶中的所有节点
  for (i=0; i<nhash(h); i++)
  {
   Node *n;
   for (n = list(h,i); n != NULL; n = n->next)
   {
    lua_markobject(&n->ref);
    lua_markobject(&n->val);
   }
  }
 } 
}

/**
 * @brief 哈希表回收函数。
 *
 * 通过 prev 对上一个节点进行追踪，防止删除节点时失去追踪；
 * 如果 mark != 1 (mark == 0)， 说明该节点记录的哈希表已经不再使用，
 * 将其从链表移除，并释放内存；
 * 如果 mark == 1，说明仍在使用，将其 mark 重置为 0，为下一轮标记做准备。
 *
 * @note 垃圾回收的 GC 机制包含 标记-回收-清除 这三个过程，
 *       每次回收完成后都需要对 mark 进行清除，
 *       否则下一轮无法识别清除的对象。
 */
void lua_hashcollector (void)
{
 ArrayList *curr = listhead, *prev = NULL;
 while (curr != NULL)
 {
  ArrayList *next = curr->next;
  if (markarray(curr->array) != 1)
  {
   if (prev == NULL) listhead = next;
   else              prev->next = next;
   hashdelete(curr->array);
   free(curr);
  }
  else
  {
   markarray(curr->array) = 0;
   prev = curr;
  }
  curr = next;
 }
}



/**
 * @brief 创建新的哈希表判断是否触发垃圾回收，并将其加入到全局链表。
 *
 * 分配一个 ArrayList 节点；
 * 对新建立的 ArrayList 节点创建哈希表；
 * 判断是否达到回收阈值，如果达到进行一次垃圾回收；
 * 对计数器进行加一，记录新加入的 ArrayList 节点；
 * 将该节点加入全局链表首位。
 *
 * @param nhash 需要创建的哈希表的容量。
 * @return 创建成功则返回指向新创建的哈希表指针；
 *         创建失败则返回 Null。
 *
 * @warning 垃圾回收不安全会导致 stack 溢出。
 * @note 补充了失败时候的节点释放。
 */
Hash *lua_createarray (int nhash)
{
 ArrayList *new = new(ArrayList);
 if (new == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 new->array = hashcreate(nhash);
 if (new->array == NULL)
 {
  free(new);
  lua_error ("not enough memory");
  return NULL;
 }

 // 如果实例计数器到达阈值那么触发垃圾回收器。
 if (lua_nentity == lua_block)
  lua_pack(); 

 lua_nentity++;
 new->next = listhead;
 listhead = new;
 return new->array;
}

/**
 * @brief 在指定桶中创建新节点并插入到链表头。
 *
 * @param t 指向目标哈希表的指针。
 * @param ref 指向键对象的指针。
 * @param h 桶索引。
 * @return 创建成功则返回新节点指针；失败则返回 Null。
 */
static Node* hashNewNode(Hash* t, Object* ref, int h) {
  Node* n = new(Node);
  if (n == NULL) {
   lua_error("not enough memory");
   return NULL;
  }
  n->ref = *ref;
  tag(&n->val) = T_NIL;
  n->next = list(t, h);
  list(t, h) = n;
  return n;
}

// /**
//  * @brief 在哈希表中查找或创建与给定 ref 对应的节点并返回节点 val 的指针。
//  *
//  * 调用 head() 根据 @p t 和 @p ref 找到对应的桶索引；
//  * 通过 present() 在桶索引的链表中搜索键对应的节点；
//  * 如果在桶的链表中没有找到 @p ref，创建新节点并将键设置成 @p ref；
//  * val 设置成 nil 类型，并将创建的节点插入桶链表的首位；
//  *
//  * @param t 指向目标哈希表的指针。
//  * @param ref 指向键对象的指针。
//  * @return 成功则返回指向节点值(val)的指针；失败则返回 Null。
//  */
// Object *lua_hashdefine (Hash *t, Object *ref)
// {
//  int   h;
//  Node *n;
//  h = head (t, ref);
//  if (h < 0) return NULL; 
 
//  n = present(t, ref, h);
//  if (n == NULL)
//  {
//   n = new(Node);
//   if (n == NULL)
//   {
//    lua_error ("not enough memory");
//    return NULL;
//   }
//   n->ref = *ref;
//   tag(&n->val) = T_NIL;
//   n->next = list(t,h);
//   list(t,h) = n;
//  }
//  return (&n->val);
// }

/**
 * @brief 在哈希表中查找与给定 ref 对应的节点并返回节点 val 的指针。
 *
 * 调用 head() 根据 @p t 和 @p ref 找到对应的桶索引；
 * 通过 hashFindNode() 在桶索引的链表中搜索键对应的节点；
 * 如果没有找到，则返回 Null, 不修改哈希表结构。
 *
 * @param t 指向目标哈希表的指针。
 * @param ref 指向键对象的指针。
 * @return 成功则返回指向节点值(val)的指针；未找到或失败则返回 Null。
 * @note 原函数会导致创建 nil 节点从而影响哈希表结构；
 *       此处修改成找不到就不修改哈希表结构。
 */
Object *lua_hashGet (Hash *t, Object *ref)
{
 int   h;
 Node *n;
 h = head (t, ref);
 if (h < 0) return NULL; 
 
 n = hashFindNode(t, ref, h);
 if (n == NULL)
 {
  return NULL;
 }
 return (&n->val);
}

/**
 * @brief 在哈希表中查找或创建与给定 @p ref 对应的节点，并返回节点 val 的指针。
 * 
 * 调用 head() 根据 @p t 和 @p ref 找到对应的桶索引；
 * 通过 hashFindNode() 在桶中搜索对应节点；
 * 如果在桶的链表中没有找到 @p ref 则创建新的节点并返回其值槽位。
 * 
 * @param t 指向目标哈希表的指针。
 * @param ref 指向键对象的指针。
 * @return 成功则返回指向节点的值(val)的指针；失败则返回 Null。
 */
Object* lua_hashEnsure(Hash* t, Object* ref) {
  int h;
  Node* n;
  h = head(t, ref);
  if (h < 0) {
   return NULL;
  }

  n = hashFindNode(t, ref, h);
  if (n == NULL) {
   n = hashNewNode(t, ref, h);
   if (n == NULL) {
    return NULL;
   }
  }
  return (&n->val);
}

/**
 * @brief 遍历哈希表并将数据压入 lua stack 中。
 *
 * 从指定哈希表的桶索引开始搜索，找到下一个 val 为非 nil 类型的节点，
 * 将其键 (ref) 和值 (val) 的指针压入 lua stack；
 * 未找到则压入两个 nil 到 lua stack。
 *
 * @param a 指向要遍历的哈希表的指针。
 * @param h 指定的桶索引开始的地方。
 *
 * @note 该函数可以实现对于数据的迭代，是迭代器的辅助函数。
 */
static void firstnode (Hash *a, int h)
{
 /** 给定索引 @p h 不超过桶容量则进行搜索，
  * 否则压入两个 nil 到 lua stack.
  */
 if (h < nhash(a))
 {  
  int i;
  // 从给定桶索引一直搜索到最后一个桶。
  for (i=h; i<nhash(a); i++)
  {
   // 如果给定桶不为空则遍历节点进行搜索。
   if (list(a,i) != NULL)
   {
    // 如果首节点 val 的类型不为 nil 则不进行遍历，直接将其 ref, val 压入 lua stack。
    if (tag(&list(a,i)->val) != T_NIL)
    {
     lua_pushobject (&list(a,i)->ref);
     lua_pushobject (&list(a,i)->val);
     return;
    }
    // 遍历链表找到 val 为非 nil 类型的节点,并将其 ref, val 压入 lua stack。
    else
    {
     Node *next = list(a,i)->next;
     while (next != NULL && tag(&next->val) == T_NIL) next = next->next;
     if (next != NULL)
     {
      lua_pushobject (&next->ref);
      lua_pushobject (&next->val);
      return;
     }
    }
   }
  }
 }
 lua_pushnil();
 lua_pushnil();
}

/**
 * @brief lua next 函数实现。
 *
 * 使用 lua_getparam 从 lua stack 获得参数 o, r 的指针；
 * 参数 o 必须是一个 array 类型，参数 r 可以是 nil 类型；
 * 根据当前的键搜索表中下一个键值对。
 *
 * @details
 * 1.参数检查：必须得到两个参数，参数 o 必须是 array 类型。
 * 2.如果参数 r 的类型是 nil，调用 firstnode() 从第一个桶开始搜索下一个有效节点。
 * 3.如果参数 r 的类型不是 nil:
 *     (1).调用 head() 计算得到桶索引 h。
 *     (2).在桶 h 中搜索键与给定键 r 相同的节点。
 *     (3).如果没有找到，抛出 lua_error。
 *     (4).如果找到了，从该节点向后寻找第一个值 (val) 为非 nil 类型的节点，
 *         并将其键 (ref) 和值 (val) 压入 lua stack
 */
void lua_next (void)
{
 Hash   *a;
 // 得到两个参数
 Object *o = lua_getparam (1);
 Object *r = lua_getparam (2);
 // 检查参数
 if (o == NULL || r == NULL)
 { lua_error ("too few arguments to function `next'"); return; }
 if (lua_getparam (3) != NULL)
 { lua_error ("too many arguments to function `next'"); return; }
 if (tag(o) != T_ARRAY)
 { lua_error ("first argument of function `next' is not a table"); return; }
 // 得到参数 o 的哈希表
 a = avalue(o);
 if (tag(r) == T_NIL)
 {
  firstnode (a, 0);
  return;
 }
 else
 {
  int h = head (a, r);
  if (h >= 0)
  {
   Node *n = list(a,h);
   // 遍历桶 h 中的所有节点
   while (n)
   {
    // 找到键与给定键相同的节点
    if (memcmp(&n->ref,r,sizeof(Object)) == 0)
    {
     // 如果该节点是 h 对应桶的最后一个，从下一个桶索引开始搜索
     if (n->next == NULL)
     {
      firstnode (a, h+1);
      return;
     }
     // 如果下一个节点的 val 的类型不是 nil，将节点的 ref, val 压入 lua stack
     else if (tag(&n->next->val) != T_NIL)
     {
      lua_pushobject (&n->next->ref);
      lua_pushobject (&n->next->val);
      return;
     }
     else
     {
      // 跳过 val 类型是 nil 的节点
      Node *next = n->next->next;
      // 遍历并跳过 val 类型是 nil 的节点，直到最后一个节点
      while (next != NULL && tag(&next->val) == T_NIL) next = next->next;
      // 如果到最后一个节点，从下一个桶索引开始搜索
      if (next == NULL)
      {
       firstnode (a, h+1);
       return;
      }
      else
      {
       lua_pushobject (&next->ref);
       lua_pushobject (&next->val);
      }
      return;
     }
    }
    n = n->next;
   }
   // 在桶 h 中没有找到键与给定键相同的节点，抛出 lua_error
   if (n == NULL)
    lua_error ("error in function 'next': reference not found");
  }
 }
}
