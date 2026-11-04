// The preprocessor and numbers: __has_include, #include by a macro, ", ##__VA_ARGS__",
// __VA_OPT__, push/pop_macro, _Pragma, __pragma, 'AB', 1'000, 0x1p4, i64.
#include "test.h"
#include <string.h>
#if __has_include(<string.h>) && !__has_include("нет такого.h")
#define HAS 1
#else
#define HAS 0
#endif
#define HDR <stddef.h>
#include HDR
#define CNT(...) (sizeof((int[]){ 0, ##__VA_ARGS__ }) / sizeof(int))
#define OPT(a, ...) a __VA_OPT__(+ __VA_ARGS__)
#define X 1
#pragma push_macro("X")
#undef X
#define X 2
static int x2 = X;
#pragma pop_macro("X")
_Pragma("once")
__pragma(warning(disable: 4996))
int main(void)
{
    ASSERT(1, HAS);
    ASSERT(1, CNT());
    ASSERT(3, CNT(5, 6));
    ASSERT(1, OPT(1));
    ASSERT(6, OPT(1, 2 + 3));
    ASSERT(1, X);
    ASSERT(2, x2);
    ASSERT(0x4142, 'AB');
    ASSERT(1000000, 1'000'000);
    ASSERT(1, 0x1p4 == 16.0 && 0x.8p0 == 0.5 && 0x1.8p1 == 3.0);
    ASSERT(1, sizeof(5i64) == 8);
    ASSERT(201112, __STDC_VERSION__);
    return g_fail;
}
