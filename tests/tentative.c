// A file-scope variable declared more than once (tentative definitions,
// C11 6.9.2): one object; the declaration with the initializer gives the value.
#include <stdio.h>

static int x;
static int x;
int y;
int y;
static int z;
static int z = 5;
int arr[];
int arr[3] = { 7, 8, 9 };

int main(void)
{
    int bad = 0;
    x = 3;
    y = 4;
    if (x != 3 || y != 4) { printf("x y\n"); bad++; }
    if (z != 5) { printf("z = %d\n", z); bad++; }
    if (sizeof(arr) != 3 * sizeof(int) || arr[2] != 9) { printf("arr\n"); bad++; }
    return bad;
}
