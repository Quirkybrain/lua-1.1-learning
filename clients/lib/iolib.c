/**
 * @file iolib.c
 * @brief 输入/输出库，用于文件操作。
 */
char *rcs_iolib="$Id: iolib.c,v 1.4 1994/04/25 20:11:23 celes Exp $";

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef __GNUC__
// 现代系统已经不需要这个库
// #include <floatingpoint.h>
#endif

#include "mm.h"

#include "lua.h"

// 现代gcc不允许将宏作为static变量初始化值
// 查看iolib.c文件发现在iolib_open函数是调用其他函数的入口
// 采取先创建指针in, out再在iolib_open中进行赋值
// static FILE *in=stdin, *out=stdout;
static FILE *in, *out;


/**
 * @brief 打开文件并且读取。
 * 
 * 读取文件成功则向 lua stack 中压入 1；
 * 读取文件失败则向 lua stack 中压入 0。
 * 
 * @interface status = readfrom (filename)
 */
static void io_readfrom (void)
{
 // 从 lua stack 中读取一个参数
 lua_Object o = lua_getparam (1);
 // 如果参数指针为空
 if (o == NULL)
 {
  /**
   * 如果 in 指针不是 stdin;
   * 关闭打开的文件；
   * 将 in 指针重置为 stdin；
   * 并压入 1 表示成功。
   */
  if (in != stdin)
  {
   fclose (in);
   in = stdin;
  }
  lua_pushnumber (1);
 }
 else
 {
  /**
   * 如果参数不是 string 类型；
   * 抛出 lua error；
   * 向 lua stack 压入 0 表示失败。
   */
  if (!lua_isstring (o))
  {
   lua_error ("incorrect argument to function 'readfrom`");
   lua_pushnumber (0);
  }
  else
  {
   // 参数是 string 类型，尝试以只读方式打开文件
   FILE *fp = fopen (lua_getstring(o),"r");
   // 如果文件指针为空，向 lua stack 压入 0 表示失败
   if (fp == NULL)
   {
    lua_pushnumber (0);
   }
   else
   {
    /**
     * 如果 in 指针不是 stdin （已经打开了一个文件）；
     * 先关闭已经打开的文件；
     * 将 in 指针设置为当前文件；
     * 向 lua stack 压入 1 表示成功。
     */ 
    if (in != stdin) fclose (in);
    in = fp;
    lua_pushnumber (1);
   }
  }
 }
}


/**
 * @brief 打开文件并写入。
 * 
 * 写入文件成功则向 lua stack 中压入 1；
 * 写入文件失败则向 lua stack 中压入 0。
 * 
 * @interface status = writeto (filename)
 */
static void io_writeto (void)
{
 // 从 lua stack 中读取一个参数
 lua_Object o = lua_getparam (1);
 // 如果参数指针为空
 if (o == NULL)
 {
  /**
   * 如果 out 指针不是 stdout;
   * 关闭打开的文件；
   * 将 out 指针重置为 stdout；
   * 并压入 1 表示成功。
   */
  if (out != stdout)
  {
   fclose (out);
   out = stdout;
  }
  lua_pushnumber (1);
 }
 else
 {
  /**
   * 如果参数不是 string 类型；
   * 抛出 lua error；
   * 向 lua stack 压入 0 表示失败。
   */
  if (!lua_isstring (o))
  {
   lua_error ("incorrect argument to function 'writeto`");
   lua_pushnumber (0);
  }
  else
  {
   // 参数是 string 类型，尝试以写入方式打开文件
   FILE *fp = fopen (lua_getstring(o),"w");
   // 如果文件指针为空，向 lua stack 压入 0 表示失败
   if (fp == NULL)
   {
    lua_pushnumber (0);
   }
   else
   {
    /**
     * 如果 out 指针不是 stdout （已经打开了一个文件）；
     * 先关闭已经打开的文件；
     * 将 out 指针设置为当前文件；
     * 向 lua stack 压入 1 表示成功。
     */ 
    if (out != stdout) fclose (out);
    out = fp;
    lua_pushnumber (1);
   }
  }
 }
}


/**
 * @brief 打开文件并追加。
 * 
 * 写入文件成功（已经存在的文件）则向 lua stack 中压入 2；
 * 写入文件成功（新创建的文件）则向 lua stack 中压入 1；
 * 写入文件失败则向 lua stack 中压入 0。
 * 
 * @interface status = appendto (filename)
 */
static void io_appendto (void)
{
 // 从 lua stack 中读取一个参数
 lua_Object o = lua_getparam (1);
 // 如果参数指针为空
 if (o == NULL)
 {
  /**
   * 如果 out 指针不是 stdout;
   * 关闭打开的文件；
   * 将 out 指针重置为 stdout；
   * 并压入 1 表示成功。
   */
  if (out != stdout)
  {
   fclose (out);
   out = stdout;
  }
  lua_pushnumber (1);
 }
 else
 {
  /**
   * 如果参数不是 string 类型；
   * 抛出 lua error；
   * 向 lua stack 压入 0 表示失败。
   */
  if (!lua_isstring (o))
  {
   lua_error ("incorrect argument to function 'appendto`");
   lua_pushnumber (0);
  }
  else
  {
   int r;
   FILE *fp;
   struct stat st;
   // 文件不存在则设置为 1
   if (stat(lua_getstring(o), &st) == -1) r = 1;
   // 文件已经存在设置为 2
   else                                   r = 2;
   // 参数是 string 类型，尝试以追加方式打开文件
   fp = fopen (lua_getstring(o),"a");
   // 如果文件指针为空，向 lua stack 压入 0 表示失败
   if (fp == NULL)
   {
    lua_pushnumber (0);
   }
   else
   {
    /**
     * 如果 out 指针不是 stdout （已经打开了一个文件）；
     * 先关闭已经打开的文件；
     * 将 out 指针设置为当前文件；
     * 向 lua stack 压入对应成功状态表示成功。
     */ 
    if (out != stdout) fclose (out);
    out = fp;
    lua_pushnumber (r);
   }
  }
 }
}

/**
 * @brief 从当前输入流读取一个值。
 *
 * 未提供格式字符串时按自由格式读取；
 * 提供格式字符串时按指定类型和宽度读取；
 * 读取失败时向 lua stack 压入 nil。
 *
 * @interface variable = read ([format])
 */
static void io_read (void)
{
 lua_Object o = lua_getparam (1);
 // 未提供格式字符串或格式参数不是 string 时，按自由格式读取
 if (o == NULL || !lua_isstring(o))	/* free format */
 {
  int c;
  char s[256];
  // 跳过输入流中的前导空白字符
  while (isspace(c=fgetc(in)))
   ;
  // 双引号开头时读取一个双引号字符串
  if (c == '\"')
  {
   int c, n=0;
   while((c = fgetc(in)) != '\"')
   {
    // 提前遇到 EOF 说明字符串未闭合，返回 nil
    if (c == EOF)
    {
     lua_pushnil ();
     return;
    }
    // 将读取到的字符依次写入缓冲区
    s[n++] = c;
   }
   // 补上字符串结束符
   s[n] = 0;
  }
  // 单引号开头时读取一个单引号字符串
  else if (c == '\'')
  {
   int c, n=0;
   while((c = fgetc(in)) != '\'')
   {
    // 提前遇到 EOF 说明字符串未闭合，返回 nil
    if (c == EOF)
    {
     lua_pushnil ();
     return;
    }
    // 将读取到的字符依次写入缓冲区
    s[n++] = c;
   }
   // 补上字符串结束符
   s[n] = 0;
  }
  // 非引号输入时先读取一个词，再判断它是否是数字
  else
  {
   char *ptr;
   double d;
   // 将刚读到的首字符放回输入流，交给 fscanf 完整读取
   ungetc (c, in);
   // 无法读取到一个词时返回 nil
   if (fscanf (in, "%s", s) != 1)
   {
    lua_pushnil ();
    return;
   }
   // 尝试把读取结果解释成数字
   d = strtod (s, &ptr);
   // 完整转换成功时按 number 返回
   if (!(*ptr))
   {
    lua_pushnumber (d);
    return;
   }
  }
  // 其余自由格式输入按字符串返回
  lua_pushstring (s);
  return;
 }
 // 提供了格式字符串时，按格式化方式读取
 else				/* formatted */
 {
  char *e = lua_getstring(o);
  char t;
  int  m=0;
  // 跳过格式串前导空白并读取类型说明符
  while (isspace(*e)) e++;
  t = *e++;
  // 读取格式串中可选的宽度限制
  while (isdigit(*e))
   m = m*10 + (*e++ - '0');
  
  // 指定了读取宽度时，先按文本块读取再转换类型
  if (m > 0)
  {
   char f[80];
   char s[256];
   sprintf (f, "%%%ds", m);
   // 固定宽度读取失败时返回 nil
   if (fgets (s, m, in) == NULL)
   {
    lua_pushnil();
    return;
   }
   // 固定宽度读取成功后清理行尾换行
   else
   {
    // fgets 读到换行时去掉换行符
    if (s[strlen(s)-1] == '\n')
     s[strlen(s)-1] = 0;
   }
   // 根据格式说明符决定最终返回的 lua 类型
   switch (tolower(t))
   {
    case 'i':
    {
     long int l;
     // 按整数解析固定宽度读取结果
     sscanf (s, "%ld", &l);
     lua_pushnumber(l);
    }
    break;
    case 'f': case 'g': case 'e':
    {
     float f;
     // 按浮点数解析固定宽度读取结果
     sscanf (s, "%f", &f);
     lua_pushnumber(f);
    }
    break;
    default: 
     // 其他格式直接保留为字符串
     lua_pushstring(s); 
    break;
   }
  }
  // 未指定宽度时，直接按类型从输入流读取
  else
  {
   switch (tolower(t))
   {
    case 'i':
    {
     long int l;
     // 整数读取失败时返回 nil，成功时返回 number
     if (fscanf (in, "%ld", &l) == EOF)
       lua_pushnil();
       else lua_pushnumber(l);
    }
    break;
    case 'f': case 'g': case 'e':
    {
     float f;
     // 浮点数读取失败时返回 nil，成功时返回 number
     if (fscanf (in, "%f", &f) == EOF)
       lua_pushnil();
       else lua_pushnumber(f);
    }
    break;
    default: 
    {
     char s[256];
     // 默认按字符串读取，失败返回 nil，成功返回字符串
     if (fscanf (in, "%s", s) == EOF)
       lua_pushnil();
       else lua_pushstring(s);
    }
    break;
   }
  }
 }
}

/**
 * @brief 根据格式字符串构造输出文本。
 *
 * 解析类型、对齐方式、宽度和精度；
 * 将给定 lua 对象格式化到静态缓冲区中；
 * 返回可直接写入输出流的字符串指针。
 *
 * @param e 格式字符串。
 * @param o 要格式化的 lua 对象。
 * @return 指向格式化结果的字符串指针；
 *         不支持的格式则返回空字符串。
 */
static char *buildformat (char *e, lua_Object o)
{
 static char buffer[512];
 static char f[80];
 char *string = &buffer[255];
 char t, j='r';
 int  m=0, n=0, l;
 // 跳过格式串前导空白并读取类型说明符
 while (isspace(*e)) e++;
 t = *e++;
 // 读取可选的对齐方式，默认保持右对齐
 if (*e == '<' || *e == '|' || *e == '>') j = *e++;
 // 读取最小宽度
 while (isdigit(*e))
  m = m*10 + (*e++ - '0');
 e++;	/* skip point */
 // 读取精度或最小位数
 while (isdigit(*e))
  n = n*10 + (*e++ - '0');

 // 从 '%' 开始拼接 sprintf 使用的格式串
 sprintf(f,"%%");
 // 左对齐和居中都先通过负宽度实现
 if (j == '<' || j == '|') sprintf(strchr(f,0),"-");
 // 指定宽度时写入宽度
 if (m != 0)   sprintf(strchr(f,0),"%d", m);
 // 指定精度时写入精度
 if (n != 0)   sprintf(strchr(f,0),".%d", n);
 // 补上最终的类型说明符
 sprintf(strchr(f,0), "%c", t);
 // 根据目标类型把 lua 对象格式化为字符串
 switch (tolower(t))
 {
  case 'i': t = 'i';
   // 按整数格式输出
   sprintf (string, f, (long int)lua_getnumber(o));
  break;
  case 'f': case 'g': case 'e': t = 'f';
   // 按浮点数格式输出
   sprintf (string, f, (float)lua_getnumber(o));
  break;
  case 's': t = 's';
   // 按字符串格式输出
   sprintf (string, f, lua_getstring(o));
  break;
  default:
   // 不支持的类型说明符直接返回空串
   return "";
 }
 l = strlen(string);
 // 超过最大宽度时，用 '*' 覆盖整个输出区域
 if (m!=0 && l>m)
 {
  int i;
  // 将超长内容替换为等宽星号提示
  for (i=0; i<m; i++)
   string[i] = '*';
  string[i] = 0;
 }
 // 需要居中显示时，重新调整起始指针并补齐空格
 else if (m!=0 && j=='|')
 {
  int i=l-1;
  // 跳过末尾空格，估算有效字符末端位置
  while (isspace(string[i])) i--;
  // 向左调整起始位置，使文本尽量居中
  string -= (m-i) / 2;
  i=0;
  // 将前导空洞补成空格
  while (string[i]==0) string[i++] = ' ';
  // 重新写入字符串结束符
  string[l] = 0;
 }
 return string;
}

/**
 * @brief 向当前输出流写入一个值。
 *
 * 不带参数时输出换行；
 * 不带格式串时按自由格式输出；
 * 带格式串时按格式化文本输出；
 * 返回写入状态或写入字符数。
 *
 * @interface status = write (variable [,format])
 */
static void io_write (void)
{
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 // 未提供参数时仅输出换行，并返回成功状态
 if (o1 == NULL)			/* new line */
 {
  fprintf (out, "\n");
  lua_pushnumber(1);
 }
 // 未提供格式串时按自由格式输出
 else if (o2 == NULL)   		/* free format */
 {
  int status=0;
  // number 参数按数值格式输出
  if (lua_isnumber(o1))
   status = fprintf (out, "%g", lua_getnumber(o1));
  // string 参数按字符串格式输出
  else if (lua_isstring(o1))
   status = fprintf (out, "%s", lua_getstring(o1));
  // 返回 fprintf 的执行结果
  lua_pushnumber(status);
 }
 // 提供格式串时构造格式化文本再输出
 else					/* formated */
 {
  // 第二个参数必须是 string 类型的格式串
  if (!lua_isstring(o2))
  { 
   lua_error ("incorrect format to function `write'"); 
   lua_pushnumber(0);
   return;
  }
  // 将格式化后的文本写入当前输出流
  lua_pushnumber(fprintf (out, "%s", buildformat(lua_getstring(o2),o1)));
 }
}

/**
 * @brief 执行外部程序。
 * 
 * 使用标准 C 库函数 system() 执行外部程序；
 * 并将返回值压入 lua stack。
 */
void io_execute (void)
{
 lua_Object o = lua_getparam (1);
 /**
  * 如果参数指针为空或者参数不是 string 类型；
  * 抛出 lua error；
  * 向 lua stack 压入 0 表示失败。
  */
 if (o == NULL || !lua_isstring (o))
 {
  lua_error ("incorrect argument to function 'execute`");
  lua_pushnumber (0);
 }
 else
 {
  // 执行外部程序
  int res = system(lua_getstring(o));
  lua_pushnumber (res);
 }
 return;
}

/**
 * @brief 移除文件。
 * 
 * 如果成功移除，则向 lua stack 中压入 1；
 * 失败则压入 0。
 */
void io_remove  (void)
{
 lua_Object o = lua_getparam (1);
 /**
  * 如果参数指针为空或者参数不是 string 类型；
  * 抛出 lua error；
  * 向 lua stack 压入 0 表示失败。
  */
 if (o == NULL || !lua_isstring (o))
 {
  lua_error ("incorrect argument to function 'execute`");
  lua_pushnumber (0);
 }
 else
 {
  // 删除成功时返回 1
  if (remove(lua_getstring(o)) == 0)
   lua_pushnumber (1);
  // 删除失败时返回 0
  else
   lua_pushnumber (0);
 }
 return;
}

/**
 * @brief 注册输入输出库函数。
 *
 * 初始化默认输入输出流；
 * 并将文件读写、格式化输入输出等函数注册到 lua 全局环境中。
 */
void iolib_open (void)
{
 // 默认输入流为标准输入
 in = stdin;
 // 默认输出流为标准输出
 out = stdout;
 // 注册输入输出库公开接口
 lua_register ("readfrom", io_readfrom);
 lua_register ("writeto",  io_writeto);
 lua_register ("appendto", io_appendto);
 lua_register ("read",     io_read);
 lua_register ("write",    io_write);
 lua_register ("execute",  io_execute);
 lua_register ("remove",   io_remove);
}
