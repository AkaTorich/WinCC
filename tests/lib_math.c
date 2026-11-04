// math.h against known values; the expected numbers go through strtod,
// which rounds correctly, so the tolerances are in units in the last place
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <errno.h>

static void near(const char *want_text, double got, double ulps, const char *code, int line)
{
    double want = strtod(want_text, 0);
    double err = got - want;
    if (err < 0) err = -err;
    double scale = want < 0 ? -want : want;
    if (scale < DBL_MIN) scale = DBL_MIN;
    if (!(err <= ulps * DBL_EPSILON * scale)) {
        printf("\n    строка %d: %s = %.17g, ожидалось %s (%.1f ulp)", line, code, got, want_text,
               err / (DBL_EPSILON * scale));
        g_fail++;
    }
}

// within 1.5 units in the last place
#define NEAR(want, expr) near(want, expr, 1.5, #expr, __LINE__)
#define NEARU(want, expr, u) near(want, expr, u, #expr, __LINE__)

static void same(double want, double got, const char *code, int line)
{
    if (!(want == got) || signbit(want) != signbit(got)) {
        printf("\n    строка %d: %s = %.17g, ожидалось %.17g", line, code, got, want);
        g_fail++;
    }
}

#define SAME(want, expr) same(want, expr, #expr, __LINE__)

int main(void)
{
    // trigonometry
    NEAR("0.841470984807896506652502321630", sin(1.0));
    NEAR("0.540302305868139717400936607443", cos(1.0));
    NEAR("1.557407724654902230506974807458", tan(1.0));
    NEAR("0.479425538604203000273287935216", sin(0.5));
    NEAR("0.877582561890372716116281582604", cos(0.5));
    NEAR("-0.506365641109758793656557610459", sin(100.0));
    NEAR("0.862318872287683934105636895777", cos(100.0));
    NEAR("-0.349993502171292952117652486780771", sin(1e6));
    NEAR("-0.8522008497671888017727", sin(1e22));      // needs the exact reduction
    NEAR("0.5232147853951389454975944733847", cos(1e22));
    NEAR("1.2246467991473531772e-16", sin(M_PI));
    NEAR("-1.0", cos(M_PI));
    SAME(0.0, sin(0.0));
    SAME(-0.0, sin(-0.0));
    SAME(1.0, cos(0.0));
    NEARU("1.0", tan(M_PI_4), 2);
    NEAR("0.785398163397448309615660845819875721", atan(1.0));
    NEAR("0.463647609000806116214256231461214402", atan(0.5));
    NEAR("1.107148717794090503017065460178537040", atan(2.0));
    NEAR("-1.5707963267948966192313216916397514", atan(-1e300));
    NEAR("2.356194490192344928846982537459627163", atan2(1.0, -1.0));
    NEAR("-2.356194490192344928846982537459627163", atan2(-1.0, -1.0));
    NEAR("1.5707963267948966192313216916397514", atan2(1.0, 0.0));
    NEAR("3.1415926535897932384626433832795029", atan2(0.0, -1.0));
    SAME(-0.0, atan2(-0.0, 1.0));
    NEAR("0.523598775598298873077107230546583814", asin(0.5));
    NEAR("1.047197551196597746154214461093167628", acos(0.5));
    NEAR("1.5707963267948966192313216916397514", asin(1.0));
    NEAR("3.1415926535897932384626433832795029", acos(-1.0));
    SAME(0.0, acos(1.0));
    errno = 0;
    ASSERT(1, isnan(asin(2.0)));
    ASSERT(EDOM, errno);
    // sin^2 + cos^2 = 1 at any size, and inverse functions
    srand(3);
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        double x = (rand() - 16384) / 16.0 * pow(10.0, rand() % 40 - 10);
        double s = sin(x), c = cos(x);
        if (fabs(s * s + c * c - 1.0) > 4e-16) bad++;
        double t = (rand() - 16384) / 10000.0;
        if (fabs(atan(tan(t)) - t) > 4e-16 * (fabs(t) + 1e-300) && fabs(t) < 1.5) bad++;
        double a = (rand() - 16384) / 16384.0;
        if (fabs(sin(asin(a)) - a) > 4e-16) bad++;
    }
    ASSERT(0, bad);
    // exponents and logarithms
    NEAR("2.718281828459045235360287471352662498", exp(1.0));
    NEAR("0.367879441171442321595523770161460867", exp(-1.0));
    NEAR("22026.46579480671651695790064528424436", exp(10.0));
    NEAR("1.648721270700128146848650787814163572", exp(0.5));
    NEAR("8.2184074615549721892413723865e307", exp(709.0));
    SAME(1.0, exp(0.0));
    ASSERT(1, exp(-746.0) == 0.0);
    ASSERT(1, isinf(exp(710.0)));
    NEAR("0.693147180559945309417232121458176568", log(2.0));
    NEAR("2.302585092994045684017991454684364208", log(10.0));
    NEAR("-690.7755278982137052053974364053092", log(1e-300));
    NEAR("-744.44007192138126231410729844608", log(DBL_TRUE_MIN));
    SAME(0.0, log(1.0));
    ASSERT(1, log(0.0) < -1e308);
    ASSERT(1, isnan(log(-1.0)));
    SAME(3.0, log2(8.0));
    SAME(-1074.0, log2(DBL_TRUE_MIN));
    NEAR("3.321928094887362347870319429489390175", log2(10.0));
    SAME(3.0, log10(1000.0));
    SAME(22.0, log10(1e22));
    NEAR("0.301029995663981195213738894724493027", log10(2.0));
    NEAR("9.9999999995000000000333333333308333e-11", log1p(1e-10));
    NEAR("0.693147180559945309417232121458176568", log1p(1.0));
    NEAR("1.00000000005000000000166666666670833e-10", expm1(1e-10));
    NEAR("1.718281828459045235360287471352662498", expm1(1.0));
    SAME(1024.0, exp2(10.0));
    NEAR("1.414213562373095048801688724209698079", exp2(0.5));
    // powers and roots
    SAME(1024.0, pow(2.0, 10.0));
    SAME(0.01, pow(10.0, -2.0));
    SAME(1e22, pow(10.0, 22.0));
    SAME(-8.0, pow(-2.0, 3.0));
    SAME(DBL_TRUE_MIN, pow(2.0, -1074.0));
    SAME(1.0, pow(NAN, 0.0));
    SAME(1.0, pow(1.0, NAN));
    ASSERT(1, isnan(pow(-8.0, 1.0 / 3)));
    ASSERT(1, isinf(pow(0.0, -1.0)));
    SAME(-0.0, pow(-0.0, 3.0));
    NEAR("1.414213562373095048801688724209698079", pow(2.0, 0.5));
    NEAR("2.75567596063107536047194458404", pow(1.5, 2.5));
    NEAR("31.6227766016837933199889354443271853", pow(10.0, 1.5));
    NEARU("1e308", pow(10.0, 308.0), 4);
    NEARU("1e-300", pow(10.0, -300.0), 4);
    NEARU("2.6881171418161354484126255515800135873611118773741922415191608615e43", exp(100.0), 2);
    NEAR("1.259921049894873164767210607278228350", cbrt(2.0));
    SAME(3.0, cbrt(27.0));
    SAME(-2.0, cbrt(-8.0));
    SAME(1.4142135623730951, sqrt(2.0));
    SAME(12.0, sqrt(144.0));
    errno = 0;
    ASSERT(1, isnan(sqrt(-1.0)));
    ASSERT(EDOM, errno);
    SAME(5.0, hypot(3.0, 4.0));
    NEARU("1.4142135623730950488", hypot(1e300, 1e300) / 1e300, 2);
    // hyperbolic
    NEAR("1.175201193643801456882381850595600815", sinh(1.0));
    NEAR("1.543080634815243778477905620757061682", cosh(1.0));
    NEAR("0.462117157260009758502318483643672548", tanh(0.5));
    NEAR("1e-10", sinh(1e-10));
    SAME(1.0, tanh(30.0));
    NEAR("0.881373587019543025232609324979792309", asinh(1.0));
    NEAR("1.316957896924816708625046347307968444", acosh(2.0));
    NEAR("0.549306144334054845697622618461262852", atanh(0.5));
    // special functions
    NEARU("0.842700792949714869341220635082609259", erf(1.0), 3);
    NEARU("0.520499877813046537682746653891964528", erf(0.5), 3);
    NEARU("2.209049699858544137277612958232037e-5", erfc(3.0), 16);
    SAME(24.0, tgamma(5.0));
    NEARU("1.772453850905516027298167483341145183", tgamma(0.5), 16);
    NEARU("2.363271801207354703064223311121526910", tgamma(-1.5), 16);
    NEARU("12.80182748008146961120771787456770736", lgamma(10.0), 16);
    NEARU("359.1342053695753987760440104602869", lgamma(100.0), 16);
    // rounding and pieces
    SAME(-3.0, floor(-2.5));
    SAME(-2.0, ceil(-2.5));
    SAME(-0.0, ceil(-0.5));
    SAME(3.0, round(2.5));
    SAME(-3.0, round(-2.5));
    SAME(0.0, round(0.49999999999999994));
    SAME(-2.0, trunc(-2.7));
    SAME(2.0, rint(2.5));
    SAME(4.0, rint(3.5));
    SAME(-0.0, nearbyint(-0.5));
    ASSERT(-3, lround(-2.5));
    ASSERT(4, llrint(3.5));
    SAME(1e300, floor(1e300));
    SAME(4503599627370497.0, floor(4503599627370497.0));
    double ip;
    SAME(0.25, modf(3.25, &ip));
    SAME(3.0, ip);
    SAME(-0.5, modf(-1.5, &ip));
    int ex;
    SAME(0.5, frexp(8.0, &ex));
    ASSERT(4, ex);
    SAME(0.5, frexp(DBL_TRUE_MIN, &ex));
    ASSERT(-1073, ex);
    SAME(8.0, ldexp(0.5, 4));
    SAME(DBL_TRUE_MIN, ldexp(1.0, -1074));
    ASSERT(1, isinf(ldexp(1.0, 1024)));
    SAME(1.5, fmod(5.5, 2.0));
    SAME(-1.5, fmod(-5.5, 2.0));
    SAME(0.0, fmod(6.0, 3.0));
    NEAR("0.09999999999999945", fmod(10.0, 0.1));
    SAME(1.0, fmod(1e300, 3.0) == 1.0 || fmod(1e300, 3.0) == 2.0 || fmod(1e300, 3.0) == 0.0);
    ASSERT(1, isnan(fmod(1.0, 0.0)));
    SAME(-0.5, remainder(5.5, 2.0));
    SAME(1.0, remainder(5.0, 2.0));
    SAME(-1.0, remainder(7.0, 2.0));
    int q;
    SAME(-1.0, remquo(7.0, 2.0, &q));
    ASSERT(4, q);
    SAME(1.0, remquo(-11.0, -3.0, &q) == 1.0);
    ASSERT(4, q);
    SAME(0.0, fabs(-0.0));
    SAME(-3.0, copysign(3.0, -0.0));
    SAME(1.0, fmin(1.0, NAN));
    SAME(2.0, fmax(NAN, 2.0));
    SAME(-1.0, fmin(-1.0, 2.0));
    SAME(2.0, fdim(5.0, 3.0));
    SAME(0.0, fdim(3.0, 5.0));
    SAME(10.0, fma(2.0, 3.0, 4.0));
    SAME(1.0 + DBL_EPSILON, nextafter(1.0, 2.0));
    SAME(DBL_TRUE_MIN, nextafter(0.0, 1.0));
    ASSERT(3, ilogb(8.0));
    SAME(-1022.0, logb(DBL_MIN));
    // classification
    ASSERT(1, isnan(NAN));
    ASSERT(0, isnan(1.0));
    ASSERT(1, isinf(INFINITY));
    ASSERT(1, isinf(-HUGE_VAL));
    ASSERT(1, isfinite(1.0));
    ASSERT(0, isfinite(INFINITY));
    ASSERT(0, isfinite(NAN));
    ASSERT(1, signbit(-1.0));
    ASSERT(0, signbit(NAN));
    ASSERT(FP_ZERO, fpclassify(0.0));
    ASSERT(FP_SUBNORMAL, fpclassify(DBL_MIN / 2));
    ASSERT(FP_NORMAL, fpclassify(1.0));
    ASSERT(FP_INFINITE, fpclassify(INFINITY));
    ASSERT(FP_NAN, fpclassify(NAN));
    ASSERT(1, isnormal(1.0));
    ASSERT(0, isless(NAN, 1.0));
    ASSERT(1, isunordered(NAN, 1.0));
    ASSERT(1, isnan(nan("")));
    // float versions
    ASSERT(1, sqrtf(4.0f) == 2.0f);
    ASSERT(1, sinf(0.5f) == (float)sin(0.5));
    ASSERT(1, powf(2.0f, 3.0f) == 8.0f);
    ASSERT(1, fabsf(-1.5f) == 1.5f);
    ASSERT(1, floorf(-1.5f) == -2.0f);
    ASSERT(1, atan2f(1.0f, 1.0f) == (float)(M_PI / 4));
    return g_fail;
}
