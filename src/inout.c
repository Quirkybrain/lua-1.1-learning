/**
 * @file inout.c
 * @brief 输入/输出辅助，处理文件/字符串的打开关闭、错误报告、调试信息记录。
 */

char *rcs_inout="$Id: inout.c,v 1.2 1993/12/22 21:15:16 roberto Exp $";

#include <stdio.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"

/* Exported variables */
int lua_linenumber; // 正在处理的输入行号
int lua_debug; // 标记是否进入调试模式
int lua_debugline; // 调试行号

/* Internal variables */
#ifndef MAXFUNCSTACK
#define MAXFUNCSTACK 32
#endif
static struct { int file; int function; } funcstack[MAXFUNCSTACK];
static int nfuncstack=0; // function stack 元素计数器

static FILE *fp;
static char *st;
static void (*usererror) (char *s);

/**
 * @brief 注册用户自定义错误处理函数。
 *
 * 允许用户提供错误处理函数替代 lua 内部的错误处理。
 *
 * @param fn 指向用户提供的错误处理函数的指针；
 *        如果传入 Null，仍旧使用内部错误处理方法。
 *
 * @note 需要用户确保传入的函数指针指向的函数有效。
 *       该方法使得 lua 更加具有灵活性。
 * @see lua_error
 */
void lua_errorfunction (void (*fn) (char *s))
{
 usererror = fn;
}

/**
 * @brief 输入源为文件时读取字符。
 * @return 读取到的字符；
 *         如果是文件结尾的 EOF 标识则返回空字符。
 * @note fp 在lua_openfile中检查了是否为 Null。
 */
static int fileinput (void)
{
 int c = fgetc (fp);
 return (c == EOF ? 0 : c);
}

/**
 * @brief 输入源为字符串时读取字符。
 *
 * @details 先递增指向下一个位置再返回原位置；
 *          `st` 先递增到 `\0` 之后的位置，然后返回原位置的 `\0`。
 * @return 读取到的字符。
 * @warning 这个函数会导致字符串数组的访问越界行为，
 *          需要调用者确保内存安全。
 * @note 是否需要注意 st 为 Null 呢？
 *       如果追求安全，可以先检查当前指针是否指向空字符。
 */
static int stringinput (void)
{
 if(*st == 0) return 0; // 修改了访问越界的 bug，先检查当前指针是否指向空字符。
 st++;
 return (*(st-1));
}

/**
 * @brief 打开一个文件作为输入源。
 *
 * 重置行号为 1 并将 fileinput 注册为输入函数；
 * 尝试以只读方式打开输入文件；
 * 将文件加入内部文件表。
 *
 * @param fn 要打开的文件的路径。
 * @return 成功则返回 0；
 *         失败(文件目录不存在或内存空间不足)则返回 1。
 * @note 使用完 lua_openfile 后必须使用 lua_closefile 进行资源释放。
 */
int lua_openfile (char *fn)
{
 lua_linenumber = 1;
 lua_setinput (fileinput);
 fp = fopen (fn, "r");
 if (fp == NULL) return 1;
 if (lua_addfile (fn)) return 1; // 将文件压入 file stack
 return 0;
}

/**
 * @brief 关闭当前已打开的文件输入源。
 *
 * 该函数用于清理通过 lua_openfile 打开的文件资源。
 */
void lua_closefile (void)
{
 if (fp != NULL)
 {
  lua_delfile(); // 将文件弹出 file stack
  fclose (fp);
  fp = NULL;
 }
}

/**
 * @brief 打开一个字符串作为输入源。
 *
 * 重置行号为 1 并将 stringinput 注册为输入函数；
 * 构造一个描述性字符串 sn 并将 sn 加入内部文件表。
 *
 * @param s 指向字符串源的指针，要求该字符串符合 lua 的语法。
 * @return 成功则返回 0；
 *         失败则返回 1。
 * @note 使用完 lua_openstring 后必须使用 lua_closestring 进行资源释放。
 */
int lua_openstring (char *s)
{
 lua_linenumber = 1;
 lua_setinput (stringinput);
 st = s;
 if (st == NULL) return 1; // 为提高代码强度，加入对于 st 指针是否为 Null 的检查。
 {
  char sn[64];
  sprintf (sn, "String: %10.10s...", s);
  if (lua_addfile (sn)) return 1; // 将字符串压入 file stack
 }
 return 0;
}

/**
 * @brief 关闭当前已打开的字符串输入源。
 *
 * 该函数用于清理通过 lua_openstring 打开的字符串资源。
 * @warning 原来代码中仅有 lua_delfile 的调用，
 *          会导致 st 指针悬空，造成误用。
 */
void lua_closestring (void)
{
 if (st != NULL) // 为提高代码安全性，将 st 指针置为 Null。
 {
  lua_delfile(); // 将字符串弹出 file stack
  st = NULL;
 }
}

/**
 * @brief 抛出错误的函数。
 *
 * 检查是否有用户自定义的错误处理函数，如果有则使用用户提供的函数。
 * 如果没有提供，那么使用 lua 内部错误处理函数。
 *
 * @param s 错误信息字符串。
 */
void lua_error (char *s)
{
 if (usererror != NULL) usererror (s);
 else			    fprintf (stderr, "lua: %s\n", s);
}

/**
 * @brief 将函数信息压入 function stack。
 *
 * 在执行 SETFUNCTION 操作码的时候，
 * 将函数对应的文件的文件索引和函数名索引压入 function stack，
 * 用于记录当前执行的函数信息。
 *
 * @param file 函数所在文件在 lua_file (file stack)的索引。
 * @param function 函数名在 lua_table 的索引。
 * @return 成功则返回 0；失败则返回 1。
 */
int lua_pushfunction (int file, int function)
{
 // function stack 溢出
 if (nfuncstack >= MAXFUNCSTACK-1)
 {
  lua_error ("function stack overflow");
  return 1;
 }
 // 压入 function stack
 funcstack[nfuncstack].file = file;
 funcstack[nfuncstack].function = function;
 nfuncstack++; // 计数器自增
 return 0;
}

/**
 * @brief 将函数信息弹出 function stack。
 *
 * 在执行 RESET 操作码的时候，
 * 将函数对应的文件的文件索引和函数名索引弹出 function stack。
 * @note 补充了下溢报错。
 */
void lua_popfunction (void)
{
 if (nfuncstack <= 0) {
  lua_error("function stack underflow");
  nfuncstack = 0;
  return;
 }
 nfuncstack--; // 计数器自减
}

/**
 * @brief 构造详细的错误消息并抛出 lua_error。
 *
 * @param s 基础错误信息字符串。
 */
void lua_reportbug (char *s)
{
 char msg[1024];
 strcpy (msg, s);

 // 如果有调试行号，则附加错误发生的位置
 if (lua_debugline != 0)
 {
  int i;
  // 如果 function stack 非空，说明调用了函数。
  if (nfuncstack > 0)
  {
   sprintf (strchr(msg,0), 
         "\n\tin statement beginning at line %d in function \"%s\" of file \"%s\"",
         lua_debugline, s_name(funcstack[nfuncstack-1].function),
  	 lua_file[funcstack[nfuncstack-1].file]);
   sprintf (strchr(msg,0), "\n\tactive stack\n");
   for (i=nfuncstack-1; i>=0; i--)
    sprintf (strchr(msg,0), "\t-> function \"%s\" of file \"%s\"\n", 
                            s_name(funcstack[i].function),
			    lua_file[funcstack[i].file]);
  }
  else // 不在函数内
  {
   sprintf (strchr(msg,0),
         "\n\tin statement beginning at line %d of file \"%s\"",
         lua_debugline, lua_filename());
  }
 }
 lua_error (msg);
}

