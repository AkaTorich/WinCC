// strtof / wcstof: ERANGE beyond FLT_MAX and below FLT_MIN; NaN payloads in
// strtod, strtof, nan, nanf
#include "test.h"
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <wchar.h>

static unsigned long long dbits(double d) { unsigned long long u; memcpy(&u, &d, 8); return u; }
static unsigned fbits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

// the value's bits and errno after strtof(s)
static int range_of(const char *s, float *out)
{
    errno = 0;
    char *end;
    *out = strtof(s, &end);
    return errno == ERANGE;
}

int main(void)
{
    float f;
    ASSERT(1, range_of("1e39", &f));
    ASSERT(1, isinf(f) && f > 0);
    ASSERT(1, f == HUGE_VALF);
    ASSERT(1, range_of("-1e39", &f));
    ASSERT(1, isinf(f) && f < 0);
    ASSERT(1, range_of("3.5e38", &f));
    ASSERT(0, range_of("3.4e38", &f));              // still below FLT_MAX
    ASSERT(1, f > 3.3e38f);
    ASSERT(1, range_of("1e-39", &f));               // a denormal float
    ASSERT(1, f > 0 && f < FLT_MIN);
    ASSERT(1, range_of("1e-50", &f));               // nothing left
    ASSERT(1, f == 0);
    ASSERT(1, range_of("-1e-50", &f));
    ASSERT(1, f == 0 && signbit(f));
    ASSERT(0, range_of("1.17549435e-38", &f));      // FLT_MIN itself
    ASSERT(0, range_of("0", &f));
    ASSERT(0, range_of("0e-500", &f));              // a zero is no underflow
    ASSERT(0, range_of("1.5", &f));
    ASSERT(1, f == 1.5f);
    ASSERT(0, range_of("inf", &f));
    ASSERT(1, isinf(f));
    ASSERT(0, range_of("nan", &f));
    ASSERT(1, isnan(f));
    // the end pointer still goes past the number
    char *end;
    strtof("2e40xyz", &end);
    ASSERT(0, strcmp(end, "xyz"));
    // wcstof the same
    wchar_t *wend;
    errno = 0;
    f = wcstof(L"1e39", &wend);
    ASSERT(ERANGE, errno);
    ASSERT(1, isinf(f) && *wend == 0);
    errno = 0;
    f = wcstof(L"0.25", &wend);
    ASSERT(0, errno);
    ASSERT(1, f == 0.25f);

    // NaN payloads
    ASSERT(0x7FF8000000000005ull, dbits(strtod("nan(5)", 0)));
    ASSERT(0x7FF8000000001234ull, dbits(strtod("NAN(0x1234)", 0)));
    ASSERT(0xFFF8000000000007ull, dbits(strtod("-nan(7)", 0)));
    ASSERT(0x7FF8000000000000ull, dbits(strtod("nan(abc)", 0)));   // not a number: no payload
    ASSERT(0x7FF8000000000000ull, dbits(strtod("nan()", 0)));
    ASSERT(0x7FF8000000000005ull, dbits(nan("5")));
    ASSERT(0x7FF8000000000010ull, dbits(nan("0x10")));
    ASSERT(0x7FF8000000000008ull, dbits(nan("010")));               // octal
    ASSERT(0x7FF8000000000000ull, dbits(nan("")));
    ASSERT(0x7FF8000000000000ull, dbits(nan("5x")));
    ASSERT(0x7FF8000000000000ull, dbits(nan("tag")));
    ASSERT(0x7FC00005u, fbits(nanf("5")));
    ASSERT(0x7FC00005u, fbits(strtof("nan(5)", 0)));
    ASSERT(0x7FC00000u, fbits(nanf("")));
    ASSERT(1, isnan(nan("123")));
    errno = 0;
    nan("99999999999999999999999");                 // too big for the parser: errno stays
    ASSERT(0, errno);
    return g_fail;
}
