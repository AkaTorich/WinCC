// fenv.h: the floating-point environment -- how results are rounded and which
// exceptions (division by zero, overflow...) have happened. Kept in SSE's MXCSR
// (the double and float arithmetic) and the x87 control word together.
#pragma once

typedef unsigned long fexcept_t;
typedef struct { unsigned long _Fe_ctl, _Fe_stat; } fenv_t;

#define FE_INEXACT      0x01
#define FE_UNDERFLOW    0x02
#define FE_OVERFLOW     0x04
#define FE_DIVBYZERO    0x08
#define FE_INVALID      0x10
#define FE_ALL_EXCEPT   0x1F

#define FE_TONEAREST    0x000
#define FE_DOWNWARD     0x100
#define FE_UPWARD       0x200
#define FE_TOWARDZERO   0x300

#define FE_DFL_ENV      (&__wcc_fe_dfl_env)
static const fenv_t __wcc_fe_dfl_env = { 0x1F80, 0 };   // round to nearest, every exception masked

int feclearexcept(int excepts);
int fetestexcept(int excepts);
int feraiseexcept(int excepts);
int fegetexceptflag(fexcept_t *flag, int excepts);
int fesetexceptflag(const fexcept_t *flag, int excepts);
int fegetround(void);
int fesetround(int round);
int fegetenv(fenv_t *env);
int fesetenv(const fenv_t *env);
int feholdexcept(fenv_t *env);
int feupdateenv(const fenv_t *env);
