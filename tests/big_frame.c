// Кадр функции больше страницы: стек трогается постранично сверху вниз до вычитания (иначе в Windows
// под cctest прыжок через сторожевую страницу стека — нарушение доступа).
#include "test.h"

static int deep(int n)
{
    volatile unsigned char big[300000];
    big[0] = (unsigned char)n;
    big[sizeof big - 1] = (unsigned char)(n + 1);
    int r = big[0] + big[sizeof big - 1];
    if (n > 0) r += deep(n - 1);
    return r;
}

int main(void)
{
    ASSERT(2 * (0 + 1 + 2 + 3) + 4, deep(3));
    return g_fail;
}
