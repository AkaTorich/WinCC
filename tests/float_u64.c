// float/double -> unsigned long long for values from 2^63 up (cvttsd2si alone gives 0x8000000000000000)
#include "test.h"

int main(void)
{
    volatile double d1 = 1.6e19, d2 = 9223372036854775808.0, d3 = 12345.75, d4 = 18446744073709549568.0;
    volatile float f1 = 1.6e19f, f2 = 3.0e9f, f3 = 9.3e18f;
    ASSERT(16000000000000000000ull == (unsigned long long)d1, 1);
    ASSERT((unsigned long long)d1 == 16000000000000000000ull, 1);
    ASSERT(1, (unsigned long long)d2 == 9223372036854775808ull);
    ASSERT(12345, (long long)(unsigned long long)d3);
    ASSERT(1, (unsigned long long)d4 == 18446744073709549568ull);
    ASSERT(1, (unsigned long long)f1 == 15999999748907991040ull);
    ASSERT(3000000000ll, (long long)(unsigned long long)f2);
    ASSERT(1, (unsigned long long)f3 == 9300000300729368576ull);
    unsigned long long u = (unsigned long long)(d1 * 1.0);
    ASSERT(1, u / 1000000000000ull == 16000000ull);
    return g_fail;
}
