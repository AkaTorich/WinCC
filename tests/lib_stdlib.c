// stdlib.h: qsort/bsearch with program comparators, rand, div, heap, misc
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return x < y ? -1 : x > y;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

typedef struct { char name[12]; int age; double score; } Person;

static int cmp_person(const void *a, const void *b)
{
    const Person *p = a, *q = b;
    if (p->age != q->age) return p->age - q->age;
    return strcmp(p->name, q->name);
}

static int g_calls;
static int cmp_desc_counting(const void *a, const void *b)
{
    g_calls++;
    return cmp_int(b, a);
}

int main(void)
{
    // qsort: random, sorted, reversed, all equal, tiny
    static int v[5000];
    for (int round = 0; round < 5; round++) {
        int n = round == 4 ? 3 : 5000;
        long long sum = 0;
        srand(round + 1);
        for (int i = 0; i < n; i++) {
            v[i] = round == 0 ? rand() : round == 1 ? i : round == 2 ? n - i : round == 3 ? 7 : rand() % 3;
            sum += v[i];
        }
        qsort(v, n, sizeof(int), cmp_int);
        int sorted = 1;
        long long sum2 = 0;
        for (int i = 0; i < n; i++) {
            if (i && v[i - 1] > v[i]) sorted = 0;
            sum2 += v[i];
        }
        ASSERT(1, sorted);
        ASSERT(sum, sum2);
    }
    g_calls = 0;
    int w[] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5 };
    qsort(w, 11, sizeof(int), cmp_desc_counting);
    ASSERT(9, w[0]);
    ASSERT(1, w[10]);
    ASSERT(1, g_calls > 0);
    qsort(w, 0, sizeof(int), cmp_int);
    const char *words[] = { "pear", "apple", "fig", "banana", "cherry" };
    qsort(words, 5, sizeof(words[0]), cmp_str);
    ASSERT(0, strcmp(words[0], "apple"));
    ASSERT(0, strcmp(words[4], "pear"));
    Person people[4] = { { "Vera", 30, 1.5 }, { "Anna", 25, 2.5 }, { "Boris", 30, 3.5 }, { "Oleg", 20, 4.5 } };
    qsort(people, 4, sizeof(Person), cmp_person);
    ASSERT(0, strcmp(people[0].name, "Oleg"));
    ASSERT(0, strcmp(people[2].name, "Boris"));
    ASSERT(1, people[3].score == 1.5);
    // bsearch
    int sorted[] = { 1, 3, 5, 7, 9, 11 };
    int key = 7;
    int *hit = bsearch(&key, sorted, 6, sizeof(int), cmp_int);
    ASSERT(3, hit - sorted);
    key = 4;
    ASSERT(1, bsearch(&key, sorted, 6, sizeof(int), cmp_int) == 0);
    key = 1;
    ASSERT(0, (int *)bsearch(&key, sorted, 6, sizeof(int), cmp_int) - sorted);
    key = 11;
    ASSERT(5, (int *)bsearch(&key, sorted, 6, sizeof(int), cmp_int) - sorted);
    ASSERT(1, bsearch(&key, sorted, 0, sizeof(int), cmp_int) == 0);
    // rand: the generator of the C standard, reproducible
    srand(1);
    ASSERT(16838, rand());
    int first[10], again[10], inrange = 1;
    srand(12345);
    for (int i = 0; i < 10; i++) first[i] = rand();
    srand(12345);
    for (int i = 0; i < 10; i++) again[i] = rand();
    ASSERT(0, memcmp(first, again, sizeof(first)));
    for (int i = 0; i < 1000; i++) {
        int r = rand();
        if (r < 0 || r > RAND_MAX) inrange = 0;
    }
    ASSERT(1, inrange);
    ASSERT(32767, RAND_MAX);
    // integer arithmetic
    ASSERT(5, abs(-5));
#ifdef _WIN32                           // Windows: long is 4 bytes
    ASSERT(2000000000, labs(-2000000000L));
#else
    ASSERT(5000000000LL, labs(-5000000000L));
#endif
    ASSERT(9, llabs(-9LL));
    div_t dv = div(7, -2);
    ASSERT(-3, dv.quot);
    ASSERT(1, dv.rem);
#ifdef _WIN32
    ldiv_t lv = ldiv(-2000000000L, 2L);
    ASSERT(-1000000000, lv.quot);
#else
    ldiv_t lv = ldiv(-7000000000L, 2L);
    ASSERT(-3500000000LL, lv.quot);
#endif
    ASSERT(0, lv.rem);
    lldiv_t llv = lldiv(-7LL, 2LL);
    ASSERT(-3, llv.quot);
    ASSERT(-1, llv.rem);
    // heap
    char *p = malloc(100);
    ASSERT(1, p != 0);
    ASSERT(0, (uintptr_t)p & 15);
    strcpy(p, "keep me");
    p = realloc(p, 10000);
    ASSERT(0, strcmp(p, "keep me"));
    p = realloc(p, 20);
    ASSERT(0, strcmp(p, "keep me"));
    free(p);
    int *z = calloc(100, sizeof(int));
    int zero = 1;
    for (int i = 0; i < 100; i++) if (z[i]) zero = 0;
    ASSERT(1, zero);
    free(z);
    ASSERT(1, malloc((size_t)1 << 50) == 0);
    void *many[200];
    for (int i = 0; i < 200; i++) many[i] = malloc(i * 13 + 1);
    for (int i = 0; i < 200; i += 2) free(many[i]);
    for (int i = 1; i < 200; i += 2) free(many[i]);
    char *big = malloc(10 << 20);           // all merged again: a big block fits
    ASSERT(1, big != 0);
    free(big);
    free(0);
    ASSERT(1, realloc(0, 8) != 0);
    void *al = aligned_alloc(16, 64);
    ASSERT(0, (uintptr_t)al & 15);
    // environment
#ifdef _WIN32                           // Windows: the environment and cmd.exe are there
    ASSERT(1, getenv("PATH") != 0);
    ASSERT(1, getenv("NO_SUCH_VARIABLE_WINCC") == 0);
    ASSERT(1, system(0) != 0);
    ASSERT(0, system("exit 0"));
#else
    ASSERT(1, getenv("PATH") == 0);
    ASSERT(0, system(0));
    ASSERT(-1, system("dir"));
#endif
    return g_fail;
}
