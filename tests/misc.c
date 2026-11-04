#include "test.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define STR(x) #x
#define CAT(a, b) a##b
#define COUNT(...) count_args(__VA_ARGS__, 0)
int count_args(int a, ...) { va_list ap; va_start(ap, a); int n = 1; while (va_arg(ap, int)) n++; va_end(ap); return n; }
int sum_va(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    int s = 0;
    for (int i = 0; i < n; i++) s += va_arg(ap, int);
    va_end(ap);
    return s;
}
int many(int a, int b, int c, int d, int e, int f, int g) { return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7; }
int twice(int x) { return x * 2; }
int apply(int (*fn)(int), int v) { return fn(v); }
int counter(void) { static int n; return ++n; }
#if defined(MAX) && 1 + 1 == 2
int g_pp = 1;
#else
int g_pp = 0;
#endif
#ifdef NOPE
#error should not happen
#elif 0
int g_bad;
#else
int g_elif = 7;
#endif
int main(void)
{
    ASSERT(5, MAX(3, 5));
    ASSERT(3, strlen(STR(abc)));
    int CAT(my, var) = 9;
    ASSERT(9, myvar);
    ASSERT(15, sum_va(5, 1, 2, 3, 4, 5));
    ASSERT(3, COUNT(4, 5, 6));
    ASSERT(140, many(1, 2, 3, 4, 5, 6, 7));
    ASSERT(8, apply(twice, 4));
    int (*fp)(int) = twice;
    ASSERT(6, fp(3));
    counter(); counter();
    ASSERT(3, counter());
    ASSERT(1, g_pp);
    ASSERT(7, g_elif);
    char buf[64];
    sprintf(buf, "%d-%s-%05.1f", 12, "ab", 3.14159);
    ASSERT(0, strcmp(buf, "12-ab-003.1"));
    char *m = malloc(100);
    strcpy(m, "dynasty");
    ASSERT(7, strlen(m));
    free(m);
    int arr[] = { 5, 6, 7, 8 };
    ASSERT(4, sizeof(arr) / sizeof(arr[0]));
    char s[] = "hi";
    ASSERT(3, sizeof(s));
    ASSERT(__LINE__, __LINE__);
    return g_fail;
}
