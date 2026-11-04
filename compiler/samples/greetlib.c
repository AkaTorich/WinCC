#include <stdio.h>
static const char *g_prefix = "[из DLL] ";
void greet(const char *who) { printf("%sпривет, %s!\n", g_prefix, who); }
int twice(int x) { return x * 2; }
