// Программа, которая берёт функции из своей mathlib.dll.
// Собрать:  wcc examples\dll\usemath.c -l mathlib     (mathlib.dll лежит рядом)
#include <stdio.h>

int add(int a, int b);
double average(const double *v, int n);
void greet(const char *name);
int calls(void);

int main(void)
{
    double v[] = { 1.5, 2.5, 3.5, 4.5 };
    greet("мир");
    printf("add(40, 2) = %d\n", add(40, 2));
    printf("average = %.2f\n", average(v, 4));
    printf("вызовов DLL: %d\n", calls());
    return add(2, 2) == 4 ? 0 : 1;
}
