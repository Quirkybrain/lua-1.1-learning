/**
 * @file strlib.c
 * @brief 字符串库，字符串操作。
 */

char *rcs_strlib="$Id: strlib.c,v 1.2 1994/03/28 15:14:02 celes Exp $";

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mm.h"


#include "lua.h"

/*
** Return the position of the first caracter of a substring into a string
** LUA interface:
**			n = strfind (string, substring)
*/
/**
 * @brief 查找子串在字符串中的首次出现位置。
 *
 * 从 lua stack 读取原字符串和子串；
 * 参数类型错误时抛出 lua error；
 * 找到时返回从 1 开始的位置，找不到时返回 nil。
 */
static void str_find (void)
{
 char *s1, *s2, *f;
 // 读取原字符串和待查找的子串
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 // 两个参数都必须是 string
 if (!lua_isstring(o1) || !lua_isstring(o2))
 { lua_error ("incorrect arguments to function `strfind'"); return; }
 s1 = lua_getstring(o1);
 s2 = lua_getstring(o2);
 f = strstr(s1,s2);
 // 找到子串时返回 1 基下标
 if (f != NULL)
  lua_pushnumber (f-s1+1);
 // 未找到时返回 nil
 else
  lua_pushnil();
}

/*
** Return the string length
** LUA interface:
**			n = strlen (string)
*/
/**
 * @brief 返回字符串长度。
 *
 * 从 lua stack 读取一个字符串参数；
 * 参数类型错误时抛出 lua error；
 * 成功时将字符串长度压回 lua stack。
 */
static void str_len (void)
{
 // 读取要计算长度的字符串参数
 lua_Object o = lua_getparam (1);
 // 参数必须是 string
 if (!lua_isstring(o))
 { lua_error ("incorrect arguments to function `strlen'"); return; }
 // 返回字符串长度
 lua_pushnumber(strlen(lua_getstring(o)));
}


/*
** Return the substring of a string, from start to end
** LUA interface:
**			substring = strsub (string, start, end)
*/
/**
 * @brief 截取字符串子串。
 *
 * 从 lua stack 读取原字符串、起始位置和可选结束位置；
 * 参数类型错误时抛出 lua error；
 * 范围非法时返回空字符串，否则返回指定区间的子串。
 */
static void str_sub (void)
{
 int start, end;
 char *s;
 // 读取字符串、起始位置和可选结束位置
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 lua_Object o3 = lua_getparam (3);
 // 原字符串和起始位置的类型必须正确
 if (!lua_isstring(o1) || !lua_isnumber(o2))
 { lua_error ("incorrect arguments to function `strsub'"); return; }
 // 给出结束位置时也必须是 number
 if (o3 != NULL && !lua_isnumber(o3))
 { lua_error ("incorrect third argument to function `strsub'"); return; }
 s = lua_copystring(o1);
 start = lua_getnumber (o2);
 end = o3 == NULL ? strlen(s) : lua_getnumber (o3);
 // 截取范围非法时返回空字符串
 if (end < start || start < 1 || end > strlen(s))
  lua_pushstring("");
 // 截取范围合法时返回指定区间内容
 else
 {
  s[end] = 0;
  lua_pushstring (&s[start-1]);
 }
 free (s);
}

/*
** Convert a string to lower case.
** LUA interface:
**			lowercase = strlower (string)
*/
/**
 * @brief 将字符串转换为小写。
 *
 * 从 lua stack 读取一个字符串参数；
 * 参数类型错误时抛出 lua error；
 * 成功时返回一份转换为小写的新字符串。
 */
static void str_lower (void)
{
 char *s, *c;
 // 读取待转换的字符串
 lua_Object o = lua_getparam (1);
 // 参数必须是 string
 if (!lua_isstring(o))
 { lua_error ("incorrect arguments to function `strlower'"); return; }
 c = s = strdup(lua_getstring(o));
 // 逐字符转换为小写
 while (*c != 0)
 {
  *c = tolower(*c);
  c++;
 }
 // 返回转换后的字符串并释放临时内存
 lua_pushstring(s);
 free(s);
} 


/*
** Convert a string to upper case.
** LUA interface:
**			uppercase = strupper (string)
*/
/**
 * @brief 将字符串转换为大写。
 *
 * 从 lua stack 读取一个字符串参数；
 * 参数类型错误时抛出 lua error；
 * 成功时返回一份转换为大写的新字符串。
 */
static void str_upper (void)
{
 char *s, *c;
 // 读取待转换的字符串
 lua_Object o = lua_getparam (1);
 // 参数必须是 string
 if (!lua_isstring(o))
 { lua_error ("incorrect arguments to function `strlower'"); return; }
 c = s = strdup(lua_getstring(o));
 // 逐字符转换为大写
 while (*c != 0)
 {
  *c = toupper(*c);
  c++;
 }
 // 返回转换后的字符串并释放临时内存
 lua_pushstring(s);
 free(s);
} 


/*
** Open string library
*/
/**
 * @brief 注册字符串库函数。
 *
 * 将字符串处理相关的 C 函数注册到 lua 全局环境中。
 */
void strlib_open (void)
{
 // 注册查找与长度函数
 lua_register ("strfind", str_find);
 lua_register ("strlen", str_len);
 // 注册截取与大小写转换函数
 lua_register ("strsub", str_sub);
 lua_register ("strlower", str_lower);
 lua_register ("strupper", str_upper);
}
