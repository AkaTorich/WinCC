// Локальный union, заданный выражением того же типа (union u = other;), и union-поле структуры
// из выражения: копируется целиком (раньше разбор падал с «ожидалось ','»).
#include "test.h"

typedef union { unsigned char b[16]; unsigned int w[4]; } V;
typedef struct { int tag; V v; } S;

static V make(unsigned x) { V r; for (int i = 0; i < 4; i++) r.w[i] = x + (unsigned)i; return r; }

static long long run(void)
{
    V a = make(10);
    V b = a;                       // из переменной
    V c = make(100);               // из вызова
    const V* pa = &a;
    V d = *pa;                     // из разыменования
    S s = { 7, c };                // поле-union из выражения
    b.w[0] += 1;
    return (long long)a.w[0] + b.w[0] + c.w[3] + d.w[1] + s.v.w[2] + s.tag;
}

int main(void)
{
    ASSERT(10 + 11 + 103 + 11 + 102 + 7, run());
    return g_fail;
}
