// What lib_exit_a.c left behind, and exit codes from main's own return.
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_ran;
static void mark(void) { g_ran++; }

int main(void)
{
    char buf[128];
    FILE *f = fopen("cctest exit log.txt", "r");
    ASSERT(1, f != 0);
    if (!f)
        return g_fail;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    ASSERT(0, strcmp(buf, "main\nleave\nsecond\nfirst\n"));
    // more than one registration of the same function counts each time
    ASSERT(0, atexit(mark));
    ASSERT(0, atexit(mark));
    ASSERT(0, g_ran);
    ASSERT(0, EXIT_SUCCESS);
    ASSERT(1, EXIT_FAILURE);
    return g_fail;
}
