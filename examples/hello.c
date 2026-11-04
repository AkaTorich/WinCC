#include <stdio.h>

int main(int argc, char **argv)
{
    printf("Привет, мир! Это WinCC.\n");
    for (int i = 0; i < argc; i++)
        printf("  argv[%d] = %s\n", i, argv[i]);
    long x = -5;
    printf("long: %ld, размер %d байта\n", x, (int)sizeof(long));
    return 0;
}
