#include "test.h"
void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }
int sum(int *a, int n) { int s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }
int g_arr[5] = { 1, 2, 3, 4, 5 };
int *g_ptr = &g_arr[2];
char *g_str = "hello";
int main(void)
{
    int x = 3, y = 5;
    swap(&x, &y);
    ASSERT(5, x); ASSERT(3, y);
    int a[10];
    for (int i = 0; i < 10; i++) a[i] = i * i;
    ASSERT(81, a[9]);
    ASSERT(285, sum(a, 10));
    int *p = a + 3;
    ASSERT(9, *p);
    ASSERT(16, p[1]);
    ASSERT(3, p - a);
    ASSERT(4, *(a + 2));
    ASSERT(15, sum(g_arr, 5));
    ASSERT(3, *g_ptr);
    ASSERT('l', g_str[2]);
    int m[3][4];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) m[i][j] = i * 10 + j;
    ASSERT(23, m[2][3]);
    ASSERT(48, sizeof(m));
    ASSERT(16, sizeof(m[0]));
    int **pp = &p;
    ASSERT(9, **pp);
    char buf[8];
    char *s = "abc";
    int i = 0;
    while ((buf[i] = s[i])) i++;
    ASSERT(3, i);
    ASSERT('c', buf[2]);
    ASSERT(8, sizeof(p));
    ASSERT(4, sizeof(int));
#ifdef _WIN32                           // Windows: long is 4 bytes
    ASSERT(4, sizeof(long));
#else
    ASSERT(8, sizeof(long));
#endif
    ASSERT(1, sizeof(char));
    void *v = &x;
    ASSERT(5, *(int *)v);
    return g_fail;
}
