// Number parsing: strtod (correct rounding), strtol family, atoi/atof,
// and printf -> strtod round trips on random doubles of every size
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>

typedef union { double d; unsigned long long u; } Bits;

static unsigned long long bits_of(double d) { Bits b; b.d = d; return b.u; }
static double from_bits(unsigned long long u) { Bits b; b.u = u; return b.d; }

static void same_bits(unsigned long long want, double got, const char *code, int line)
{
    if (bits_of(got) != want) {
        printf("\n    строка %d: %s = %a, ожидалось %a", line, code, got, from_bits(want));
        g_fail++;
    }
}

#define BITS(want, expr) same_bits(want, expr, #expr, __LINE__)

static unsigned long long rnd64(void)
{
    unsigned long long v = 0;
    for (int i = 0; i < 5; i++)
        v = (v << 15) ^ (unsigned long long)rand();
    return v;
}

int main(void)
{
    char *e;
    const char *s;
    // strtod: the usual forms
    ASSERT(1, strtod("3.14", &e) == 3.14);
    ASSERT(0, *e);
    s = "  -2.5e3xyz";
    ASSERT(1, strtod(s, &e) == -2500.0);
    ASSERT('x', *e);
    ASSERT(1, strtod(".5", 0) == 0.5);
    ASSERT(1, strtod("5.", &e) == 5.0);
    ASSERT(0, *e);
    s = "abc";
    ASSERT(1, strtod(s, &e) == 0.0);
    ASSERT(1, e == s);
    s = "1e+";
    ASSERT(1, strtod(s, &e) == 1.0);
    ASSERT(1, e == s + 1);
    ASSERT(1, strtod("0x1.8p1", 0) == 3.0);
    ASSERT(1, strtod("0X10", 0) == 16.0);
    ASSERT(1, strtod("-0x.8", 0) == -0.5);
    ASSERT(1, isinf(strtod("inf", 0)));
    ASSERT(1, strtod("-Infinity", &e) < 0);
    ASSERT(0, *e);
    ASSERT(1, isnan(strtod("nan", 0)));
    ASSERT(1, isnan(strtod("NAN(123)", &e)));
    ASSERT(0, *e);
    ASSERT(1, signbit(strtod("-0", 0)));
    // exact rounding
    BITS(0x3FB999999999999AULL, strtod("0.1", 0));
    BITS(0x3FD5555555555555ULL, strtod("0.333333333333333333333333333333", 0));
    BITS(0x4340000000000000ULL, strtod("9007199254740993", 0));           // a tie: to even
    BITS(0x4340000000000001ULL, strtod("9007199254740993.0000000001", 0)); // just above the tie
    BITS(0x4340000000000002ULL, strtod("9007199254740995", 0));           // a tie: to even, upward
    BITS(0x0000000000000001ULL, strtod("4.9406564584124654e-324", 0));
    BITS(0x0000000000000001ULL, strtod("3e-324", 0));
    BITS(0x0000000000000000ULL, strtod("2e-324", 0));
    BITS(0x000FFFFFFFFFFFFFULL, strtod("2.2250738585072009e-308", 0));
    BITS(0x0010000000000000ULL, strtod("2.2250738585072014e-308", 0));
    BITS(0x7FEFFFFFFFFFFFFFULL, strtod("1.7976931348623157e308", 0));
    BITS(0x7FF0000000000000ULL, strtod("1.7976931348623159e308", 0));
    BITS(0x3FF0000000000000ULL, strtod("1.00000000000000011102230246251565404236316680908203124", 0));
    BITS(0x3FF0000000000002ULL, strtod("1.00000000000000033306690738754696212708950042724609375", 0)); // tie, to even
    BITS(0x44B52D02C7E14AF6ULL, strtod("1e23", 0));
    ASSERT(1, strtod("1e-400", 0) == 0.0);
    errno = 0;
    strtod("1e400", 0);
    ASSERT(ERANGE, errno);
    ASSERT(1, strtod("2.2250738585072014e-308", 0) == DBL_MIN);
    ASSERT(1, strtod("1.7976931348623157e308", 0) == DBL_MAX);
    ASSERT(1, strtod("2.220446049250313e-16", 0) == DBL_EPSILON);
    // 1000 digits: the ones past 800 still decide the rounding
    {
        static char big[1200];
        strcpy(big, "1.00000000000000011102230246251565404236316680908203125");    // 1 + 2^-53
        int len = strlen(big);
        for (int i = 0; i < 900; i++) big[len++] = '0';
        big[len++] = '1';
        big[len] = 0;
        BITS(0x3FF0000000000001ULL, strtod(big, 0));     // a hair above the tie
        big[len - 1] = '0';
        BITS(0x3FF0000000000000ULL, strtod(big, 0));     // exactly the tie
    }
    // round trips: %.17g and %a give back the same bits
    srand(7);
    int bad = 0;
    char buf[64];
    for (int i = 0; i < 3000; i++) {
        unsigned long long u = rnd64();
        if (((u >> 52) & 0x7FF) == 0x7FF) continue;      // no inf, nan
        double d = from_bits(u);
        sprintf(buf, "%.17g", d);
        if (bits_of(strtod(buf, 0)) != u) { if (bad++ < 5) printf("\n    %%.17g: %s", buf); }
        sprintf(buf, "%a", d);
        if (bits_of(strtod(buf, 0)) != u) { if (bad++ < 5) printf("\n    %%a: %s", buf); }
        sprintf(buf, "%.16e", d);
        if (bits_of(strtod(buf, 0)) != u) { if (bad++ < 5) printf("\n    %%.16e: %s", buf); }
    }
    ASSERT(0, bad);
    // short forms still read back
    double vals[] = { 1.0 / 3, 2.0 / 3, 0.1 + 0.2, 1e100 / 3, 1e-100 / 7, 123456789.125 };
    for (int i = 0; i < 6; i++) {
        sprintf(buf, "%.17g", vals[i]);
        ASSERT(1, strtod(buf, 0) == vals[i]);
    }
    // strtol family
    s = "  -123abc";
    ASSERT(-123, strtol(s, &e, 10));
    ASSERT('a', *e);
    ASSERT(31, strtol("0x1F", 0, 0));
    ASSERT(31, strtol("1f", 0, 16));
    ASSERT(31, strtol("0x1f", 0, 16));
    ASSERT(511, strtol("0777", 0, 0));
    ASSERT(35, strtol("z", 0, 36));
    ASSERT(5, strtol("101", 0, 2));
    s = "0x";
    ASSERT(0, strtol(s, &e, 16));
    ASSERT(1, e == s + 1);
    s = "";
    ASSERT(0, strtol(s, &e, 10));
    ASSERT(1, e == s);
    errno = 0;
#ifdef _WIN32                           // Windows: long is 4 bytes
    ASSERT(LONG_MAX, strtol("2147483648", 0, 10));
    ASSERT(ERANGE, errno);
    errno = 0;
    ASSERT(LONG_MIN, strtol("-2147483648", 0, 10));
    ASSERT(0, errno);
    ASSERT(LLONG_MIN, strtoll("-99999999999999999999", 0, 10));
#else
    ASSERT(LONG_MAX, strtol("9223372036854775808", 0, 10));
    ASSERT(ERANGE, errno);
    errno = 0;
    ASSERT(LONG_MIN, strtol("-9223372036854775808", 0, 10));
    ASSERT(0, errno);
    ASSERT(LONG_MIN, strtoll("-99999999999999999999", 0, 10));
#endif
    ASSERT(1, strtoul("-1", 0, 10) == ULONG_MAX);
    ASSERT(1, strtoull("18446744073709551615", 0, 10) == 18446744073709551615ULL);
    errno = 0;
    ASSERT(1, strtoull("18446744073709551616", 0, 10) == 18446744073709551615ULL);
    ASSERT(ERANGE, errno);
    ASSERT(42, atoi(" 42"));
    ASSERT(-7, atoi("-7x"));
#ifdef _WIN32
    ASSERT(123456789, atol("123456789"));
#else
    ASSERT(123456789012LL, atol("123456789012"));
#endif
    ASSERT(-5, atoll("-5"));
    ASSERT(1, atof("2.5") == 2.5);
    ASSERT(1, strtof("0.5", 0) == 0.5f);
    return g_fail;
}
