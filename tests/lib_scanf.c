// sscanf / vsscanf: every conversion, widths, sets, %n, failures
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static int my_scan(const char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsscanf(s, fmt, ap);
    va_end(ap);
    return n;
}

int main(void)
{
    int a = 0, b = 0, c = 0, d = 0;
    ASSERT(4, sscanf("42 -17 0x1f 077", "%d %d %x %o", &a, &b, &c, &d));
    ASSERT(42, a);
    ASSERT(-17, b);
    ASSERT(31, c);
    ASSERT(63, d);
    ASSERT(3, sscanf("0x10 010 10", "%i %i %i", &a, &b, &c));
    ASSERT(16, a);
    ASSERT(8, b);
    ASSERT(10, c);
    unsigned u;
    ASSERT(1, sscanf("4294967295", "%u", &u));
    ASSERT(1, u == 4294967295u);
    long long ll;
    long l;
    short h;
    char hh;
    size_t z;
    ASSERT(5, sscanf("-9000000000 123456789012 -2 65 77", "%lld %ld %hd %hhd %zu", &ll, &l, &h, &hh, &z));
    ASSERT(-9000000000LL, ll);
#ifdef _WIN32                           // Windows: long is 4 bytes, the low ones stay
    ASSERT((long)123456789012LL, l);
#else
    ASSERT(123456789012LL, l);
#endif
    ASSERT(-2, h);
    ASSERT(65, hh);
    ASSERT(77, z);
    // floats
    float f;
    double x, y;
    ASSERT(3, sscanf("3.25 -1e3 inf", "%f %lf %lf", &f, &x, &y));
    ASSERT(1, f == 3.25f);
    ASSERT(1, x == -1000.0);
    ASSERT(1, y > 1e308);
    ASSERT(1, sscanf("0x1.8p1", "%lf", &x));
    ASSERT(1, x == 3.0);
    char s[32], t[32];
    ASSERT(2, sscanf("1ex", "%lf%s", &x, s));    // "e" without digits is not taken
    ASSERT(1, x == 1.0);
    ASSERT(0, strcmp(s, "ex"));
    ASSERT(2, sscanf("2.5e+x", "%lf%s", &x, s));
    ASSERT(1, x == 2.5);
    ASSERT(0, strcmp(s, "e+x"));
    // strings, characters, widths
    ASSERT(2, sscanf("  hello   world  ", "%s %s", s, t));
    ASSERT(0, strcmp(s, "hello"));
    ASSERT(0, strcmp(t, "world"));
    ASSERT(2, sscanf("abcdefgh", "%3s%s", s, t));
    ASSERT(0, strcmp(s, "abc"));
    ASSERT(0, strcmp(t, "defgh"));
    ASSERT(2, sscanf("12345", "%3d%d", &a, &b));
    ASSERT(123, a);
    ASSERT(45, b);
    char ch[4] = { 0 };
    ASSERT(1, sscanf(" xy", "%c", ch));
    ASSERT(' ', ch[0]);                          // %c does not skip spaces
    ASSERT(1, sscanf("xyz", "%2c", ch));
    ASSERT(0, memcmp(ch, "xy", 2));
    ASSERT(1, sscanf("  q", " %c", ch));
    ASSERT('q', ch[0]);
    // sets
    ASSERT(2, sscanf("abc123def", "%[a-z]%[0-9]", s, t));
    ASSERT(0, strcmp(s, "abc"));
    ASSERT(0, strcmp(t, "123"));
    ASSERT(2, sscanf("key=value;rest", "%[^=]=%[^;]", s, t));
    ASSERT(0, strcmp(s, "key"));
    ASSERT(0, strcmp(t, "value"));
    ASSERT(1, sscanf("]]x", "%[]]", s));
    ASSERT(0, strcmp(s, "]]"));
    ASSERT(1, sscanf("a-b-c+", "%[-a-c]", s));
    ASSERT(0, strcmp(s, "a-b-c"));
    // literals, %%, %n, %*
    ASSERT(3, sscanf("2024-10-01", "%d-%d-%d", &a, &b, &c));
    ASSERT(2024, a);
    ASSERT(10, b);
    ASSERT(1, c);
    ASSERT(1, sscanf("50%", "%d%%", &a));
    ASSERT(50, a);
    int n = 0;
    ASSERT(1, sscanf("abc  def", "%s%n", s, &n));
    ASSERT(3, n);
    ASSERT(1, sscanf("10 20 30", "%*d %*d %d", &a));
    ASSERT(30, a);
    void *p = 0;
    char pbuf[32];
    sprintf(pbuf, "%p", (void *)pbuf);
    ASSERT(1, sscanf(pbuf, "%p", &p));
    ASSERT(1, p == (void *)pbuf);
    // failures
    ASSERT(EOF, sscanf("", "%d", &a));
    ASSERT(EOF, sscanf("   ", "%d", &a));
    ASSERT(0, sscanf("abc", "%d", &a));
    ASSERT(1, sscanf("7 x", "%d %d", &a, &b));
    ASSERT(0, sscanf("x", "y%d", &a));
    ASSERT(2, my_scan("8:9", "%d:%d", &a, &b));
    ASSERT(8, a);
    ASSERT(9, b);
    return g_fail;
}
