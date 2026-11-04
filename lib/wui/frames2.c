// WUI — узорные рамки окна из BackdropFrames.cs: меандр, ар-деко, строчка, кельтская плетёнка, арабеска, руны.
#include <stdlib.h>
#include "frames.h"

#define PI 3.14159265358979323846

// Сторона окна между угловыми блоками: x идёт вдоль неё по часовой, y — внутрь от края.
typedef struct { double ox, oy, ax, ay, ix, iy, len; } Side;

static void sides(double w, double h, double corner, Side *s)
{
    Side v[4] = {
        { corner, 0, 1, 0, 0, 1, w - 2 * corner },
        { w, corner, 0, 1, -1, 0, h - 2 * corner },
        { w - corner, h, -1, 0, 0, -1, w - 2 * corner },
        { 0, h - corner, 0, -1, 1, 0, h - 2 * corner }
    };
    for (int i = 0; i < 4; i++) s[i] = v[i];
}

static FrPt side_at(const Side *s, double x, double y)
{
    FrPt p;
    p.x = s->ox + s->ax * x + s->ix * y;
    p.y = s->oy + s->ay * x + s->iy * y;
    return p;
}

// Восьмиконечная звезда: шестнадцать вершин попеременно на внешнем и внутреннем радиусе.
static GpPath *star8(double cx, double cy, double r)
{
    FrPt pts[16];
    for (int i = 0; i < 16; i++) {
        double a = i * PI / 8 - PI / 2;
        double rr = i % 2 == 0 ? r : r * 0.62;
        pts[i].x = cx + cos(a) * rr;
        pts[i].y = cy + sin(a) * rr;
    }
    return fr_polyline(pts, 16, 1);
}

// Дуга окружности между углами в градусах (0° — вдоль +x, по часовой на экране).
static GpPath *arc(double cx, double cy, double r, double from, double to)
{
    GpPath *p = NULL;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathArc(p, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r), (REAL)from, (REAL)(to - from));
    return p;
}

static GpPath *sector(double cx, double cy, double r, double from, double to)
{
    GpPath *p = NULL;
    double a0 = from * PI / 180;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathLine(p, (REAL)cx, (REAL)cy, (REAL)(cx + cos(a0) * r), (REAL)(cy + sin(a0) * r));
    GdipAddPathArc(p, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r), (REAL)from, (REAL)(to - from));
    GdipClosePathFigure(p);
    return p;
}

// ---- меандр ----

// Греческий ключ: сплошной меандр между двумя линиями, квадратные блоки в углах.
void fr_meander(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double outer = 5, inner = 23, top = 8, k = 1.5;
    // одно звено на сетке 10×8; начинается и кончается на внутренней линии, так звенья сцепляются
    static const double unit[11][2] = { { 0, 8 }, { 0, 0 }, { 8, 0 }, { 8, 6 }, { 4, 6 }, { 4, 4 }, { 6, 4 },
                                        { 6, 2 }, { 2, 2 }, { 2, 8 }, { 10, 8 } };
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *gold = fr_gilded(a2, w, h, 1), *soft = fr_gilded(a2, w, h, 0.45);
    GpBrush *fill = fr_solid(t->c[WUI_C_ACCENT]), *ground = fr_solid(fr_ground(t));
    GpPen *rule = fr_pen(gold, 1.1), *key = fr_pen(gold, 1.3), *hair = fr_pen(soft, 0.5), *mid = fr_pen(gold, 0.9);
    // Polyline ставит LineTo(..., isSmoothJoin: true): WPF соединяет такие отрезки скруглённо, перо Miter не действует
    GdipSetPenLineJoin(key, LineJoinRound);
    Side s[4];
    fr_inset(g, rule, w, h, outer);
    fr_inset(g, rule, w, h, inner);
    fr_inset(g, hair, w, h, 2.5);

    sides(w, h, inner, s);
    for (int e = 0; e < 4; e++) {
        int n = (int)wui_round(s[e].len / 15), np = 0;
        double kx;
        FrPt *pts;
        if (n < 1) n = 1;
        kx = s[e].len / n / 10;
        pts = (FrPt *)malloc(sizeof(FrPt) * (n * 10 + 1));
        for (int u = 0; u < n; u++)
            for (int i = u == 0 ? 0 : 1; i < 11; i++)
                pts[np++] = side_at(&s[e], (u * 10 + unit[i][0]) * kx, top + unit[i][1] * k);
        fr_geo_free(g, NULL, key, fr_polyline(pts, np, 0));
        free(pts);
    }

    for (int i = 0; i < 4; i++) {
        UINT st = fr_mirror(g, i, w, h);
        fr_rect(g, ground, rule, outer, outer, inner - outer, inner - outer);
        fr_rect(g, NULL, mid, outer + 4, outer + 4, inner - outer - 8, inner - outer - 8);
        fr_rect(g, fill, NULL, outer + 7, outer + 7, inner - outer - 14, inner - outer - 14);
        fr_pop(g, st);
    }
    GdipDeletePen(mid);
    GdipDeletePen(hair);
    GdipDeletePen(key);
    GdipDeletePen(rule);
    GdipDeleteBrush(ground);
    GdipDeleteBrush(fill);
    GdipDeleteBrush(soft);
    GdipDeleteBrush(gold);
}

// ---- ар-деко ----

// Ар-деко: двойная линия, веера-лучи в углах, вложенные шевроны сверху и снизу, параллели по бокам.
void fr_artdeco(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    // веер не выходит из углового куска 34 DIP, на которые веб-страница режет картинку рамки
    const double o = 5, i = 9, r = 24;
    static const double offsets[3] = { 4, 7, 10 }, lengths[3] = { 40, 28, 16 };
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *gold = fr_gilded(a2, w, h, 1), *soft = fr_gilded(a2, w, h, 0.6);
    GpPen *pen = fr_pen(gold, 1.4), *thin = fr_pen(gold, 0.8), *hair = fr_pen(soft, 0.6);
    fr_inset(g, pen, w, h, o);
    fr_inset(g, thin, w, h, i);

    for (int m = 0; m < 4; m++) {
        UINT st = fr_mirror(g, m, w, h);
        // веер: лучи через 10°, две дуги, сплошная четверть круга у ступицы
        for (int a = 0; a <= 90; a += 10) {
            double rad = a * PI / 180, dx = cos(rad), dy = sin(rad);
            int major = a % 30 == 0;
            double len = major ? r : r - 6;
            fr_line(g, major ? thin : hair, i + dx * 8, i + dy * 8, i + dx * len, i + dy * len);
        }
        fr_geo_free(g, NULL, thin, arc(i, i, r - 6, 0, 90));
        fr_geo_free(g, NULL, hair, arc(i, i, 15, 0, 90));
        fr_geo_free(g, gold, NULL, sector(i, i, 7, 0, 90));
        fr_pop(g, st);
    }

    // вложенные шевроны остриём внутрь посередине верхнего и нижнего края
    for (int f = 0; f < 2; f++) {
        double flip = f == 0 ? 1.0 : -1.0, y0 = flip > 0 ? i : h - i;
        for (int n = 0; n < 3; n++) {
            double half = 16 - n * 5, depth = 9 - n * 2.5;
            FrPt pts[3];
            pts[0].x = w / 2 - half; pts[0].y = y0;
            pts[1].x = w / 2;        pts[1].y = y0 + flip * depth;
            pts[2].x = w / 2 + half; pts[2].y = y0;
            GpPen *cp = NULL;
            GdipClonePen(n == 0 ? thin : hair, &cp);
            GdipSetPenLineJoin(cp, LineJoinRound);       // гладкое соединение Polyline
            fr_geo_free(g, NULL, cp, fr_polyline(pts, 3, 0));
            GdipDeletePen(cp);
        }
        fr_geo_free(g, gold, NULL, fr_diamond(w / 2, y0 + flip * 2.5, 2.2));
    }

    // три параллели посередине боковых сторон
    for (int k = 0; k < 3; k++) {
        double xs[2];
        xs[0] = i + offsets[k];
        xs[1] = w - i - offsets[k];
        for (int j = 0; j < 2; j++)
            fr_line(g, hair, xs[j], h / 2 - lengths[k] / 2, xs[j], h / 2 + lengths[k] / 2);
    }
    GdipDeletePen(hair);
    GdipDeletePen(thin);
    GdipDeletePen(pen);
    GdipDeleteBrush(soft);
    GdipDeleteBrush(gold);
}

// ---- строчка ----

// Перо стежка: круглые концы, острые углы.
static GpPen *stitch(GpBrush *b, double th)
{
    GpPen *p = fr_pen(b, th);
    GdipSetPenLineCap197819(p, LineCapRound, LineCapRound, 2);
    return p;
}

// Строчка по ломаной: штрих 4.2 и пропуск 3.2 DIP. Штрихи нарезаются здесь: в WPF круглые концы
// штриха выступают за его длину, а GDI+ со своим штрихом ставит их внутрь — стежки выходят короче.
static void stitch_line(GpGraphics *g, GpPen *pen, const FrPt *pts, int n)
{
    const double dash = 4.2, gap = 3.2;
    GpPath *p = NULL;
    double pos = 0;           // место на узоре: 0..dash — штрих, dash..dash+gap — пропуск
    int on = 0;
    GdipCreatePath(FillModeAlternate, &p);
    for (int i = 0; i + 1 < n; i++) {
        double dx = pts[i + 1].x - pts[i].x, dy = pts[i + 1].y - pts[i].y;
        double len = sqrt(dx * dx + dy * dy), s = 0;
        if (len <= 0) continue;
        dx /= len;
        dy /= len;
        while (s < len) {
            double left = (pos < dash ? dash : dash + gap) - pos, step = left < len - s ? left : len - s;
            double x0 = pts[i].x + dx * s, y0 = pts[i].y + dy * s;
            double x1 = pts[i].x + dx * (s + step), y1 = pts[i].y + dy * (s + step);
            if (pos < dash) {
                if (!on) { GdipStartPathFigure(p); on = 1; }
                GdipAddPathLine(p, (REAL)x0, (REAL)y0, (REAL)x1, (REAL)y1);   // через угол — тот же штрих
            }
            s += step;
            pos += step;
            if (pos >= dash && on) on = 0;
            if (pos >= dash + gap) pos = 0;
        }
    }
    fr_geo_free(g, NULL, pen, p);
}

static void stitch_seg(GpGraphics *g, GpPen *pen, double x0, double y0, double x1, double y1)
{
    FrPt pts[2] = { { x0, y0 }, { x1, y1 } };
    stitch_line(g, pen, pts, 2);
}

static void stitch_rect(GpGraphics *g, GpPen *pen, double x, double y, double w, double h)
{
    FrPt pts[5] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h }, { x, y } };
    stitch_line(g, pen, pts, 5);
}

// Шов: подогнутый край, строчка цветом нитки и пришитые заплатки в углах.
void fr_stitched(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    // заплатки входят в угловой кусок 34 DIP веб-картинки рамки
    const double size = 32;
    WuiColor ground = fr_ground(t);
    WuiColor patch = wui_darken(wui_mix(ground, t->c[WUI_C_ACCENT], t->is_dark ? 0.35 : 0.6), t->is_dark ? 0.25 : 0.05);
    GpBrush *light_b = fr_solid(wui_alpha(0xFFFFFFFF, t->is_dark ? 0x22 : 0x55));
    GpBrush *dark_b = fr_solid(wui_alpha(0xFF000000, t->is_dark ? 0x60 : 0x30));
    GpBrush *thread = fr_solid(t->c[WUI_C_ACCENT2]), *shade = fr_solid(0x55000000);
    GpBrush *fill = fr_solid(wui_alpha(patch, 0xE6)), *cut_b = fr_solid(0x50000000), *edge_b = fr_solid(0x30FFFFFF);
    GpPen *light = fr_pen(light_b, 1), *dark = fr_pen(dark_b, 1), *cut = fr_pen(cut_b, 1.2), *edge = fr_pen(edge_b, 0.6);
    GpPen *shade13 = stitch(shade, 1.3), *thread13 = stitch(thread, 1.3);
    GpPen *shade14 = stitch(shade, 1.4), *thread14 = stitch(thread, 1.4);
    double rx = 11, ry = 11, rw = w - 22, rh = h - 22;

    fr_inset(g, light, w, h, 3.5);
    fr_inset(g, dark, w, h, 4.6);

    // заплатки в углах — потемнее, с оттенком акцента и строчкой по срезу
    for (int i = 0; i < 4; i++) {
        UINT st = fr_mirror(g, i, w, h);
        FrPt tri[3] = { { 0, 0 }, { size, 0 }, { 0, size } };
        fr_geo_free(g, fill, NULL, fr_polyline(tri, 3, 1));
        fr_line(g, cut, size + 0.6, 0.6, 0.6, size + 0.6);
        fr_line(g, edge, size - 1, 0, 0, size - 1);
        stitch_seg(g, shade13, size - 6 + 0.6, 0.8, 0.6, size - 6 + 0.8);
        stitch_seg(g, thread13, size - 6, 0, 0, size - 6);
        fr_pop(g, st);
    }

    // строчка вокруг окна: тень ниже, затем нитка
    if (rw < 0) rw = 0;
    if (rh < 0) rh = 0;
    stitch_rect(g, shade14, rx + 0.6, ry + 0.8, rw, rh);
    stitch_rect(g, thread14, rx, ry, rw, rh);

    GdipDeletePen(thread14);
    GdipDeletePen(shade14);
    GdipDeletePen(thread13);
    GdipDeletePen(shade13);
    GdipDeletePen(edge);
    GdipDeletePen(cut);
    GdipDeletePen(dark);
    GdipDeletePen(light);
    GdipDeleteBrush(edge_b);
    GdipDeleteBrush(cut_b);
    GdipDeleteBrush(fill);
    GdipDeleteBrush(shade);
    GdipDeleteBrush(thread);
    GdipDeleteBrush(dark_b);
    GdipDeleteBrush(light_b);
}

// ---- кельтская плетёнка ----

typedef struct { GpPen *gap, *edge, *core; } Cord;

// Шнур: просвет цветом фона, тёмный край, золотая середина.
static void cord(GpGraphics *g, const Cord *c, GpPath *p)
{
    fr_geo(g, NULL, c->gap, p);
    fr_geo(g, NULL, c->edge, p);
    fr_geo(g, NULL, c->core, p);
    GdipDeletePath(p);
}

// Прядь синусоидой вдоль стороны от from до to (шаг 1 DIP).
static GpPath *strand(const Side *s, double sign, double from, double to, double period, double mid, double amp)
{
    int cap = (int)(to - from) + 4, n = 0;
    FrPt *pts = (FrPt *)malloc(sizeof(FrPt) * (cap > 1 ? cap : 1));
    GpPath *p;
    for (double x = from; x <= to + 0.01; x += 1)
        if (n < cap) pts[n++] = side_at(s, x, mid + sign * amp * sin(x / period * PI * 2));
    p = fr_polyline(pts, n, 0);
    free(pts);
    return p;
}

// Кельтская плетёнка: два шнура переплетаются между линиями, кольца в углах.
void fr_celtic(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double mid = 13, amp = 5, corner = 21;
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *gold = fr_gilded(a2, w, h, 1), *rule_b = fr_gilded(a2, w, h, 0.7);
    GpBrush *gap_b = fr_solid(fr_ground(t)), *edge_b = fr_solid(wui_darken(a2, 0.45)), *dot = fr_solid(t->c[WUI_C_ACCENT]);
    GpPen *rule = fr_pen(rule_b, 0.9);
    Cord c;
    Side s[4];
    c.gap = fr_round_pen(gap_b, 5);
    c.edge = fr_round_pen(edge_b, 3.4);
    c.core = fr_round_pen(gold, 2.2);
    fr_inset(g, rule, w, h, 4.5);
    fr_inset(g, rule, w, h, 21.5);

    sides(w, h, corner, s);
    for (int e = 0; e < 4; e++) {
        int n = (int)wui_round(s[e].len / 22);
        double period;
        if (n < 1) n = 1;
        period = s[e].len / n;
        cord(g, &c, strand(&s[e], 1, -2, s[e].len + 2, period, mid, amp));
        cord(g, &c, strand(&s[e], -1, -2, s[e].len + 2, period, mid, amp));
        // на каждом перехлёсте один шнур идёт поверх другого, попеременно вдоль полосы
        for (int k = 1; k < 2 * n; k++) {
            double x = k * period / 2;
            cord(g, &c, strand(&s[e], k % 2 == 0 ? 1 : -1, x - period / 5, x + period / 5, period, mid, amp));
        }
    }

    for (int i = 0; i < 4; i++) {
        UINT st = fr_mirror(g, i, w, h);
        GpPath *ring = NULL;
        GdipCreatePath(FillModeAlternate, &ring);
        GdipAddPathEllipse(ring, (REAL)(mid - 7), (REAL)(mid - 7), 14, 14);
        cord(g, &c, ring);
        fr_ellipse(g, dot, NULL, mid, mid, 2.2, 2.2);
        fr_pop(g, st);
    }
    GdipDeletePen(c.core);
    GdipDeletePen(c.edge);
    GdipDeletePen(c.gap);
    GdipDeletePen(rule);
    GdipDeleteBrush(dot);
    GdipDeleteBrush(edge_b);
    GdipDeleteBrush(gap_b);
    GdipDeleteBrush(rule_b);
    GdipDeleteBrush(gold);
}

// ---- арабеска ----

// Арабеска: двойная золотая линия с малыми восьмиконечными звёздами, большие звёзды в углах и посередине сторон.
void fr_arabesque(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double a = 6, b = 18, mid = 12, corner = 18;
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *gold = fr_gilded(a2, w, h, 1), *soft = fr_gilded(a2, w, h, 0.4);
    GpBrush *hole = fr_solid(fr_ground(t)), *accent = fr_solid(t->c[WUI_C_ACCENT]);
    GpPen *pen = fr_pen(gold, 1.3), *thin = fr_pen(gold, 0.7), *hair = fr_pen(soft, 0.5);
    Side s[4];
    fr_inset(g, pen, w, h, a);
    fr_inset(g, thin, w, h, b);
    fr_inset(g, hair, w, h, b + 3);

    sides(w, h, corner, s);
    for (int e = 0; e < 4; e++) {
        int n = (int)wui_round(s[e].len / 28);
        double step;
        if (n < 2) n = 2;
        step = s[e].len / n;
        for (int i = 1; i < n; i++) {
            FrPt p = side_at(&s[e], i * step, mid);
            int centre = fabs(i * step - s[e].len / 2) < step / 2;
            if (centre) {
                fr_geo_free(g, gold, NULL, star8(p.x, p.y, 8.5));
                fr_ellipse(g, hole, NULL, p.x, p.y, 4, 4);
                fr_ellipse(g, accent, NULL, p.x, p.y, 2.3, 2.3);
            } else if (i % 2 == 1) {
                fr_geo_free(g, gold, NULL, star8(p.x, p.y, 4.6));
                fr_ellipse(g, hole, NULL, p.x, p.y, 1.5, 1.5);
            } else fr_geo_free(g, accent, NULL, fr_diamond(p.x, p.y, 2));
        }
    }

    for (int i = 0; i < 4; i++) {
        UINT st = fr_mirror(g, i, w, h);
        fr_ellipse(g, hole, thin, mid, mid, 13.5, 13.5);
        fr_geo_free(g, gold, NULL, star8(mid, mid, 11.5));
        fr_geo_free(g, hole, NULL, star8(mid, mid, 6.5));
        fr_ellipse(g, accent, NULL, mid, mid, 3.4, 3.4);
        fr_pop(g, st);
    }
    GdipDeletePen(hair);
    GdipDeletePen(thin);
    GdipDeletePen(pen);
    GdipDeleteBrush(accent);
    GdipDeleteBrush(hole);
    GdipDeleteBrush(soft);
    GdipDeleteBrush(gold);
}

// ---- руны ----

static const unsigned runes[24] = {
    0x16A0, 0x16A2, 0x16A6, 0x16A8, 0x16B1, 0x16B2, 0x16B7, 0x16B9, 0x16BA, 0x16BE, 0x16C1, 0x16C3,
    0x16C7, 0x16C8, 0x16C9, 0x16CA, 0x16CF, 0x16D2, 0x16D6, 0x16D7, 0x16DA, 0x16DC, 0x16DE, 0x16DF
};

// Руны: фризы рун сверху и снизу между линиями, слова разделены двоеточиями, узлы в углах.
void fr_runic(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    static const WCHAR *const fonts[2] = { L"Segoe UI Historic", L"Segoe UI Symbol" };
    const double top = 8, bottom = 34, step = 17;
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *line = fr_solid(a2), *fine_b = fr_solid(fr_opacity(a2, 0.6));
    GpBrush *ink = fr_solid(t->c[WUI_C_ACCENT]), *ground = fr_solid(fr_ground(t));
    GpPen *pen = fr_pen(line, 1.4), *fine = fr_pen(fine_b, 0.6), *ring = fr_pen(ink, 1.2);
    const double ys[4] = { top, bottom, h - bottom, h - top };
    const double ys2[4] = { top + 2.5, bottom - 2.5, h - bottom + 2.5, h - top - 2.5 };
    double mid = (top + bottom) / 2, inner = w - 2 * (top + 22), x0;
    int count = (int)floor(inner / step), left = 0;
    int *slots;
    GpPath *shapes[24];
    WuiRandom rnd;

    for (int i = 0; i < 4; i++) fr_line(g, pen, top, ys[i], w - top, ys[i]);
    for (int i = 0; i < 4; i++) fr_line(g, fine, top + 6, ys2[i], w - top - 6, ys2[i]);
    fr_line(g, pen, top, top, top, h - top);
    fr_line(g, pen, w - top, top, w - top, h - top);
    fr_line(g, fine, top + 3, bottom, top + 3, h - bottom);
    fr_line(g, fine, w - top - 3, bottom, w - top - 3, h - bottom);

    fr_glyph_shapes(fonts, 2, runes, 24, 17, shapes);
    if (count < 0) count = 0;
    x0 = (w - (count - 1) * step) / 2;
    wui_random_init(&rnd, 1701);
    slots = (int *)malloc(sizeof(int) * (count > 0 ? count : 1));
    // слова по 3–6 рун, между ними пустое место с двоеточием
    for (int i = 0; i < count; i++) {
        if (left == 0 && i > 0 && i < count - 1 && slots[i - 1] >= 0) {
            slots[i] = -1;
            left = 3 + wui_random_below(&rnd, 4);
            continue;
        }
        if (left == 0) left = 3 + wui_random_below(&rnd, 4);
        slots[i] = wui_random_below(&rnd, 24);
        left--;
    }
    for (int band = 0; band < 2; band++) {
        double cy = band == 0 ? mid : h - mid;
        for (int i = 0; i < count; i++) {
            double cx = x0 + i * step;
            if (slots[i] < 0) {
                fr_ellipse(g, line, NULL, cx, cy - 3.5, 1.4, 1.4);
                fr_ellipse(g, line, NULL, cx, cy + 3.5, 1.4, 1.4);
                continue;
            }
            fr_place(g, ink, shapes[slots[i]], cx, cy);
        }
    }
    free(slots);
    fr_free_paths(shapes, 24);

    // угловые узлы: квадрат на острие с кольцом сквозь него
    {
        const double cx[4] = { top, w - top, top, w - top }, cy[4] = { mid, mid, h - mid, h - mid };
        for (int i = 0; i < 4; i++) {
            fr_geo_free(g, line, NULL, fr_diamond(cx[i], cy[i], 8));
            fr_geo_free(g, ground, NULL, fr_diamond(cx[i], cy[i], 5));
            fr_ellipse(g, NULL, ring, cx[i], cy[i], 4.2, 4.2);
            fr_ellipse(g, ink, NULL, cx[i], cy[i], 1.3, 1.3);
        }
    }
    GdipDeletePen(ring);
    GdipDeletePen(fine);
    GdipDeletePen(pen);
    GdipDeleteBrush(ground);
    GdipDeleteBrush(ink);
    GdipDeleteBrush(fine_b);
    GdipDeleteBrush(line);
}
