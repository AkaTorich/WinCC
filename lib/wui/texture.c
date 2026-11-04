// WUI — фактуры: плитки-фоны окна (пергамент, мрамор, дерево…). Точка в точку как Textures
// в ThemeLibrary: те же рецепты, тот же порядок случайных чисел и действий с double.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

#define SIZE 512                            // плитка в DIP
#define PI 3.14159265358979323846           // Math.PI
#ifndef TEX_THREADS_MAX
#define TEX_THREADS_MAX 8                   // главный проход — не больше стольких потоков
#endif

// ---- математика как в .NET Framework (x64) ----
// Math.Sin/Cos там — команды x87 fsin/fcos, а Exp/Log/Pow — из ucrtbase.dll. Свои sin/pow
// у WinCC точнее, но расходятся с .NET в последнем бите, а это меняет точки.

static double tex_sin(double x)
{
    asm("fld qword ptr [rbp+16]\n"
        "fsin\n"
        "fstp qword ptr [rbp+16]\n"
        "movsd xmm0, qword ptr [rbp+16]\n"
        "jmp .L.return.tex_sin");
    return 0;
}

static double tex_cos(double x)
{
    asm("fld qword ptr [rbp+16]\n"
        "fcos\n"
        "fstp qword ptr [rbp+16]\n"
        "movsd xmm0, qword ptr [rbp+16]\n"
        "jmp .L.return.tex_cos");
    return 0;
}

typedef double (*Math1)(double);
typedef double (*Math2)(double, double);
static Math1 crt_exp, crt_log;
static Math2 crt_pow;

static double own_exp(double x) { return exp(x); }
static double own_log(double x) { return log(x); }
static double own_pow(double x, double y) { return pow(x, y); }

static void math_init(void)
{
    if (crt_exp)
        return;
    HMODULE h = LoadLibraryA("ucrtbase.dll");
    Math1 e = h ? (Math1)GetProcAddress(h, "exp") : 0;
    Math1 l = h ? (Math1)GetProcAddress(h, "log") : 0;
    Math2 p = h ? (Math2)GetProcAddress(h, "pow") : 0;
    if (!e || !l || !p) { e = own_exp; l = own_log; p = own_pow; }   // нет ucrtbase — свои, почти те же
    crt_log = l;
    crt_pow = p;
    crt_exp = e;
}

// Math.Min / Math.Max для чисел (без NaN): при равенстве — второй
static double dmin(double a, double b) { return a < b ? a : b; }
static double dmax(double a, double b) { return a > b ? a : b; }

static int wrap(int v, int n) { return ((v % n) + n) % n; }

static double smooth(double a, double b, double v)
{
    double t = (v - a) / (b - a);
    t = 1 < t ? 1 : t;
    t = 0 > t ? 0 : t;
    return t * t * (3 - 2 * t);
}

static int clamp255(double v) { return v < 0 ? 0 : v > 255 ? 255 : (int)v; }

static unsigned rgb(double r, double g, double b)
{
    return 0xFF000000u | ((unsigned)clamp255(r) << 16) | ((unsigned)clamp255(g) << 8) | (unsigned)clamp255(b);
}

static unsigned blend(unsigned px, WuiColor c, double a)
{
    int r = (px >> 16) & 0xFF, g = (px >> 8) & 0xFF, b = px & 0xFF;
    return rgb(r + (WUI_R(c) - r) * a, g + (WUI_G(c) - g) * a, b + (WUI_B(c) - b) * a);
}

// ---- плитка в работе ----

typedef struct Grid Grid;

typedef struct {
    int s, px;                  // точек на DIP; точек в стороне
    WuiRandom rnd;
    unsigned *pix;
    double *noise;              // случайные числа главного прохода — заранее и по порядку
    int per_pixel;              // сколько их на точку
    Grid *grids[40];
    int ngrids;
} Tex;

static double rnd(Tex *t) { return wui_random_double(&t->rnd); }

// Числа главного прохода: в C# там по одному-два NextDouble на точку подряд — берём их заранее,
// тогда строки можно считать в нескольких потоках.
static void noise_fill(Tex *t, int per_pixel)
{
    long long n = (long long)t->px * t->px * per_pixel;
    t->noise = malloc(n * sizeof(double));
    for (long long i = 0; i < n; i++)
        t->noise[i] = rnd(t);
    t->per_pixel = per_pixel;
}

static double noise_at(Tex *t, int x, int y, int k)
{
    return t->noise[((long long)y * t->px + x) * t->per_pixel + k];
}

// ---- шум на сетке (Grid, Lattice, Smp, Sample, Fbm) ----

struct Grid {
    int nx, ny;
    double *v;                  // v[y·nx + x] = g[x, y]
    int *ax, *bx;               // по столбцам точек: ячейки слева и справа
    int *ay, *by;               // по строкам: ячейки сверху и снизу, уже умножены на nx
    double *fx, *fy;            // доли внутри ячейки (после SmoothStep)
};

// Ячейки и доля по одной оси: координата c (DIP) на сетке из n ячеек на плитку.
static void axis(int n, double c, int *a, int *b, double *f)
{
    double x = c * n / SIZE;
    int x0 = (int)floor(x);
    *f = smooth(0, 1, x - x0);
    *a = wrap(x0, n);
    *b = wrap(x0 + 1, n);
}

// Grid(rnd, nx, ny), он же Lattice(rnd, n) = Grid(n, n): Sample(g, u / k, …) с n = Size / k
// даёт ровно то же, что Smp (деление на степень двойки точное).
static Grid *grid(Tex *t, int nx, int ny)
{
    Grid *g = malloc(sizeof(Grid));
    g->nx = nx;
    g->ny = ny;
    g->v = malloc(sizeof(double) * nx * ny);
    for (int i = 0; i < nx * ny; i++)       // C#: по y, внутри по x
        g->v[i] = rnd(t);
    // Таблицы для точек плитки: u = x / s.
    int px = t->px;
    g->ax = malloc(sizeof(int) * px * 4);
    g->bx = g->ax + px;
    g->ay = g->bx + px;
    g->by = g->ay + px;
    g->fx = malloc(sizeof(double) * px * 2);
    g->fy = g->fx + px;
    for (int i = 0; i < px; i++) {
        double c = (double)i / t->s;
        axis(nx, c, &g->ax[i], &g->bx[i], &g->fx[i]);
        axis(ny, c, &g->ay[i], &g->by[i], &g->fy[i]);
        g->ay[i] *= nx;
        g->by[i] *= nx;
    }
    t->grids[t->ngrids++] = g;
    return g;
}

static void grid_free(Grid *g)
{
    free(g->v);
    free(g->ax);
    free(g->fx);
    free(g);
}

static double cells4(const Grid *g, int ax, int bx, double fx, int ay, int by, double fy)
{
    const double *v = g->v;
    double top = v[ay + ax] + (v[ay + bx] - v[ay + ax]) * fx;
    double bottom = v[by + ax] + (v[by + bx] - v[by + ax]) * fx;
    return top + (bottom - top) * fy;
}

// Smp в любой точке (u, v), DIP
static double smp(const Grid *g, double u, double v)
{
    int ax, bx, ay, by;
    double fx, fy;
    axis(g->nx, u, &ax, &bx, &fx);
    axis(g->ny, v, &ay, &by, &fy);
    return cells4(g, ax, bx, fx, ay * g->nx, by * g->nx, fy);
}

// Smp в точке плитки (x, y)
static double smp_px(const Grid *g, int x, int y)
{
    return cells4(g, g->ax[x], g->bx[x], g->fx[x], g->ay[y], g->by[y], g->fy[y]);
}

// Smp: столбец — точка плитки x, строка — любая v
static double smp_col(const Grid *g, int x, double v)
{
    int ay, by;
    double fy;
    axis(g->ny, v, &ay, &by, &fy);
    return cells4(g, g->ax[x], g->bx[x], g->fx[x], ay * g->nx, by * g->nx, fy);
}

// Smp: столбец — любая u, строка — точка плитки y
static double smp_row(const Grid *g, double u, int y)
{
    int ax, bx;
    double fx;
    axis(g->nx, u, &ax, &bx, &fx);
    return cells4(g, ax, bx, fx, g->ay[y], g->by[y], g->fy[y]);
}

// Fbm: октавы, у каждой следующей вдвое меньший вес
static double fbm(Grid **o, int n, double u, double v)
{
    double sum = 0, weight = 1, total = 0;
    for (int i = 0; i < n; i++) {
        sum += smp(o[i], u, v) * weight;
        total += weight;
        weight *= 0.5;
    }
    return sum / total;
}

static double fbm_px(Grid **o, int n, int x, int y)
{
    double sum = 0, weight = 1, total = 0;
    for (int i = 0; i < n; i++) {
        sum += smp_px(o[i], x, y) * weight;
        total += weight;
        weight *= 0.5;
    }
    return sum / total;
}

// Октавы квадратных сеток со сторонами first, 2·first, … (count штук)
static void octaves(Tex *t, Grid **o, int count, int first)
{
    for (int i = 0; i < count; i++)
        o[i] = grid(t, first << i, first << i);
}

// ---- плоские волны (Waves, Wave) ----

typedef struct { int n; double m[8], k[8], phase[8], w[8]; } Waves;

static void waves(Tex *t, Waves *w, int count, int max_freq)
{
    w->n = count;
    for (int i = 0; i < count; i++) {
        int m, n;
        do {
            m = wui_random_range(&t->rnd, -max_freq, max_freq + 1);
            n = wui_random_range(&t->rnd, -max_freq, max_freq + 1);
        } while (m == 0 && n == 0);
        w->m[i] = m;
        w->k[i] = n;
        w->phase[i] = rnd(t) * PI * 2;
        w->w[i] = 1.0 / sqrt(m * m + n * n);
    }
}

static double wave(const Waves *w, double u, double v)
{
    double sum = 0, total = 0;
    const double k = PI * 2 / SIZE;
    for (int i = 0; i < w->n; i++) {
        sum += w->w[i] * tex_sin((w->m[i] * u + w->k[i] * v) * k + w->phase[i]);
        total += w->w[i];
    }
    return 0.5 + 0.5 * sum / total;
}

// ---- полосы (Strips, StripAt) ----

typedef struct { int n; double bounds[40], shades[40]; } Strips;

static void strips(Tex *t, Strips *st, double min, double max)
{
    double widths[40];
    int n = 0;
    double total = 0;
    while (total < SIZE) {
        double w = min + rnd(t) * (max - min);
        widths[n++] = w;
        total += w;
    }
    double pos = 0;
    for (int i = 0; i < n; i++) {
        st->bounds[i] = pos;
        pos += widths[i] * SIZE / total;
        st->shades[i] = rnd(t) - 0.5;
    }
    st->bounds[n] = SIZE;
    st->n = n;
}

static void strip_at(const Strips *st, double p, double *edge, double *shade)
{
    const double *b = st->bounds;
    p -= SIZE * floor(p / SIZE);
    int i = 0;
    while (i < st->n - 1 && p >= b[i + 1])
        i++;
    *edge = dmin(p - b[i], b[i + 1] - p);
    *shade = st->shades[i];
}

// ---- ячейки (Cells, Nearest) ----

static double *cells(Tex *t, int n)
{
    double cell = (double)SIZE / n;
    double *pts = malloc(sizeof(double) * n * n * 2);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            pts[(j * n + i) * 2] = (i + 0.1 + rnd(t) * 0.8) * cell;
            pts[(j * n + i) * 2 + 1] = (j + 0.1 + rnd(t) * 0.8) * cell;
        }
    return pts;
}

static void nearest(const double *pts, int n, double u, double v, double *f1, double *f2, int *id)
{
    double cell = (double)SIZE / n;
    int ci = (int)floor(u / cell), cj = (int)floor(v / cell);
    double d1 = 1.7976931348623157e308, d2 = d1;
    int k1 = 0;
    for (int dj = -1; dj <= 1; dj++)
        for (int di = -1; di <= 1; di++) {
            int ii = ci + di, jj = cj + dj;
            int wi = wrap(ii, n), wj = wrap(jj, n);
            int k = wj * n + wi;
            double x = pts[k * 2] + (ii - wi) * cell, y = pts[k * 2 + 1] + (jj - wj) * cell;
            double dx = u - x, dy = v - y;
            double d = sqrt(dx * dx + dy * dy);
            if (d < d1) { d2 = d1; d1 = d; k1 = k; }
            else if (d < d2) d2 = d;
        }
    *f1 = d1;
    *f2 = d2;
    *id = k1;
}

// ---- пятнышки (Speck, Splat) ----

// Мягкое круглое пятнышко в (cx, cy) радиуса r DIP; через край плитки — с другой стороны
static void speck(Tex *t, double cx, double cy, double r, WuiColor c, double strength)
{
    int px = t->px, s = t->s;
    double pr = r * s;
    int x0 = (int)floor(cx * s - pr), x1 = (int)ceil(cx * s + pr);
    int y0 = (int)floor(cy * s - pr), y1 = (int)ceil(cy * s + pr);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - cx * s, dy = y + 0.5 - cy * s;
            double d = sqrt(dx * dx + dy * dy) / pr;
            if (d > 1) continue;
            double a = strength * (1 - d * d);
            int i = wrap(y, px) * px + wrap(x, px);
            t->pix[i] = blend(t->pix[i], c, a);
        }
}

// Мягкий мазок amount в буфер плотности
static void splat(Tex *t, double *buf, double cx, double cy, double r, double amount)
{
    int px = t->px, s = t->s;
    double pr = dmax(0.75, r * s);
    double fx = cx * s, fy = cy * s;
    int x0 = (int)floor(fx - pr), x1 = (int)ceil(fx + pr);
    int y0 = (int)floor(fy - pr), y1 = (int)ceil(fy + pr);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - fx, dy = y + 0.5 - fy;
            double d = sqrt(dx * dx + dy * dy) / pr;
            if (d >= 1) continue;
            buf[wrap(y, px) * px + wrap(x, px)] += amount * (1 - d);
        }
}

// ---- главный проход по строкам, в нескольких потоках ----

typedef void (*RowFn)(Tex *t, const void *ctx, int y);
typedef struct { Tex *t; RowFn fn; const void *ctx; int first, step; } RowJob;

static DWORD WINAPI row_thread(LPVOID p)
{
    RowJob *j = p;
    for (int y = j->first; y < j->t->px; y += j->step)
        j->fn(j->t, j->ctx, y);
    return 0;
}

static void run_rows(Tex *t, RowFn fn, const void *ctx)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors;
    if (n > TEX_THREADS_MAX) n = TEX_THREADS_MAX;
    if (n < 1) n = 1;
    RowJob jobs[TEX_THREADS_MAX];
    HANDLE th[TEX_THREADS_MAX];
    int started = 0;
    for (int i = 0; i < n; i++) {
        jobs[i].t = t; jobs[i].fn = fn; jobs[i].ctx = ctx; jobs[i].first = i; jobs[i].step = n;
    }
    for (int i = 1; i < n; i++) {
        th[started] = CreateThread(0, 0, row_thread, &jobs[i], 0, 0);
        if (!th[started]) {                 // поток не дали — эти строки сами
            row_thread(&jobs[i]);
            continue;
        }
        started++;
    }
    row_thread(&jobs[0]);
    if (started)
        WaitForMultipleObjects(started, th, TRUE, INFINITE);
    for (int i = 0; i < started; i++)
        CloseHandle(th[i]);
    free(t->noise);
    t->noise = 0;
}

// ======================= рецепты =======================

// ---- Parchment: кляксы, большие пятна, зерно и волокна ----

typedef struct { Grid *blot, *stain; WuiColor paper; } ParchmentCtx;

static void parchment_row(Tex *t, const void *p, int y)
{
    const ParchmentCtx *c = p;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double b = smp_px(c->blot, x, y);
        double st = smp_px(c->stain, x, y);
        double grain = noise_at(t, x, y, 0) - 0.5;
        double fiber = tex_sin((u * 0.8 + v * 0.15) * 0.33 + noise_at(t, x, y, 1) * 0.7) * 0.5;
        double shade = (b - 0.5) * 20 + (st - 0.5) * 26 + grain * 8 + fiber * 3;
        out[x] = rgb(WUI_R(c->paper) + shade * 0.8, WUI_G(c->paper) + shade, WUI_B(c->paper) + shade * 1.15);
    }
}

static void parchment(Tex *t, WuiColor paper)
{
    wui_random_init(&t->rnd, 1563);
    ParchmentCtx c;
    c.blot = grid(t, SIZE / 32, SIZE / 32);
    c.stain = grid(t, SIZE / 128, SIZE / 128);
    c.paper = paper;
    noise_fill(t, 2);
    run_rows(t, parchment_row, &c);
}

// ---- Lapis: ультрамарин, кальцитовая дымка, искры пирита ----

typedef struct { Grid *o1, *o2, *o3, *c1, *c2; WuiColor deep, light; } LapisCtx;

static void lapis_row(Tex *t, const void *p, int y)
{
    const LapisCtx *c = p;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double n = smp_px(c->o1, x, y) * 0.5
                 + smp_px(c->o2, x, y) * 0.3
                 + smp_px(c->o3, x, y) * 0.2;
        double cloud = smp_px(c->c1, x, y) * 0.6 + smp_px(c->c2, x, y) * 0.4;
        WuiColor col = wui_mix(c->deep, c->light, n);
        double k = smooth(0.66, 0.86, cloud) * (0.2 + 0.25 * n);
        col = wui_mix(col, 0xFFC4CFE4, k);                       // кальцит
        double grain = (noise_at(t, x, y, 0) - 0.5) * 10;
        out[x] = rgb(WUI_R(col) + grain, WUI_G(col) + grain, WUI_B(col) + grain * 1.2);
    }
}

static void lapis(Tex *t, WuiColor stone)
{
    wui_random_init(&t->rnd, 2029);
    LapisCtx c;
    c.o1 = grid(t, SIZE / 64, SIZE / 64);
    c.o2 = grid(t, SIZE / 32, SIZE / 32);
    c.o3 = grid(t, SIZE / 16, SIZE / 16);
    c.c1 = grid(t, SIZE / 64, SIZE / 64);
    c.c2 = grid(t, SIZE / 32, SIZE / 32);
    c.deep = wui_darken(stone, 0.5);
    c.light = wui_lighten(stone, 0.1);
    noise_fill(t, 1);
    run_rows(t, lapis_row, &c);

    // Пирит: мелкие золотые искры, изредка крупнее
    for (int i = 0; i < 700; i++) {
        double cx = rnd(t) * SIZE;
        double cy = rnd(t) * SIZE;
        WuiColor g = wui_mix(0xFFA88028, 0xFFFAE086, rnd(t));
        double r = rnd(t) < 0.15 ? 2.2 : 1.1;
        speck(t, cx, cy, r, g, 0.95);
    }
}

// ---- Papyrus: горизонтальные полосы сердцевины поверх вертикальных ----

typedef struct {
    Strips h_strips, v_strips;
    Grid *h_wave, *v_wave, *h_fib[2], *h_line, *v_fib, *tone[4], *warmth[2];
    WuiColor paper;
} PapyrusCtx;

static void papyrus_row(Tex *t, const void *p, int y)
{
    const PapyrusCtx *c = p;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;

        // верхний слой: горизонтальные полосы с волнистыми краями и волокнами вдоль
        double hv = v + (smp_px(c->h_wave, x, 0) - 0.5) * 6;
        double hd, h_shade;
        strip_at(&c->h_strips, hv, &hd, &h_shade);
        double h_edge = 1 - smooth(0, 1.6, hd);
        double h_rise = smooth(1.2, 2.4, hd) * (1 - smooth(2.4, 5, hd));
        double hf = (smp_col(c->h_fib[0], x, hv) - 0.5) * 0.6 + (smp_col(c->h_fib[1], x, hv) - 0.5) * 0.4;
        double line = crt_pow(smp_col(c->h_line, x, hv), 5);
        double gleam = crt_pow(1 - smp(c->h_line, u + 256, hv), 6);

        // нижний слой просвечивает чуть-чуть
        double vu = u + (smp_px(c->v_wave, 0, y) - 0.5) * 6;
        double vd, v_shade;
        strip_at(&c->v_strips, vu, &vd, &v_shade);
        double v_edge = 1 - smooth(0, 2.5, vd);
        double vf = smp_row(c->v_fib, vu, y) - 0.5;

        double n = fbm_px((Grid **)c->tone, 4, x, y) - 0.5;
        double shade = h_shade * 12 + hf * 16 - line * 22 + gleam * 9 - h_edge * 16 + h_rise * 5
                     + v_shade * 4 + vf * 4 - v_edge * 5
                     + n * 26
                     + (noise_at(t, x, y, 0) - 0.5) * 4;
        double warm = (fbm_px((Grid **)c->warmth, 2, x, y) - 0.5) * 10;
        out[x] = rgb(WUI_R(c->paper) + shade + warm * 0.3, WUI_G(c->paper) + shade * 0.95,
                     WUI_B(c->paper) + shade * 0.8 - warm);
    }
}

static void papyrus(Tex *t, WuiColor paper)
{
    wui_random_init(&t->rnd, 3141);
    PapyrusCtx *c = malloc(sizeof(PapyrusCtx));
    strips(t, &c->h_strips, 38, 66);
    strips(t, &c->v_strips, 40, 70);
    c->h_wave = grid(t, 6, 1);
    c->v_wave = grid(t, 1, 6);
    c->h_fib[0] = grid(t, 4, 256);
    c->h_fib[1] = grid(t, 12, 512);
    c->h_line = grid(t, 6, 320);
    c->v_fib = grid(t, 256, 4);
    octaves(t, c->tone, 4, 3);
    octaves(t, c->warmth, 2, 4);
    c->paper = paper;
    noise_fill(t, 1);
    run_rows(t, papyrus_row, c);
    free(c);

    // тонкие тёмные обрывки волокон вдоль верхних полос
    WuiColor fleck = wui_darken(paper, 0.4);
    for (int i = 0; i < 90; i++) {
        double cx = rnd(t) * SIZE;
        double cy = rnd(t) * SIZE;
        double len = 6 + rnd(t) * 18;
        double a = 0.1 + rnd(t) * 0.18;
        double bend = (rnd(t) - 0.5) * 0.06;
        for (double d = 0; d < len; d += 0.5) {
            double fade = dmin(1, dmin(d, len - d) / 3);
            speck(t, cx + d, cy + d * bend, 0.55, fleck, a * fade);
        }
    }
}

// ---- Washi: светлая бумага, длинные тонкие волокна, мягкие просветы ----

typedef struct { Grid *cloud[4], *thin[2]; double *dark, *light; WuiColor paper, fibre; } WashiCtx;

static void washi_row(Tex *t, const void *p, int y)
{
    const WashiCtx *c = p;
    int px = t->px;
    unsigned *out = t->pix + y * px;
    for (int x = 0; x < px; x++) {
        double cl = fbm_px((Grid **)c->cloud, 4, x, y);
        double tn = fbm_px((Grid **)c->thin, 2, x, y);
        // плотная мякоть чуть темнее, тонкие места пропускают свет
        double shade = (cl - 0.5) * -8 + smooth(0.55, 0.78, tn) * 4 + (noise_at(t, x, y, 0) - 0.5) * 2.5;
        int i = y * px + x;
        double r = WUI_R(c->paper) + shade, g = WUI_G(c->paper) + shade, b = WUI_B(c->paper) + shade * 0.9;
        double kd = 1 - crt_exp(-c->dark[i] * 1.6);
        double kl = 1 - crt_exp(-c->light[i] * 1.6);
        r += (WUI_R(c->fibre) - r) * kd * 0.5; g += (WUI_G(c->fibre) - g) * kd * 0.5; b += (WUI_B(c->fibre) - b) * kd * 0.5;
        r += (255 - r) * kl * 0.7; g += (255 - g) * kl * 0.7; b += (253 - b) * kl * 0.7;
        out[x] = rgb(r, g, b);
    }
}

static void washi(Tex *t, WuiColor paper)
{
    wui_random_init(&t->rnd, 1185);
    WashiCtx c;
    octaves(t, c.cloud, 4, 4);
    octaves(t, c.thin, 2, 6);
    int px = t->px;

    // плотность волокон по точкам, потом — в цвет
    c.dark = calloc((size_t)px * px, sizeof(double));
    c.light = calloc((size_t)px * px, sizeof(double));
    for (int i = 0; i < 520; i++) {
        int bundle = i < 24;
        int bright = !bundle && rnd(t) < 0.4;
        double x = rnd(t) * SIZE;
        double y = rnd(t) * SIZE;
        double ang = rnd(t) * PI * 2;
        double turn = (rnd(t) - 0.5) * 0.006;
        double len = 70 + crt_pow(rnd(t), 1.4) * 260;
        double width = bundle ? 0.9 + rnd(t) * 0.5 : 0.3 + crt_pow(rnd(t), 2) * 0.4;
        double amount = bundle ? 0.12 + rnd(t) * 0.1 : 0.2 + rnd(t) * 0.35;
        double *buf = bright ? c.light : c.dark;
        for (double d = 0; d < len; d += 0.4) {
            turn += (rnd(t) - 0.5) * 0.0012;
            turn = dmax(-0.005, dmin(0.005, turn));
            ang += turn;
            x += tex_cos(ang) * 0.4;
            y += tex_sin(ang) * 0.4;
            double fade = dmin(1, dmin(d, len - d) / 30);
            splat(t, buf, x, y, width, amount * fade * 0.25);
        }
    }

    c.paper = paper;
    c.fibre = WUI_ARGB(255, (int)(WUI_R(paper) * 0.86), (int)(WUI_G(paper) * 0.84), (int)(WUI_B(paper) * 0.79));
    noise_fill(t, 1);
    run_rows(t, washi_row, &c);
    free(c.dark);
    free(c.light);
}

// ---- Marble: белый каррарский мрамор — облака, перистые жилы, тонкие ветви ----

typedef struct { Grid *turb[6], *turb2[6], *cloud[4], *mask[2], *mask2[2]; WuiColor stone; } MarbleCtx;

static void marble_row(Tex *t, const void *p, int y)
{
    const MarbleCtx *c = p;
    const double k = PI * 2 / SIZE;
    const WuiColor vein = 0xFF727780, haze = 0xFFAEB2B8, warm = 0xFFECDEC8;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double t1 = fbm_px((Grid **)c->turb, 6, x, y);
        double cl = fbm_px((Grid **)c->cloud, 4, x, y);
        double m1 = fbm_px((Grid **)c->mask, 2, x, y);

        // главные жилы: диагональные полосы, изогнутые турбулентностью, то ярче, то бледнее
        double p1 = fabs(tex_sin((u + v) * k + t1 * 6.0));
        double strength = 0.25 + 0.75 * smooth(0.3, 0.62, m1);
        double core = crt_pow(1 - p1, 55) * strength;
        double body = crt_pow(1 - p1, 12) * strength;
        double halo = crt_pow(1 - p1, 3) * strength;

        // поперечные: тоньше и лишь местами — открытая сетка
        double t2 = fbm_px((Grid **)c->turb2, 6, x, y);
        double p2 = fabs(tex_sin((2 * u - v) * k + t2 * 8.0));
        double show = smooth(0.5, 0.7, fbm_px((Grid **)c->mask2, 2, x, y));
        double branch = (crt_pow(1 - p2, 80) * 0.8 + crt_pow(1 - p2, 16) * 0.3) * show;

        WuiColor col = wui_mix(c->stone, warm, smooth(0.55, 0.85, cl) * 0.22);
        col = wui_mix(col, haze, smooth(0.35, 0.05, cl) * 0.2 + halo * 0.07);
        double vk = dmin(1, core * 0.5 + body * 0.34 + branch * 0.42) * 0.5;
        double grain = (noise_at(t, x, y, 0) - 0.5) * 3;
        double r = WUI_R(col) + (WUI_R(vein) - WUI_R(col)) * vk + grain;
        double g = WUI_G(col) + (WUI_G(vein) - WUI_G(col)) * vk + grain;
        double b = WUI_B(col) + (WUI_B(vein) - WUI_B(col)) * vk + grain;
        out[x] = rgb(r, g, b);
    }
}

static void marble(Tex *t, WuiColor stone)
{
    wui_random_init(&t->rnd, 4411);
    MarbleCtx c;
    octaves(t, c.turb, 6, 3);
    octaves(t, c.turb2, 6, 3);
    octaves(t, c.cloud, 4, 3);
    octaves(t, c.mask, 2, 3);
    octaves(t, c.mask2, 2, 3);
    c.stone = stone;
    noise_fill(t, 1);
    run_rows(t, marble_row, &c);
}

// ---- Malachite: концентрические светлые и тёмные зелёные полосы вокруг слитых центров ----

typedef struct { Waves warp, warp_b; Grid *ripple[2], *ripple_b[2], *jitter[2], *fine[2]; WuiColor stone, deep, pale; } MalachiteCtx;

static void malachite_row(Tex *t, const void *p, int y)
{
    static const double cx[6] = { 90.0, 330, 210, 440, 40, 470 };
    static const double cy[6] = { 120.0, 60, 330, 300, 420, 150 };
    const double soft = 13;
    const MalachiteCtx *c = p;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double wu = u + (wave(&c->warp, u, v) - 0.5) * 110 + (fbm_px((Grid **)c->ripple, 2, x, y) - 0.5) * 18;
        double wv = v + (wave(&c->warp_b, u, v) - 0.5) * 110 + (fbm_px((Grid **)c->ripple_b, 2, x, y) - 0.5) * 18;
        // мягкий минимум расстояний на торе: наросты сливаются без швов
        double sum = 0;
        for (int i = 0; i < 6; i++) {
            double dx = wu - cx[i], dy = wv - cy[i];
            dx -= SIZE * wui_round(dx / SIZE);
            dy -= SIZE * wui_round(dy / SIZE);
            sum += crt_exp(-sqrt(dx * dx + dy * dy) / soft);
        }
        double d = -soft * crt_log(sum);
        double f = d / 28 + (fbm_px((Grid **)c->jitter, 2, x, y) - 0.5) * 0.12;
        // неровный профиль полос из некратных синусов и волосяные кольца
        double b = 0.5 * tex_sin(f * PI * 2)
                 + 0.28 * tex_sin(f * PI * 2 * 2.37 + 1.1)
                 + 0.14 * tex_sin(f * PI * 2 * 5.13 + 2.3)
                 + 0.08 * tex_sin(f * PI * 2 * 11.3 + 0.4);
        double tone = 0.5 + (smooth(-0.6, 0.6, b) - 0.5) * 0.8;
        WuiColor col = tone < 0.5 ? wui_mix(c->deep, c->stone, tone * 2) : wui_mix(c->stone, c->pale, (tone - 0.5) * 2);
        double fn = (fbm_px((Grid **)c->fine, 2, x, y) - 0.5) * 8 + (noise_at(t, x, y, 0) - 0.5) * 3;
        out[x] = rgb(WUI_R(col) + fn * 0.5, WUI_G(col) + fn, WUI_B(col) + fn * 0.7);
    }
}

static void malachite(Tex *t, WuiColor stone)
{
    wui_random_init(&t->rnd, 8117);
    MalachiteCtx c;
    waves(t, &c.warp, 7, 3);
    waves(t, &c.warp_b, 7, 3);
    octaves(t, c.ripple, 2, 12);
    octaves(t, c.ripple_b, 2, 12);
    octaves(t, c.jitter, 2, 24);
    octaves(t, c.fine, 2, 48);
    c.stone = stone;
    c.deep = wui_darken(stone, 0.38);
    c.pale = wui_mix(wui_lighten(stone, 0.24), 0xFF8ED8B0, 0.2);
    noise_fill(t, 1);
    run_rows(t, malachite_row, &c);
}

// ---- Obsidian: почти чёрное вулканическое стекло, дымные завитки, слабый радужный отлив ----

typedef struct { Waves wa, wb, smoke_low, sheen, hue; Grid *smoke[3]; WuiColor stone; } ObsidianCtx;

static void obsidian_row(Tex *t, const void *p, int y)
{
    const ObsidianCtx *c = p;
    const WuiColor fog = 0xFF626068;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        // искажение координат гнёт полосы течения и дым в мягкие вуали
        double qu = u + (wave(&c->wa, u, v) - 0.5) * 140;
        double qv = v + (wave(&c->wb, u, v) - 0.5) * 140;
        double sm = wave(&c->smoke_low, qu, qv) * 0.65 + fbm((Grid **)c->smoke, 3, qu, qv) * 0.35;
        double veil = smooth(0.32, 0.82, sm);
        // слои течения вулканического стекла внутри вуалей
        double band = crt_pow(fabs(tex_sin((qv * 3 / SIZE + sm * 1.5) * PI)), 10) * veil;

        double r = WUI_R(c->stone), g = WUI_G(c->stone), b = WUI_B(c->stone);
        double k = veil * veil * 0.10 + band * 0.07;
        r += (WUI_R(fog) - r) * k; g += (WUI_G(fog) - g) * k; b += (WUI_B(fog) - b) * k;

        // отлив: медленный сдвиг оттенка, лишь в нескольких широких местах
        double m = smooth(0.55, 0.85, wave(&c->sheen, qu, qv)) * 5;
        double h = wave(&c->hue, u, v) * 1.2;
        r += m * tex_cos(PI * 2 * h);
        g += m * tex_cos(PI * 2 * (h - 1.0 / 3));
        b += m * tex_cos(PI * 2 * (h - 2.0 / 3)) + m * 0.3;

        double grain = (noise_at(t, x, y, 0) - 0.5) * 2;
        out[x] = rgb(r + grain, g + grain, b + grain);
    }
}

static void obsidian(Tex *t, WuiColor stone)
{
    wui_random_init(&t->rnd, 6619);
    ObsidianCtx c;
    waves(t, &c.wa, 6, 3);
    waves(t, &c.wb, 6, 3);
    waves(t, &c.smoke_low, 8, 4);
    octaves(t, c.smoke, 3, 8);
    waves(t, &c.sheen, 5, 2);
    waves(t, &c.hue, 4, 2);
    c.stone = stone;
    noise_fill(t, 1);
    run_rows(t, obsidian_row, &c);
}

// ---- BrushedMetal: шлифованный металл; латунь с редкой патиной, иначе тёмный алюминий ----

typedef struct { Grid *s1, *s2, *s3, *s4, *broad[2], *where[2], *crust[3]; WuiColor metal; int brass; } MetalCtx;

static void metal_row(Tex *t, const void *p, int y)
{
    const MetalCtx *c = p;
    const WuiColor verdigris = 0xFF4E8E7C, verdigris_pale = 0xFF98C6B0;
    double amp = c->brass ? 1.0 : 0.8;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double streak = (smp_px(c->s1, x, y) - 0.5) * 0.18 + (smp_px(c->s2, x, y) - 0.5) * 0.35
                      + (smp_px(c->s3, x, y) - 0.5) * 0.2 + (smp_px(c->s4, x, y) - 0.5) * 0.35;
        double polish = fbm_px((Grid **)c->broad, 2, x, y) - 0.5;
        double grain = noise_at(t, x, y, 0) - 0.5;
        // широкий диагональный блик (один период на плитку)
        double sheen = tex_cos((u + v) / SIZE * PI * 2) * 0.5 + 0.5;
        double shade = (streak * 26 + polish * 12 + grain * 5 + (sheen - 0.5) * 22) * amp;
        double r = WUI_R(c->metal) + shade, g = WUI_G(c->metal) + shade, b = WUI_B(c->metal) + shade * (c->brass ? 0.8 : 1.1);

        if (c->brass) {
            // редкая патина: шершавые пятнышки в одном-двух местах, отчасти стёртые
            double zone = smooth(0.8, 0.9, fbm_px((Grid **)c->where, 2, x, y));
            if (zone > 0) {
                double cr = fbm_px((Grid **)c->crust, 3, x, y);
                double pat = smooth(0.42, 0.6, zone * 0.65 + (cr - 0.5) * 1.5) * (0.7 + streak * 1.2);
                pat = dmax(0, dmin(1, pat));
                double tarnish = zone * 0.05;
                r *= 1 - tarnish; g *= 1 - tarnish; b *= 1 - tarnish;
                WuiColor vc = wui_mix(verdigris, verdigris_pale, smooth(0.45, 0.7, cr));
                r += (WUI_R(vc) - r) * pat * 0.3; g += (WUI_G(vc) - g) * pat * 0.3; b += (WUI_B(vc) - b) * pat * 0.3;
            }
        }
        out[x] = rgb(r, g, b);
    }
}

static void brushed_metal(Tex *t, WuiColor metal, int brass)
{
    wui_random_init(&t->rnd, brass ? 5501 : 5502);
    MetalCtx c;
    c.s1 = grid(t, 2, 512);
    c.s2 = grid(t, 8, 512);
    c.s3 = grid(t, 32, 256);
    c.s4 = grid(t, 4, 64);
    c.broad[0] = grid(t, 2, 4);
    c.broad[1] = grid(t, 4, 8);
    octaves(t, c.where, 2, 3);
    octaves(t, c.crust, 3, 16);
    c.metal = metal;
    c.brass = brass;
    noise_fill(t, 1);
    run_rows(t, metal_row, &c);

    if (brass) {
        // мелкие раковинки патины вокруг пятен
        for (int i = 0; i < 1500; i++) {
            double cx = rnd(t) * SIZE;
            double cy = rnd(t) * SIZE;
            double zone = smooth(0.78, 0.9, fbm(c.where, 2, cx, cy));
            if (rnd(t) > zone) continue;
            double r = 0.6 + rnd(t) * 0.9;
            double a = 0.35 + rnd(t) * 0.35;
            speck(t, cx, cy, r, 0xFF5E9C88, a);
        }
    }
}

// ---- Wood: годичные кольца вдоль плитки, изогнутые сучками; поры и тонкие штрихи ----

typedef struct { Grid *warp[3], *streak[2], *pore, *tone[2]; WuiColor wood; } WoodCtx;

static void wood_row(Tex *t, const void *p, int y)
{
    const WoodCtx *c = p;
    const int rings = 13;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        // колец целое число на плитку — изогнутые полосы сходятся на краях
        double bend = (fbm_px((Grid **)c->warp, 3, x, y) - 0.5) * 70 + tex_sin(u / SIZE * PI * 2 + 1.3) * 9;
        double f = (v + bend) * rings / SIZE;
        f -= floor(f);
        double late = crt_pow(f, 5);
        double st = (fbm_px((Grid **)c->streak, 2, x, y) - 0.5);
        double pp = smp_px(c->pore, x, y);
        double pores = smooth(0.78, 0.92, pp) * (0.4 + late);
        double shade = -late * 30 + (f - 0.5) * 6 + st * 26 - pores * 22
                     + (fbm_px((Grid **)c->tone, 2, x, y) - 0.5) * 18 + (noise_at(t, x, y, 0) - 0.5) * 4;
        out[x] = rgb(WUI_R(c->wood) + shade, WUI_G(c->wood) + shade * 0.82, WUI_B(c->wood) + shade * 0.66);
    }
}

static void wood(Tex *t, WuiColor col)
{
    wui_random_init(&t->rnd, 4401);
    WoodCtx c;
    c.warp[0] = grid(t, 3, 6);
    c.warp[1] = grid(t, 6, 12);
    c.warp[2] = grid(t, 12, 24);
    c.streak[0] = grid(t, 4, 256);
    c.streak[1] = grid(t, 8, 512);
    c.pore = grid(t, 96, 512);
    c.tone[0] = grid(t, 2, 4);
    c.tone[1] = grid(t, 4, 8);
    c.wood = col;
    noise_fill(t, 1);
    run_rows(t, wood_row, &c);
}

// ---- Leather: округлые бугорки, тёмные складки между ними, пятнистый тон, морщины ----

typedef struct { double *cells; Grid *mottle[3]; Waves wrinkles; WuiColor hide; } LeatherCtx;

static void leather_row(Tex *t, const void *p, int y)
{
    const LeatherCtx *c = p;
    const int n = 96;
    double cell = (double)SIZE / n;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double f1, f2;
        int id;
        nearest(c->cells, n, u, v, &f1, &f2, &id);
        double dome = 1 - dmin(1, f1 / (cell * 0.75));
        double crease = 1 - smooth(0, cell * 0.22, f2 - f1);
        double wr = wave(&c->wrinkles, u, v);
        double fold = smooth(0.46, 0.5, wr) * (1 - smooth(0.5, 0.54, wr));
        double shade = dome * 12 - crease * 24 - fold * 10
                     + (fbm_px((Grid **)c->mottle, 3, x, y) - 0.5) * 22 + (noise_at(t, x, y, 0) - 0.5) * 4;
        out[x] = rgb(WUI_R(c->hide) + shade, WUI_G(c->hide) + shade * 0.9, WUI_B(c->hide) + shade * 0.8);
    }
}

static void leather(Tex *t, WuiColor hide)
{
    wui_random_init(&t->rnd, 6203);
    LeatherCtx c;
    c.cells = cells(t, 96);
    octaves(t, c.mottle, 3, 4);
    waves(t, &c.wrinkles, 7, 5);
    c.hide = hide;
    noise_fill(t, 1);
    run_rows(t, leather_row, &c);
    free(c.cells);
}

// ---- Linen: полотняное плетение чуть неровных нитей основы и утка, с утолщениями ----

#define LINEN_THREADS 128       // (int)(Size / pitch)

typedef struct { double warp_tone[LINEN_THREADS], weft_tone[LINEN_THREADS]; Grid *warp_slub, *weft_slub, *mottle[3]; WuiColor cloth; } LinenCtx;

static void linen_row(Tex *t, const void *p, int y)
{
    const LinenCtx *c = p;
    const double pitch = 4;
    const int threads = LINEN_THREADS;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        int i = (int)(u / pitch) % threads, j = (int)(v / pitch) % threads;
        double fu = u / pitch - floor(u / pitch), fv = v / pitch - floor(v / pitch);
        int warp_on_top = ((i + j) & 1) == 0;
        double across = warp_on_top ? fu : fv;
        double along = warp_on_top ? fv : fu;
        double slub = warp_on_top ? smp_px(c->warp_slub, x, y) : smp_px(c->weft_slub, x, y);
        double width = 0.78 + slub * 0.22;
        double edge = fabs(across - 0.5) * 2 / width;
        double round = edge >= 1 ? 0 : sqrt(1 - edge * edge);
        // нить ныряет там, где уходит под соседнюю
        double dip = tex_sin(along * PI);
        double tone = warp_on_top ? c->warp_tone[i] : c->weft_tone[j];
        double shade = (round * (0.55 + 0.45 * dip) - 0.62) * 34 + tone * 12 + (slub - 0.5) * 10
                     + (fbm_px((Grid **)c->mottle, 3, x, y) - 0.5) * 12 + (noise_at(t, x, y, 0) - 0.5) * 3;
        out[x] = rgb(WUI_R(c->cloth) + shade, WUI_G(c->cloth) + shade * 0.97, WUI_B(c->cloth) + shade * 0.9);
    }
}

static void linen(Tex *t, WuiColor cloth)
{
    wui_random_init(&t->rnd, 7307);
    LinenCtx *c = malloc(sizeof(LinenCtx));
    for (int i = 0; i < LINEN_THREADS; i++) {
        c->warp_tone[i] = rnd(t) - 0.5;
        c->weft_tone[i] = rnd(t) - 0.5;
    }
    c->warp_slub = grid(t, LINEN_THREADS, 24);
    c->weft_slub = grid(t, 24, LINEN_THREADS);
    octaves(t, c->mottle, 3, 4);
    c->cloth = cloth;
    noise_fill(t, 1);
    run_rows(t, linen_row, c);
    free(c);
}

// ---- Granite: мозаика кристаллов — полевой шпат, серый кварц, чёрная слюда, белые зёрна ----

typedef struct { double *cells; WuiColor *kinds; Grid *cloud[2]; } GraniteCtx;

static void granite_row(Tex *t, const void *p, int y)
{
    const GraniteCtx *c = p;
    const int n = 96;
    double cell = (double)SIZE / n;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double f1, f2;
        int id;
        nearest(c->cells, n, u, v, &f1, &f2, &id);
        WuiColor col = c->kinds[id];
        double seam = 1 - smooth(0, cell * 0.12, f2 - f1);
        double shade = -seam * 6 + (fbm_px((Grid **)c->cloud, 2, x, y) - 0.5) * 12 + (noise_at(t, x, y, 0) - 0.5) * 4;
        out[x] = rgb(WUI_R(col) + shade, WUI_G(col) + shade, WUI_B(col) + shade);
    }
}

static void granite(Tex *t, WuiColor stone)
{
    const int n = 96;
    wui_random_init(&t->rnd, 8111);
    GraniteCtx c;
    c.cells = cells(t, n);
    c.kinds = malloc(sizeof(WuiColor) * n * n);
    for (int k = 0; k < n * n; k++) {
        double r = rnd(t);
        double vary = (rnd(t) - 0.5) * 0.06;
        // близко к основному цвету: текст должен читаться прямо на камне
        c.kinds[k] = r < 0.5 ? (vary > 0 ? wui_lighten(stone, vary) : wui_darken(stone, -vary))
                   : r < 0.75 ? wui_lighten(wui_mix(stone, 0xFF909094, 0.2), 0.02 + vary * 0.3)
                   : r < 0.92 ? wui_darken(stone, 0.14 + vary)
                   : wui_lighten(stone, 0.1 + vary);
    }
    octaves(t, c.cloud, 2, 4);
    noise_fill(t, 1);
    run_rows(t, granite_row, &c);
    free(c.cells);
    free(c.kinds);
}

// ---- Slate: тёмный слоистый камень, облака и слабые светлые царапины ----

typedef struct { Grid *strata[3], *cloud[3], *dust[2]; WuiColor stone; } SlateCtx;

static void slate_row(Tex *t, const void *p, int y)
{
    const SlateCtx *c = p;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double layer = fbm_px((Grid **)c->strata, 3, x, y) - 0.5;
        double cl = fbm_px((Grid **)c->cloud, 3, x, y) - 0.5;
        double haze = smooth(0.55, 0.8, fbm_px((Grid **)c->dust, 2, x, y));
        double shade = layer * 22 + cl * 16 + haze * 9 + (noise_at(t, x, y, 0) - 0.5) * 6;
        out[x] = rgb(WUI_R(c->stone) + shade, WUI_G(c->stone) + shade, WUI_B(c->stone) + shade * 1.08);
    }
}

static void slate(Tex *t, WuiColor stone)
{
    wui_random_init(&t->rnd, 9013);
    SlateCtx c;
    c.strata[0] = grid(t, 3, 48);
    c.strata[1] = grid(t, 6, 96);
    c.strata[2] = grid(t, 12, 192);
    octaves(t, c.cloud, 3, 3);
    octaves(t, c.dust, 2, 5);
    c.stone = stone;
    noise_fill(t, 1);
    run_rows(t, slate_row, &c);

    // слабые царапины: тонкие светлые штрихи в разные стороны
    WuiColor pale = wui_lighten(stone, 0.45);
    for (int i = 0; i < 70; i++) {
        double cx = rnd(t) * SIZE;
        double cy = rnd(t) * SIZE;
        double ang = rnd(t) * PI;
        double len = 10 + rnd(t) * 50;
        double a = 0.05 + rnd(t) * 0.1;
        double dx = tex_cos(ang), dy = tex_sin(ang);
        for (double d = 0; d < len; d += 0.5) {
            double fade = dmin(1, dmin(d, len - d) / 6);
            speck(t, cx + dx * d, cy + dy * d, 0.5, pale, a * fade);
        }
    }
}

// ---- Carbon: саржа 2×2 из блестящих жгутов; горизонтальные светлее вертикальных ----

typedef struct { Grid *h_fib, *v_fib, *sheen[2]; WuiColor fibre; } CarbonCtx;

static void carbon_row(Tex *t, const void *p, int y)
{
    const CarbonCtx *c = p;
    const double cell = 8;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        int i = (int)(u / cell), j = (int)(v / cell);
        double fu = u / cell - i, fv = v / cell - j;
        int horizontal = ((i + j) & 3) < 2;
        double across = horizontal ? fv : fu;
        double gloss = crt_pow(tex_sin(across * PI), 1.6);
        double fib = horizontal ? smp_px(c->h_fib, x, y) : smp_px(c->v_fib, x, y);
        double gap = 1 - smooth(0, 0.08, dmin(across, 1 - across));
        double light = horizontal ? 18 : 9;
        double shade = gloss * light + (fib - 0.5) * 8 - gap * 8
                     + (fbm_px((Grid **)c->sheen, 2, x, y) - 0.5) * 10 + (noise_at(t, x, y, 0) - 0.5) * 3;
        out[x] = rgb(WUI_R(c->fibre) + shade, WUI_G(c->fibre) + shade, WUI_B(c->fibre) + shade * 1.06);
    }
}

static void carbon(Tex *t, WuiColor fibre)
{
    wui_random_init(&t->rnd, 1021);
    CarbonCtx c;
    c.h_fib = grid(t, 6, 512);
    c.v_fib = grid(t, 512, 6);
    octaves(t, c.sheen, 2, 2);
    c.fibre = fibre;
    noise_fill(t, 1);
    run_rows(t, carbon_row, &c);
}

// ---- Frost: бледный лёд, мягкие облака, перистые кристаллы-папоротники и искры ----

// Один ледяной папоротник: чуть изогнутый стебель, боковые ветви под ~60°, всё тоньше вглубь
static void fern(Tex *t, double *buf, double x, double y, double ang, double len, int depth)
{
    double width = depth == 0 ? 0.55 : depth == 1 ? 0.42 : 0.32;
    double amount = depth == 0 ? 0.32 : depth == 1 ? 0.24 : 0.16;
    double next = 3 + rnd(t) * 4;
    for (double d = 0; d < len; d += 0.4) {
        ang += (rnd(t) - 0.5) * 0.02;
        x += tex_cos(ang) * 0.4;
        y += tex_sin(ang) * 0.4;
        double fade = 1 - d / len * 0.7;
        splat(t, buf, x, y, width, amount * fade);
        if (depth < 2 && d >= next) {
            next = d + 3 + rnd(t) * 4;
            double rest = (len - d) * (0.35 + rnd(t) * 0.25);
            double side = rnd(t) < 0.5 ? 1 : -1;
            fern(t, buf, x, y, ang + side * PI / 3, rest, depth + 1);
        }
    }
}

typedef struct { Grid *cloud[4]; double *crystal; WuiColor ice; } FrostCtx;

static void frost_row(Tex *t, const void *p, int y)
{
    const FrostCtx *c = p;
    int px = t->px;
    unsigned *out = t->pix + y * px;
    for (int x = 0; x < px; x++) {
        int k = y * px + x;
        double shade = (fbm_px((Grid **)c->cloud, 4, x, y) - 0.5) * 18 + (noise_at(t, x, y, 0) - 0.5) * 3;
        double r = WUI_R(c->ice) + shade, g = WUI_G(c->ice) + shade, b = WUI_B(c->ice) + shade * 0.6;
        double a = 1 - crt_exp(-c->crystal[k] * 2.2);
        r += (255 - r) * a * 0.75; g += (255 - g) * a * 0.75; b += (255 - b) * a * 0.75;
        out[x] = rgb(r, g, b);
    }
}

static void frost(Tex *t, WuiColor ice)
{
    wui_random_init(&t->rnd, 1213);
    FrostCtx c;
    octaves(t, c.cloud, 4, 3);
    int px = t->px;
    c.crystal = calloc((size_t)px * px, sizeof(double));
    for (int i = 0; i < 46; i++) {
        double x = rnd(t) * SIZE;
        double y = rnd(t) * SIZE;
        double ang = rnd(t) * PI * 2;
        double len = 50 + rnd(t) * 110;
        fern(t, c.crystal, x, y, ang, len, 0);
    }
    c.ice = ice;
    noise_fill(t, 1);
    run_rows(t, frost_row, &c);
    free(c.crystal);

    for (int i = 0; i < 260; i++) {
        double x = rnd(t) * SIZE;
        double y = rnd(t) * SIZE;
        double r = 0.5 + rnd(t) * 0.7;
        double a = 0.5 + rnd(t) * 0.5;
        speck(t, x, y, r, 0xFFFFFFFF, a);
    }
}

// ---- Sand: ветровая рябь — пологий светлый склон, крутой затенённый, и песчинки ----

typedef struct { Grid *warp[3], *calm[2], *tone[3]; WuiColor sand; } SandCtx;

static void sand_row(Tex *t, const void *p, int y)
{
    const SandCtx *c = p;
    const int ripples = 38;
    int s = t->s;
    double v = (double)y / s;
    unsigned *out = t->pix + y * t->px;
    for (int x = 0; x < t->px; x++) {
        double u = (double)x / s;
        double bend = (fbm_px((Grid **)c->warp, 3, x, y) - 0.5) * 60 + tex_sin(u / SIZE * PI * 4 + 0.7) * 7;
        double f = (v + bend) * ripples / SIZE;
        f -= floor(f);
        // наветренный склон (на свету) до гребня у 0.72, потом короткий крутой в тени
        double lit = smooth(0.05, 0.6, f) * (1 - smooth(0.66, 0.74, f));
        double lee = smooth(0.7, 0.78, f) * (1 - smooth(0.9, 1.0, f));
        double strength = 0.35 + 0.65 * smooth(0.3, 0.7, fbm_px((Grid **)c->calm, 2, x, y));
        double shade = (lit * 9 - lee * 18) * strength
                     + (fbm_px((Grid **)c->tone, 3, x, y) - 0.5) * 16 + (noise_at(t, x, y, 0) - 0.5) * 12;
        out[x] = rgb(WUI_R(c->sand) + shade, WUI_G(c->sand) + shade * 0.92, WUI_B(c->sand) + shade * 0.8);
    }
}

static void sand(Tex *t, WuiColor col)
{
    wui_random_init(&t->rnd, 1307);
    SandCtx c;
    octaves(t, c.warp, 3, 3);
    octaves(t, c.calm, 2, 3);
    octaves(t, c.tone, 3, 4);
    c.sand = col;
    noise_fill(t, 1);
    run_rows(t, sand_row, &c);

    // тёмные и светлые песчинки
    WuiColor dark = wui_darken(col, 0.35);
    WuiColor pale = wui_lighten(col, 0.35);
    for (int i = 0; i < 2600; i++) {
        double x = rnd(t) * SIZE;
        double y = rnd(t) * SIZE;
        double r = 0.45 + rnd(t) * 0.45;
        WuiColor g = rnd(t) < 0.6 ? dark : pale;
        double a = 0.35 + rnd(t) * 0.4;
        speck(t, x, y, r, g, a);
    }
}

// ======================= вход и кэш =======================

typedef struct TexEntry { int kind, scale; WuiColor base; unsigned *pix; struct TexEntry *next; } TexEntry;
static TexEntry *g_cache;

const unsigned *wui_texture(int kind, WuiColor base, int scale)
{
    if (kind <= WUI_TEX_NONE || kind > WUI_TEX_SAND)
        return 0;
    scale = scale >= 2 ? 2 : 1;             // как ThemeManager.HiDpi: 1 или 2
    for (TexEntry *e = g_cache; e; e = e->next)
        if (e->kind == kind && e->scale == scale && e->base == base)
            return e->pix;

    math_init();
    Tex t;
    memset(&t, 0, sizeof(t));
    t.s = scale;
    t.px = SIZE * scale;
    t.pix = malloc(sizeof(unsigned) * t.px * t.px);
    switch (kind) {
    case WUI_TEX_PARCHMENT: parchment(&t, base); break;
    case WUI_TEX_LAPIS:     lapis(&t, base); break;
    case WUI_TEX_PAPYRUS:   papyrus(&t, base); break;
    case WUI_TEX_WASHI:     washi(&t, base); break;
    case WUI_TEX_MARBLE:    marble(&t, base); break;
    case WUI_TEX_MALACHITE: malachite(&t, base); break;
    case WUI_TEX_OBSIDIAN:  obsidian(&t, base); break;
    case WUI_TEX_BRASS:     brushed_metal(&t, base, 1); break;
    case WUI_TEX_ALUMINIUM: brushed_metal(&t, base, 0); break;
    case WUI_TEX_WOOD:      wood(&t, base); break;
    case WUI_TEX_LEATHER:   leather(&t, base); break;
    case WUI_TEX_LINEN:     linen(&t, base); break;
    case WUI_TEX_GRANITE:   granite(&t, base); break;
    case WUI_TEX_SLATE:     slate(&t, base); break;
    case WUI_TEX_CARBON:    carbon(&t, base); break;
    case WUI_TEX_FROST:     frost(&t, base); break;
    case WUI_TEX_SAND:      sand(&t, base); break;
    }
    for (int i = 0; i < t.ngrids; i++)
        grid_free(t.grids[i]);

    TexEntry *e = malloc(sizeof(TexEntry));
    e->kind = kind;
    e->scale = scale;
    e->base = base;
    e->pix = t.pix;
    e->next = g_cache;
    g_cache = e;
    return e->pix;
}
