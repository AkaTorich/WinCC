// WUI — рисование оформления: скруглённые фигуры, кисти ThemeLibrary (сплошные, градиенты),
// рамки, тени и свечение (DropShadowEffect) — размытием по точкам, с кэшем.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>


float wui_paint_scale = 1;          // точек в DIP у окна, которое сейчас рисуется

WuiCorners wui_corners(float radius)
{
    WuiCorners k = { radius, radius, radius, radius };
    return k;
}

WuiRect wui_inset(WuiRect r, float l, float t, float rr, float b)
{
    r.x += l; r.y += t; r.w -= l + rr; r.h -= t + b;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

WuiRect wui_deflate(WuiRect r, WuiEdges e)
{
    return wui_inset(r, e.left, e.top, e.right, e.bottom);
}

GpPath *wui_round_path(WuiRect r, WuiCorners k)
{
    GpPath *p;
    GdipCreatePath(FillModeAlternate, &p);
    float m = (r.w < r.h ? r.w : r.h) / 2;
    float a = k.top_left > m ? m : k.top_left, b = k.top_right > m ? m : k.top_right;
    float c = k.bottom_right > m ? m : k.bottom_right, d = k.bottom_left > m ? m : k.bottom_left;
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (c < 0) c = 0;
    if (d < 0) d = 0;
    if (a + b + c + d < 0.01f) {
        GdipAddPathRectangle(p, r.x, r.y, r.w, r.h);
        return p;
    }
    float x2 = r.x + r.w, y2 = r.y + r.h;
    if (a > 0) GdipAddPathArc(p, r.x, r.y, 2 * a, 2 * a, 180, 90);
    else GdipAddPathLine(p, r.x, r.y, r.x, r.y);
    if (b > 0) GdipAddPathArc(p, x2 - 2 * b, r.y, 2 * b, 2 * b, 270, 90);
    else GdipAddPathLine(p, x2, r.y, x2, r.y);
    if (c > 0) GdipAddPathArc(p, x2 - 2 * c, y2 - 2 * c, 2 * c, 2 * c, 0, 90);
    else GdipAddPathLine(p, x2, y2, x2, y2);
    if (d > 0) GdipAddPathArc(p, r.x, y2 - 2 * d, 2 * d, 2 * d, 90, 90);
    else GdipAddPathLine(p, r.x, y2, r.x, y2);
    GdipClosePathFigure(p);
    return p;
}

static ARGB fade(WuiColor c, float opacity)
{
    if (opacity >= 1) return c;
    int a = (int)(WUI_A(c) * opacity + 0.5f);
    return (c & 0xFFFFFF) | ((ARGB)a << 24);
}

GpBrush *wui_solid(WuiColor c)
{
    GpSolidFill *b;
    GdipCreateSolidFill(c, &b);
    return b;
}

WuiColor wui_brush_color(int index)
{
    const WuiBrushDef *d = &wui_brushes[index];
    if (d->kind == WUI_BRUSH_NONE) return 0;
    if (d->kind == WUI_BRUSH_TEXTURE) return wui_t->c[WUI_C_BACKGROUND2];
    return fade(d->stops[0].color, d->opacity);
}

// остановки → GDI+ (позиции 0…1 строго, крайние цвета продлены)
static int stops(const WuiBrushDef *d, float opacity, float from, float to, ARGB *col, REAL *pos)
{
    int n = 0;
    float span = to - from;
    col[n] = fade(d->stops[0].color, opacity);
    pos[n++] = 0;
    for (int i = 0; i < d->count; i++) {
        float p = (d->stops[i].offset - from) / span;
        if (p <= 0 || p >= 1) {
            if (p <= 0) col[0] = fade(d->stops[i].color, opacity);
            continue;
        }
        col[n] = fade(d->stops[i].color, opacity);
        pos[n++] = p;
    }
    // цвет за последней остановкой
    WuiColor last = d->stops[d->count - 1].color;
    for (int i = 0; i < d->count; i++)
        if (d->stops[i].offset >= to) { last = d->stops[i].color; break; }
    col[n] = fade(last, opacity);
    pos[n++] = 1;
    return n;
}

GpBrush *wui_brush(int index, WuiRect r, float opacity)
{
    const WuiBrushDef *d = &wui_brushes[index];
    opacity *= d->opacity;
    if (d->kind == WUI_BRUSH_NONE || opacity <= 0) return 0;
    if (d->kind == WUI_BRUSH_TEXTURE) return wui_solid(fade(wui_t->c[WUI_C_BACKGROUND2], opacity));
    if (d->kind == WUI_BRUSH_SOLID || d->count < 2) return wui_solid(fade(d->stops[0].color, opacity));
    if (r.w < 0.5f) r.w = 0.5f;
    if (r.h < 0.5f) r.h = 0.5f;
    ARGB col[10];
    REAL pos[10];
    if (d->kind == WUI_BRUSH_LINEAR) {
        // градиент WPF задан в долях рамки: строим в квадрате 1×1 и растягиваем на рамку.
        // GDI+ повторяет градиент за концами — концы раздвигаются, чтобы квадрат весь был внутри
        float dx = d->x2 - d->x1, dy = d->y2 - d->y1, len2 = dx * dx + dy * dy;
        if (len2 < 1e-6f) return wui_solid(fade(d->stops[d->count - 1].color, opacity));
        float lo = 0, hi = 1;
        float cx[4] = { 0, 1, 0, 1 }, cy[4] = { 0, 0, 1, 1 };
        for (int i = 0; i < 4; i++) {
            float t = ((cx[i] - d->x1) * dx + (cy[i] - d->y1) * dy) / len2;
            if (t < lo) lo = t;
            if (t > hi) hi = t;
        }
        lo -= 0.02f;
        hi += 0.02f;
        PointF a = { d->x1 + dx * lo, d->y1 + dy * lo }, b = { d->x1 + dx * hi, d->y1 + dy * hi };
        GpLineGradient *g;
        GdipCreateLineBrush(&a, &b, 0, 0, WrapModeTileFlipXY, &g);
        int n = stops(d, opacity, lo, hi, col, pos);
        GdipSetLinePresetBlend(g, col, pos, n);
        GpMatrix *m;
        GdipCreateMatrix2(r.w, 0, 0, r.h, r.x, r.y, &m);
        GdipSetLineTransform(g, m);
        GdipDeleteMatrix(m);
        return g;
    }
    // круговой: эллипс с центром и радиусами в долях рамки, середина — в точке истока
    GpPath *p;
    GdipCreatePath(FillModeAlternate, &p);
    float cx = r.x + d->x1 * r.w, cy = r.y + d->y1 * r.h, rx = d->rx * r.w, ry = d->ry * r.h;
    GdipAddPathEllipse(p, cx - rx, cy - ry, 2 * rx, 2 * ry);
    GpPathGradient *g;
    GdipCreatePathGradientFromPath(p, &g);
    GdipDeletePath(p);
    PointF o = { r.x + d->x2 * r.w, r.y + d->y2 * r.h };
    GdipSetPathGradientCenterPoint(g, &o);
    int n = stops(d, opacity, 0, 1, col, pos);
    ARGB rc[10];
    REAL rp[10];
    for (int i = 0; i < n; i++) {           // у GDI+ 0 — край, 1 — середина
        rc[i] = col[n - 1 - i];
        rp[i] = 1 - pos[n - 1 - i];
    }
    GdipSetPathGradientPresetBlend(g, rc, rp, n);
    return g;
}

void wui_fill(GpGraphics *g, int brush, WuiRect r, WuiCorners k, float opacity)
{
    if (!brush || r.w <= 0 || r.h <= 0) return;
    const WuiBrushDef *d = &wui_brushes[brush];
    GpPath *p = wui_round_path(r, k);
    if (d->kind == WUI_BRUSH_RADIAL) {          // за эллипсом WPF тянет последний цвет
        GpBrush *s = wui_solid(fade(d->stops[d->count - 1].color, opacity * d->opacity));
        GdipFillPath(g, s, p);
        GdipDeleteBrush(s);
    }
    GpBrush *b = wui_brush(brush, r, opacity);
    if (b) {
        GdipFillPath(g, b, p);
        GdipDeleteBrush(b);
    }
    GdipDeletePath(p);
}

// рамка Border: внешний край — по рамке элемента, толщина внутрь
void wui_stroke(GpGraphics *g, int brush, WuiRect r, WuiCorners k, WuiEdges t, float opacity)
{
    if (!brush || r.w <= 0 || r.h <= 0) return;
    if (t.left <= 0 && t.top <= 0 && t.right <= 0 && t.bottom <= 0) return;
    GpBrush *b = wui_brush(brush, r, opacity);
    if (!b) return;
    if (t.left == t.top && t.top == t.right && t.right == t.bottom) {
        float w = t.left;
        WuiRect m = wui_inset(r, w / 2, w / 2, w / 2, w / 2);
        WuiCorners mk = { k.top_left - w / 2, k.top_right - w / 2, k.bottom_right - w / 2, k.bottom_left - w / 2 };
        GpPath *p = wui_round_path(m, mk);
        GpPen *pen;
        GdipCreatePen2(b, w, UnitPixel, &pen);
        GdipDrawPath(g, pen, p);
        GdipDeletePen(pen);
        GdipDeletePath(p);
    }
    else {                                      // разная толщина: внешняя фигура минус внутренняя
        GpPath *p = wui_round_path(r, k);
        WuiRect in = wui_inset(r, t.left, t.top, t.right, t.bottom);
        WuiCorners ik = { k.top_left - (t.left > t.top ? t.left : t.top), k.top_right - (t.right > t.top ? t.right : t.top),
                          k.bottom_right - (t.right > t.bottom ? t.right : t.bottom),
                          k.bottom_left - (t.left > t.bottom ? t.left : t.bottom) };
        GpPath *q = wui_round_path(in, ik);
        GpRegion *reg;
        GdipCreateRegionPath(p, &reg);
        GdipCombineRegionPath(reg, q, CombineModeExclude);
        GdipFillRegion(g, b, reg);
        GdipDeleteRegion(reg);
        GdipDeletePath(q);
        GdipDeletePath(p);
    }
    GdipDeleteBrush(b);
}

void wui_fill_color(GpGraphics *g, WuiColor c, WuiRect r, float radius)
{
    GpBrush *b = wui_solid(c);
    GpPath *p = wui_round_path(r, wui_corners(radius));
    GdipFillPath(g, b, p);
    GdipDeletePath(p);
    GdipDeleteBrush(b);
}

void wui_line(GpGraphics *g, WuiColor c, float width, float x1, float y1, float x2, float y2)
{
    GpPen *pen;
    GdipCreatePen1(c, width, UnitPixel, &pen);
    GdipDrawLine(g, pen, x1, y1, x2, y2);
    GdipDeletePen(pen);
}

// ---- тени и свечение ----
// DropShadowEffect размывает то, что нарисовал элемент: его альфу. Размытие — по Гауссу
// (сигма — треть радиуса), приближённое тремя проходами «коробкой».

float wui_effect_reach(int effect)
{
    if (!effect) return 0;
    const WuiEffectDef *e = &wui_effects[effect];
    return e->blur + e->depth + 1;
}

static void box_pass(float *src, float *dst, int w, int h, int r, int horizontal)
{
    float k = 1.0f / (2 * r + 1);
    int lines = horizontal ? h : w, len = horizontal ? w : h;
    int step = horizontal ? 1 : w;
    for (int l = 0; l < lines; l++) {
        float *s = src + (horizontal ? l * w : l), *d = dst + (horizontal ? l * w : l);
        float acc = 0;
        for (int i = -r; i <= r; i++)
            if (i >= 0 && i < len) acc += s[i * step];
        for (int i = 0; i < len; i++) {
            d[i * step] = acc * k;
            int out = i - r, in = i + r + 1;
            if (out >= 0) acc -= s[out * step];
            if (in < len) acc += s[in * step];
        }
    }
}

static void blur(float *a, int w, int h, float sigma)
{
    if (sigma < 0.3f) return;
    float *t = malloc(sizeof(float) * (size_t)w * h);
    // три коробки дают почти Гаусса: ширина из 12σ²/3 + 1
    int r = (int)(sqrtf(12 * sigma * sigma / 3 + 1) / 2);
    if (r < 1) r = 1;
    for (int pass = 0; pass < 3; pass++) {
        box_pass(a, t, w, h, r, 1);
        box_pass(t, a, w, h, r, 0);
    }
    free(t);
}

typedef struct {
    int effect, brush, pw, ph;
    float r[4], opacity, scale;
    GpBitmap *bmp;
    int pad;                        // поле вокруг фигуры, точки
    unsigned age;
} Shadow;

#define SHADOWS 96
static Shadow g_shadows[SHADOWS];
static unsigned g_clock;

static Shadow *make_shadow(int effect, int brush, WuiRect r, WuiCorners k, float opacity)
{
    float s = wui_paint_scale;
    const WuiEffectDef *e = &wui_effects[effect];
    int pw = (int)(r.w * s + 0.5f), ph = (int)(r.h * s + 0.5f);
    float rr[4] = { k.top_left, k.top_right, k.bottom_right, k.bottom_left };
    Shadow *free_slot = &g_shadows[0];
    for (int i = 0; i < SHADOWS; i++) {
        Shadow *c = &g_shadows[i];
        if (c->bmp && c->effect == effect && c->brush == brush && c->pw == pw && c->ph == ph && c->scale == s
            && c->opacity == opacity && memcmp(c->r, rr, sizeof rr) == 0) {
            c->age = ++g_clock;
            return c;
        }
        if (!c->bmp || c->age < free_slot->age || (free_slot->bmp && !c->bmp)) free_slot = c;
    }
    Shadow *c = free_slot;
    if (c->bmp) GdipDisposeImage(c->bmp);
    memset(c, 0, sizeof *c);
    c->effect = effect; c->brush = brush; c->pw = pw; c->ph = ph; c->scale = s; c->opacity = opacity;
    memcpy(c->r, rr, sizeof rr);
    c->age = ++g_clock;

    float sigma = e->blur * s / 3;
    int pad = (int)(sigma * 3) + 2;
    int w = pw + 2 * pad, h = ph + 2 * pad;
    c->pad = pad;
    // фигура той же кистью в отдельную картинку — нужна её альфа
    GpBitmap *shape;
    GdipCreateBitmapFromScan0(w, h, 0, PixelFormat32bppPARGB, NULL, &shape);
    GpGraphics *sg;
    GdipGetImageGraphicsContext(shape, &sg);
    GdipSetSmoothingMode(sg, SmoothingModeAntiAlias);
    GdipScaleWorldTransform(sg, s, s, MatrixOrderAppend);
    WuiRect at = { pad / s, pad / s, r.w, r.h };
    wui_fill(sg, brush, at, k, opacity);
    GdipDeleteGraphics(sg);
    c->bmp = wui_effect_bitmap(shape, effect);
    GdipDisposeImage(shape);
    return c;
}

int wui_effect_pad(int effect)
{
    return (int)(wui_effects[effect].blur * wui_paint_scale) + 2;
}

GpBitmap *wui_effect_bitmap(GpBitmap *shape, int effect)
{
    const WuiEffectDef *e = &wui_effects[effect];
    UINT uw, uh;
    GdipGetImageWidth(shape, &uw);
    GdipGetImageHeight(shape, &uh);
    int w = (int)uw, h = (int)uh;
    float *a = malloc(sizeof(float) * (size_t)w * h);
    Rect all = { 0, 0, w, h };
    BitmapData bd;
    GdipBitmapLockBits(shape, &all, ImageLockModeRead, PixelFormat32bppPARGB, &bd);
    for (int y = 0; y < h; y++) {
        unsigned *row = (unsigned *)((char *)bd.Scan0 + y * bd.Stride);
        for (int x = 0; x < w; x++) a[y * w + x] = (float)(row[x] >> 24);
    }
    GdipBitmapUnlockBits(shape, &bd);

    blur(a, w, h, e->blur * wui_paint_scale / 3);

    GpBitmap *out;
    GdipCreateBitmapFromScan0(w, h, 0, PixelFormat32bppPARGB, NULL, &out);
    GdipBitmapLockBits(out, &all, ImageLockModeWrite, PixelFormat32bppPARGB, &bd);
    float k_alpha = e->opacity * WUI_A(e->color) / 255.0f / 255.0f;
    int cr = WUI_R(e->color), cg = WUI_G(e->color), cb = WUI_B(e->color);
    for (int y = 0; y < h; y++) {
        unsigned *row = (unsigned *)((char *)bd.Scan0 + y * bd.Stride);
        for (int x = 0; x < w; x++) {
            float al = a[y * w + x] * k_alpha;
            if (al > 1) al = 1;
            unsigned A = (unsigned)(al * 255 + 0.5f);
            row[x] = (A << 24) | ((unsigned)(cr * A / 255) << 16) | ((unsigned)(cg * A / 255) << 8) | (unsigned)(cb * A / 255);
        }
    }
    GdipBitmapUnlockBits(out, &bd);
    free(a);
    return out;
}

void wui_effect(GpGraphics *g, int effect, int brush, WuiRect r, WuiCorners k, float opacity)
{
    if (!effect || !brush || r.w < 1 || r.h < 1) return;
    const WuiEffectDef *e = &wui_effects[effect];
    if (e->opacity <= 0) return;
    Shadow *c = make_shadow(effect, brush, r, k, opacity);
    float s = wui_paint_scale;
    float rad = e->direction * 3.14159265f / 180;
    float dx = e->depth * cosf(rad), dy = -e->depth * sinf(rad);
    UINT w, h;
    GdipGetImageWidth(c->bmp, &w);
    GdipGetImageHeight(c->bmp, &h);
    GdipDrawImageRect(g, c->bmp, r.x + dx - c->pad / s, r.y + dy - c->pad / s, w / s, h / s);
}
