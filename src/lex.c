/**
 * @file lex.c
 * @brief 词法分析器，识别关键字、标识符、数字、字符串等 token，供语法分析器使用。
 */

char *rcs_lex = "$Id: lex.c,v 2.1 1994/04/15 19:00:28 celes Exp $";
/*$Log: lex.c,v $
 * Revision 2.1  1994/04/15  19:00:28  celes
 * Retirar chamada da funcao lua_findsymbol associada a cada
 * token NAME. A decisao de chamar lua_findsymbol ou lua_findconstant
 * fica a cargo do modulo "lua.stx".
 *
 * Revision 1.3  1993/12/28  16:42:29  roberto
 * "include"s de string.h e stdlib.h para evitar warnings
 *
 * Revision 1.2  1993/12/22  21:39:15  celes
 * Tratamento do token $debug e $nodebug
 *
 * Revision 1.1  1993/12/22  21:15:16  roberto
 * Initial revision
 **/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "y.tab.h"

#define next() { current = input(); } // 获取下一个字符
#define save(x) { *yytextLast++ = (x); } // 将当前字符保存到 yytext 缓冲区
#define save_and_next()  { save(current); next(); } // 保存当前字符并读取下一个字符

static int current; // 当前正在处理的字符
static char yytext[256]; // 构建 token 字符串的缓冲区
static char *yytextLast; // 指向 yytext 下一个空闲位置的指针

static Input input; // 函数指针，用于获取输入

/**
 * @brief 设置当前输入源并重置当前字符。
 *
 * 将 current 初始化为空格 ' '，并注册输入函数；
 * 提供两种输入方式，从文件读取和从字符串读取。
 *
 * @param fn 输入函数指针。
 */
void lua_setinput (Input fn)
{
  current = ' ';
  input = fn;
}

/**
 * @brief 返回已经处理识别的 token 字符串。
 * @return 返回将结尾标记为空字符的 token 字符串。
 */
char *lua_lasttext (void)
{
  *yytextLast = 0; // 将 token 字符串结尾标记为空字符
  return yytext;
}

/**
 * 保留字表，记录保留字和对应的 token 值；
 * 根据字母顺序进行设计，便于二分查找。
 */
static struct 
  {
    char *name;
    int token;
  } reserved [] = {
      {"and", AND},
      {"do", DO},
      {"else", ELSE},
      {"elseif", ELSEIF},
      {"end", END},
      {"function", FUNCTION},
      {"if", IF},
      {"local", LOCAL},
      {"nil", NIL},
      {"not", NOT},
      {"or", OR},
      {"repeat", REPEAT},
      {"return", RETURN},
      {"then", THEN},
      {"until", UNTIL},
      {"while", WHILE} };

#define RESERVEDSIZE (sizeof(reserved)/sizeof(reserved[0]))

/**
 * @brief 通过二分查找在保留字表中搜索 @p name。
 * @param name 需要查找的字符串
 * @return 返回该保留字对应的 token 值；未找到则返回 0。
 */
int findReserved (char *name)
{
  int l = 0;
  int h = RESERVEDSIZE - 1;
  while (l <= h)
  {
    int m = (l+h)/2;
    int comp = strcmp(name, reserved[m].name);
    if (comp < 0)
      h = m-1;
    else if (comp == 0)
      return reserved[m].token;
    else
      l = m+1;
  }
  return 0;
}

/**
 * @brief 词法分析函数。
 * @return 返回 token 值。
 */
int yylex ()
{
  while (1)
  {
    yytextLast = yytext;
    switch (current)
    {
      // （行号自增）移动至下一行
      case '\n': lua_linenumber++;

      // 获取下一个字符
      case ' ':
      case '\t':
        next();
        continue;

      // 读取 debug 模式的开与关
      case '$':
	      next();
        // 读取并保存所有数字、字母、下划线字符
	      while (isalnum(current) || current == '_')
          save_and_next();
        *yytextLast = 0; // 将 token 字符串结尾标记为空字符

        // 通过设置 yylval.vInt 来对 DEBUG token 带上开关信息。
	      if (strcmp(yytext, "debug") == 0)
	      {
	        yylval.vInt = 1; // 开启 DEBUG
	        return DEBUG;
	      }
	      else if (strcmp(yytext, "nodebug") == 0)
	      {
	        yylval.vInt = 0; // 关闭 DEBUG
	        return DEBUG;
	      }
	      return WRONGTOKEN;

      // lua 语言注释
      case '-':
        save_and_next();
        // 判断是否为两个连续的 '-' 字符
        if (current != '-') return '-';
        do { next(); } while (current != '\n' && current != 0);
        continue;

      // lua 语言注释和除法运算
      case '/':
        save_and_next();
        // 判断是否为两个连续的 '/' 字符
        if (current != '/') return '/';
        do { next(); } while (current != '\n' && current != 0);
        continue;


      // <= 小于等于
      case '<':
        save_and_next();
        if (current != '=') return '<';
        else { save_and_next(); return LE; }
      // >= 大于等于
      case '>':
        save_and_next();
        if (current != '=') return '>';
        else { save_and_next(); return GE; }
      // ~= 不等于
      case '~':
        save_and_next();
        if (current != '=') return '~';
        else { save_and_next(); return NE; }

      // 读取定界符 "" or '' 包围的的内容并返回 STRING token
      case '"':
      case '\'':
      {
        int del = current; // 保存当前定界符
        next();
        // 循环直到下一个相同定界符
        while (current != del) 
        {
          switch (current)
          {
            // 错误 token
            case 0: // 未到达定界符却到达字符串结尾空字符
            case '\n': // 字符串内出现换行符
              return WRONGTOKEN;
            // 转义字符
            case '\\':
              next(); // 读取下一个字符
              switch (current)
              {
                /**
                 * 转义字符 '\' 下一个是 n，
                 * 说明输入的是 '\' 和 'n' 这两个字符，
                 * 组合成 '\n' 换行符并保存下来。
                 * 其他特殊字符同上。
                 */
                case 'n': save('\n'); next(); break;
                case 't': save('\t'); next(); break;
                case 'r': save('\r'); next(); break;
                // 未知转义内容，保存转义字符'\'，下一个字符当作正常字符处理
                default : save('\\'); break;
              }
              break;
            // 普通字符，保存当前字符并读取下一个字符
            default: 
              save_and_next();
          }
        }
        next(); // 跳过定界符
        *yytextLast = 0;
        // 字符串存入常量表，返回索引存入 yylval.vWord
        yylval.vWord = lua_findconstant (yytext);
        return STRING;
      }

      // 区分普通标识符号和关键字
      case 'a': case 'b': case 'c': case 'd': case 'e':
      case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o':
      case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y':
      case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E':
      case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O':
      case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y':
      case 'Z':
      case '_':
      {
        int res;

        // 构建 C合法字符串
        do { save_and_next(); } while (isalnum(current) || current == '_');
        *yytextLast = 0;

        // 检查是否为保留字
        res = findReserved(yytext);
        if (res) return res;
        // 不是保留字就将指向该字符串的指针赋给 yylval.pChar
        yylval.pChar = yytext;
        return NAME;
      }

      // 判断 '.' 是作为小数点还是 '..' 连接符使用
      case '.':
        save_and_next();
        // 如果 '.' 后面是'.'，说明输入 '..' （在 lua 中作为连接符号）
        if (current == '.') 
        { 
          save_and_next(); 
          return CONC;
        }
        // 如果 '.' 后面不是数字，返回 '.'
        else if (!isdigit(current)) return '.';
        /**
         * 如果 '.' 后面是数字，跳转到构建小数部分。
         * lua 认为 .123 是合法浮点数，会被读取为 0.123。
         */
        goto fraction;

      // 构建数字
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':

        // 读取整数部分
        do { save_and_next(); } while (isdigit(current));
        // 读取小数点
        if (current == '.') save_and_next();

fraction:
        // 读取小数部分
        while (isdigit(current)) save_and_next();

        // 科学计数
        if (current == 'e' || current == 'E')
        {
          save_and_next(); // 保存 'e' 或 'E'
          if (current == '+' || current == '-') save_and_next(); // 正负号（可选）
          if (!isdigit(current)) return WRONGTOKEN; // 指数位必须有一个数字（不允许出现 1.1e）
          do { save_and_next(); } while (isdigit(current)); // 读取指数位数字
        }
        *yytextLast = 0;
        yylval.vFloat = atof(yytext); // 将字符串转换为浮点数
        return NUMBER;

      // 读到常见的运算符、分隔符以及文件结束符时，返回刚刚读取的字符
      default:
      {
        save_and_next();
        return *yytext;      
      }
    }
  }
}
        
