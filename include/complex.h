// complex.h as in Microsoft C: a complex number is a structure of two parts,
// made by _Cbuild (the language's _Complex is not here: __STDC_NO_COMPLEX__).
//
//   _Dcomplex z = _Cbuild(1, 2), w = csqrt(z);
//   printf("%g%+gi\n", creal(w), cimag(w));
//   _Dcomplex p = _Cmulcc(z, w);           // z * w
#pragma once

typedef struct _C_double_complex { double _Val[2]; } _Dcomplex;
typedef struct _C_float_complex { float _Val[2]; } _Fcomplex;
typedef _Dcomplex _Lcomplex;                // long double is double here

#define _Complex_I _FCbuild(0.0f, 1.0f)
#define I _Complex_I

_Dcomplex _Cbuild(double re, double im);
_Fcomplex _FCbuild(float re, float im);
#define _LCbuild _Cbuild
_Dcomplex _Cmulcc(_Dcomplex a, _Dcomplex b);
_Dcomplex _Cmulcr(_Dcomplex a, double b);
// not in Microsoft's: the sum, the difference and the quotient
_Dcomplex _Caddcc(_Dcomplex a, _Dcomplex b);
_Dcomplex _Csubcc(_Dcomplex a, _Dcomplex b);
_Dcomplex _Cdivcc(_Dcomplex a, _Dcomplex b);
_Fcomplex _FCmulcc(_Fcomplex a, _Fcomplex b);
_Fcomplex _FCmulcr(_Fcomplex a, float b);

double creal(_Dcomplex z);
double cimag(_Dcomplex z);
double cabs(_Dcomplex z);
double carg(_Dcomplex z);
_Dcomplex conj(_Dcomplex z);
_Dcomplex cproj(_Dcomplex z);
_Dcomplex cexp(_Dcomplex z);
_Dcomplex clog(_Dcomplex z);
_Dcomplex clog10(_Dcomplex z);
_Dcomplex csqrt(_Dcomplex z);
_Dcomplex cpow(_Dcomplex a, _Dcomplex b);
_Dcomplex csin(_Dcomplex z);
_Dcomplex ccos(_Dcomplex z);
_Dcomplex ctan(_Dcomplex z);
_Dcomplex csinh(_Dcomplex z);
_Dcomplex ccosh(_Dcomplex z);
_Dcomplex ctanh(_Dcomplex z);
_Dcomplex casin(_Dcomplex z);
_Dcomplex cacos(_Dcomplex z);
_Dcomplex catan(_Dcomplex z);
_Dcomplex casinh(_Dcomplex z);
_Dcomplex cacosh(_Dcomplex z);
_Dcomplex catanh(_Dcomplex z);

float crealf(_Fcomplex z);
float cimagf(_Fcomplex z);
float cabsf(_Fcomplex z);
float cargf(_Fcomplex z);
_Fcomplex conjf(_Fcomplex z);
_Fcomplex cexpf(_Fcomplex z);
_Fcomplex clogf(_Fcomplex z);
_Fcomplex csqrtf(_Fcomplex z);
_Fcomplex cpowf(_Fcomplex a, _Fcomplex b);
_Fcomplex csinf(_Fcomplex z);
_Fcomplex ccosf(_Fcomplex z);

#define creall creal
#define cimagl cimag
#define cabsl cabs
#define cargl carg
#define conjl conj
#define cexpl cexp
#define clogl clog
#define csqrtl csqrt
#define cpowl cpow
