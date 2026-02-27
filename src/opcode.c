/**
 * @file opcode.c
 * @brief 虚拟机核心，执行字节码指令（lua_execute），实现栈操作、算术运算、跳转、函数调用等，提供 API 函数的实现。
 */

char *rcs_opcode="$Id: opcode.c,v 2.1 1994/04/20 22:07:57 celes Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stdlib.h does not have this in SunOS */
extern double strtod(const char *, char **);

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define tonumber(o) ((tag(o) != T_NUMBER) && (lua_tonumber(o) != 0))
#define tostring(o) ((tag(o) != T_STRING) && (lua_tostring(o) != 0))

#ifndef MAXSTACK
#define MAXSTACK 256
#endif
/**
 * 初始化一个 lua stack，用于将 lua object 压入栈。
 * 将 stack[0] 标记为栈底，可以避免向下溢出。
 * 将栈顶指针和栈基指针对齐到第一个可用栈空间 stack[1]。
 */
static Object stack[MAXSTACK] = {{T_MARK, {NULL}}};
static Object *top=stack+1, *base=stack+1;

/**
 * @brief lua 字符串连接函数。
 *
 * 将 @p r 连接到 @p l 的后面；
 * 在新得到的字符串首尾添加空字符标志 '\0'。
 *
 * @param l DEST 字符串
 * @param r SRC 字符串
 * @return 连接成功返回指向新字符串的指针；
 *         连接失败抛出 lua_error 并返回 Null。
 * @note 这里返回的 s 是自增过的指针，指向的是堆上分配的空间索引 1 的位置；
 *       此时首标记在 s[-1] 的位置；
 *       满足了垃圾回收等底层机制的需求，又保持了字符串接口的简单和标准。
 */
static char *lua_strconc (char *l, char *r)
{
 // 为生成的字符串开辟空间
 // +2 为了预留出首、尾的标记空字符 '\0'
 char *s = calloc (strlen(l)+strlen(r)+2, sizeof(char));
 if (s == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 *s++ = 0; // 将首位标记为 '\0'
 return strcat(strcpy(s,l),r); // 将字符串 l 复制到 s，再将 r 连接到 s 后面
}

/**
 * @brief lua 字符串深拷贝函数。
 *
 * 为新字符串开辟新的内存空间，并将原字符串复制到新的字符串；
 * 两个字符串之间互不干扰，是一个深拷贝；
 * 在新得到的字符串首尾添加空字符标志 '\0'。
 *
 * @param l 指向需要复制的字符串的指针。
 * @return 复制成功则返回指向新字符串的指针；
 *         复制失败则抛出 lua_error 并返回 Null。
 */
char *lua_strdup (char *l)
{
 char *s = calloc (strlen(l)+2, sizeof(char));
 if (s == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 *s++ = 0; 			/* create mark space */
 return strcpy(s,l);
}

/**
 * @brief 将 string 转为 number 类型的数字。
 *
 * @param obj 指向需要转换的 lua 对象的指针。
 * @return 返回 0 说明成功转换；返回非 0 说明转换失败。
 * @note 直接改变原对象。
 */
static int lua_tonumber (Object *obj)
{
 char *ptr; // 用于判断是否将整个字符串转换完成

 // 判断传入的 lua 对象是否为 string 类型
 if (tag(obj) != T_STRING)
 {
  lua_reportbug ("unexpected type at conversion to number");
  return 1;
 }

 // 通过 C 标准库 stdlib.h 提供的函数解析字符串
 nvalue(obj) = strtod(svalue(obj), &ptr);

 /**
  * 如果最终 ptr 指向结尾空字符，说明将整个字符串转换成功，
  * 如果最终 ptr 指向的不是结尾空字符，说明有没能解析的内容，转换失败。
  */
 if (*ptr)
 {
  lua_reportbug ("string to number conversion failed");
  return 2;
 }
 tag(obj) = T_NUMBER; // 将 lua 对象的类型转换成 number 类型
 return 0;
}

/**
 * @brief lua 对象转换成 number 类型对象的函数。
 *
 * @param obj 指向需要转换的对象的指针
 * @return 返回一个指向静态对象的指针。
 *         转换成功则返回 number 类型的对象；
 *         转换失败则返回 nil 类型的对象。
 * @note 转换得到的对象存储在静态局部变量中。
 *       不改变原对象。
 */
static Object *lua_convtonumber (Object *obj)
{
 static Object cvt;
 
 if (tag(obj) == T_NUMBER)
 {
  cvt = *obj;
  return &cvt;
 }
  
 tag(&cvt) = T_NIL;
 if (tag(obj) == T_STRING)
 {
  char *ptr;
  nvalue(&cvt) = strtod(svalue(obj), &ptr);
  if (*ptr == 0)
   tag(&cvt) = T_NUMBER;
 }
 return &cvt;
}

/**
 * @brief 将 number 转为 string 类型的数字。
 *
 * @param obj 指向需要转换的 lua 对象的指针。
 * @return 返回 0 说明成功转换；返回非 0 说明转换失败。
 * @note 直接改变原对象。
 */
static int lua_tostring (Object *obj)
{
 static char s[256]; // 创建缓冲区域

 // 如果对象是非 number 类型的，抛出错误并返回 1
 if (tag(obj) != T_NUMBER)
 {
  lua_reportbug ("unexpected type at conversion to string");
  return 1;
 }

 // 判断是否为 int 类型的数据(会影响写入字符缓冲数组时候的占位符)
 if ((int) nvalue(obj) == nvalue(obj))
  sprintf (s, "%d", (int) nvalue(obj));
 else
  sprintf (s, "%g", nvalue(obj));

 /**
  * 将 lua 对象的值(value)设置成转换得到的字符串。
  * 转换失败得到 Null。
  * 这里的 Null 只是一种防御性编程，
  * 实际上在 lua_strdup() 出现错误会抛出 lua_error,
  * 使得 "svalue(obj) =" 这一步根本不执行，不会破坏数据。
  */
 svalue(obj) = lua_createstring(lua_strdup(s));
 if (svalue(obj) == NULL)
  return 1;
 tag(obj) = T_STRING;
 return 0;
}


/*
** Execute the given opcode. Return 0 in success or 1 on error.
*/
int lua_execute (Byte *pc)
{
 // 进入新的作用域的基本步骤
 Object *oldbase = base; // 保留原栈基指针
 base = top; // 栈指针对齐
 while (1) // 直到遇到 HALT 退出
 {
  OpCode opcode;
  // 从 pc 读取一个操作码，然后pc自增
  switch (opcode = (OpCode)*pc++)
  {
   // 将 nil 对象压入 lua stack，栈顶指针自增
   case PUSHNIL: tag(top++) = T_NIL; break;

   // 将 number 对象压入 lua stack，栈顶指针自增
   case PUSH0: tag(top) = T_NUMBER; nvalue(top++) = 0; break;
   case PUSH1: tag(top) = T_NUMBER; nvalue(top++) = 1; break;
   case PUSH2: tag(top) = T_NUMBER; nvalue(top++) = 2; break;

   // 将 byte 对象压入 lua stack，从 pc 读取一个字节压入 lua stack，栈顶指针自增，pc 自增
   case PUSHBYTE: tag(top) = T_NUMBER; nvalue(top++) = *pc++; break;

   /**
    * 压入一个字到 lua stack。
    * 这里使用了 union 共享内存的机制（PUSHFLOAT 也用到同样思路），
    * 通过读取两个 byte 类型的数据到 CodeWord 联合体的结构体变量中，
    * 再通过 Word 类型变量取出压入 lua stack。
    */
   case PUSHWORD: 
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 读取两个字节并存入 code
    tag(top) = T_NUMBER; nvalue(top++) = code.w; // 将字信息压入 lua stack
   }
   break;

   // 压入一个浮点数到 lua stack
   case PUSHFLOAT:
   {
    CodeFloat code;
    get_float(code,pc); // 从 pc 读取四个字节并存入 code
    tag(top) = T_NUMBER; nvalue(top++) = code.f; // 将浮点数信息压入 lua stack
   }
   break;

   // 压入一个字符串到 lua stack
   case PUSHSTRING:
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 读取一个字并存入 code 作为索引
    tag(top) = T_STRING;
    svalue(top++) = lua_constant[code.w]; // 从全局常量表取出对应字符串并压栈
   }
   break;

   // 局部变量快速访问，通过栈基指针加偏移量的形式计算位置，并将内容复制到栈顶指针处
   case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2:
   case PUSHLOCAL3: case PUSHLOCAL4: case PUSHLOCAL5:
   case PUSHLOCAL6: case PUSHLOCAL7: case PUSHLOCAL8:
   case PUSHLOCAL9: *top++ = *(base + (int)(opcode-PUSHLOCAL0)); break;

   // 压入指定偏移量的局部变量
   case PUSHLOCAL: *top++ = *(base + (*pc++)); break; // 从 pc 读取一个字节作为偏移量

   // 压入一个全局变量到 lua stack
   case PUSHGLOBAL: 
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 读取一个字并存入 code 作为索引
    *top++ = s_object(code.w); // 从全局符号表中取出对应全局变量并压入 lua stack
   }
   break;

   // 执行索引操作，例如 table[ref]
   case PUSHINDEXED:
    /**
     * 从下到栈顶数据依次是：表，索引
     */
    --top; // 将栈顶指针指向索引
    // 通过索引查询的必须是一个 array 类型对象
    if (tag(top-1) != T_ARRAY)
    {
     lua_reportbug ("indexed expression not a table");
     return 1;
    }
    {
     Object *h = lua_hashdefine (avalue(top-1), top); // 查询到索引对应的值(val)
     if (h == NULL) return 1;
     *(top-1) = *h; // 将查询到的结果压入 lua stack，替代栈中原先的 array 对象位置的数据
    }
   break;

   // 为 lua stack 对象添加特殊标记
   case PUSHMARK: tag(top++) = T_MARK; break;

   // 复制 lua stack 中从栈顶向下第3个对象到栈顶
   case PUSHOBJECT: *top = *(top-3); top++; break;

   // 局部变量快速存储，通过栈基指针加偏移量的形式计算位置，将栈顶对象弹出并复制到此处
   case STORELOCAL0: case STORELOCAL1: case STORELOCAL2:
   case STORELOCAL3: case STORELOCAL4: case STORELOCAL5:
   case STORELOCAL6: case STORELOCAL7: case STORELOCAL8:
   case STORELOCAL9: *(base + (int)(opcode-STORELOCAL0)) = *(--top); break;

   // 弹出栈顶数据并存储到指定偏移量的局部变量处
   case STORELOCAL: *(base + (*pc++)) = *(--top); break;

   // 存储全局变量
   case STOREGLOBAL:
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 读取一个字并存入 code 作为索引
    s_object(code.w) = *(--top); // 将栈顶对象弹出并存储到全局符号表对应索引处
   }
   break;

   // 执行表赋值 例如 table[ref] = value
   case STOREINDEXED0:
    /**
     * 从下到栈顶的对象内容依次是：表对象，索引，值
     */
    // 类型检查，必须是表类型(array)
    if (tag(top-3) != T_ARRAY)
    {
     lua_reportbug ("indexed expression not a table");
     return 1;
    }
    {
     // 通过表和索引创建对应节点并且得到指向值的指针
     Object *h = lua_hashdefine (avalue(top-3), top-2);
     if (h == NULL) return 1;
     // 将值存储到对应的节点
     *h = *(top-1);
    }
    top -= 3; // 弹出 lua stack
   break;

   // 执行带有偏移量 n 的表赋值
   case STOREINDEXED:
   {
    int n = *pc++; // 从 pc 读取一个字节作为偏移量
    if (tag(top-3-n) != T_ARRAY)
    {
     lua_reportbug ("indexed expression not a table");
     return 1;
    }
    {
     Object *h = lua_hashdefine (avalue(top-3-n), top-2-n);
     if (h == NULL) return 1;
     *h = *(top-1);
    }
    top--;
   }
   break;

   // 存储表
   case STORELIST0:
   case STORELIST:
   {
    /**
     * 从下到栈顶依次是：表对象，n 个元素
     */
    int m, n; // m 是索引偏移，n 是元素个数
    Object *arr;
    if (opcode == STORELIST0) m = 0;
    else m = *(pc++) * FIELDS_PER_FLUSH;
    n = *(pc++); // 从 pc 中获得元素个数
    arr = top-n-1; // 得到指向表对象的指针
    // 类型检查，必须是表类型(array)
    if (tag(arr) != T_ARRAY)
    {
     lua_reportbug ("internal error - table expected");
     return 1;
    }
    /**
     * 遍历每个元素
     *
     * 每处理一个就弹出一个，最终 lua stack 中元素都将弹出，仅留下表对象。
     */
    while (n)
    {
     tag(top) = T_NUMBER;
     nvalue(top) = (float)(n+m); // 计算出索引并存在栈顶空位置
     // 创建对应索引的节点并将值(value)设置成栈顶数据
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   // 存储字符串，用于初始化表的记录
   case STORERECORD:
   {
    int n = *(pc++); // 从 pc 中获得元素个数
    Object *arr = top-n-1; // 得到指向表对象的指针
    // 类型检查，必须是表类型(array)
    if (tag(arr) != T_ARRAY)
    {
     lua_reportbug ("internal error - table expected");
     return 1;
    }

    /**
     * 遍历每个元素
     *
     * 每处理一个就弹出一个，最终 lua stack 中元素都将弹出，仅留下表对象。
     */
    while (n)
    {
     CodeWord code;
     get_word(code,pc); // 从 pc 读取一个字并存入 code，作为索引
     tag(top) = T_STRING;
     svalue(top) = lua_constant[code.w]; // 从全局常量表取出对应字符串并存在栈顶空位置
     // 创建对应索引的节点并将值(value)设置成栈顶数据
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   // 调整栈顶到指定位置
   case ADJUST:
   {
    Object *newtop = base + *(pc++); // 计算栈顶指针的位置(相对于栈基指针)

    /**
     * 如果栈顶指针位置比计算得到的 newtop 低，
     * 栈顶指针上移并加入 nil 对象；
     * 直到栈顶指针指向新位置结束。
     */
    while (top < newtop) tag(top++) = T_NIL;
    // 如果栈顶指针比计算得到的 newtop 高，调整栈顶到新位置
    top = newtop;
   }
   break;

   // 创建新表
   case CREATEARRAY:
    // 如果栈顶是 nil 对象，默认哈希表容量是 101
    if (tag(top-1) == T_NIL) 
     nvalue(top-1) = 101;
    else 
    {
     if (tonumber(top-1)) return 1; // 本身是否为 number 类型，如果不是尝试是否能转换成 number
     // 如果原本容量小于 0，默认容量是 101
     if (nvalue(top-1) <= 0) nvalue(top-1) = 101;
    }
    // 弹出栈顶对象，根据容量创建相应哈希表，将表压入栈顶
    avalue(top-1) = lua_createarray(nvalue(top-1));
    if (avalue(top-1) == NULL)
     return 1;
    tag(top-1) = T_ARRAY; // 改变标签
   break;

   // 相等比较 (类型(tag)与值(value)都需要相等)
   case EQOP:
   {
    Object *l = top-2; // 记录符号左边参数
    Object *r = top-1; // 记录符号右边参数
    --top; // 弹出右边参数

    // 如果类型不同直接返回 nil 对象
    if (tag(l) != tag(r)) 
     tag(top-1) = T_NIL; // 弹出左边参数 并设置为 nil 对象
    else
    {
     // 如果值(value)相等则返回 number 类型对象，不相等返回 nil 类型对象
     switch (tag(l))
     {
      case T_NIL:       tag(top-1) = T_NUMBER; break;
      case T_NUMBER:    tag(top-1) = (nvalue(l) == nvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_ARRAY:     tag(top-1) = (avalue(l) == avalue(r)) ? T_NUMBER : T_NIL; break;
      case T_FUNCTION:  tag(top-1) = (bvalue(l) == bvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_CFUNCTION: tag(top-1) = (fvalue(l) == fvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_USERDATA:  tag(top-1) = (uvalue(l) == uvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_STRING:    tag(top-1) = (strcmp (svalue(l), svalue(r)) == 0) ? T_NUMBER : T_NIL; break;
      case T_MARK:      return 1; // 如果是 T_MARK 的特殊标记说明出现错误
     }
    }
    // 将返回的对象的值(value)设置成 1
    nvalue(top-1) = 1;
   }
   break;

   // 小于比较
   case LTOP:
   {
    // 小于成立则返回 number 类型对象，不成立返回 nil 类型对象
    Object *l = top-2;
    Object *r = top-1;
    --top;

    // number 类型比较
    if (tag(l) == T_NUMBER && tag(r) == T_NUMBER)
     tag(top-1) = (nvalue(l) < nvalue(r)) ? T_NUMBER : T_NIL;
    else
    {
     // 本身是否为 string 类型，如果不是尝试是否能转换成 string
     if (tostring(l) || tostring(r))
      return 1;
     tag(top-1) = (strcmp (svalue(l), svalue(r)) < 0) ? T_NUMBER : T_NIL;
    }
    // 将返回的对象的值(value)设置成 1
    nvalue(top-1) = 1; 
   }
   break;

   // 小于等于比较
   case LEOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) == T_NUMBER && tag(r) == T_NUMBER)
     tag(top-1) = (nvalue(l) <= nvalue(r)) ? T_NUMBER : T_NIL;
    else
    {
     if (tostring(l) || tostring(r))
      return 1;
     tag(top-1) = (strcmp (svalue(l), svalue(r)) <= 0) ? T_NUMBER : T_NIL;
    }
    // 将返回的对象的值(value)设置成 1
    nvalue(top-1) = 1; 
   }
   break;

   // 加运算
   case ADDOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    // 本身是否为 number 类型，如果不是尝试是否能转换成 number
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) += nvalue(r); // 将右运算数加到左运算数
    --top; // 弹出右运算数
   }
   break; 

   // 减运算
   case SUBOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    // 本身是否为 number 类型，如果不是尝试是否能转换成 number
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) -= nvalue(r); // 将右运算数减到左运算数
    --top; // 弹出右运算数
   }
   break; 

   // 乘运算
   case MULTOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    // 本身是否为 number 类型，如果不是尝试是否能转换成 number
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) *= nvalue(r); // 将右运算数乘到左运算数
    --top; // 弹出右运算数
   }
   break; 

   // 除运算
   case DIVOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    // 本身是否为 number 类型，如果不是尝试是否能转换成 number
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) /= nvalue(r); // 将右运算数除到左运算数
    --top; // 弹出右运算数
   }
   break; 

   // 字符串连接运算
   case CONCOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    // 本身是否为 string 类型，如果不是尝试是否能转换成 string
    if (tostring(r) || tostring(l))
     return 1;
    // 连接字符串并且加入 string table(lua_string) 中，存储到左字符串的位置
    svalue(l) = lua_createstring (lua_strconc(svalue(l),svalue(r)));
    if (svalue(l) == NULL)
     return 1;
    --top; // 将右字符串弹出
   }
   break; 

   // 一元负号
   case MINUSOP:
    // 栈顶对象本身是否为 number 类型，如果不是尝试是否能转换成 number
    if (tonumber(top-1))
     return 1;
    nvalue(top-1) = - nvalue(top-1); // 设置成负值
   break; 

   // 逻辑非
   case NOTOP:
    // 如果栈顶对象是 nil 类型(假)那么设置成 number 类型(真)，不是 nil 则设置成 nil
    tag(top-1) = tag(top-1) == T_NIL ? T_NUMBER : T_NIL;
   break; 

   // 栈顶为真则跳转
   case ONTJMP:
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 中读取一个字的偏移量
    if (tag(top-1) != T_NIL) pc += code.w; // 如果栈顶为真，pc 加上偏移量
   }
   break;

   // 栈顶为假则跳转
   case ONFJMP:	   
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 中读取一个字的偏移量
    if (tag(top-1) == T_NIL) pc += code.w; // 如果栈顶为假，pc 加上偏移量
   }
   break;

   // 跳转指令
   case JMP:
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 中读取一个字的偏移量
    pc += code.w; // pc 加上偏移量
   }
   break;

   // 向后跳转
   case UPJMP:
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 中读取一个字的偏移量
    pc -= code.w; // pc 减去偏移量
   }
   break;

   // 如果弹出的对象是假，跳转
   case IFFJMP:
   {
    CodeWord code;
    get_word(code,pc);  // 从 pc 中读取一个字的偏移量
    top--; // 弹出栈顶对象
    if (tag(top) == T_NIL) pc += code.w; // 如果刚刚弹出的对象为假，pc 加上偏移量
   }
   break;

   // 如果弹出的对象是假，向后跳转
   case IFFUPJMP:
   {
    CodeWord code;
    get_word(code,pc); // 从 pc 中读取一个字的偏移量
    top--; // 弹出栈顶对象
    if (tag(top) == T_NIL) pc -= code.w; // 如果刚刚弹出的对象为假，pc 减去偏移量
   }
   break;

   // 将栈顶对象弹出栈
   case POP: --top; break;

   // 函数调用
   case CALLFUNC:
   {
    Byte *newpc;
    Object *b = top-1; // 记录栈顶对象

    while (tag(b) != T_MARK) b--; // 向下移动指针直到找到特殊标记 T_MARK

    // 如果特殊标记 T_MARK 下面的对象类型为 T_FUNCTION(lua function)
    if (tag(b-1) == T_FUNCTION)
    {
     lua_debugline = 0; // 重置 debug 行号
     newpc = bvalue(b-1); // 获取函数地址
     bvalue(b-1) = pc;	// 保留当前地址
     nvalue(b) = (base-stack);	// 保存当前栈基指针对于栈底的偏移量到 b(原标记对象)处
     base = b+1; // 进入函数作用域，将栈基指针指向 b+1
     pc = newpc; // 传入函数地址

     // 检查栈是否溢出
     if (MAXSTACK-(base-stack) < STACKGAP)
     {
      lua_error ("stack overflow");
      return 1;
     }
    }
    // 如果特殊标记 T_MARK 下面的对象类型为 T_CFUNCTION(C function)
    else if (tag(b-1) == T_CFUNCTION)
    {
     int nparam; 
     lua_debugline = 0;	// 重置 debug 行号
     nvalue(b) = (base-stack);		// 保存当前栈基指针对于栈底的偏移量到 b(原标记对象)处
     base = b+1; // 进入函数作用域，将栈基指针指向 b+1
     nparam = top-base;	// 参数个数
     (fvalue(b-1))();	// 调用 C 函数

     { 
      int i;
      int nretval = top - base - nparam; // 计算返回多少个参数
      // 退出函数作用域，将栈顶指针和栈基指针置回原位
      top = base - 2;
      base = stack + (int) nvalue(base-1);

      // 通过循环将函数得到的返回结果压入栈
      for (i=0; i<nretval; i++)
      {
       *top = *(top+nparam+2);
       ++top;
      }
     }
    }
    else
    {
     lua_reportbug ("call expression not a function");
     return 1;
    }
   }
   break;

   // 从函数返回
   case RETCODE:
   {
    int i;
    int shift = *pc++; // 参数与局部变量个数的和
    int nretval = top - base - shift; // 计算返回多少个参数
    // 退出函数作用域，将栈顶指针和栈基指针置回原位
    top = base - 2;
    pc = bvalue(base-2); // 获取调用函数前的执行地址
    base = stack + (int) nvalue(base-1);

    // 通过循环将函数得到的返回结果压入栈
    for (i=0; i<nretval; i++)
    {
     *top = *(top+shift+2);
     ++top;
    }
   }
   break;

   // 退出操作码
   case HALT:
    base = oldbase; // 退出当前作用域
   return 0;		/* success */
   
   case SETFUNCTION:
   {
    CodeWord file, func;
    get_word(file,pc); // 获取文件信息
    get_word(func,pc); // 获取函数信息
    if (lua_pushfunction (file.w, func.w))
     return 1;
   }
   break;

   // 设置 debug 行号
   case SETLINE:
   {
    CodeWord code;
    get_word(code,pc);
    lua_debugline = code.w;
   }
   break;

   // 弹出当前函数
   case RESET:
    lua_popfunction ();
   break;
   
   default:
    lua_error ("internal error - opcode didn't match");
   return 1;
  }
 }
}

/**
 * @brief 遍历 lua stack 中所有对象并调用回调函数 fn。
 *
 * 通过栈指针遍历栈内有效数据，从 (top-1) 到栈底；
 * 包含 stack[0] 的特殊标记。
 *
 * @param fn 函数指针，接受一个 Object* 参数，无返回值。
 */
void lua_travstack (void (*fn)(Object *))
{
 Object *o;
 for (o = top-1; o >= stack; o--)
  fn (o);
}

/**
 * @brief 执行 lua 脚本文件。
 *
 * 打开指定的源文件，解析并执行文件全局代码，
 * 执行结束后自动关闭文件。
 *
 * @param filename 文件名
 * @return 执行成功则返回 0；执行失败则返回错误码 1。
 */
int lua_dofile (char *filename)
{
 if (lua_openfile (filename)) return 1;
 if (lua_parse ()) { lua_closefile (); return 1; }
 lua_closefile ();
 return 0;
}

/**
 * @brief 执行 lua 脚本字符串。
 *
 * 将给定的字符串作为 lua 代码，解析并执行全局代码，
 * 执行结束后自动关闭文件。
 *
 * @param string 脚本字符串。
 * @return 执行成功则返回 0；执行失败则返回错误码 1。
 * @note 原版在此处发生错误时未释放内存，现已修复。
 */
int lua_dostring (char *string)
{
 if (lua_openstring (string)) return 1;
 if (lua_parse ())
 {
  lua_closestring(); // 添加：释放指向字符串源的指针
  return 1;
 }
 lua_closestring();
 return 0;
}

/**
 * @brief 调用指定的 lua 函数。
 *
 * 给定需要调用的函数名和参数个数，在全局符号表中寻找函数对象，
 * 调整 lua stack 结构，执行字节码序列调用该函数。
 *
 * @param functionname 函数名。
 * @param nparam 传递给该函数的参数个数。注意，参数需要提前压入 lua stack 中。
 * @return 成功则返回 0；失败则返回 1。
 *
 * @note lua stack 调整后结构从下到上依次是：
 *       函数对象，标记信息（用于分离函数和参数），函数参数。
 */
int lua_call (char *functionname, int nparam)
{
 static Byte startcode[] = {CALLFUNC, HALT};
 int i; 
 Object func = s_object(lua_findsymbol(functionname));
 if (tag(&func) != T_FUNCTION) return 1;
 // 将 lua stack 中对应参数向上移动两个对象位置
 for (i=1; i<=nparam; i++)
  *(top-i+2) = *(top-i);
 // 将指针移动到新的栈顶
 top += 2;
 // 在参数下面添加标记信息 T_MARK
 tag(top-nparam-1) = T_MARK;
 // 在标记信息下面添加函数对象
 *(top-nparam-2) = func;
 return (lua_execute (startcode));
}

/**
 * @brief 获取 lua stack 中的参数。
 *
 * 根据给定参数的索引 @p number，返回在 lua stack 中对应的对象指针。
 * 索引范围必须是 [1, top-base]，否则返回 Null。
 *
 * @param number 需要在 lua stack 中获取的参数索引。
 * @return 返回给定索引 @p number 在 lua stack 中对应的对象指针；
 *         索引超范围则返回 Null。
 * @note 这里的 @p number 代表 lua stack 中的第几个对象；
 *       例如 传入的 number 是 1，表示第一个栈对象，
 *       数据存储在 base+number-1 指向的位置。
 */
Object *lua_getparam (int number)
{
 if (number <= 0 || number > top-base) return NULL;
 return (base+number-1);
}

/**
 * @brief 获取 lua 对象的数值。
 *
 * @param object 指向要获取数值的 lua 对象的指针。
 * @return 对象是 number 类型或者可以转为 number 的类型，
 *         则返回 real(float) 类型的对应数值；
 *         对象指针是 Null 或者对象是 nil 类型的，则返回 0.0。
 * @note 如果该函数执行成功，那么原对象 (非 number 类型) 会被修改为 number 类型，
 *       这是因为 macro tonumber() 展开后调用了 lua_tonumber() 函数。
 */
real lua_getnumber (Object *object)
{
 // 对象指针为 Null 或者对象类型为 nil，返回0.0
 if (object == NULL || tag(object) == T_NIL) return 0.0;
 // 对象为非 number 类型 && 对象不能转换为 number 类型，返回0.0
 if (tonumber (object)) return 0.0;
 else                   return (nvalue(object));
}

/**
 * @brief 获取 lua 对象的字符串值。
 *
 * @param object 指向要获取字符串值的 lua 对象的指针。
 * @return 对象是 string 类型或者可以转为 string 的类型，
 *         则返回对应的字符串；
 *         对象指针是 Null 或者对象是 nil 类型的，则返回 Null。
 * @note 如果该函数执行成功，那么原对象 (非 string 类型) 会被修改为 string 类型，
 *       这是因为 macro tostring() 展开后调用了 lua_tostring() 函数。
 */
char *lua_getstring (Object *object)
{
 if (object == NULL || tag(object) == T_NIL) return NULL;
 if (tostring (object)) return NULL;
 else                   return (svalue(object));
}

/**
 * @brief 获取 lua 对象的字符串表示并返回其副本。
 *
 * @param object 指向要获取字符串表示并创建副本的 lua 对象的指针。
 * @return 对象是 string 类型或者可以转为 string 的类型，
 *         则返回对应的字符串深拷贝后得到的副本的指针；
 *         对象指针是 Null 或者对象是 nil 类型的，则返回 Null。
 * @note 如果该函数执行成功，那么原对象 (非 string 类型) 会被修改为 string 类型，
 *       这是因为 macro tostring() 展开后调用了 lua_tostring() 函数。
 */
char *lua_copystring (Object *object)
{
 if (object == NULL || tag(object) == T_NIL) return NULL;
 if (tostring (object)) return NULL;
 else                   return (strdup(svalue(object)));
}

/**
 * @brief 获取 Lua 对象的 C 函数指针。
 *
 * @param object 指向要获取 C 函数指针的 Lua 对象的指针。
 * @return 对象为 cfunction 类型，则返回对象的 C 函数指针；
 *         如果对象为 NULL 或类型不是 cfunction 类型，返回 NULL。
 * @note 该函数不会修改对象，仅进行类型检查和取值。
 */
lua_CFunction lua_getcfunction (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_CFUNCTION) return NULL;
 else                            return (fvalue(object));
}

/**
 * @brief 获取 Lua 对象的用户数据指针。
 *
 * @param object 指向要获取用户数据指针的 Lua 对象的指针。
 * @return 对象为 userdata 类型，则返回对象的用户数据指针；
 *         如果对象为 NULL 或类型不是 userdata 类型，返回 NULL。
 * @note 该函数不会修改对象，仅进行类型检查和取值。
 */
void *lua_getuserdata (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_USERDATA) return NULL;
 else                           return (uvalue(object));
}

/**
 * @brief 从 lua 表对象中获取指向指定字段的值的指针。
 *
 * 给定一个 lua 表对象 @p object (类型为 array ) 和字段名称，
 * 在表对象中进行查找或者创建这个对应的节点，返回指向节点值的指针。
 *
 * @param object lua 表对象，类型为 array。
 * @param field 字段名称。
 * @return 成功则返回指向表中指定字段的节点的值的指针；
 *         失败则返回 Null。
 */
Object *lua_getfield (Object *object, char *field)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_ARRAY)
  return NULL;
 else
 {
  Object ref;
  tag(&ref) = T_STRING;
  svalue(&ref) = lua_createstring(lua_strdup(field));
  return (lua_hashdefine(avalue(object), &ref));
 }
}

/**
 * @brief 从 lua 表对象中获取指向指定索引的值的指针。
 *
 * 给定一个 lua 表对象 @p object (类型为 array ) 和索引，
 * 在表对象中进行查找或者创建这个对应的节点，返回指向节点值的指针。
 *
 * @param object lua 表对象，类型为 array。
 * @param index 索引。
 * @return 成功则返回指向表中指定索引的节点的值的指针；
 *         失败则返回 Null。
 */
Object *lua_getindexed (Object *object, float index)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_ARRAY)
  return NULL;
 else
 {
  Object ref;
  tag(&ref) = T_NUMBER;
  nvalue(&ref) = index;
  return (lua_hashdefine(avalue(object), &ref));
 }
}

/**
 * @brief 从全局变量中搜索。
 *
 * 根据给定的全局变量名 @p name，在全局符号表中查找对应的索引 n；
 * 返回索引 n 对应的表的对象。
 *
 * @param name 全局变量名。
 * @return 返回指向名称 @p name 对应的全局符号表中对象的指针。
 */
Object *lua_getglobal (char *name)
{
 int n = lua_findsymbol(name);
 if (n < 0) return NULL;
 return &s_object(n);
}

/**
 * @brief lua stack 出栈函数
 *
 * 将栈顶指针(top)向下移动，
 * 将指向的对象的弹出 lua stack。
 *
 * @return 返回栈顶对象的指针
 */
Object *lua_pop (void)
{
 if (top <= base) return NULL;
 top--;
 return top;
}

/**
 * @brief 将 nil 类型对象压入 lua stack。
 *
 * @return 压栈成功则返回 0；压栈失败(lua stack 溢出)则返回 1。
 */
int lua_pushnil (void)
{
 // lua stack 溢出
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 // tag(top) = T_NIL; // 这里可能是一个bug,没有进行top++
 tag(top++) = T_NIL;
 return 0;
}

/**
 * @brief 将 number 类型对象压入 lua stack。
 *
 * @return 压栈成功则返回 0；压栈失败(lua stack 溢出)则返回 1。
 */
int lua_pushnumber (real n)
{
 // lua stack 溢出
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_NUMBER;
 nvalue(top++) = n; // 赋值并将栈顶指针(top)向上移动
 return 0;
}

/**
 * @brief 将 string 类型对象压入 lua stack。
 *
 * @return 压栈成功则返回 0；压栈失败(lua stack 溢出)则返回 1。
 */
int lua_pushstring (char *s)
{
 // lua stack 溢出
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_STRING; 
 svalue(top++) = lua_createstring(lua_strdup(s)); // 赋值并将栈顶指针(top)向上移动
 return 0;
}

/**
 * @brief 将 cfunction 类型对象压入 lua stack。
 *
 * @return 压栈成功则返回 0；压栈失败(lua stack 溢出)则返回 1。
 */
int lua_pushcfunction (lua_CFunction fn)
{
 // lua stack 溢出
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_CFUNCTION;
 fvalue(top++) = fn; // 赋值并将栈顶指针(top)向上移动
 return 0;
}

/**
 * @brief 将 userdata 类型对象压入 lua stack。
 *
 * @return 压栈成功则返回 0；压栈失败(lua stack 溢出)则返回 1。
 */
int lua_pushuserdata (void *u)
{
 // lua stack 溢出
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_USERDATA;
 uvalue(top++) = u; // 赋值并将栈顶指针(top)向上移动
 return 0;
}

/**
 * @brief 将 lua 对象压入 lua stack。
 *
 * @return 压栈成功则返回 0；压栈失败(lua stack 溢出)则返回 1。
 */
int lua_pushobject (Object *o)
{
 // lua stack 溢出
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 *top++ = *o; // 赋值并将栈顶指针(top)向上移动
 return 0;
}

/**
 * @brief 将栈顶对象储存到全局变量中。
 *
 * 根据给定的全局变量名 @p name，在全局符号表中查找对应的索引 n；
 * 如果栈顶对象不是特殊标记 (T_MARK) 将该元素弹出，
 * 并加入索引为 n 的全局符号表。
 *
 * @param name 全局变量名
 * @return 储存成功返回 0；储存失败返回 1。
 */
int lua_storeglobal (char *name)
{
 int n = lua_findsymbol (name);
 if (n < 0) return 1;
 if (tag(top-1) == T_MARK) return 1;
 s_object(n) = *(--top);
 return 0;
}

/**
 * @brief 将栈顶对象存储到表对象(array)的指定字段。
 *
 * 字段名 @p field 会被转换为字符串对象作为键(ref)，
 * 调用 hash_define() 函数在数组的哈希表中查找或创建对应的节点；
 * 如果栈顶对象不是特殊标记 (T_MARK) 将该元素弹出，
 * 并储存在对应节点的值(val)中。
 *
 * @param object 指向表对象(array)的指针。
 * @param field 给定的字段名。
 * @return 成功则返回 0；失败则返回 1。
 */
int lua_storefield (lua_Object object, char *field)
{
 // 对象必须是数组类型的
 if (tag(object) != T_ARRAY)
  return 1;
 else
 {
  Object ref, *h;
  tag(&ref) = T_STRING;
  svalue(&ref) = lua_createstring(lua_strdup(field));
  h = lua_hashdefine(avalue(object), &ref);
  if (h == NULL) return 1;
  if (tag(top-1) == T_MARK) return 1;
  *h = *(--top);
 }
 return 0;
}


/**
 * @brief 将栈顶对象存储到表对象(array)的指定索引。
 *
 * 索引 @p index 会被转换为 T_NUMBER 类型对象作为键(ref)，
 * 调用 hash_define() 函数在数组的哈希表中查找或创建对应的节点；
 * 如果栈顶对象不是特殊标记 (T_MARK) 将该元素弹出，
 * 并储存在对应节点的值(val)中。
 *
 * @param object 指向表对象(array)的指针。
 * @param index 给定的索引。
 * @return 成功则返回 0；失败则返回 1。
 */
int lua_storeindexed (lua_Object object, float index)
{
 // 对象必须是数组类型的
 if (tag(object) != T_ARRAY)
  return 1;
 else
 {
  Object ref, *h;
  tag(&ref) = T_NUMBER;
  nvalue(&ref) = index;
  h = lua_hashdefine(avalue(object), &ref);
  if (h == NULL) return 1;
  if (tag(top-1) == T_MARK) return 1;
  *h = *(--top);
 }
 return 0;
}


/**
 * @brief lua 对象是 nil 类型
 * @param object lua 对象
 * @return 返回 1 代表对象类型是 nil；
 *         返回 0 代表对象类型不是 nil 或者对象指针为 Null
 */
int lua_isnil (Object *object)
{
 return (object != NULL && tag(object) == T_NIL);
}

/**
 * @brief lua 对象是 number 类型
 * @param object lua 对象
 * @return 返回 1 代表对象类型是 number；
 *         返回 0 代表对象类型不是 number 或者对象指针为 Null
 */
int lua_isnumber (Object *object)
{
 return (object != NULL && tag(object) == T_NUMBER);
}

/**
 * @brief lua 对象是 string 类型
 * @param object lua 对象
 * @return 返回 1 代表对象类型是 string；
 *         返回 0 代表对象类型不是 string 或者对象指针为 Null
 */
int lua_isstring (Object *object)
{
 return (object != NULL && tag(object) == T_STRING);
}

/**
 * @brief lua 对象是 表(array) 类型
 * @param object lua 对象
 * @return 返回 1 代表对象类型是 表(array)；
 *         返回 0 代表对象类型不是 表(array) 或者对象指针为 Null
 */
int lua_istable (Object *object)
{
 return (object != NULL && tag(object) == T_ARRAY);
}

/**
 * @brief lua 对象是 cfunction 类型
 * @param object lua 对象
 * @return 返回 1 代表对象类型是 cfunction；
 *         返回 0 代表对象类型不是 cfunction 或者对象指针为 Null
 */
int lua_iscfunction (Object *object)
{
 return (object != NULL && tag(object) == T_CFUNCTION);
}

/**
 * @brief lua 对象是 userdata 类型
 * @param object lua 对象
 * @return 返回 1 代表对象类型是 userdata；
 *         返回 0 代表对象类型不是 userdata 或者对象指针为 Null
 */
int lua_isuserdata (Object *object)
{
 return (object != NULL && tag(object) == T_USERDATA);
}

/** @brief 将 lua 对象的类型名称压栈。
 *
 * 该函数从 lua stack 获取一个 lua 对象，
 * 确定其类型，并将对应的类型名称字符串压入 lua stack。
 */
void lua_type (void)
{
 Object *o = lua_getparam(1);
 lua_pushstring (lua_constant[tag(o)]);
}

/** @brief 将 lua 对象转换成 number 类型并压栈。
 *
 * 该函数从 lua stack 获取一个 lua 对象，
 * 将其副本类型转换为 number，并压入 lua stack。
 */
void lua_obj2number (void)
{
 Object *o = lua_getparam(1);
 lua_pushobject (lua_convtonumber(o));
}

/**
 * @brief lua 内置的 print 函数实现。
 *
 * 从 lua stack 中依次取出参数，并且根据参数类型进行相应的输出。
 *
 * @note 在 table.c 文件中注册为 lua 内置函数，添加到全局符号表。
 *       如果是表对象，只打印地址而不是展开的内容。
 * @note 原版在此处发生 obj 指针类型不明确，现已修复。
 */
void lua_print (void)
{
 int i=1;
 // void *obj; // lua_getparam 返回的是 lua_Object，指针类型不明确。
 lua_Object obj = NULL;
 while ((obj=lua_getparam (i++)) != NULL)
 {
  if      (lua_isnumber(obj))    printf("%g\n",lua_getnumber (obj));
  else if (lua_isstring(obj))    printf("%s\n",lua_getstring (obj));
  else if (lua_iscfunction(obj)) printf("cfunction: %p\n",lua_getcfunction (obj));
  else if (lua_isuserdata(obj))  printf("userdata: %p\n",lua_getuserdata (obj));
  else if (lua_istable(obj))     printf("table: %p\n",obj);
  else if (lua_isnil(obj))       printf("nil\n");
  else			         printf("invalid value to print\n");
 }
}

/**
 * @brief lua 内置的 dofile 函数实现。
 *
 * 从 lua stack 中取出参数(文件名)。
 * 向 lua stack 压入数字 1 表示成功；压入 nil 表示失败。
 *
 * @note 在 table.c 文件中注册为 lua 内置函数，添加到全局符号表。
 */
void lua_internaldofile (void)
{
 lua_Object obj = lua_getparam (1); // lua 脚本中传入的文件名
 // 确定传入的是字符串并且能够通过 lua_dofile 执行并解析文件内容。
 if (lua_isstring(obj) && !lua_dofile(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}

/**
 * @brief lua 内置的 dostring 函数实现。
 *
 * 从 lua stack 中取出参数(字符串内容为可执行的 Lua 代码)。
 * 向 lua stack 压入数字 1 表示成功；压入 nil 表示失败。
 *
 * @note 在 table.c 文件中注册为 lua 内置函数，添加到全局符号表。
 */
void lua_internaldostring (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dostring(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}


