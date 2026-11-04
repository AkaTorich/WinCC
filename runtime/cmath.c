// complex.h as in Microsoft C (_Dcomplex: two doubles, _Cbuild makes one) and
// fenv.h: the rounding and the exceptions of floating point (SSE's MXCSR and x87's
// control word, both kept the same).
#include "rtwin.h"

typedef struct { double re, im; } Dc;          // _Dcomplex: _Val[0] the real part, _Val[1] the imaginary
typedef struct { float re, im; } Fc;            // _Fcomplex

double rt_sqrt(double x);
double rt_exp(double x);
double rt_log(double x);
double rt_sin(double x);
double rt_cos(double x);
double rt_sinh(double x);
double rt_cosh(double x);
double rt_atan2(double y, double x);
double rt_hypot(double x, double y);
double rt_fabs(double x);
double rt_copysign(double x, double y);
int rt_isinf(double x);
int rt_isnan(double x);

static Dc mk(double re, double im) { Dc z; z.re = re; z.im = im; return z; }

Dc rt__Cbuild(double re, double im) { return mk(re, im); }
double rt_creal(Dc z) { return z.re; }
double rt_cimag(Dc z) { return z.im; }
double rt_cabs(Dc z) { return rt_hypot(z.re, z.im); }
double rt_carg(Dc z) { return rt_atan2(z.im, z.re); }
Dc rt_conj(Dc z) { return mk(z.re, -z.im); }
Dc rt_cproj(Dc z)
{
    if (rt_isinf(z.re) || rt_isinf(z.im))
        return mk(rt_exp(1e300), rt_copysign(0.0, z.im));       // infinity
    return z;
}

Dc rt__Cmulcc(Dc a, Dc b) { return mk(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
Dc rt__Cmulcr(Dc a, double b) { return mk(a.re * b, a.im * b); }
Dc rt__Caddcc(Dc a, Dc b) { return mk(a.re + b.re, a.im + b.im); }
Dc rt__Csubcc(Dc a, Dc b) { return mk(a.re - b.re, a.im - b.im); }

// a / b, scaled so that big parts do not overflow (Smith's way)
Dc rt__Cdivcc(Dc a, Dc b)
{
    if (rt_fabs(b.re) >= rt_fabs(b.im)) {
        double r = b.im / b.re, d = b.re + b.im * r;
        return mk((a.re + a.im * r) / d, (a.im - a.re * r) / d);
    }
    double r = b.re / b.im, d = b.re * r + b.im;
    return mk((a.re * r + a.im) / d, (a.im * r - a.re) / d);
}

Dc rt_cexp(Dc z)
{
    double m = rt_exp(z.re);
    return mk(m * rt_cos(z.im), m * rt_sin(z.im));
}

Dc rt_clog(Dc z) { return mk(rt_log(rt_cabs(z)), rt_carg(z)); }
Dc rt_clog10(Dc z) { Dc l = rt_clog(z); return mk(l.re / 2.302585092994045684, l.im / 2.302585092994045684); }

Dc rt_csqrt(Dc z)
{
    if (z.re == 0 && z.im == 0)
        return mk(0, z.im);
    double t = rt_sqrt((rt_fabs(z.re) + rt_cabs(z)) / 2);
    if (z.re >= 0)
        return mk(t, z.im / (2 * t));
    return mk(rt_fabs(z.im) / (2 * t), rt_copysign(t, z.im));
}

Dc rt_cpow(Dc a, Dc b)
{
    if (a.re == 0 && a.im == 0)
        return b.re == 0 && b.im == 0 ? mk(1, 0) : mk(0, 0);
    return rt_cexp(rt__Cmulcc(b, rt_clog(a)));
}

Dc rt_csin(Dc z) { return mk(rt_sin(z.re) * rt_cosh(z.im), rt_cos(z.re) * rt_sinh(z.im)); }
Dc rt_ccos(Dc z) { return mk(rt_cos(z.re) * rt_cosh(z.im), -rt_sin(z.re) * rt_sinh(z.im)); }
Dc rt_ctan(Dc z) { return rt__Cdivcc(rt_csin(z), rt_ccos(z)); }
Dc rt_csinh(Dc z) { return mk(rt_sinh(z.re) * rt_cos(z.im), rt_cosh(z.re) * rt_sin(z.im)); }
Dc rt_ccosh(Dc z) { return mk(rt_cosh(z.re) * rt_cos(z.im), rt_sinh(z.re) * rt_sin(z.im)); }
Dc rt_ctanh(Dc z) { return rt__Cdivcc(rt_csinh(z), rt_ccosh(z)); }

// asin z = -i log(iz + sqrt(1 - z^2)), and the others the same way
Dc rt_casin(Dc z)
{
    Dc w = rt_csqrt(mk(1 - (z.re * z.re - z.im * z.im), -2 * z.re * z.im));
    Dc l = rt_clog(mk(w.re - z.im, w.im + z.re));
    return mk(l.im, -l.re);
}

Dc rt_cacos(Dc z)
{
    Dc a = rt_casin(z);
    return mk(1.57079632679489661923 - a.re, -a.im);
}

Dc rt_catan(Dc z)       // (i/2) (log(1 - iz) - log(1 + iz))
{
    Dc a = rt_clog(mk(1 + z.im, -z.re)), b = rt_clog(mk(1 - z.im, z.re));
    return mk(-(a.im - b.im) / 2, (a.re - b.re) / 2);
}

Dc rt_casinh(Dc z)      // -i asin(iz)
{
    Dc a = rt_casin(mk(-z.im, z.re));
    return mk(a.im, -a.re);
}

Dc rt_cacosh(Dc z)      // log(z + sqrt(z + 1) sqrt(z - 1))
{
    Dc p = rt__Cmulcc(rt_csqrt(mk(z.re + 1, z.im)), rt_csqrt(mk(z.re - 1, z.im)));
    return rt_clog(mk(z.re + p.re, z.im + p.im));
}

Dc rt_catanh(Dc z)      // -i atan(iz)
{
    Dc a = rt_catan(mk(-z.im, z.re));
    return mk(a.im, -a.re);
}

// the float ones: through double
static Fc fc(Dc z) { Fc r; r.re = (float)z.re; r.im = (float)z.im; return r; }
static Dc dc(Fc z) { return mk(z.re, z.im); }
Fc rt__FCbuild(float re, float im) { Fc z; z.re = re; z.im = im; return z; }
float rt_crealf(Fc z) { return z.re; }
float rt_cimagf(Fc z) { return z.im; }
float rt_cabsf(Fc z) { return (float)rt_cabs(dc(z)); }
float rt_cargf(Fc z) { return (float)rt_carg(dc(z)); }
Fc rt_conjf(Fc z) { z.im = -z.im; return z; }
Fc rt__FCmulcc(Fc a, Fc b) { return fc(rt__Cmulcc(dc(a), dc(b))); }
Fc rt__FCmulcr(Fc a, float b) { return fc(rt__Cmulcr(dc(a), b)); }
Fc rt_cexpf(Fc z) { return fc(rt_cexp(dc(z))); }
Fc rt_clogf(Fc z) { return fc(rt_clog(dc(z))); }
Fc rt_csqrtf(Fc z) { return fc(rt_csqrt(dc(z))); }
Fc rt_cpowf(Fc a, Fc b) { return fc(rt_cpow(dc(a), dc(b))); }
Fc rt_csinf(Fc z) { return fc(rt_csin(dc(z))); }
Fc rt_ccosf(Fc z) { return fc(rt_ccos(dc(z))); }

// ---- fenv.h ----
// The flags as Microsoft's: FE_INEXACT 1, FE_UNDERFLOW 2, FE_OVERFLOW 4, FE_DIVBYZERO 8, FE_INVALID 0x10;
// the rounding: FE_TONEAREST 0, FE_DOWNWARD 0x100, FE_UPWARD 0x200, FE_TOWARDZERO 0x300.

typedef struct { unsigned long ctl, stat; } Fenv;

static unsigned get_mxcsr(void)
{
    unsigned m = 0;
    asm("stmxcsr dword ptr @m");
    return m;
}

static void set_mxcsr(unsigned m)
{
    asm("ldmxcsr dword ptr @m");
}

static unsigned short get_cw(void)
{
    unsigned short cw = 0;
    asm("fnstcw word ptr @cw");
    return cw;
}

static void set_cw(unsigned short cw)
{
    asm("fldcw word ptr @cw");
}

// MXCSR's flags (IE 1, DE 2, ZE 4, OE 8, UE 0x10, PE 0x20) <-> fenv's
static unsigned to_fe(unsigned m)
{
    return (m & 0x20 ? 1 : 0) | (m & 0x10 ? 2 : 0) | (m & 0x08 ? 4 : 0) | (m & 0x04 ? 8 : 0) | (m & 0x01 ? 0x10 : 0);
}

static unsigned to_mx(unsigned f)
{
    return (f & 1 ? 0x20 : 0) | (f & 2 ? 0x10 : 0) | (f & 4 ? 0x08 : 0) | (f & 8 ? 0x04 : 0) | (f & 0x10 ? 0x01 : 0);
}

int rt_feclearexcept(int e)
{
    set_mxcsr(get_mxcsr() & ~to_mx((unsigned)e));
    asm("fnclex");
    return 0;
}

int rt_fetestexcept(int e)
{
    unsigned short sw = 0;
    asm("fnstsw word ptr @sw");
    unsigned x87 = (sw & 0x20 ? 1 : 0) | (sw & 0x10 ? 2 : 0) | (sw & 0x08 ? 4 : 0) | (sw & 0x04 ? 8 : 0) | (sw & 0x01 ? 0x10 : 0);
    return (int)((to_fe(get_mxcsr()) | x87) & (unsigned)e & 0x1F);
}

int rt_feraiseexcept(int e)
{
    set_mxcsr(get_mxcsr() | to_mx((unsigned)e));
    return 0;
}

int rt_fegetround(void)
{
    return (int)(((get_mxcsr() >> 13) & 3) << 8);
}

int rt_fesetround(int r)
{
    if (r & ~0x300) return 1;
    unsigned rc = (unsigned)r >> 8;
    set_mxcsr((get_mxcsr() & ~0x6000u) | (rc << 13));
    set_cw((unsigned short)((get_cw() & ~0xC00) | (rc << 10)));
    return 0;
}

int rt_fegetenv(Fenv *env)
{
    env->ctl = get_mxcsr() & ~0x3Fu;
    env->stat = to_fe(get_mxcsr());
    return 0;
}

int rt_fesetenv(const Fenv *env)
{
    if ((u64)env == 0) return 1;
    set_mxcsr((unsigned)(env->ctl & ~0x3Fu) | to_mx((unsigned)env->stat));
    set_cw((unsigned short)((get_cw() & ~0xC00) | (((env->ctl >> 13) & 3) << 10)));
    return 0;
}

int rt_feholdexcept(Fenv *env)
{
    rt_fegetenv(env);
    set_mxcsr((get_mxcsr() & ~0x3Fu) | 0x1F80);     // flags cleared, every exception masked
    return 0;
}

int rt_feupdateenv(const Fenv *env)
{
    unsigned raised = to_fe(get_mxcsr());
    rt_fesetenv(env);
    rt_feraiseexcept((int)raised);
    return 0;
}

int rt_fegetexceptflag(unsigned long *flag, int e)
{
    *flag = (unsigned long)rt_fetestexcept(e);
    return 0;
}

int rt_fesetexceptflag(const unsigned long *flag, int e)
{
    unsigned m = get_mxcsr() & ~to_mx((unsigned)e);
    set_mxcsr(m | to_mx((unsigned)(*flag & (unsigned long)e)));
    return 0;
}

#define F(name, fn) ".alias " name ", " #fn "\n"
asm(
    F("_Cbuild", rt__Cbuild) F("creal", rt_creal) F("cimag", rt_cimag) F("cabs", rt_cabs) F("carg", rt_carg)
    F("conj", rt_conj) F("cproj", rt_cproj) F("_Cmulcc", rt__Cmulcc) F("_Cmulcr", rt__Cmulcr)
    F("_Caddcc", rt__Caddcc) F("_Csubcc", rt__Csubcc) F("_Cdivcc", rt__Cdivcc)
    F("cexp", rt_cexp) F("clog", rt_clog) F("clog10", rt_clog10) F("csqrt", rt_csqrt) F("cpow", rt_cpow)
    F("csin", rt_csin) F("ccos", rt_ccos) F("ctan", rt_ctan) F("csinh", rt_csinh) F("ccosh", rt_ccosh)
    F("ctanh", rt_ctanh) F("casin", rt_casin) F("cacos", rt_cacos) F("catan", rt_catan)
    F("casinh", rt_casinh) F("cacosh", rt_cacosh) F("catanh", rt_catanh)
    F("_FCbuild", rt__FCbuild) F("crealf", rt_crealf) F("cimagf", rt_cimagf) F("cabsf", rt_cabsf)
    F("cargf", rt_cargf) F("conjf", rt_conjf) F("_FCmulcc", rt__FCmulcc) F("_FCmulcr", rt__FCmulcr)
    F("cexpf", rt_cexpf) F("clogf", rt_clogf) F("csqrtf", rt_csqrtf) F("cpowf", rt_cpowf)
    F("csinf", rt_csinf) F("ccosf", rt_ccosf)
    F("feclearexcept", rt_feclearexcept) F("fetestexcept", rt_fetestexcept) F("feraiseexcept", rt_feraiseexcept)
    F("fegetround", rt_fegetround) F("fesetround", rt_fesetround) F("fegetenv", rt_fegetenv)
    F("fesetenv", rt_fesetenv) F("feholdexcept", rt_feholdexcept) F("feupdateenv", rt_feupdateenv)
    F("fegetexceptflag", rt_fegetexceptflag) F("fesetexceptflag", rt_fesetexceptflag)
);
#undef F
