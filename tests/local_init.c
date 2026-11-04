// Locals with initializers: a scalar is assigned whole (no zeroing first), arrays, structs
// and unions are zeroed and then get the given elements. The stack is dirtied before each check.
#include "test.h"

static void dirty(void)
{
    volatile unsigned char junk[256];
    for (int i = 0; i < 256; i++) junk[i] = 0xA5;
}

typedef struct { char c; long long v; short s; } S;
typedef union { char c; long long v; } U;

static long long scalars(void)
{
    _Bool b = 7;
    char c = -3;
    unsigned short us = 65535;
    int i = -123456;
    long long ll = 0x123456789ALL;
    float f = 1.5f;
    double d = -2.25;
    int *p = &i;
    return b + c + us + i + ll + (long long)(f * 4) + (long long)(d * 4) + (*p == i);
}

static int aggregates(void)
{
    int a[6] = { 1, 2 };
    S s = { 5 };
    U u = { 9 };
    char str[8] = "ab";
    int sum = 0;
    for (int k = 0; k < 6; k++) sum += a[k];
    return sum * 1000 + s.c + (int)s.v + s.s + (int)(u.v & 0xFF) + str[2] + str[7] + (int)sizeof str;
}

int main(void)
{
    dirty();
    ASSERT(1 - 3 + 65535 - 123456 + 0x123456789ALL + 6 - 9 + 1, scalars());
    dirty();
    ASSERT(3 * 1000 + 5 + 9 + 8, aggregates());
    return g_fail;
}
