#include "test.h"
int g_value;
void set_value(void) { asm("mov dword ptr [rip+g_value], 1234"); }
long get_tsc(void)
{
    asm("rdtsc\n"
        "shl rdx, 32\n"
        "or rax, rdx\n"
        "jmp .L.return.get_tsc");
    return 0;
}
int main(void)
{
    set_value();
    ASSERT(1234, g_value);
    long a = get_tsc();
    long b = get_tsc();
    ASSERT(1, b > a);
    return g_fail;
}
