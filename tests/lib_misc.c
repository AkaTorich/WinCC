// The small headers: errno, assert, limits, float, stdint, inttypes,
// stdbool, stddef, stdalign, iso646, signal, locale; math constants
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>
#include <iso646.h>
#include <signal.h>
#include <locale.h>
#include <time.h>

typedef union { double d; unsigned long long u; } Bits;
static unsigned long long bits_of(double d) { Bits b; b.d = d; return b.u; }

static int g_sig;
static void on_sig(int s) { g_sig = s; }

struct S { char c; double d; int i; };

int main(void)
{
    char buf[64];
    // errno
    errno = 0;
    strtol("99999999999999999999", 0, 10);
    ASSERT(ERANGE, errno);
    errno = 0;
    log(-1.0);
    ASSERT(EDOM, errno);
    errno = 0;
    ASSERT(1, fopen("no such file 456.txt", "r") == 0);
    ASSERT(ENOENT, errno);
    // assert: a true condition does nothing
    assert(1 + 1 == 2);
    assert(strlen("abc") == 3);
#define NDEBUG
#include <assert.h>
    assert(0);                                   // switched off by NDEBUG
    // limits.h
    ASSERT(8, CHAR_BIT);
    ASSERT(2147483647, INT_MAX);
    ASSERT(-2147483647 - 1, INT_MIN);
#ifdef _WIN32                           // Windows: long is 4 bytes
    ASSERT(1, LONG_MAX == 2147483647 && sizeof(long) == 4);
#else
    ASSERT(1, LONG_MAX == 9223372036854775807L);
#endif
    ASSERT(1, ULLONG_MAX == 18446744073709551615ULL);
    ASSERT(65535, USHRT_MAX);
    // float.h: the exact values
    ASSERT(1, bits_of(DBL_MAX) == 0x7FEFFFFFFFFFFFFFULL);
    ASSERT(1, bits_of(DBL_MIN) == 0x0010000000000000ULL);
    ASSERT(1, bits_of(DBL_TRUE_MIN) == 1);
    ASSERT(1, bits_of(DBL_EPSILON) == 0x3CB0000000000000ULL);
    ASSERT(1, FLT_MAX == (float)strtod("3.40282347e38", 0));
    ASSERT(1, FLT_MIN == (float)strtod("1.17549435e-38", 0));
    ASSERT(1, FLT_EPSILON == (float)strtod("1.1920929e-7", 0));
    float fbits[4] = { FLT_MAX, FLT_MIN, FLT_TRUE_MIN, FLT_EPSILON };
    unsigned fexp[4] = { 0x7F7FFFFFu, 0x00800000u, 1u, 0x34000000u };
    for (int i = 0; i < 4; i++)
        ASSERT(fexp[i], *(unsigned *)&fbits[i]);
    ASSERT(1, 0.1 + 0.2 == 0.30000000000000004);   // literals are read exactly too
    ASSERT(1, bits_of(1e23) == 0x44B52D02C7E14AF6ULL);
    ASSERT(53, DBL_MANT_DIG);
    ASSERT(15, DBL_DIG);
    static double gmax = DBL_MAX;                // works in a constant initialiser too
    ASSERT(1, gmax == DBL_MAX);
    // math constants: each as strtod reads the same digits
    ASSERT(1, M_PI == strtod("3.14159265358979323846", 0));
    ASSERT(1, M_E == strtod("2.71828182845904523536", 0));
    ASSERT(1, M_LN2 == strtod("0.693147180559945309417", 0));
    ASSERT(1, M_LN10 == strtod("2.30258509299404568402", 0));
    ASSERT(1, M_SQRT2 == strtod("1.41421356237309504880", 0));
    ASSERT(1, M_SQRT1_2 == strtod("0.707106781186547524401", 0));
    ASSERT(1, M_PI_2 == strtod("1.57079632679489661923", 0));
    ASSERT(1, M_LOG2E == strtod("1.44269504088896340736", 0));
    ASSERT(1, M_LOG10E == strtod("0.434294481903251827651", 0));
    ASSERT(1, M_1_PI == strtod("0.318309886183790671538", 0));
    ASSERT(1, M_2_PI == strtod("0.636619772367581343076", 0));
    ASSERT(1, M_PI_4 == strtod("0.785398163397448309616", 0));
    ASSERT(1, M_2_SQRTPI == strtod("1.12837916709551257390", 0));
    ASSERT(1, isinf(HUGE_VAL) && HUGE_VAL > 0);
    ASSERT(1, isnan(NAN) && !signbit(NAN));
    // stdint.h, inttypes.h
    int64_t big = INT64_MIN;
    sprintf(buf, "%" PRId64, big);
    ASSERT(0, strcmp(buf, "-9223372036854775808"));
    sprintf(buf, "%" PRIu32 " %" PRIx64 " %" PRIX8, (uint32_t)4000000000u, (uint64_t)255, (uint8_t)171);
    ASSERT(0, strcmp(buf, "4000000000 ff AB"));
    int16_t h16;
    ASSERT(1, sscanf("-300", "%" SCNd16, &h16));
    ASSERT(-300, h16);
    ASSERT(1, UINT64_C(1) << 63 == 9223372036854775808ULL);
    ASSERT(1, SIZE_MAX == 18446744073709551615ULL);
    ASSERT(8, sizeof(intmax_t));
    ASSERT(2, sizeof(int_least16_t));
    ASSERT(-1, INT8_MIN + 127);
    ASSERT(5, imaxabs(-5));
    imaxdiv_t q = imaxdiv(17, 5);
    ASSERT(3, q.quot);
    ASSERT(2, q.rem);
    ASSERT(-77, strtoimax("-77", 0, 10));
    ASSERT(1, strtoumax("ff", 0, 16) == 255);
    // stdbool.h, stddef.h, stdalign.h, iso646.h
    bool yes = true, no = false;
    ASSERT(1, yes && !no);
    ASSERT(1, __bool_true_false_are_defined);
    ASSERT(8, offsetof(struct S, d));
    ASSERT(16, offsetof(struct S, i));
#ifdef _WIN32
    ASSERT(2, sizeof(wchar_t));
#else
    ASSERT(4, sizeof(wchar_t));
#endif
    ASSERT(8, alignof(double));
    ASSERT(1, 1 and not 0);
    ASSERT(1, (0 or 1) and (3 bitand 1));
    int k = 5;
    k xor_eq 1;
    ASSERT(4, k);
    // signal.h
    ASSERT(1, signal(SIGINT, on_sig) == SIG_DFL);
    ASSERT(0, raise(SIGINT));
    ASSERT(SIGINT, g_sig);
    ASSERT(1, signal(SIGTERM, SIG_IGN) == SIG_DFL);
    ASSERT(0, raise(SIGTERM));
    ASSERT(1, signal(999, on_sig) == SIG_ERR);
    // locale.h
    ASSERT(0, strcmp(setlocale(LC_ALL, 0), "C"));
    ASSERT(0, strcmp(setlocale(LC_ALL, "C"), "C"));
    ASSERT(0, strcmp(setlocale(LC_ALL, "ru_RU.UTF-8"), "ru_RU.UTF-8"));
    ASSERT(0, strcmp(setlocale(LC_ALL, 0), "ru_RU.UTF-8"));
    ASSERT(0, strcmp(setlocale(LC_CTYPE, "en_US.utf8"), "en_US.UTF-8"));
    ASSERT(1, setlocale(LC_ALL, "") != 0);                 // the user's locale
    ASSERT(0, strcmp(setlocale(LC_NUMERIC, "POSIX"), "POSIX"));
    ASSERT(0, strcmp(setlocale(LC_ALL, "C.UTF-8"), "C.UTF-8"));
    ASSERT(1, setlocale(LC_ALL, "xx_QQ.UTF-8") == 0);
    ASSERT(1, setlocale(99, "C") == 0);
    ASSERT(0, strcmp(setlocale(LC_ALL, 0), "C.UTF-8"));    // a refused name changes nothing
    ASSERT(4, MB_CUR_MAX);
    struct lconv *lc = localeconv();
    ASSERT(0, strcmp(lc->decimal_point, "."));
    ASSERT(127, lc->frac_digits);
#ifdef __WINCC__
    // locales of Windows: the decimal point, the names of months, the order of words, the messages
    ASSERT(0, strcmp(setlocale(LC_ALL, "Russian"), "ru_RU.UTF-8"));
    ASSERT(0, strcmp(localeconv()->decimal_point, ","));
    char lb[64];
    snprintf(lb, sizeof(lb), "%.1f", 2.5);
    ASSERT(0, strcmp(lb, "2,5"));
    ASSERT(1, strtod("3,25", 0) == 3.25);
    struct tm jan = { 0 };
    jan.tm_mday = 1;
    jan.tm_year = 126;
    strftime(lb, sizeof(lb), "%B", &jan);
    ASSERT(0xD0, (unsigned char)lb[0]);              // a Cyrillic letter: the January of Russian
    ASSERT(1, strcoll("ёж", "яблоко") < 0);          // ё goes with е, before я...
    ASSERT(1, strcmp("ёж", "яблоко") > 0);           // ...though its bytes are greater
    ASSERT(0xD0, (unsigned char)strerror(ENOENT)[0]);
    ASSERT(0, strcmp(setlocale(LC_ALL, "C"), "C"));
    ASSERT(0, strcmp(localeconv()->decimal_point, "."));
    snprintf(lb, sizeof(lb), "%.1f", 2.5);
    ASSERT(0, strcmp(lb, "2.5"));
#endif
    return g_fail;
}
