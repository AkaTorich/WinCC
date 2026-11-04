#include <stdio.h>

int square(int x) { return x * x; }

int main(void)
{
    printf("Привет из DyOS! %d + %d = %d\n", 2, 3, 2 + 3);
    int sum = 0;
    for (int i = 1; i <= 10; i++)
        sum += square(i);
    printf("сумма квадратов 1..10 = %d\n", sum);
    return 42;
}
