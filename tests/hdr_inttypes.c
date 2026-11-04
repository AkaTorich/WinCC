// <inttypes.h>: every PRI / SCN macro, MAX and PTR, LEAST and FAST among them;
// <stdint.h>: WINT_*, SIG_ATOMIC_*, WCHAR_*, SIZE_MAX, PTRDIFF_*
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <wchar.h>
#include <stdint.h>

static char out[128];

int main(void)
{
    // scanning octal into the widest types
    intmax_t m = 0;
    ASSERT(1, sscanf("777", "%" SCNoMAX, (uintmax_t *)&m));
    ASSERT(511, m);
    uintptr_t p = 0;
    ASSERT(1, sscanf("1777777777777777777777", "%" SCNoPTR, &p));
    ASSERT(1, p == UINTPTR_MAX);
    // printing with each of them
    sprintf(out, "%" PRIoMAX " %" PRIoPTR " %" PRIXMAX, (uintmax_t)8, (uintptr_t)64, (uintmax_t)255);
    ASSERT(0, strcmp(out, "10 100 FF"));
    // LEAST and FAST: printing
    int_least8_t l8 = -5; int_least16_t l16 = -300; int_least32_t l32 = -70000; int_least64_t l64 = -5000000000LL;
    sprintf(out, "%" PRIdLEAST8 " %" PRIdLEAST16 " %" PRIdLEAST32 " %" PRIdLEAST64, l8, l16, l32, l64);
    ASSERT(0, strcmp(out, "-5 -300 -70000 -5000000000"));
    int_fast8_t f8 = -6; int_fast16_t f16 = -40000; int_fast32_t f32 = 5000000000LL; int_fast64_t f64 = -1;
    sprintf(out, "%" PRIiFAST8 " %" PRIdFAST16 " %" PRIiFAST32 " %" PRIdFAST64, f8, f16, f32, f64);
    ASSERT(0, strcmp(out, "-6 -40000 5000000000 -1"));
    uint_least16_t u16 = 65535; uint_fast32_t u32 = 4294967296ULL; uint_least64_t u64 = UINT64_MAX;
    sprintf(out, "%" PRIuLEAST16 " %" PRIuFAST32 " %" PRIxLEAST64 " %" PRIXFAST16 " %" PRIoLEAST8,
            u16, u32, u64, (uint_fast16_t)0xABCDEF012ULL, (uint_least8_t)8);
    ASSERT(0, strcmp(out, "65535 4294967296 ffffffffffffffff ABCDEF012 10"));
    // LEAST and FAST: scanning writes exactly the type's width
    struct { int_least8_t v; char guard; } s8 = { 0, 'g' };
    ASSERT(1, sscanf("-7", "%" SCNdLEAST8, &s8.v));
    ASSERT(-7, s8.v);
    ASSERT('g', s8.guard);
    struct { uint_least16_t v; short guard; } s16 = { 0, 99 };
    ASSERT(1, sscanf("ffff", "%" SCNxLEAST16, &s16.v));
    ASSERT(65535, s16.v);
    ASSERT(99, s16.guard);
    int_fast16_t fs = 0;
    ASSERT(1, sscanf("-123456789012", "%" SCNdFAST16, &fs));
    ASSERT(-123456789012LL, fs);
    uint_fast8_t fu = 0;
    ASSERT(1, sscanf("17", "%" SCNoFAST8, &fu));
    ASSERT(15, fu);
    uint_fast64_t f64u = 0;
    ASSERT(1, sscanf("42", "%" SCNuFAST64, &f64u));
    ASSERT(42, f64u);
    int_least32_t i32 = 0;
    ASSERT(1, sscanf("0x10", "%" SCNiLEAST32, &i32));
    ASSERT(16, i32);
    // stdint limits
    ASSERT(0, WINT_MIN);
    ASSERT(4294967295LL, WINT_MAX);
    ASSERT(1, (wint_t)WINT_MAX == WEOF);
    ASSERT(1, WINT_MIN < 1 && WINT_MAX > 0);        // unsigned, as wint_t
    ASSERT(-2147483647LL - 1, SIG_ATOMIC_MIN);
    ASSERT(2147483647, SIG_ATOMIC_MAX);
    ASSERT(1, sizeof(sig_atomic_t) == 4);
#ifdef _WIN32                           // Windows: wchar_t is UTF-16
    ASSERT(0, WCHAR_MIN);
    ASSERT(65535, WCHAR_MAX);
    ASSERT(1, sizeof(wchar_t) == 2);
#else
    ASSERT(-2147483647LL - 1, WCHAR_MIN);
    ASSERT(2147483647, WCHAR_MAX);
    ASSERT(1, sizeof(wchar_t) == 4);
#endif
    ASSERT(1, SIZE_MAX == (size_t)-1);
    ASSERT(1, PTRDIFF_MAX == 9223372036854775807LL && PTRDIFF_MIN == -PTRDIFF_MAX - 1);
    ASSERT(1, INTPTR_MAX == PTRDIFF_MAX && UINTPTR_MAX == SIZE_MAX);
    return g_fail;
}
