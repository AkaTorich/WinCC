// exit() from deep inside: atexit functions run in reverse order, then the
// files still open are written. lib_exit_b.c reads what this one leaves.
#include <stdio.h>
#include <stdlib.h>

static FILE *log_file;

static void first(void) { fputs("first\n", log_file); }
static void second(void) { fputs("second\n", log_file); }

static void leave(int code)
{
    fputs("leave\n", log_file);
    exit(code);
}

int main(void)
{
    log_file = fopen("cctest exit log.txt", "w");    // never closed: exit writes it
    if (!log_file)
        return 1;
    if (atexit(first) || atexit(second))
        return 2;
    fputs("main\n", log_file);
    leave(0);
    fputs("after exit\n", log_file);
    return 3;
}
