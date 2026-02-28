/**
 * @file table.c
 * @brief 管理符号表（全局变量）、常量表、字符串表，实现字符串驻留和垃圾收集。
 * 本文件负责管理 Lua 运行时的关键数据结构：
 * 1. 符号表 (Symbol table) 用于存储全局变量及其对应的值。
 * 2. 常量表 (Constant table) 存储代码中写死的常量字符串。
 * 3. 字符串表 (String table) 用于实现字符串的驻留（同一字符串只保存一份拷贝）。
 * 同时包含了基于标记-清除 (Mark & Sweep) 算法的垃圾收集器，用于回收不再使用的字符串和数组。
 */
char *rcs_table="$Id: table.c,v 2.1 1994/04/20 22:07:57 celes Exp $";

#include <stdlib.h>
#include <string.h>

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

/**
 * @brief 比较两个字符串是否相等
 *
 * 先比较字符串是否拥有相同的第一个字符 s[0]，再使用 strcmp 进行具体对比。
 * 调用 strcmp 会产生性能开销，这样可以避免多次调用 strcmp ，可以提升性能。
 */
#define streq(s1,s2)	(s1[0]==s2[0]&&strcmp(s1+1,s2+1)==0)

#ifndef MAXSYMBOL
#define MAXSYMBOL	512
#endif
/**
 * @brief 全局符号表缓冲区
 *
 * 初始化时预先加入了一些 lua 内置函数。
 * type 将 lua 对象的类型名称压栈。
 * tonumber 将 lua 对象转换成 number 类型并压栈。
 * next 搜索表中下一个键值对
 * nextvar 返回下一个全局变量
 * print 打印输出
 * dofile
 * dostring
 */
static Symbol  		tablebuffer[MAXSYMBOL] = {
                                    {"type",{T_CFUNCTION,{lua_type}}},
                                    {"tonumber",{T_CFUNCTION,{lua_obj2number}}},
                                    {"next",{T_CFUNCTION,{lua_next}}},
                                    {"nextvar",{T_CFUNCTION,{lua_nextvar}}},
                                    {"print",{T_CFUNCTION,{lua_print}}},
                                    {"dofile",{T_CFUNCTION,{lua_internaldofile}}},
                                    {"dostring",{T_CFUNCTION,{lua_internaldostring}}}
                                                 };
Symbol	       	       *lua_table=tablebuffer;
Word   	 		lua_ntable=7; // 记录全局符号表已有元素个数

// 搜索链表
struct List
{
 Symbol *s;
 struct List *next;
};

// 初始化预设定的符号的搜索链表
static struct List o6={ tablebuffer+6, 0};
static struct List o5={ tablebuffer+5, &o6 };
static struct List o4={ tablebuffer+4, &o5 };
static struct List o3={ tablebuffer+3, &o4 };
static struct List o2={ tablebuffer+2, &o3 };
static struct List o1={ tablebuffer+1, &o2 };
static struct List o0={ tablebuffer+0, &o1 };
static struct List *searchlist=&o0;

#ifndef MAXCONSTANT
#define MAXCONSTANT	256
#endif

// 预定义常量的字符串。在最前面预留了一个空格，用于垃圾收集(GC)时的标记(mark)字节
static char tm[] = " mark";
static char ti[] = " nil";
static char tn[] = " number";
static char ts[] = " string";
static char tt[] = " table";
static char tf[] = " function";
static char tc[] = " cfunction";
static char tu[] = " userdata";
// 常量表缓冲区，存储运行时的字符串常量，预先加入了类型名常量
static char  	       *constantbuffer[MAXCONSTANT] = {tm+1, ti+1,
						       tn+1, ts+1,
						       tt+1, tf+1,
						       tc+1, tu+1
                                                      };
char  	      	      **lua_constant = constantbuffer;
Word    		lua_nconstant=T_USERDATA+1; // 字符串常量计数器

#ifndef MAXSTRING
#define MAXSTRING	512
#endif
// 字符串缓冲区，用于所有动态产生的字符串
static char 	       *stringbuffer[MAXSTRING];
char  		      **lua_string = stringbuffer;
Word    		lua_nstring=0; // 字符串计数器

#define MAXFILE 	20
// 正在解析的文件名栈，用于报告错误发生时的文件上下文
char  		       *lua_file[MAXFILE];
int      		lua_nfile; // 文件计数器

// 字符串实际地址的前一个字节用于标记该字符串是否被使用
#define markstring(s)   (*((s)-1))


// 用于控制垃圾回收触发机制的变量
Word lua_block=10; // 检查什么时候触发垃圾回收
Word lua_nentity; // 新实例的计数器（字符串和数组）


/**
 * @brief 通过变量名在全局符号表中查找并返回索引。
 *
 * 如果找不到，则在表尾新分配一个表并返回新索引，会检查是否溢出。
 *
 * @param s 变量名。
 * @return 符号所在的表索引；出错返回 -1。
 * @note 该函数使用链表进行线性查找，找到后会将节点移到链表头部以加速后续访问。
 * @note 原版本在对进行 p = malloc(sizeof(*p)); 未检查指针是否为空。
 */
int lua_findsymbol (char *s)
{
 struct List *l, *p; // l 记录当前的表位置，p 记录上一个表位置
 for (p=NULL, l=searchlist; l!=NULL; p=l, l=l->next)
 {
  // 是否找到节点
  if (streq(s,l->s->name))
  {
   // 不在链表头部，将该节点移到搜索链表最前方
   if (p!=NULL)
   {
    p->next = l->next; // 将 l 从链表中取出
    // 将 l 置于表头
    l->next = searchlist;
    searchlist = l;
   }
   return (l->s-lua_table); // 指针相减，计算出索引
  }
 }
 // 如果全局符号表溢出，抛出 lua_error
 if (lua_ntable >= MAXSYMBOL-1)
 {
  lua_error ("symbol table overflow");
  return -1;
 }
 // 根据变量名在全局符号表结尾创建一个新符号（变量）
 s_name(lua_ntable) = strdup(s);
 // 如果全局符号表溢出，抛出 lua_error
 if (s_name(lua_ntable) == NULL)
 {
  lua_error ("not enough memory");
  return -1;
 }
 // 将创建的表中的对象类型设置为 nil
 s_tag(lua_ntable) = T_NIL;
 // 创建指向表的节点，并将节点移到搜索链表头部以加速后续访问
 p = malloc(sizeof(*p));
 if (p == NULL) // 添加了对于指针是否为空的检查
 {
  lua_error ("not enough memory");
  return -1;
 }
 p->s = lua_table+lua_ntable; // 指向创建的表
 p->next = searchlist; // 加入搜索链表的头部
 searchlist = p;

 return lua_ntable++; // 返回索引，计数器自增
}

/**
 * @brief 通过目标字符串在字符串常量表中查找并返回索引。
 *
 * 如果找不到，则在表尾新分配一个字符串常量并返回新索引，会检查是否溢出。
 *
 * @param s 目标字符串。
 * @return 符号所在的表索引；出错返回 -1。
 */
int lua_findconstant (char *s)
{
 int i;
 // 在常量表搜索
 for (i=0; i<lua_nconstant; i++)
  // 如果在常量表找到对应的常量字符串，返回索引
  if (streq(s,lua_constant[i]))
   return i;
 // 判断是否表溢出
 if (lua_nconstant >= MAXCONSTANT-1)
 {
  lua_error ("lua: constant string table overflow"); 
  return -1;
 }
 {
  // 分配字符串空间，头部预留 1 个 GC 标记字节，尾部预留 1 个 C 字符串结束符 '\0'
  char *c = calloc(strlen(s)+2,sizeof(char));
  c++; // 指向第二个空间，将首位留作标记
  lua_constant[lua_nconstant++] = strcpy(c,s); // 将字符串加入常量表结尾，并将字符串常量计数器自增
 }
 return (lua_nconstant-1); // 返回索引
}

/**
 * @brief 遍历所有的全局符号对象，并执行给定的函数。
 */
void lua_travsymbol (void (*fn)(Object *))
{
 int i;
 for (i=0; i<lua_ntable; i++)
  fn(&s_object(i));
}

/**
 * @brief 标记对象。
 *
 * 字符串对象 将预留标记位设置为 1。
 * 数组对象 调用 lua_hashmark() 进行标记
 * @param o 需要被标记的对象。
 */
void lua_markobject (Object *o)
{
 if (tag(o) == T_STRING)
  markstring (svalue(o)) = 1; // 将首尾预留标记位设置为 1
 else if (tag(o) == T_ARRAY)
   lua_hashmark (avalue(o));
}

/**
 * @brief 触发垃圾回收。
 *
 * 从 lua stack 和全局符号表出发，先对内容进行标记，
 * 再将不再使用的进行回收。
 *
 */
void lua_pack (void)
{
 // 标记 lua stack 中对象(string 和 array 类型的)
 lua_travstack(lua_markobject);

 // 标记全局符号表中对象(string 和 array 类型的)
 lua_travsymbol(lua_markobject);

 // 对 string 和 哈希表(array) 进行回收
 lua_stringcollector();
 lua_hashcollector();

 lua_nentity = 0; // 新实例的计数器清零
}

/**
 * @brief 字符串回收。
 *
 * 将所有没有被标记的字符串进行回收。
 * 在这个步骤会在判断后将标记清除。
 *
 */
void lua_stringcollector (void)
{
 int i, j;

 // 循环遍历字符串表
 for (i=j=0; i<lua_nstring; i++)
  // 如果标记是 1，表示正在被使用
  if (markstring(lua_string[i]) == 1)
  {
   lua_string[j++] = lua_string[i]; // 将正在被使用的字符串向前移动成不间断序列
   markstring(lua_string[i]) = 0; // 清空标记
  }
  else
  {
   // 释放未被使用的字符串
   free (lua_string[i]-1);
  }
 lua_nstring = j; // 更新字符串计数器
}

/**
 * @brief 创建新的字符串。
 *
 * 先在字符串表进行查找，如果已经存在了这个字符串，释放传入的字符串，
 * 如果不存在，先检查是否有溢出的危险或者触发 GC，
 * 如果有则先进行垃圾回收并且检查是否还有溢出风险，
 * 如果有则返回 Null，没有则将传入的字符串加入字符串表的结尾。
 *
 * @param s 已经分配了内存（并带有标记位前缀）的字符串。
 * @return 如果成功则返回唯一字符串指针；如果失败则返回 Null。
 * @note 原版本在进行垃圾回收后如果字符串表仍然即将溢出，没有释放字符串 s 的内存空间；
 *       调用 lua_createstring 的地方传入的 s 都是通过 calloc 分配的堆内存，
 *       加入表中也不手动释放，就会导致内存失去跟踪。
 */
char *lua_createstring (char *s)
{
 int i;
 // 如果字符串为空则返回空
 if (s == NULL) return NULL;

 // 遍历字符串表，查找是否已经存在
 for (i=0; i<lua_nstring; i++)
  // 如果在表中找到这个字符串
  if (streq(s,lua_string[i]))
  {
   // 释放传入的字符串 s
   free(s-1);
   // 将存在的唯一字符串指针返回
   return lua_string[i];
  }

 // 如果实例计数器到达阈值(触发GC)或者字符串表即将溢出
 if (lua_nentity == lua_block || lua_nstring >= MAXSTRING-1)
 {
  lua_pack (); // 触发 GC
  // 如果字符串表仍然即将溢出
  if (lua_nstring >= MAXSTRING-1)
  {
   // 抛出 lua_error
   lua_error ("string table overflow");
   free (s-1); // 原版本在这里会造成内存泄漏
   return NULL;
  }
 }
 // 将传入的字符串加入字符串表的结尾并将字符串计数器自增
 lua_string[lua_nstring++] = s;
 lua_nentity++; // 实例计数器自增
 return s;
}

/**
 * @brief 添加文件名入栈。
 * @return 如果成功则返回 0；失败则返回 1。
 */
int lua_addfile (char *fn)
{
 // 判断文件计数器是否会溢出
 if (lua_nfile >= MAXFILE-1)
 {
  lua_error ("too many files");
  return 1;
 }
 // 将文件名添加到文件名栈
 if ((lua_file[lua_nfile++] = strdup (fn)) == NULL)
 {
  lua_error ("not enough memory");
  return 1;
 }
 return 0;
}

/**
 * @brief 将文件栈的栈顶文件名弹出。
 * @return
 */
int lua_delfile (void)
{
 lua_nfile--; 
 return 1;
}

/**
 * @brief 获取当前的栈顶文件。
 */
char *lua_filename (void)
{
 return lua_file[lua_nfile-1];
}

/**
 * @brief 内置函数，获取下一个全局变量。
 */
void lua_nextvar (void)
{
 int index;
 Object *o = lua_getparam (1);
 // 如果没有得到第一个参数，抛出缺少参数的错误
 if (o == NULL)
 { lua_error ("too few arguments to function `nextvar'"); return; }
 // 如果得到第二个参数，抛出太多参数的错误
 if (lua_getparam (2) != NULL)
 { lua_error ("too many arguments to function `nextvar'"); return; }
 // 如果取的参数为 nil 类型，将索引设置成 0(从第一个全局变量开始)
 if (tag(o) == T_NIL)
 {
  index = 0;
 }
 // 如果取的参数为非 string 类型，抛出类型错误
 else if (tag(o) != T_STRING) 
 { 
  lua_error ("incorrect argument to function `nextvar'"); 
  return;
 }
 else // string 类型的参数
 {
  // 遍历全局符号表，找到全局符号表中名字与参数对象的字符串值相同的索引
  for (index=0; index<lua_ntable; index++)
   if (streq(s_name(index),svalue(o))) break;
  // 如果没有找到，抛出没有找到名字的错误
  if (index == lua_ntable) 
  {
   lua_error ("name not found in function `nextvar'");
   return;
  }
  // 指向全局符号表中下一个数据
  index++;
  // 跳过 nil 类型数据
  while (index < lua_ntable && tag(&s_object(index)) == T_NIL) index++;

  // 如果没有找到下一个非 nil 类型的对象，将两个 nil 类型对象压入 lua stack
  if (index == lua_ntable)
  {
   lua_pushnil();
   lua_pushnil();
   return;
  }
 }
 {
  Object name; // 构建键对象
  tag(&name) = T_STRING; // 将对象类型设置为 string
  // 将对象的值设置成全局符号表里找到的字符串
  svalue(&name) = lua_createstring(lua_strdup(s_name(index)));
  // 将键对象和值对象压入 lua stack
  if (lua_pushobject (&name)) return;
  if (lua_pushobject (&s_object(index))) return;
 }
}
