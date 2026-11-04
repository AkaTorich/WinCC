// Structs through "...": sizes 1, 2, 4, 8 travel in the slot, any other size
// by address (Microsoft x64); va_arg takes each the same way
#include "test.h"
#include <stdarg.h>

typedef struct { int a, b, c; } Three;              // 12 bytes: by address
typedef struct { double x, y; } Pair;               // 16 bytes: by address
typedef struct { char s[3]; } Odd;                  // 3 bytes: by address
typedef struct { long long a, b, c; } Big;          // 24 bytes: by address
typedef struct { int lo, hi; } Small;               // 8 bytes: in the slot
typedef struct { short v; } Tiny;                   // 2 bytes: in the slot

static long long sum_threes(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    long long s = 0;
    for (int i = 0; i < n; i++) {
        Three t = va_arg(ap, Three);
        s = s * 1000 + t.a * 100 + t.b * 10 + t.c;
    }
    va_end(ap);
    return s;
}

// every kind in a row, with ints and doubles between them: the wrong ones counted
static int mixed(int first, ...)
{
    va_list ap;
    va_start(ap, first);
    Three t = va_arg(ap, Three);
    int k = va_arg(ap, int);
    Pair p = va_arg(ap, Pair);
    double d = va_arg(ap, double);
    Odd o = va_arg(ap, Odd);
    Small sm = va_arg(ap, Small);
    Big b = va_arg(ap, Big);
    Tiny ti = va_arg(ap, Tiny);
    int last = va_arg(ap, int);
    va_end(ap);
    int bad = first != 9;
    bad += t.a != 1 || t.b != 2 || t.c != 3;
    bad += k != 4;
    bad += p.x != 4.5 || p.y != -5.25;
    bad += d != 6.0;
    bad += o.s[0] != 'a' || o.s[1] != 'b' || o.s[2] != 'c';
    bad += sm.lo != 3 || sm.hi != -4;
    bad += b.a != 5 || b.b != 6000000000LL || b.c != -7;
    bad += ti.v != 8;
    bad += last != 99;
    return bad;
}

// a va_list handed on, as vprintf takes one
static int third_of(va_list ap)
{
    Three t = va_arg(ap, Three);
    return t.c;
}

static int pass_on(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    int r = third_of(ap);
    va_end(ap);
    return r;
}

// va_arg straight into an expression and a member
static int member_of(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    int r = va_arg(ap, Three).b;
    r += (int)va_arg(ap, Big).c;
    va_end(ap);
    return r;
}

int main(void)
{
    Three t1 = { 1, 2, 3 }, t2 = { 4, 5, 6 };
    ASSERT(123, sum_threes(1, t1));
    ASSERT(123456, sum_threes(2, t1, t2));
    Three t3 = { 7, 8, 9 };
    ASSERT(123456789, sum_threes(3, t1, t2, t3));
    Pair p = { 4.5, -5.25 };
    Odd o = { { 'a', 'b', 'c' } };
    Small sm = { 3, -4 };
    Big b = { 5, 6000000000LL, -7 };
    Tiny ti = { 8 };
    ASSERT(0, mixed(9, t1, 4, p, 6.0, o, sm, b, ti, 99));
    ASSERT(3, pass_on(1, t1));
    ASSERT(2 - 7, member_of(2, t1, b));
    // through a pointer to the function
    long long (*fn)(int, ...) = sum_threes;
    ASSERT(456, fn(1, t2));
    // the caller's struct is not changed by the callee's copy
    ASSERT(1, t1.a == 1 && t1.b == 2 && t1.c == 3);
    return g_fail;
}
