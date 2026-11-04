// printf family: flags, widths, precisions, every conversion, exact floats
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>

static char buf[1024];

static void check_str(const char *want, const char *got, int line)
{
    if (strcmp(want, got)) {
        printf("\n    строка %d: \"%s\", ожидалось \"%s\"", line, got, want);
        g_fail++;
    }
}

#define STR(want, got) check_str(want, got, __LINE__)
#define FMT(want, ...) (sprintf(buf, __VA_ARGS__), check_str(want, buf, __LINE__))

static int my_vs(char *out, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, 64, fmt, ap);
    va_end(ap);
    return n;
}

int main(void)
{
    // integers
    FMT("42", "%d", 42);
    FMT("-42", "%i", -42);
    FMT("  42", "%4d", 42);
    FMT("42  |", "%-4d|", 42);
    FMT("0042", "%04d", 42);
    FMT("-042", "%04d", -42);
    FMT("+42", "%+d", 42);
    FMT(" 42", "% d", 42);
    FMT("00042", "%.5d", 42);
    FMT("   00042", "%8.5d", 42);
    FMT("   42", "%05.2d", 42);
    FMT("", "%.0d", 0);
    FMT("ff", "%x", 255);
    FMT("FF", "%X", 255);
    FMT("0xff", "%#x", 255);
    FMT("0", "%#x", 0);
    FMT("377", "%o", 255);
    FMT("0377", "%#o", 255);
    FMT("4294967295", "%u", 4294967295u);
    FMT("-1", "%ld", -1L);
    FMT("9223372036854775807", "%lld", 9223372036854775807LL);
    FMT("-9223372036854775808", "%lld", -9223372036854775807LL - 1);
    FMT("18446744073709551615", "%llu", 18446744073709551615ULL);
    FMT("123", "%zu", (size_t)123);
    FMT("4464", "%hd", 70000);
    FMT("-1", "%hhd", 255);
    FMT("255", "%hhu", -1);
    FMT("1010", "%b", 10);
    FMT("0x1234", "%p", (void *)0x1234);
    FMT("   7", "%*d", 4, 7);
    FMT("7   |", "%*d|", -4, 7);
    // characters and strings
    FMT("x", "%c", 'x');
    FMT("  x", "%3c", 'x');
    FMT("hello", "%s", "hello");
    FMT("hel", "%.3s", "hello");
    FMT("   hi", "%5s", "hi");
    FMT("hi   |", "%-5s|", "hi");
    FMT("(null)", "%s", (char *)0);
    FMT("  жук", "%5s", "жук");        // width in characters
    FMT("100%", "%d%%", 100);
    FMT("1,2,3", "%d,%d,%d", 1, 2, 3);
    // fixed
    FMT("3.141593", "%f", 3.14159265);
    FMT("3.14", "%.2f", 3.14159);
    FMT("3", "%.0f", 3.14159);
    FMT("3.", "%#.0f", 3.0);
    FMT("0.50", "%.2f", 0.5);
    FMT("2", "%.0f", 2.5);             // ties go to even
    FMT("4", "%.0f", 3.5);
    FMT("0", "%.0f", 0.5);
    FMT("2", "%.0f", 1.5);
    FMT("0.2", "%.1f", 0.25);
    FMT("0.1", "%.1f", 0.05);          // 0.05 is a little above
    FMT("1.500000", "%f", 1.5f);
    FMT("-0.000000", "%f", -0.0);
    FMT("  3.14", "%6.2f", 3.14159);
    FMT("003.14", "%06.2f", 3.14159);
    FMT("-03.14", "%06.2f", -3.14159);
    FMT("+3.1", "%+.1f", 3.14159);
    FMT("1000000.000000", "%f", 1e6);
    FMT("0.000001", "%f", 1e-6);
    FMT("0.000000", "%f", 1e-7);
    FMT("0.00", "%.2f", 0.004);
    FMT("0.01", "%.2f", 0.006);
    FMT("10.0", "%.1f", 9.96);
    FMT("0.1000000000000000055511151231257827021181583404541015625", "%.55f", 0.1);
    FMT("3.14", "%.*f", 2, 3.14159);
    // exponent
    FMT("1.000000e+00", "%e", 1.0);
    FMT("1.234560e+05", "%e", 123456.0);
    FMT("1.23E-05", "%.2E", 0.0000123);
    FMT("1e+02", "%.0e", 100.0);
    FMT("1.0000000000000000555e-01", "%.19e", 0.1);
    FMT("4.941e-324", "%.3e", DBL_TRUE_MIN);
    FMT("1.797693e+308", "%e", DBL_MAX);
    FMT("2.225074e-308", "%e", DBL_MIN);
    // general
    FMT("100000", "%g", 100000.0);
    FMT("1e+06", "%g", 1000000.0);
    FMT("0.0001", "%g", 0.0001);
    FMT("1e-05", "%g", 0.00001);
    FMT("3.14159", "%g", 3.14159265);
    FMT("0", "%g", 0.0);
    FMT("1.5", "%g", 1.5);
    FMT("1.50000", "%#g", 1.5);
    FMT("1e+300", "%g", 1e300);
    FMT("0.10000000000000001", "%.17g", 0.1);
    FMT("123457", "%.6g", 123456.7);
    FMT("1.2346E+07", "%.5G", 12345678.0);
    // hexadecimal floats
    FMT("0x1p+0", "%a", 1.0);
    FMT("0x1.8p+1", "%a", 3.0);
    FMT("-0x1p-1", "%a", -0.5);
    FMT("0x0p+0", "%a", 0.0);
    FMT("0x1.999999999999ap-4", "%a", 0.1);
    FMT("0x1.99999ap-4", "%.6a", 0.1);
    FMT("0X1.8P+1", "%A", 3.0);
    // not numbers
    FMT("inf", "%f", HUGE_VAL);
    FMT("-inf", "%f", -HUGE_VAL);
    FMT("  inf", "%5f", INFINITY);
    FMT("INF", "%F", INFINITY);
    FMT("nan", "%f", NAN);
    FMT("nan", "%g", NAN);
    // DBL_MAX written out whole: 309 digits
    sprintf(buf, "%.0f", DBL_MAX);
    ASSERT(309, strlen(buf));
    ASSERT(0, strncmp(buf, "1797693134862315708145274237317043567980", 40));
    ASSERT(0, strcmp(buf + 303, "858368"));
    // %n, return values, truncation
    int n = 0;
    sprintf(buf, "abc%nde", &n);
    ASSERT(3, n);
    ASSERT(5, snprintf(buf, 4, "hello"));
    STR("hel", buf);
    ASSERT(11, sprintf(buf, "%s %s", "hello", "world"));
    ASSERT(5, my_vs(buf, "%d-%s", 12, "ab"));
    STR("12-ab", buf);
    ASSERT(0, snprintf(0, 0, ""));
    ASSERT(3, snprintf(0, 0, "%d", 123));
    ASSERT(0, printf(""));
    return g_fail;
}
