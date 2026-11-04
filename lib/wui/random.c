// WUI — System.Random из .NET Framework, число в число: фактуры и звёзды ThemeLibrary
// рассыпаны им, с тем же зерном выходит тот же рисунок.
#include "wui_int.h"

#define MBIG 2147483647
#define MSEED 161803398

void wui_random_init(WuiRandom *r, int seed)
{
    int *a = r->seed;
    int sub = seed == (int)0x80000000 ? MBIG : (seed < 0 ? -seed : seed);
    int mj = MSEED - sub, mk = 1;
    a[55] = mj;
    for (int i = 1; i < 55; i++) {
        int ii = (21 * i) % 55;
        a[ii] = mk;
        mk = mj - mk;
        if (mk < 0) mk += MBIG;
        mj = a[ii];
    }
    for (int k = 1; k < 5; k++)
        for (int i = 1; i < 56; i++) {
            a[i] -= a[1 + (i + 30) % 55];
            if (a[i] < 0) a[i] += MBIG;
        }
    r->next = 0;
    r->nextp = 21;
}

static int sample(WuiRandom *r)
{
    int n = r->next + 1, p = r->nextp + 1;
    if (n >= 56) n = 1;
    if (p >= 56) p = 1;
    int v = r->seed[n] - r->seed[p];
    if (v == MBIG) v--;
    if (v < 0) v += MBIG;
    r->seed[n] = v;
    r->next = n;
    r->nextp = p;
    return v;
}

int wui_random_next(WuiRandom *r)
{
    return sample(r);
}

double wui_random_double(WuiRandom *r)
{
    return sample(r) * (1.0 / MBIG);
}

int wui_random_below(WuiRandom *r, int max)
{
    return (int)(wui_random_double(r) * max);
}

int wui_random_range(WuiRandom *r, int min, int max)
{
    long long range = (long long)max - min;
    if (range <= MBIG) return (int)(wui_random_double(r) * range) + min;
    // большой размах (.NET: GetSampleForLargeRange)
    int s = sample(r);
    if (sample(r) % 2 == 0) s = -s;
    double d = s;
    d += MBIG - 1;
    d /= 2.0 * MBIG - 1;
    return (int)((long long)(d * range) + min);
}
