// setjmp / longjmp in programs
#include "test.h"
#include <setjmp.h>
#include <stdlib.h>
#include <signal.h>

static jmp_buf env, env2;

static void deep(int n)
{
    char pad[200];
    pad[0] = (char)n;
    if (n == 0)
        longjmp(env, 42);
    deep(n - 1);
    g_fail += pad[0] != (char)n;                 // never reached
}

static int cmp_jump(const void *a, const void *b)
{
    (void)a; (void)b;
    longjmp(env2, 7);                            // straight out of qsort
    return 0;
}

static long sum8(long a, long b, long c, long d, long e, long f, long g, long h)
{
    return a + b + c + d + e + f + g + h;
}


static void on_abort(int sig)
{
    longjmp(env, sig);
}

int main(void)
{
    int count = 0;
    int r = setjmp(env);
    count++;
    if (r == 0) {
        deep(20);
        ASSERT(0, 1);                            // not here
    }
    ASSERT(42, r);
    ASSERT(2, count);
    // longjmp(env, 0) gives 1
    r = setjmp(env);
    if (r == 0)
        longjmp(env, 0);
    ASSERT(1, r);
    // local variables keep what they were given before the jump
    int x = 5;
    if (!setjmp(env)) {
        x = 7;
        longjmp(env, 1);
    }
    ASSERT(7, x);
    // setjmp inside an expression
    int y = 10 + setjmp(env);
    if (y == 10)
        longjmp(env, 5);
    ASSERT(15, y);
    // out of a comparator called by the library
    int v[] = { 3, 2, 1 };
    if ((r = setjmp(env2)) == 0)
        qsort(v, 3, sizeof(int), cmp_jump);
    ASSERT(7, r);
    // a loop around setjmp
    int rounds = 0;
    r = setjmp(env);
    if (r < 3) {
        rounds++;
        longjmp(env, r + 1);
    }
    ASSERT(3, rounds);
    // abort() runs a SIGABRT handler, which may jump away
    signal(SIGABRT, on_abort);
    r = setjmp(env);
    if (r == 0)
        abort();
    ASSERT(SIGABRT, r);
    // deep inside a big expression: some 110 temporaries wait on the stack
    // around setjmp (more than 800 bytes) and must be there after longjmp
    static int jumped;
    // ((setjmp + 1) + 1) ... : each 1 is pushed before the inner part is worked out
    long big = sum8(((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((
        setjmp(env)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1)
         + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1) + 1),
        2, 3, 4, 5, 6, 7, 8);
    if (!jumped) {
        jumped = 1;
        longjmp(env, 9);
    }
    ASSERT(9 + 100 + 35, big);
    return g_fail;
}
