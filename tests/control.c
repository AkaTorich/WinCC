#include "test.h"
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
int sw(int x)
{
    switch (x) {
    case 1: return 10;
    case 2:
    case 3: return 20;
    case 5 ... 9: return 50;
    default: return -1;
    }
}
int fallthrough(int x)
{
    int r = 0;
    switch (x) {
    case 0: r += 1;
    case 1: r += 10;
        break;
    case 2: r += 100;
    }
    return r;
}
int main(void)
{
    int i, s = 0;
    for (i = 0; i < 10; i++) s += i;
    ASSERT(45, s);
    s = 0; i = 0;
    while (i < 5) { s += i; i++; }
    ASSERT(10, s);
    s = 0; i = 0;
    do { s++; } while (++i < 7);
    ASSERT(7, s);
    s = 0;
    for (i = 0; i < 100; i++) { if (i == 10) break; if (i % 2) continue; s += i; }
    ASSERT(20, s);
    ASSERT(55, fib(10));
    ASSERT(10, sw(1)); ASSERT(20, sw(3)); ASSERT(50, sw(7)); ASSERT(-1, sw(4));
    ASSERT(11, fallthrough(0)); ASSERT(10, fallthrough(1)); ASSERT(100, fallthrough(2));
    i = 0;
again:
    i++;
    if (i < 5) goto again;
    ASSERT(5, i);
    int n = 0;
    for (int a = 0; a < 3; a++) for (int b = 0; b < 4; b++) n++;
    ASSERT(12, n);
    ASSERT(3, ({ int t = 1; t + 2; }));
    return g_fail;
}
