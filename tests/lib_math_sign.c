// math.h: FP_ILOGB0 / FP_ILOGBNAN are what ilogb gives; signgam after lgamma
#include "test.h"
#include <math.h>
#include <limits.h>

int main(void)
{
    ASSERT(FP_ILOGB0, ilogb(0.0));
    ASSERT(FP_ILOGB0, ilogb(-0.0));
    ASSERT(FP_ILOGBNAN, ilogb(NAN));
    ASSERT(INT_MIN, FP_ILOGB0);
    ASSERT(INT_MAX, ilogb(INFINITY));
    ASSERT(3, ilogb(8.5));
    ASSERT(-1074, ilogb(4.9406564584124654e-324));
    // signgam: the sign of Gamma(x)
    ASSERT(0, signgam);                     // nothing computed yet
    lgamma(2.5);
    ASSERT(1, signgam);
    lgamma(-0.5);                           // Gamma(-0.5) = -2 sqrt(pi)
    ASSERT(-1, signgam);
    lgamma(-1.5);                           // Gamma(-1.5) = 4/3 sqrt(pi)
    ASSERT(1, signgam);
    lgamma(-2.5);
    ASSERT(-1, signgam);
    lgamma(0.25);
    ASSERT(1, signgam);
    lgamma(-0.0);
    ASSERT(-1, signgam);
    lgamma(0.0);
    ASSERT(1, signgam);
    lgamma(-3.0);                           // a pole
    ASSERT(1, signgam);
    lgamma(100.0);
    ASSERT(1, signgam);
    // lgamma's value with the sign gives Gamma back
    double x = -2.3, g = tgamma(x);
    double lg = lgamma(x);
    double back = signgam * exp(lg);
    ASSERT(1, fabs(back - g) < 1e-12 * fabs(g));
    ASSERT(1, signgam == (g < 0 ? -1 : 1));
    return g_fail;
}
