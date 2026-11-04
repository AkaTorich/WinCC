// CPU faults as C signals: SIGFPE (integer division by zero), SIGSEGV
// (a bad pointer), SIGILL (an invalid instruction), caught by handlers
// that longjmp back. (A handler that returns ends the program.)
#include "test.h"
#include <signal.h>
#include <setjmp.h>

static jmp_buf env;
static int g_caught;
int g_zero;                                     // in memory, so nothing is worked out ahead

static void handler(int sig)
{
    g_caught = sig;
    longjmp(env, sig);
}

int main(void)
{
    signal(SIGFPE, handler);
    int r = setjmp(env);
    if (r == 0) {
        int x = 7 / g_zero;
        ASSERT(0, x + 1);                       // not reached
    }
    ASSERT(SIGFPE, r);
    ASSERT(SIGFPE, g_caught);
    // the handler was reset to the default: catch again explicitly
    ASSERT(1, signal(SIGFPE, handler) == SIG_DFL);
    r = setjmp(env);
    if (r == 0)
        g_caught = 100 % g_zero;
    ASSERT(SIGFPE, r);
    signal(SIGSEGV, handler);
    r = setjmp(env);
    if (r == 0) {
        int *p = (int *)16;
        *p = 1;
    }
    ASSERT(SIGSEGV, r);
    signal(SIGILL, handler);
    r = setjmp(env);
    if (r == 0)
        asm("db 0x0F, 0x0B");                   // ud2
    ASSERT(SIGILL, r);
    // the program goes on normally afterwards
    int sum = 0;
    for (int i = 1; i <= 10; i++) sum += i;
    ASSERT(55, sum);
    return g_fail;
}
