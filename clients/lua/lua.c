/**
 * @file lua.c
 * @brief 主程序入口，初始化库，读取文件或交互式输入，调用解析器执行代码。
 */

char *rcs_lua="$Id: lua.c,v 1.1 1993/12/17 18:41:19 celes Exp $";

#include <stdio.h>

#include "lua.h"
#include "lualib.h"

// 现代C语言main的返回值使用int而不是void

int main (int argc, char *argv[])
{
 int i;
 // 注册标准库函数
 iolib_open ();
 strlib_open ();
 mathlib_open ();

 // 仅执行lua二进制文件，创建缓冲区，循环读取用户输入
if (argc < 2)
 {
   char buffer[2048];
   while (gets(buffer) != 0)
     lua_dostring(buffer);
 }
 // 带有参数（例如: ./lua test.lua），执行目标lua源文件
 else
   for (i=1; i<argc; i++)
    lua_dofile (argv[i]);

 return 0;
}
