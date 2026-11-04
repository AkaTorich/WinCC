#include "test.h"
#include <math.h>
double half(double x) { return x / 2; }
float addf(float a, float b) { return a + b; }
double mix(int a, double b, float c, long d, double e) { return a + b + c + d + e; }
int main(void)
{
    double d = 3.5;
    ASSERT(3, (int)d);
    ASSERT(7, (int)(d * 2));
    ASSERT(1, 0.1 + 0.2 > 0.3);
    ASSERT(1, 1.5 < 2.5);
    ASSERT(0, 2.5 == 2.4);
    float f = 1.25f;
    ASSERT(5, (int)(f * 4));
    ASSERT(2, (int)half(4.0));
    ASSERT(3, (int)addf(1.5f, 1.5f));
    ASSERT(15, (int)mix(1, 2.0, 3.0f, 4, 5.0));
    ASSERT(12, (int)sqrt(144.0));
    ASSERT(-3, (int)-3.9);
    long long L = 10000000000LL;
    double dl = L;
    ASSERT(10000000000, (long long)dl);
    unsigned long long ul = 18446744073709551615ULL;
    ASSERT(1, (double)ul > 1e19);
    double arr[3] = { 1.5, 2.5, 3.0 };
    double s = 0;
    for (int i = 0; i < 3; i++) s += arr[i];
    ASSERT(7, (int)s);
    ASSERT(1, !0.0);
    ASSERT(0, !1.0);
    return g_fail;
}
