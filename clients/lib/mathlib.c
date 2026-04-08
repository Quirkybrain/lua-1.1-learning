/**
 * @file mathlib.c
 * @brief 数学库，提供数学函数。
 */

char *rcs_mathlib="$Id: mathlib.c,v 1.1 1993/12/17 18:41:19 celes Exp $";

#include <stdio.h>		/* NULL */
#include <math.h>

#include "lua.h"

#define TODEGREE(a) ((a)*180.0/3.14159)
#define TORAD(a)    ((a)*3.14159/180.0)

/**
 * @brief 计算绝对值。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将绝对值压回 lua stack。
 *
 * @interface value = abs (number)
 */
static void math_abs (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `abs'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `abs'"); return; }
 // 读取参数值并按需转换为正数
 d = lua_getnumber(o);
 // 负数需要取相反数
 if (d < 0) d = -d;
 // 将结果压回 lua stack
 lua_pushnumber (d);
}


/**
 * @brief 计算角度值的正弦。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时先转为弧度再计算正弦值。
 *
 * @interface value = sin (degree)
 */
static void math_sin (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `sin'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `sin'"); return; }
 // 读取角度值并转换为弧度计算
 d = lua_getnumber(o);
 lua_pushnumber (sin(TORAD(d)));
}



/**
 * @brief 计算角度值的余弦。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时先转为弧度再计算余弦值。
 *
 * @interface value = cos (degree)
 */
static void math_cos (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `cos'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `cos'"); return; }
 // 读取角度值并转换为弧度计算
 d = lua_getnumber(o);
 lua_pushnumber (cos(TORAD(d)));
}



/**
 * @brief 计算角度值的正切。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时先转为弧度再计算正切值。
 *
 * @interface value = tan (degree)
 */
static void math_tan (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `tan'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `tan'"); return; }
 // 读取角度值并转换为弧度计算
 d = lua_getnumber(o);
 lua_pushnumber (tan(TORAD(d)));
}


/**
 * @brief 计算反正弦并返回角度值。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将反正弦结果从弧度转换为角度后压栈。
 *
 * @interface degree = asin (number)
 */
static void math_asin (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `asin'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `asin'"); return; }
 // 读取参数并将结果转换为角度制
 d = lua_getnumber(o);
 lua_pushnumber (TODEGREE(asin(d)));
}


/**
 * @brief 计算反余弦并返回角度值。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将反余弦结果从弧度转换为角度后压栈。
 *
 * @interface degree = acos (number)
 */
static void math_acos (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `acos'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `acos'"); return; }
 // 读取参数并将结果转换为角度制
 d = lua_getnumber(o);
 lua_pushnumber (TODEGREE(acos(d)));
}



/**
 * @brief 计算反正切并返回角度值。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将反正切结果从弧度转换为角度后压栈。
 *
 * @interface degree = atan (number)
 */
static void math_atan (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `atan'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `atan'"); return; }
 // 读取参数并将结果转换为角度制
 d = lua_getnumber(o);
 lua_pushnumber (TODEGREE(atan(d)));
}


/**
 * @brief 向上取整。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将 ceil 结果压回 lua stack。
 *
 * @interface value = ceil (number)
 */
static void math_ceil (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `ceil'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `ceil'"); return; }
 // 读取参数并执行向上取整
 d = lua_getnumber(o);
 lua_pushnumber (ceil(d));
}


/**
 * @brief 向下取整。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将 floor 结果压回 lua stack。
 *
 * @interface value = floor (number)
 */
static void math_floor (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `floor'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `floor'"); return; }
 // 读取参数并执行向下取整
 d = lua_getnumber(o);
 lua_pushnumber (floor(d));
}

/**
 * @brief 计算两个整数的取模结果。
 *
 * 从 lua stack 读取两个参数；
 * 任一参数不是 number 时抛出 lua error；
 * 成功时将两个参数转换为 int 再求余。
 *
 * @interface value = mod (number1, number2)
 */
static void math_mod (void)
{
 int d1, d2;
 // 读取前两个参数
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 // 任一参数不是 number 时报告类型错误
 if (!lua_isnumber(o1) || !lua_isnumber(o2))
 { lua_error ("incorrect arguments to function `mod'"); return; }
 // 转换为整数后执行取模
 d1 = (int) lua_getnumber(o1);
 d2 = (int) lua_getnumber(o2);
 lua_pushnumber (d1%d2);
}


/**
 * @brief 计算平方根。
 *
 * 从 lua stack 读取一个 number 参数；
 * 参数不足或类型错误时抛出 lua error；
 * 成功时将平方根结果压回 lua stack。
 *
 * @interface value = sqrt (number)
 */
static void math_sqrt (void)
{
 double d;
 // 读取第一个参数
 lua_Object o = lua_getparam (1);
 // 参数缺失时报告参数不足
 if (o == NULL)
 { lua_error ("too few arguments to function `sqrt'"); return; }
 // 参数不是 number 时报告类型错误
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `sqrt'"); return; }
 // 读取参数并计算平方根
 d = lua_getnumber(o);
 lua_pushnumber (sqrt(d));
}

/**
 * @brief 计算幂运算。
 *
 * 从 lua stack 读取底数和指数两个参数；
 * 任一参数不是 number 时抛出 lua error；
 * 成功时调用 pow 计算结果。
 *
 * @interface value = pow (base, exponent)
 */
static void math_pow (void)
{
 double d1, d2;
 // 读取底数和指数
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 // 任一参数不是 number 时报告类型错误
 if (!lua_isnumber(o1) || !lua_isnumber(o2))
 { lua_error ("incorrect arguments to function `pow'"); return; }
 // 读取数值并进行幂运算
 d1 = lua_getnumber(o1);
 d2 = lua_getnumber(o2);
 lua_pushnumber (pow(d1,d2));
}

/**
 * @brief 返回参数列表中的最小值。
 *
 * 从 lua stack 逐个读取 number 参数；
 * 至少需要一个参数，任一参数类型错误时抛出 lua error；
 * 成功时将最小值压回 lua stack。
 *
 * @interface value = min (number1 [,number2 ...])
 */
static void math_min (void)
{
 int i=1;
 double d, dmin;
 lua_Object o;
 // 先读取第一个参数作为初始最小值
 if ((o = lua_getparam(i++)) == NULL)
 { lua_error ("too few arguments to function `min'"); return; }
 // 第一个参数必须是 number
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `min'"); return; }
 dmin = lua_getnumber (o);
 // 逐个比较剩余参数并刷新最小值
 while ((o = lua_getparam(i++)) != NULL)
 {
  // 只接受 number 参数
  if (!lua_isnumber(o))
  { lua_error ("incorrect arguments to function `min'"); return; }
  d = lua_getnumber (o);
  // 当前值更小时更新最小值
  if (d < dmin) dmin = d;
 }
 // 返回最终最小值
 lua_pushnumber (dmin);
}


/**
 * @brief 返回参数列表中的最大值。
 *
 * 从 lua stack 逐个读取 number 参数；
 * 至少需要一个参数，任一参数类型错误时抛出 lua error；
 * 成功时将最大值压回 lua stack。
 *
 * @interface value = max (number1 [,number2 ...])
 */
static void math_max (void)
{
 int i=1;
 double d, dmax;
 lua_Object o;
 // 先读取第一个参数作为初始最大值
 if ((o = lua_getparam(i++)) == NULL)
 { lua_error ("too few arguments to function `max'"); return; }
 // 第一个参数必须是 number
 if (!lua_isnumber(o))
 { lua_error ("incorrect arguments to function `max'"); return; }
 dmax = lua_getnumber (o);
 // 逐个比较剩余参数并刷新最大值
 while ((o = lua_getparam(i++)) != NULL)
 {
  // 只接受 number 参数
  if (!lua_isnumber(o))
  { lua_error ("incorrect arguments to function `max'"); return; }
  d = lua_getnumber (o);
  // 当前值更大时更新最大值
  if (d > dmax) dmax = d;
 }
 // 返回最终最大值
 lua_pushnumber (dmax);
}



/*
** Open math library
*/
/**
 * @brief 注册数学库函数。
 *
 * 将数学相关的 C 函数注册到 lua 全局环境中。
 */
void mathlib_open (void)
{
 // 注册绝对值函数
 lua_register ("abs",   math_abs);
 // 注册三角函数与反三角函数
 lua_register ("sin",   math_sin);
 lua_register ("cos",   math_cos);
 lua_register ("tan",   math_tan);
 lua_register ("asin",  math_asin);
 lua_register ("acos",  math_acos);
 lua_register ("atan",  math_atan);
 // 注册常用数值运算函数
 lua_register ("ceil",  math_ceil);
 lua_register ("floor", math_floor);
 lua_register ("mod",   math_mod);
 lua_register ("sqrt",  math_sqrt);
 lua_register ("pow",   math_pow);
 lua_register ("min",   math_min);
 lua_register ("max",   math_max);
}
