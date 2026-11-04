// x87 in the assembler: fld/fstp of every size, fsqrt, fldpi, fsincos, fild/fistp, arithmetic, fcomi.
// Arguments and results go through the home slots [rbp+16], [rbp+24] (the caller always gives them).
#include "test.h"

double sqrt(double x);

static double x87_sqrt(double x)
{
    asm("fld qword ptr [rbp+16]\n"
        "fsqrt\n"
        "fstp qword ptr [rbp+16]\n"
        "movsd xmm0, qword ptr [rbp+16]\n"
        "jmp .L.return.x87_sqrt");
    return 0;
}

static double x87_pi(void)
{
    asm("fldpi\n"
        "fstp qword ptr [rbp+16]\n"
        "movsd xmm0, qword ptr [rbp+16]\n"
        "jmp .L.return.x87_pi");
    return 0;
}

// sin(x)*sin(x) + cos(x)*cos(x) through fsincos, fmul, fxch, faddp: must be 1
static double x87_one(double x)
{
    asm("fld qword ptr [rbp+16]\n"
        "fsincos\n"                     // st0 = cos, st1 = sin
        "fmul st, st(0)\n"
        "fxch\n"
        "fmul st, st(0)\n"
        "faddp\n"
        "fstp qword ptr [rbp+16]\n"
        "movsd xmm0, qword ptr [rbp+16]\n"
        "jmp .L.return.x87_one");
    return 0;
}

// integers through the x87: fild dword, fild word, fmulp, fistp qword
static long long x87_mul(int a, short b)
{
    asm("fild dword ptr [rbp+16]\n"
        "fild word ptr [rbp+24]\n"
        "fmulp\n"
        "fistp qword ptr [rbp+16]\n"
        "mov rax, qword ptr [rbp+16]\n"
        "jmp .L.return.x87_mul");
    return 0;
}

// fcomi: 1 if a < b
static int x87_less(double a, double b)
{
    asm("fld qword ptr [rbp+24]\n"      // st0 = b
        "fld qword ptr [rbp+16]\n"      // st0 = a, st1 = b
        "fcomi st, st(1)\n"
        "fstp st(0)\n"
        "fstp st(0)\n"
        "setb al\n"
        "movzx eax, al\n"
        "jmp .L.return.x87_less");
    return 0;
}

// 80 bits out and back keep the value; fchs, fabs
static double x87_tbyte(double x, double room1, double room2)
{
    asm("fld qword ptr [rbp+16]\n"
        "fstp tbyte ptr [rbp+24]\n"     // 10 bytes over the next two home slots
        "fld tbyte ptr [rbp+24]\n"
        "fchs\n"
        "fabs\n"
        "fstp qword ptr [rbp+16]\n"
        "movsd xmm0, qword ptr [rbp+16]\n"
        "jmp .L.return.x87_tbyte");
    return 0;
}

int main(void)
{
    ASSERT(1, x87_sqrt(2.0) == sqrt(2.0));
    ASSERT(1, x87_sqrt(81.0) == 9.0);
    ASSERT(1, x87_pi() == 3.141592653589793);
    double one = x87_one(0.7);
    ASSERT(1, one > 0.999999999 && one < 1.000000001);
    ASSERT(-42, x87_mul(-6, 7));
    ASSERT(1, x87_less(1.5, 2.5));
    ASSERT(0, x87_less(3.0, 2.5));
    ASSERT(1, x87_tbyte(-1234.5, 0, 0) == 1234.5);
    return g_fail;
}
