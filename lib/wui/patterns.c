// WUI — узоры фона: соты, цветок жизни, печатная плата, сэйгайха, горизонтали.
// Прозрачные бесшовные плитки одного цвета, как Textures.Pattern в ThemeLibrary, и заливка
// ими всего окна от левого верхнего угла — Backdrop.DrawPattern.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

// ---- кэш плиток: живёт до конца программы ----

typedef struct Tile {
    int kind;
    WuiColor ink;
    float scale;
    int pw, ph;                 // размер в точках
    int dw, dh;                 // размер в DIP
    GpBitmap *bitmap;
    unsigned *pixels;           // точки плитки, premultiplied (битмап смотрит в них)
    struct Tile *next;
} Tile;

static Tile *tiles;

// Краска узора в теме: второй акцент на тёмных, цвет текста на светлых; бледная.
static WuiColor pattern_ink(const WuiTheme *t, int kind)
{
    WuiColor c = t->is_dark ? t->c[WUI_C_ACCENT2] : t->c[WUI_C_TEXT];
    int a;
    switch (kind) {
    case WUI_DECOR_CIRCUIT: a = 0x34; break;
    case WUI_DECOR_CONTOURS: a = 0x30; break;
    case WUI_DECOR_WAVES: a = 0x26; break;
    default: a = 0x22; break;
    }
    if (!t->is_dark) a = (int)(a * 0.8);
    return wui_alpha(c, a);
}

static double smooth_step(double a, double b, double v)
{
    double t = (v - a) / (b - a);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t * t * (3 - 2 * t);
}

static int wrap(int v, int n)
{
    return ((v % n) + n) % n;
}

// Точка краски с покрытием 0..1, premultiplied — как Textures.Premultiplied.
static unsigned premultiplied(WuiColor c, double coverage)
{
    if (coverage < 0) coverage = 0;
    if (coverage > 1) coverage = 1;
    double a = WUI_A(c) / 255.0 * coverage;
    unsigned ai = (unsigned)wui_round(a * 255);
    return (ai << 24) | ((unsigned)wui_round(WUI_R(c) * a) << 16) |
           ((unsigned)wui_round(WUI_G(c) * a) << 8) | (unsigned)wui_round(WUI_B(c) * a);
}

// ---- векторные узоры: рисуются непрозрачной краской, прозрачность — всей плитке разом ----

static GpPen *ink_pen(WuiColor c, float width)
{
    GpPen *pen;
    GdipCreatePen1(c, width, UnitWorld, &pen);
    GdipSetPenLineJoin(pen, LineJoinRound);
    GdipSetPenStartCap(pen, LineCapRound);
    GdipSetPenEndCap(pen, LineCapRound);
    return pen;
}

static void fill_dot(GpGraphics *g, WuiColor c, double x, double y, double r)
{
    GpSolidFill *brush;
    GdipCreateSolidFill(c, &brush);
    GdipFillEllipse(g, brush, (REAL)(x - r), (REAL)(y - r), (REAL)(2 * r), (REAL)(2 * r));
    GdipDeleteBrush(brush);
}

static void ring(GpGraphics *g, GpPen *pen, double x, double y, double r)
{
    GdipDrawEllipse(g, pen, (REAL)(x - r), (REAL)(y - r), (REAL)(2 * r), (REAL)(2 * r));
}

// Соты: два шестиугольника с плоским верхом на клетку, второй сдвинут на полряда.
static void draw_honeycomb(GpGraphics *g, WuiColor ink, double w, double h)
{
    double rx = w / 3, ry = h / sqrt(3.0);
    double cx[2], cy[2];
    GpPen *pen = ink_pen(ink, 1);
    cx[0] = 0;
    cy[0] = 0;
    cx[1] = w / 2;
    cy[1] = h / 2;
    for (int k = 0; k < 2; k++) {
        PointF p[6];
        for (int i = 0; i < 6; i++) {
            double a = i * 3.14159265358979323846 / 3;
            p[i].X = (REAL)(cx[k] + cos(a) * rx);
            p[i].Y = (REAL)(cy[k] + sin(a) * ry);
        }
        GdipDrawPolygon(g, pen, p, 6);
        fill_dot(g, ink, cx[k], cy[k], 1.2);
    }
    GdipDeletePen(pen);
}

// Цветок жизни: круги радиусом в шаг решётки на треугольной сетке.
static void draw_flower(GpGraphics *g, WuiColor ink, double w, double h)
{
    double r = w / 2;
    GpPen *pen = ink_pen(ink, 0.9f);
    ring(g, pen, 0, 0, r);
    ring(g, pen, r, 0, r);
    ring(g, pen, r / 2, h / 2, r);
    ring(g, pen, r * 1.5, h / 2, r);
    GdipDeletePen(pen);
}

// Скруглённый прямоугольник как DrawRoundedRectangle WPF (радиус r по обеим осям).
static void add_round_rect(GpPath *path, double x, double y, double w, double h, double r)
{
    REAL d = (REAL)(2 * r);
    GdipStartPathFigure(path);
    GdipAddPathArc(path, (REAL)x, (REAL)y, d, d, 180, 90);
    GdipAddPathArc(path, (REAL)(x + w) - d, (REAL)y, d, d, 270, 90);
    GdipAddPathArc(path, (REAL)(x + w) - d, (REAL)(y + h) - d, d, d, 0, 90);
    GdipAddPathArc(path, (REAL)x, (REAL)(y + h) - d, d, d, 90, 90);
    GdipClosePathFigure(path);
}

// Печатная плата: дорожки по сетке 16 DIP с поворотами на 45°, круглые площадки, две микросхемы.
static void draw_circuit(GpGraphics *g, WuiColor ink, double w, double h)
{
    static const int dirs[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
    const double step = 16;
    int n = (int)(w / step);
    char used[64 * 64];
    WuiRandom rnd;
    GpPen *trace = ink_pen(ink, 1.3f);
    GpPen *pad_pen = ink_pen(ink, 1.2f);
    GpPen *chip_pen = ink_pen(ink, 1.2f);
    (void)h;
    if (n > 64) n = 64;
    memset(used, 0, sizeof used);
    wui_random_init(&rnd, 2468);

    // сначала микросхемы — дорожки могут начинаться у их ножек
    double chips[2][4] = { { 3 * step, 4 * step, 4 * step, 2 * step }, { 10 * step, 11 * step, 3 * step, 3 * step } };
    for (int c = 0; c < 2; c++) {
        double left = chips[c][0], top = chips[c][1], right = left + chips[c][2], bottom = top + chips[c][3];
        GpPath *path;
        GdipCreatePath(FillModeWinding, &path);
        add_round_rect(path, left, top, chips[c][2], chips[c][3], 2);
        GdipDrawPath(g, chip_pen, path);
        GdipDeletePath(path);
        for (double x = left + step / 2; x < right; x += step / 2) {
            GdipDrawLine(g, trace, (REAL)x, (REAL)(top - 4), (REAL)x, (REAL)top);
            GdipDrawLine(g, trace, (REAL)x, (REAL)bottom, (REAL)x, (REAL)(bottom + 4));
        }
        for (int gx = (int)(left / step); gx <= (int)(right / step); gx++)
            for (int gy = (int)(top / step); gy <= (int)(bottom / step); gy++)
                used[gy * n + gx] = 1;
    }

    for (int k = 0; k < 34; k++) {
        int x = wui_random_below(&rnd, n);
        int y = wui_random_below(&rnd, n);
        if (used[y * n + x]) continue;
        int d = wui_random_below(&rnd, 8);
        int len = 3 + wui_random_below(&rnd, 9);
        PointF pts[16];
        int count = 0;
        pts[count].X = (REAL)(x * step);
        pts[count].Y = (REAL)(y * step);
        count++;
        used[y * n + x] = 1;
        for (int i = 0; i < len; i++) {
            if (i > 0 && wui_random_double(&rnd) < 0.3) {
                int turn = wui_random_double(&rnd) < 0.5 ? 1 : 7;
                d = (d + turn) % 8;
            }
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            int key = wrap(ny, n) * n + wrap(nx, n);
            if (used[key]) break;
            used[key] = 1;
            x = nx;
            y = ny;
            pts[count].X = (REAL)(x * step);
            pts[count].Y = (REAL)(y * step);
            count++;
        }
        if (count < 2) continue;
        GdipDrawLines(g, trace, pts, count);
        ring(g, pad_pen, pts[0].X, pts[0].Y, 3);
        fill_dot(g, ink, pts[count - 1].X, pts[count - 1].Y, 2.6);
    }
    GdipDeletePen(trace);
    GdipDeletePen(pad_pen);
    GdipDeletePen(chip_pen);
}

// Плитка векторного узора: рисунок в клетке w×h повторён со сдвигами на соседние клетки и
// обрезан — линии через край продолжаются с другой стороны. Линии рисуются непрозрачными,
// потом вся плитка получает прозрачность краски — пересечения не темнеют (как PushOpacity WPF).
// Рисуется вчетверо крупнее и усредняется по 4×4: линии тоньше полутора точек GDI+ иначе
// рисует ровно в точку (1.3 DIP вышли бы бледнее, чем у WPF).
#define TILE_SS 4

static void vector_tile(unsigned *pixels, int kind, WuiColor ink, int dw, int dh, int pw, int ph)
{
    GpBitmap *bmp;
    GpGraphics *g;
    WuiColor solid = wui_alpha(ink, 255);
    int bw = pw * TILE_SS, bh = ph * TILE_SS;    GdipCreateBitmapFromScan0(bw, bh, 0, PixelFormat32bppPARGB, 0, &bmp);
    GdipGetImageGraphicsContext(bmp, &g);
    GdipGraphicsClear(g, 0);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipScaleWorldTransform(g, (REAL)bw / dw, (REAL)bh / dh, MatrixOrderPrepend);
    GdipSetClipRect(g, 0, 0, (REAL)dw, (REAL)dh, CombineModeReplace);
    for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
            UINT state;
            GdipSaveGraphics(g, &state);
            GdipTranslateWorldTransform(g, (REAL)(ox * dw), (REAL)(oy * dh), MatrixOrderPrepend);
            if (kind == WUI_DECOR_HEXAGONS) draw_honeycomb(g, solid, dw, dh);
            else if (kind == WUI_DECOR_FLOWEROFLIFE) draw_flower(g, solid, dw, dh);
            else draw_circuit(g, solid, dw, dh);
            GdipRestoreGraphics(g, state);
        }
    GdipDeleteGraphics(g);

    // среднее по 4×4 и прозрачность краски — всем точкам разом
    Rect r;
    BitmapData data;
    int a = WUI_A(ink), n = TILE_SS * TILE_SS;
    r.X = 0;
    r.Y = 0;
    r.Width = bw;
    r.Height = bh;
    memset(pixels, 0, sizeof(unsigned) * pw * ph);
    if (GdipBitmapLockBits(bmp, &r, ImageLockModeRead, PixelFormat32bppPARGB, &data) == Ok) {
        for (int y = 0; y < ph; y++)
            for (int x = 0; x < pw; x++) {
                unsigned sum[4] = { 0, 0, 0, 0 }, out = 0;
                for (int sy = 0; sy < TILE_SS; sy++) {
                    const unsigned char *p = (const unsigned char *)data.Scan0 + (y * TILE_SS + sy) * data.Stride + x * TILE_SS * 4;
                    for (int i = 0; i < TILE_SS * 4; i++) sum[i & 3] += p[i];
                }
                for (int k = 0; k < 4; k++) {
                    unsigned v = (sum[k] + n / 2) / n;
                    out |= ((v * a + 127) / 255) << (8 * k);
                }
                pixels[y * pw + x] = out;
            }
        GdipBitmapUnlockBits(bmp, &data);
    }
    GdipDisposeImage(bmp);
}

// ---- растровые узоры: покрытие считается в каждой точке ----

// Сэйгайха: веера из четырёх дуг; каждый ряд закрывает нижнюю половину ряда выше.
static void seigaiha(unsigned *pixels, WuiColor ink, int pw, int ph, double s)
{
    const double R = 28, row_step = R / 2;
    double lw = 1.0;
    for (int y = 0; y < ph; y++)
        for (int x = 0; x < pw; x++) {
            double u = (x + 0.5) / s, v = (y + 0.5) / s;
            double a = 0;
            // нижние ряды впереди: берётся самый нижний центр, чей веер накрывает точку
            int row = (int)floor(v / row_step);
            for (int k = row + 2; k >= row - 1; k--) {
                double cy = k * row_step;
                double cx = (k & 1) != 0 ? R : 0;
                double dx = u - cx;
                dx -= 2 * R * wui_round(dx / (2 * R));
                double dy = v - cy;
                double d = sqrt(dx * dx + dy * dy);
                if (d > R) continue;
                double ring_n = d / (R / 4);
                double off = fabs(ring_n - wui_round(ring_n)) * (R / 4);
                if (wui_round(ring_n) < 1) {
                    double inner = fabs(d - R / 4);
                    if (inner > off) off = inner;
                }
                a = 1 - smooth_step(lw / 2 - 0.5 / s, lw / 2 + 0.5 / s, off);
                break;
            }
            pixels[y * pw + x] = premultiplied(ink, a);
        }
}


// Шум для горизонталей — как Grid/Smp/Fbm/Waves/Wave из TexturesExtra (плитка 512 DIP).
// Высота нужна в каждой точке и на полDIP по сторонам (наклон), поэтому всё, что зависит
// только от столбца или только от строки, считается заранее — числа те же, что у C#.
#define NOISE_SIZE 512
#define WAVE_COUNT 12

typedef struct { int nx, ny; double *g; } NoiseGrid;

static void noise_grid(NoiseGrid *grid, WuiRandom *rnd, int nx, int ny)
{
    grid->nx = nx;
    grid->ny = ny;
    grid->g = (double *)malloc(sizeof(double) * nx * ny);
    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++)
            grid->g[x * ny + y] = wui_random_double(rnd);
}

// Плоские волны с целыми частотами (потому бесшовные) в случайных направлениях.
typedef struct { double m[WAVE_COUNT], n[WAVE_COUNT], phase[WAVE_COUNT], amp[WAVE_COUNT]; } WaveSet;

static void wave_set(WaveSet *w, WuiRandom *rnd, int max_freq)
{
    for (int i = 0; i < WAVE_COUNT; i++) {
        int m, n;
        do {
            m = wui_random_range(rnd, -max_freq, max_freq + 1);
            n = wui_random_range(rnd, -max_freq, max_freq + 1);
        } while (m == 0 && n == 0);
        w->m[i] = m;
        w->n[i] = n;
        w->phase[i] = wui_random_double(rnd) * 3.14159265358979323846 * 2;
        w->amp[i] = 1.0 / sqrt((double)(m * m + n * n));
    }
}

// Одна ось (столбцы или строки): точки first..first+count−1, сдвиги −0.5, 0, +0.5 DIP (или один — 0).
//  волны: sin(A + B) = sinA·cosB + cosA·sinB — синусы и косинусы своей части угла;
//  шум: соседние узлы решётки и вес между ними у каждой из двух октав.
typedef struct {
    int count;
    double *sn, *cs;            // [сдвиг][точка][волна]
    int *a[2], *b[2];           // [октава][сдвиг][точка]
    double *f[2];
} Axis;

static void axis_make(Axis *ax, const WaveSet *w, const NoiseGrid *grids, int first, int count, int shifts,
                      double s, int is_v)
{
    const double k = 3.14159265358979323846 * 2 / NOISE_SIZE;
    ax->count = count;
    ax->sn = (double *)malloc(sizeof(double) * shifts * count * WAVE_COUNT);
    ax->cs = (double *)malloc(sizeof(double) * shifts * count * WAVE_COUNT);
    for (int oc = 0; oc < 2; oc++) {
        ax->a[oc] = (int *)malloc(sizeof(int) * shifts * count);
        ax->b[oc] = (int *)malloc(sizeof(int) * shifts * count);
        ax->f[oc] = (double *)malloc(sizeof(double) * shifts * count);
    }
    for (int o = 0; o < shifts; o++)
        for (int j = 0; j < count; j++) {
            int x = first + j;
            double u = (x + 0.5) / s + (shifts == 3 ? (o - 1) * 0.5 : 0);
            int at = o * count + j;
            for (int i = 0; i < WAVE_COUNT; i++) {
                double angle = is_v ? w->n[i] * u * k + w->phase[i] : w->m[i] * u * k;
                ax->sn[at * WAVE_COUNT + i] = sin(angle);
                ax->cs[at * WAVE_COUNT + i] = cos(angle);
            }
            for (int oc = 0; oc < 2; oc++) {
                int n = is_v ? grids[oc].ny : grids[oc].nx;
                double t = u * n / NOISE_SIZE;
                int t0 = (int)floor(t);
                ax->f[oc][at] = smooth_step(0, 1, t - t0);
                ax->a[oc][at] = wrap(t0, n);
                ax->b[oc][at] = wrap(t0 + 1, n);
            }
        }
}

static void axis_free(Axis *ax)
{
    free(ax->sn);
    free(ax->cs);
    for (int oc = 0; oc < 2; oc++) {
        free(ax->a[oc]);
        free(ax->b[oc]);
        free(ax->f[oc]);
    }
}

typedef struct {
    WaveSet waves;
    double total;               // сумма весов волн
    NoiseGrid detail[2];
    Axis u, v;
} Relief;

// Высота в точке (x, y) со сдвигами ou, ov: волны · 0.85 + мелкий шум (Fbm) · 0.15.
static double relief_height(const Relief *r, int ou, int x, int ov, int y)
{
    int iu = ou * r->u.count + x, iv = ov * r->v.count + y;
    const double *su = r->u.sn + iu * WAVE_COUNT, *cu = r->u.cs + iu * WAVE_COUNT;
    const double *sv = r->v.sn + iv * WAVE_COUNT, *cv = r->v.cs + iv * WAVE_COUNT;
    double sum = 0;
    for (int i = 0; i < WAVE_COUNT; i++)
        sum += r->waves.amp[i] * (su[i] * cv[i] + cu[i] * sv[i]);
    double wave = 0.5 + 0.5 * sum / r->total;

    double noise = 0, weight = 1, weights = 0;
    for (int oc = 0; oc < 2; oc++) {
        const double *g = r->detail[oc].g;
        int ny = r->detail[oc].ny;
        int ax = r->u.a[oc][iu], bx = r->u.b[oc][iu], ay = r->v.a[oc][iv], by = r->v.b[oc][iv];
        double fx = r->u.f[oc][iu], fy = r->v.f[oc][iv];
        double top = g[ax * ny + ay] + (g[bx * ny + ay] - g[ax * ny + ay]) * fx;
        double bottom = g[ax * ny + by] + (g[bx * ny + by] - g[ax * ny + by]) * fx;
        noise += (top + (bottom - top) * fy) * weight;
        weights += weight;
        weight *= 0.5;
    }
    return wave * 0.85 + noise / weights * 0.15;
}

// Карта рельефа: горизонтали гладкого бесшовного шума; каждая пятая — жирнее.
static void contour_lines(unsigned *pixels, WuiColor ink, int px, double s)
{
    Relief r;
    WuiRandom rnd;
    const double levels = 16;
    wui_random_init(&rnd, 3571);
    wave_set(&r.waves, &rnd, 4);
    noise_grid(&r.detail[0], &rnd, 16, 16);
    noise_grid(&r.detail[1], &rnd, 32, 32);
    r.total = 0;
    for (int i = 0; i < WAVE_COUNT; i++) r.total += r.waves.amp[i];
    // ПолDIP — целое число точек (масштаб 2, 4...): высоты по сторонам — это высоты соседних
    // точек, и всё поле считается один раз с каймой в d точек. Иначе — пять высот на точку.
    int d = (int)(s * 0.5);
    double *field = 0;
    int n = px + 2 * d;
    if (d >= 1 && d == s * 0.5) {
        axis_make(&r.u, &r.waves, r.detail, -d, n, 1, s, 0);
        axis_make(&r.v, &r.waves, r.detail, -d, n, 1, s, 1);
        field = (double *)malloc(sizeof(double) * n * n);
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) field[j * n + i] = relief_height(&r, 0, i, 0, j);
    } else {
        axis_make(&r.u, &r.waves, r.detail, 0, px, 3, s, 0);
        axis_make(&r.v, &r.waves, r.detail, 0, px, 3, s, 1);
    }

    for (int y = 0; y < px; y++)
        for (int x = 0; x < px; x++) {
            double f, gx, gy;
            if (field) {
                const double *c = field + (y + d) * n + x + d;
                f = c[0] * levels;
                gx = (c[d] - c[-d]) * levels;
                gy = (c[d * n] - c[-d * n]) * levels;
            } else {
                // сдвиги: 0 — −0.5 DIP, 1 — в точке, 2 — +0.5 DIP
                f = relief_height(&r, 1, x, 1, y) * levels;
                gx = (relief_height(&r, 2, x, 1, y) - relief_height(&r, 0, x, 1, y)) * levels;
                gy = (relief_height(&r, 1, x, 2, y) - relief_height(&r, 1, x, 0, y)) * levels;
            }
            double grad = sqrt(gx * gx + gy * gy) + 1e-6;
            double nearest = wui_round(f);
            double dist = fabs(f - nearest) / grad;
            int index = ((int)nearest % 5) == 0;
            double lw = index ? 1.6 : 0.85;
            double a = 1 - smooth_step(lw / 2 - 0.5 / s, lw / 2 + 0.5 / s, dist);
            pixels[y * px + x] = premultiplied(ink, a * (index ? 1 : 0.75));
        }

    free(field);
    axis_free(&r.u);
    axis_free(&r.v);
    free(r.detail[0].g);
    free(r.detail[1].g);
}

// ---- плитка по виду ----

static Tile *get_tile(int kind, WuiColor ink, float scale)
{
    for (Tile *t = tiles; t; t = t->next)
        if (t->kind == kind && t->ink == ink && t->scale == scale) return t;

    int dw, dh;
    switch (kind) {
    case WUI_DECOR_HEXAGONS: dw = 78; dh = 45; break;
    case WUI_DECOR_FLOWEROFLIFE: dw = 72; dh = 62; break;
    case WUI_DECOR_CIRCUIT: dw = 256; dh = 256; break;
    case WUI_DECOR_WAVES: dw = 56; dh = 28; break;
    case WUI_DECOR_CONTOURS: dw = NOISE_SIZE; dh = NOISE_SIZE; break;
    default: return 0;
    }
    // точек в плитке — размер в DIP на масштаб; при дробном масштабе — ближайшее целое
    int pw = (int)floor(dw * scale + 0.5), ph = (int)floor(dh * scale + 0.5);
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;

    Tile *t = (Tile *)calloc(1, sizeof(Tile));
    t->kind = kind;
    t->ink = ink;
    t->scale = scale;
    t->dw = dw;
    t->dh = dh;
    t->pw = pw;
    t->ph = ph;
    t->pixels = (unsigned *)malloc(sizeof(unsigned) * pw * ph);
    if (kind == WUI_DECOR_WAVES) seigaiha(t->pixels, ink, pw, ph, (double)pw / dw);
    else if (kind == WUI_DECOR_CONTOURS) contour_lines(t->pixels, ink, pw, (double)pw / dw);
    else vector_tile(t->pixels, kind, ink, dw, dh, pw, ph);
    // битмап смотрит в точки плитки — они живут вместе с ним
    GdipCreateBitmapFromScan0(pw, ph, pw * 4, PixelFormat32bppPARGB, (BYTE *)t->pixels, &t->bitmap);
    t->next = tiles;
    tiles = t;
    return t;
}

void wui_pattern_draw(GpGraphics *g, const WuiTheme *t, float w, float h, int decor_kind, float scale)
{
    Tile *tile = get_tile(decor_kind, pattern_ink(t, decor_kind), scale);
    if (!tile || !tile->bitmap) return;
    GpTexture *brush;
    UINT state;
    GdipCreateTexture(tile->bitmap, WrapModeTile, &brush);
    // плитка в точках ложится на dw×dh DIP; у g уже масштаб, так что точка плитки = точка экрана
    GdipScaleTextureTransform(brush, (REAL)tile->dw / tile->pw, (REAL)tile->dh / tile->ph, MatrixOrderPrepend);
    GdipSaveGraphics(g, &state);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipSetInterpolationMode(g, tile->pw == tile->dw * scale && tile->ph == tile->dh * scale
                                    ? InterpolationModeNearestNeighbor : InterpolationModeHighQualityBilinear);
    GdipFillRectangle(g, brush, 0, 0, w, h);
    GdipRestoreGraphics(g, state);
    GdipDeleteBrush(brush);
}
