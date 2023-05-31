/* We use 17.14 format
 * So p = 17, q = 14
 * And f = 1 << q
 *
 * x, y are floating point numbers
 * n is integer
 *
 * In stdio.c it is mentioned that..
 * We don't support floating-point arithmetic,
               and %n can be part of a security hole.
 */
#include <stdio.h>
#include <stdint.h>
#include "threads/fixed_point.h"

#define Q 14
#define F (1 << Q)

int integer_to_float(int n)
{
    return n * F;
}

int float_to_integer(int x)
{
    if (x >= 0)
        return (x + (F / 2)) / F;
    else
        return (x - (F / 2)) / F;
}

int add_x_n(int x, int n)
{
    return x + n * F;
}

int sub_n_x(int x, int n)
{
    return x - n * F;
}

int mul_x_y(int x, int y)
{
    return ((int64_t)x) * y / F;
}

int div_x_y(int x, int y)
{
    return ((int64_t)x) * F / y;
}
