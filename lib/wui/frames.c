// WUI — узорные рамки окна: DrawTripleLine из Backdrop.cs и рамки BackdropArt.cs, плюс общие помощники.
// Рамки из BackdropFrames.cs — в frames2.c. Всё в DIP, масштаб уже стоит у g.
#include <stdlib.h>
#include <string.h>
#include "frames.h"

#define PI 3.14159265358979323846

// ================= помощники =================

GpBrush *fr_solid(WuiColor c)
{
    GpSolidFill *b = NULL;
    GdipCreateSolidFill(c, &b);
    return b;
}

WuiColor fr_opacity(WuiColor c, double k)
{
    return wui_alpha(c, (int)(WUI_A(c) * k));
}

// Позолота: цвет светлеет и темнеет по диагонали окна (линейный градиент от (0,0) до (w,h)).
GpBrush *fr_gilded(WuiColor c, double w, double h, double opacity)
{
    int a = (int)(WUI_A(c) * opacity);
    PointF p1 = { 0, 0 }, p2 = { (REAL)w, (REAL)h };
    ARGB colors[4];
    REAL pos[4] = { 0, 0.3f, 0.55f, 1 };
    GpLineGradient *b = NULL;
    colors[0] = wui_alpha(wui_lighten(c, 0.25), a);
    colors[1] = wui_alpha(c, a);
    colors[2] = wui_alpha(wui_lighten(c, 0.2), a);
    colors[3] = wui_alpha(wui_darken(c, 0.2), a);
    // рамка не выходит за окно, так что повтор за краями не виден (в WPF — Pad)
    GdipCreateLineBrush(&p1, &p2, colors[0], colors[3], WrapModeTileFlipXY, &b);
    if (b) GdipSetLinePresetBlend(b, colors, pos, 4);
    return b;
}

GpPen *fr_pen(GpBrush *b, double th)
{
    GpPen *p = NULL;
    GdipCreatePen2(b, (REAL)th, UnitWorld, &p);
    return p;
}

GpPen *fr_round_pen(GpBrush *b, double th)
{
    GpPen *p = fr_pen(b, th);
    GdipSetPenLineJoin(p, LineJoinRound);
    GdipSetPenLineCap197819(p, LineCapRound, LineCapRound, 2);   // 2 — DashCapRound
    return p;
}

WuiColor fr_ground(const WuiTheme *t)
{
    if (t->texture != WUI_TEX_NONE) return t->c[WUI_C_PAPER];
    return t->is_dark ? t->c[WUI_C_BACKGROUND1] : t->c[WUI_C_PAPER];
}

// Сколько точек экрана в одном DIP (мировое преобразование g, с отражениями и поворотами).
static double world_scale(GpGraphics *g)
{
    GpMatrix *m = NULL;
    REAL e[6];
    double det;
    GdipCreateMatrix(&m);
    GdipGetWorldTransform(g, m);
    GdipGetMatrixElements(m, e);
    GdipDeleteMatrix(m);
    det = fabs((double)e[0] * e[3] - (double)e[1] * e[2]);
    return det > 0 ? sqrt(det) : 1;
}

// Обводка пути. GDI+ рисует (и расширяет) перо тоньше единицы шириной в целую единицу — линия выходит
// ярче, чем в WPF. Поэтому контур увеличивается в 16 раз, расширяется пером в 16 раз толще, уменьшается
// обратно и заливается: сглаживание даёт ту же долю покрытия, что и в WPF, для любой толщины.
#define STROKE_UP 16
static void stroke(GpGraphics *g, GpPen *pen, GpPath *p)
{
    REAL width = 1;
    double scale = world_scale(g);
    GpPath *wide = NULL;
    GpPen *big = NULL;
    GpBrush *b = NULL;
    GpMatrix *up = NULL, *down = NULL;
    GdipGetPenWidth(pen, &width);
    GdipClonePen(pen, &big);
    GdipSetPenWidth(big, width * STROKE_UP);
    GdipClonePath(p, &wide);
    GdipCreateMatrix2(STROKE_UP, 0, 0, STROKE_UP, 0, 0, &up);
    GdipCreateMatrix2(1.0f / STROKE_UP, 0, 0, 1.0f / STROKE_UP, 0, 0, &down);
    GdipTransformPath(wide, up);
    GdipWidenPath(wide, big, NULL, (REAL)(0.1 * STROKE_UP / scale));
    GdipTransformPath(wide, down);
    GdipSetPathFillMode(wide, FillModeWinding);
    GdipGetPenBrushFill(pen, &b);
    if (b) {
        GdipFillPath(g, b, wide);
        GdipDeleteBrush(b);
    }
    GdipDeleteMatrix(down);
    GdipDeleteMatrix(up);
    GdipDeletePath(wide);
    GdipDeletePen(big);
}

void fr_rect(GpGraphics *g, GpBrush *fill, GpPen *pen, double x, double y, double w, double h)
{
    if (fill) GdipFillRectangle(g, fill, (REAL)x, (REAL)y, (REAL)w, (REAL)h);
    if (pen) {
        GpPath *p = NULL;
        GdipCreatePath(FillModeAlternate, &p);
        GdipAddPathRectangle(p, (REAL)x, (REAL)y, (REAL)w, (REAL)h);
        stroke(g, pen, p);
        GdipDeletePath(p);
    }
}

void fr_inset(GpGraphics *g, GpPen *pen, double w, double h, double inset)
{
    double iw = w - 2 * inset, ih = h - 2 * inset;
    fr_rect(g, NULL, pen, inset, inset, iw > 0 ? iw : 0, ih > 0 ? ih : 0);
}

void fr_ellipse(GpGraphics *g, GpBrush *fill, GpPen *pen, double cx, double cy, double rx, double ry)
{
    REAL x = (REAL)(cx - rx), y = (REAL)(cy - ry), w = (REAL)(2 * rx), h = (REAL)(2 * ry);
    if (fill) GdipFillEllipse(g, fill, x, y, w, h);
    if (pen) {
        GpPath *p = NULL;
        GdipCreatePath(FillModeAlternate, &p);
        GdipAddPathEllipse(p, x, y, w, h);
        stroke(g, pen, p);
        GdipDeletePath(p);
    }
}

void fr_line(GpGraphics *g, GpPen *pen, double x1, double y1, double x2, double y2)
{
    GpPath *p = NULL;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathLine(p, (REAL)x1, (REAL)y1, (REAL)x2, (REAL)y2);
    stroke(g, pen, p);
    GdipDeletePath(p);
}

void fr_geo(GpGraphics *g, GpBrush *fill, GpPen *pen, GpPath *p)
{
    if (!p) return;
    if (fill) GdipFillPath(g, fill, p);
    if (pen) stroke(g, pen, p);
}

void fr_geo_free(GpGraphics *g, GpBrush *fill, GpPen *pen, GpPath *p)
{
    fr_geo(g, fill, pen, p);
    if (p) GdipDeletePath(p);
}

GpPath *fr_diamond(double cx, double cy, double r)
{
    PointF pts[4];
    GpPath *p = NULL;
    pts[0].X = (REAL)cx;       pts[0].Y = (REAL)(cy - r);
    pts[1].X = (REAL)(cx + r); pts[1].Y = (REAL)cy;
    pts[2].X = (REAL)cx;       pts[2].Y = (REAL)(cy + r);
    pts[3].X = (REAL)(cx - r); pts[3].Y = (REAL)cy;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathPolygon(p, pts, 4);
    return p;
}

// Ломаная; замкнутая ещё и заливается (заплатки в углах), открытая — только линия.
GpPath *fr_polyline(const FrPt *pts, int n, int closed)
{
    GpPath *p = NULL;
    PointF *f = (PointF *)malloc(sizeof(PointF) * (n > 0 ? n : 1));
    for (int i = 0; i < n; i++) { f[i].X = (REAL)pts[i].x; f[i].Y = (REAL)pts[i].y; }
    GdipCreatePath(FillModeAlternate, &p);
    if (n > 1) GdipAddPathLine2(p, f, n);
    if (closed) GdipClosePathFigure(p);
    free(f);
    return p;
}

// Дуга эллипса по концам (ArcSegment WPF, без поворота) — кривыми Безье не больше чем на 90°.
void fr_arc_to(GpPath *p, double x0, double y0, double x1, double y1, double rx, double ry, int large, int clockwise)
{
    double dx2, dy2, lam, num, den, coef, cxp, cyp, cx, cy, a0, a1, da;
    int segs;
    rx = fabs(rx);
    ry = fabs(ry);
    if (rx < 1e-12 || ry < 1e-12 || (x0 == x1 && y0 == y1)) {
        GdipAddPathLine(p, (REAL)x0, (REAL)y0, (REAL)x1, (REAL)y1);
        return;
    }
    dx2 = (x0 - x1) / 2;
    dy2 = (y0 - y1) / 2;
    lam = dx2 * dx2 / (rx * rx) + dy2 * dy2 / (ry * ry);
    if (lam > 1) { rx *= sqrt(lam); ry *= sqrt(lam); }
    num = rx * rx * ry * ry - rx * rx * dy2 * dy2 - ry * ry * dx2 * dx2;
    den = rx * rx * dy2 * dy2 + ry * ry * dx2 * dx2;
    coef = den > 0 && num > 0 ? sqrt(num / den) : 0;
    if (large == clockwise) coef = -coef;
    cxp = coef * rx * dy2 / ry;
    cyp = -coef * ry * dx2 / rx;
    cx = cxp + (x0 + x1) / 2;
    cy = cyp + (y0 + y1) / 2;
    a0 = atan2((y0 - cy) / ry, (x0 - cx) / rx);
    a1 = atan2((y1 - cy) / ry, (x1 - cx) / rx);
    da = a1 - a0;
    if (clockwise && da < 0) da += 2 * PI;          // по часовой на экране — угол растёт
    if (!clockwise && da > 0) da -= 2 * PI;
    segs = (int)ceil(fabs(da) / (PI / 2) - 1e-9);
    if (segs < 1) segs = 1;
    for (int i = 0; i < segs; i++) {
        double s = a0 + da * i / segs, e = a0 + da * (i + 1) / segs;
        double k = 4.0 / 3 * tan((e - s) / 4);
        double sx = cx + rx * cos(s), sy = cy + ry * sin(s);
        double ex = cx + rx * cos(e), ey = cy + ry * sin(e);
        if (i == segs - 1) { ex = x1; ey = y1; }
        GdipAddPathBezier(p, (REAL)sx, (REAL)sy,
                          (REAL)(sx - k * rx * sin(s)), (REAL)(sy + k * ry * cos(s)),
                          (REAL)(ex + k * rx * sin(e)), (REAL)(ey - k * ry * cos(e)),
                          (REAL)ex, (REAL)ey);
    }
}

// ---- разбор строки пути WPF ----

static const char *skip_sep(const char *s)
{
    while (*s == ' ' || *s == ',' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    return s;
}

static int is_num_start(char c)
{
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
}

// Читает n чисел; 0 — если чисел не хватило.
static int read_nums(const char **ps, double *v, int n)
{
    const char *s = *ps;
    for (int i = 0; i < n; i++) {
        char *end;
        s = skip_sep(s);
        if (!is_num_start(*s)) return 0;
        v[i] = strtod(s, &end);
        s = end;
    }
    *ps = s;
    return 1;
}

GpPath *fr_parse(const char *s)
{
    GpPath *p = NULL;
    double cx = 0, cy = 0, sx = 0, sy = 0, v[7];
    char cmd = 0;
    GdipCreatePath(FillModeAlternate, &p);       // как в WPF: EvenOdd, пока нет «F1»
    for (;;) {
        int rel;
        char up;
        s = skip_sep(s);
        if (!*s) break;
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) cmd = *s++;
        else if (!cmd || cmd == 'Z' || cmd == 'z') break;    // числа без команды
        rel = cmd >= 'a';
        up = rel ? (char)(cmd - 32) : cmd;
        switch (up) {
        case 'F':
            if (!read_nums(&s, v, 1)) return p;
            GdipDeletePath(p);
            GdipCreatePath(v[0] != 0 ? FillModeWinding : FillModeAlternate, &p);
            cmd = 0;
            break;
        case 'M':
            if (!read_nums(&s, v, 2)) return p;
            if (rel) { v[0] += cx; v[1] += cy; }
            GdipStartPathFigure(p);
            cx = sx = v[0];
            cy = sy = v[1];
            cmd = rel ? 'l' : 'L';                  // дальше пары чисел — линии
            break;
        case 'L':
            if (!read_nums(&s, v, 2)) return p;
            if (rel) { v[0] += cx; v[1] += cy; }
            GdipAddPathLine(p, (REAL)cx, (REAL)cy, (REAL)v[0], (REAL)v[1]);
            cx = v[0];
            cy = v[1];
            break;
        case 'H':
            if (!read_nums(&s, v, 1)) return p;
            if (rel) v[0] += cx;
            GdipAddPathLine(p, (REAL)cx, (REAL)cy, (REAL)v[0], (REAL)cy);
            cx = v[0];
            break;
        case 'V':
            if (!read_nums(&s, v, 1)) return p;
            if (rel) v[0] += cy;
            GdipAddPathLine(p, (REAL)cx, (REAL)cy, (REAL)cx, (REAL)v[0]);
            cy = v[0];
            break;
        case 'C':
            if (!read_nums(&s, v, 6)) return p;
            if (rel) for (int i = 0; i < 6; i += 2) { v[i] += cx; v[i + 1] += cy; }
            GdipAddPathBezier(p, (REAL)cx, (REAL)cy, (REAL)v[0], (REAL)v[1], (REAL)v[2], (REAL)v[3], (REAL)v[4], (REAL)v[5]);
            cx = v[4];
            cy = v[5];
            break;
        case 'Q':
            if (!read_nums(&s, v, 4)) return p;
            if (rel) for (int i = 0; i < 4; i += 2) { v[i] += cx; v[i + 1] += cy; }
            // квадратичная — та же кубическая
            GdipAddPathBezier(p, (REAL)cx, (REAL)cy,
                              (REAL)(cx + 2.0 / 3 * (v[0] - cx)), (REAL)(cy + 2.0 / 3 * (v[1] - cy)),
                              (REAL)(v[2] + 2.0 / 3 * (v[0] - v[2])), (REAL)(v[3] + 2.0 / 3 * (v[1] - v[3])),
                              (REAL)v[2], (REAL)v[3]);
            cx = v[2];
            cy = v[3];
            break;
        case 'A':
            // rx ry поворот большая по-часовой x y (поворот эллипса не поддержан)
            if (!read_nums(&s, v, 7)) return p;
            if (rel) { v[5] += cx; v[6] += cy; }
            fr_arc_to(p, cx, cy, v[5], v[6], v[0], v[1], v[3] != 0, v[4] != 0);
            cx = v[5];
            cy = v[6];
            break;
        case 'Z':
            GdipClosePathFigure(p);
            cx = sx;
            cy = sy;
            break;
        default:
            return p;
        }
    }
    return p;
}

// ---- преобразования ----

UINT fr_push(GpGraphics *g, double m11, double m12, double m21, double m22, double dx, double dy)
{
    UINT state = 0;
    GpMatrix *m = NULL;
    GdipSaveGraphics(g, &state);
    GdipCreateMatrix2((REAL)m11, (REAL)m12, (REAL)m21, (REAL)m22, (REAL)dx, (REAL)dy, &m);
    GdipMultiplyWorldTransform(g, m, MatrixOrderPrepend);
    GdipDeleteMatrix(m);
    return state;
}

// RotateTransform(angle, cx, cy): поворот по часовой (на экране) вокруг точки.
UINT fr_push_rotate(GpGraphics *g, double angle, double cx, double cy)
{
    UINT state = 0;
    GdipSaveGraphics(g, &state);
    GdipTranslateWorldTransform(g, (REAL)cx, (REAL)cy, MatrixOrderPrepend);
    GdipRotateWorldTransform(g, (REAL)angle, MatrixOrderPrepend);
    GdipTranslateWorldTransform(g, (REAL)-cx, (REAL)-cy, MatrixOrderPrepend);
    return state;
}

// Mirrors: узор левого верхнего угла — во все четыре (порядок как у Corners).
UINT fr_mirror(GpGraphics *g, int i, double w, double h)
{
    switch (i) {
    case 1:  return fr_push(g, -1, 0, 0, 1, w, 0);
    case 2:  return fr_push(g, -1, 0, 0, -1, w, h);
    case 3:  return fr_push(g, 1, 0, 0, -1, 0, h);
    default: return fr_push(g, 1, 0, 0, 1, 0, 0);
    }
}

void fr_pop(GpGraphics *g, UINT state)
{
    GdipRestoreGraphics(g, state);
}

// Точка на диагонали угла (0,0), (w,0), (w,h), (0,h) в inset DIP от обоих краёв.
void fr_toward(int corner, double w, double h, double inset, double *x, double *y)
{
    double cx = corner == 1 || corner == 2 ? w : 0;
    double cy = corner >= 2 ? h : 0;
    *x = cx < w / 2 ? inset : w - inset;
    *y = cy < h / 2 ? inset : h - inset;
}

// ---- знаки шрифтом ----

GpFontFamily *fr_family(const WCHAR *const *names, int count)
{
    for (int i = 0; i < count; i++) {
        GpFontFamily *f = NULL;
        if (GdipCreateFontFamilyFromName(names[i], NULL, &f) == Ok && f) return f;
    }
    return NULL;
}

// Контур текста: (x, y) — левый верх строки, как точка FormattedText (без полей по краям).
GpPath *fr_text_path(GpFontFamily *f, const WCHAR *text, int len, double size, int bold, double x, double y)
{
    GpPath *p = NULL;
    GpStringFormat *fmt = NULL;
    RectF r;
    r.X = (REAL)x; r.Y = (REAL)y; r.Width = 0; r.Height = 0;
    // общий объект GDI+ — его не удаляют
    GdipStringFormatGetGenericTypographic(&fmt);
    GdipCreatePath(FillModeWinding, &p);
    GdipAddPathString(p, text, len, f, bold ? FontStyleBold : FontStyleRegular, (REAL)size, &r, fmt);
    return p;
}

static int utf16(unsigned code, WCHAR *out)
{
    if (code < 0x10000) { out[0] = (WCHAR)code; return 1; }
    code -= 0x10000;
    out[0] = (WCHAR)(0xD800 + (code >> 10));
    out[1] = (WCHAR)(0xDC00 + (code & 0x3FF));
    return 2;
}

void fr_glyph_shapes(const WCHAR *const *names, int nnames, const unsigned *codes, int n, double height, GpPath **out)
{
    GpFontFamily *f = fr_family(names, nnames);
    double tallest = 0, k;
    GpMatrix *m = NULL;
    for (int i = 0; i < n; i++) out[i] = NULL;
    if (!f) return;
    for (int i = 0; i < n; i++) {
        WCHAR s[2];
        int len = utf16(codes[i], s);
        RectF b;
        GpPath *p = fr_text_path(f, s, len, 20, 0, 0, 0);
        GdipFlattenPath(p, NULL, 0.02f);
        GdipGetPathWorldBounds(p, &b, NULL, NULL);
        if (b.Width <= 0 || b.Height <= 0) { GdipDeletePath(p); continue; }
        out[i] = p;
        if (b.Height > tallest) tallest = b.Height;
    }
    GdipDeleteFontFamily(f);
    // один масштаб на все знаки — их естественные размеры друг к другу сохраняются
    k = tallest > 0 ? height / tallest : 1;
    GdipCreateMatrix2((REAL)k, 0, 0, (REAL)k, 0, 0, &m);
    for (int i = 0; i < n; i++)
        if (out[i]) GdipTransformPath(out[i], m);
    GdipDeleteMatrix(m);
}

void fr_free_paths(GpPath **paths, int n)
{
    for (int i = 0; i < n; i++)
        if (paths[i]) GdipDeletePath(paths[i]);
}

void fr_place(GpGraphics *g, GpBrush *fill, GpPath *shape, double cx, double cy)
{
    RectF b;
    UINT st;
    if (!shape) return;
    GdipGetPathWorldBounds(shape, &b, NULL, NULL);
    st = fr_push(g, 1, 0, 0, 1, wui_round(cx - (b.X + (double)b.X + b.Width) / 2),
                 wui_round(cy - (b.Y + (double)b.Y + b.Height) / 2));
    GdipFillPath(g, fill, shape);
    fr_pop(g, st);
}

// ================= рамки =================

// Рамка-прямоугольник в inset от краёв; перо по центру линии.
static void line_rect(GpGraphics *g, double w, double h, double inset, WuiColor c, double th, double opacity)
{
    GpBrush *b = fr_solid(fr_opacity(c, opacity));
    GpPen *p = fr_pen(b, th);
    fr_inset(g, p, w, h, inset);
    GdipDeletePen(p);
    GdipDeleteBrush(b);
}

// Три линии (внешняя, средняя, тонкая внутренняя) и точки в углах.
static void triple_line(GpGraphics *g, double w, double h, WuiColor outer, WuiColor middle, WuiColor inner,
                        WuiColor dot, WuiColor dot_stroke)
{
    GpBrush *fill, *sb;
    GpPen *pen;
    line_rect(g, w, h, 1, outer, 2, 1);
    line_rect(g, w, h, 7 + 1.5, middle, 3, 0.85);
    line_rect(g, w, h, 13 + 0.4, inner, 0.8, 0.7);
    fill = fr_solid(dot);
    sb = fr_solid(dot_stroke);
    pen = fr_pen(sb, 1);
    fr_ellipse(g, fill, pen, 10, 10, 4.5, 4.5);
    fr_ellipse(g, fill, pen, w - 10, 10, 4.5, 4.5);
    fr_ellipse(g, fill, pen, 10, h - 10, 4.5, 4.5);
    fr_ellipse(g, fill, pen, w - 10, h - 10, 4.5, 4.5);
    GdipDeletePen(pen);
    GdipDeleteBrush(sb);
    GdipDeleteBrush(fill);
}

// Две золочёные линии, тонкая внутренняя и квадраты с ромбами в углах.
static void frame_double_gold(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *gold = fr_gilded(a2, w, h, 1), *soft = fr_gilded(a2, w, h, 0.55);
    GpBrush *hole = fr_solid(t->is_dark ? t->c[WUI_C_BACKGROUND1] : t->c[WUI_C_PAPER]);
    GpPen *p1 = fr_pen(gold, 1.6), *p2 = fr_pen(gold, 0.8), *inner = fr_pen(soft, 0.5);
    fr_inset(g, p1, w, h, 6);
    fr_inset(g, p2, w, h, 11);
    fr_inset(g, inner, w, h, 19.5);
    for (int c = 0; c < 4; c++) {
        double x, y;
        // квадрат на обе линии с вырезанным ромбом
        fr_toward(c, w, h, 8.5, &x, &y);
        fr_rect(g, gold, NULL, x - 5, y - 5, 10, 10);
        fr_geo_free(g, hole, NULL, fr_diamond(x, y, 3.2));
        fr_geo_free(g, gold, NULL, fr_diamond(x, y, 1.4));
        // ромбик на повороте тонкой линии
        fr_toward(c, w, h, 19.5, &x, &y);
        fr_geo_free(g, soft, inner, fr_diamond(x, y, 2.4));
    }
    GdipDeletePen(inner);
    GdipDeletePen(p2);
    GdipDeletePen(p1);
    GdipDeleteBrush(hole);
    GdipDeleteBrush(soft);
    GdipDeleteBrush(gold);
}

// Модерн: линия с вогнутыми углами, плети-усики с завитками, кружки.
static void frame_art_nouveau(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double m = 8, r = 13;
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *gold = fr_gilded(a2, w, h, 1), *hair_b = fr_gilded(a2, w, h, 0.45);
    GpBrush *dot = fr_solid(t->c[WUI_C_ACCENT]);
    GpBrush *ground = fr_solid(t->is_dark ? t->c[WUI_C_BACKGROUND1] : t->c[WUI_C_PAPER]);
    GpPen *pen = fr_pen(gold, 1.3), *thin = fr_pen(gold, 0.8), *hair = fr_pen(hair_b, 0.6);
    GpPath *frame = NULL, *corner, *leaves, *lens;
    const double ys[2] = { m, h - m };

    // основная линия с вогнутыми (вырезанными) углами
    GdipCreatePath(FillModeAlternate, &frame);
    GdipAddPathLine(frame, (REAL)(m + r), (REAL)m, (REAL)(w - m - r), (REAL)m);
    fr_arc_to(frame, w - m - r, m, w - m, m + r, r, r, 0, 0);
    GdipAddPathLine(frame, (REAL)(w - m), (REAL)(m + r), (REAL)(w - m), (REAL)(h - m - r));
    fr_arc_to(frame, w - m, h - m - r, w - m - r, h - m, r, r, 0, 0);
    GdipAddPathLine(frame, (REAL)(w - m - r), (REAL)(h - m), (REAL)(m + r), (REAL)(h - m));
    fr_arc_to(frame, m + r, h - m, m, h - m - r, r, r, 0, 0);
    GdipAddPathLine(frame, (REAL)m, (REAL)(h - m - r), (REAL)m, (REAL)(m + r));
    fr_arc_to(frame, m, m + r, m + r, m, r, r, 0, 0);
    GdipClosePathFigure(frame);
    fr_geo_free(g, NULL, pen, frame);
    fr_inset(g, hair, w, h, 3.5);

    // узор левого верхнего угла, отражённый в остальные
    corner = fr_parse(
        // плеть вдоль верхнего края с завитком
        "M 17.2,17.2 C 24,23 32,23.5 40,18.5 C 47,14 54,12.5 61,14.5 C 67,16.2 68.5,21.5 64.5,22.4 C 61.2,23.1 60,19.6 62.6,18.6 "
        // то же вдоль левого
        "M 17.2,17.2 C 23,24 23.5,32 18.5,40 C 14,47 12.5,54 14.5,61 C 16.2,67 21.5,68.5 22.4,64.5 C 23.1,61.2 19.6,60 18.6,62.6 "
        // веточка по диагонали
        "M 17.2,17.2 C 20.5,19.5 22.5,21 24.5,24.5");
    leaves = fr_parse(
        "M 30,21.6 C 33,17.5 37.5,16.8 40.5,17.6 C 37.5,20.4 33.5,22.4 30,21.6 Z "
        "M 21.6,30 C 17.5,33 16.8,37.5 17.6,40.5 C 20.4,37.5 22.4,33.5 21.6,30 Z");
    for (int i = 0; i < 4; i++) {
        UINT st = fr_mirror(g, i, w, h);
        fr_geo(g, NULL, thin, corner);
        fr_geo(g, gold, NULL, leaves);
        fr_ellipse(g, NULL, thin, m, m, 3.2, 3.2);
        fr_ellipse(g, dot, NULL, m, m, 1.5, 1.5);
        fr_ellipse(g, dot, NULL, 26.2, 26.2, 1.5, 1.5);
        fr_ellipse(g, gold, NULL, 62.6, 18.6, 1.1, 1.1);
        fr_ellipse(g, gold, NULL, 18.6, 62.6, 1.1, 1.1);
        fr_pop(g, st);
    }
    GdipDeletePath(leaves);
    GdipDeletePath(corner);

    // линза посередине верхнего и нижнего края
    lens = fr_parse("M -22,0 C -14,0 -10,-5.5 0,-5.5 C 10,-5.5 14,0 22,0 C 14,0 10,5.5 0,5.5 C -10,5.5 -14,0 -22,0 Z");
    for (int i = 0; i < 2; i++) {
        UINT st = fr_push(g, 1, 0, 0, 1, w / 2, ys[i]);
        fr_geo(g, ground, thin, lens);
        fr_ellipse(g, NULL, thin, 0, 0, 3, 3);
        fr_ellipse(g, dot, NULL, 0, 0, 1.3, 1.3);
        fr_pop(g, st);
    }
    GdipDeletePath(lens);

    GdipDeletePen(hair);
    GdipDeletePen(thin);
    GdipDeletePen(pen);
    GdipDeleteBrush(ground);
    GdipDeleteBrush(dot);
    GdipDeleteBrush(hair_b);
    GdipDeleteBrush(gold);
}

// Египет: фризы иероглифов сверху и снизу между двойными линиями, по бокам — одиночные линии.
static void frame_egyptian(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    static const unsigned codes[5] = { 0x132F9, 0x13080, 0x131A3, 0x131F3, 0x132BD };   // анх, око, скарабей, солнце, джед
    static const WCHAR *const fonts[1] = { L"Segoe UI Historic" };
    const double top = 8, bottom = 34, step = 30;
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    GpBrush *line = fr_solid(a2), *fine_b = fr_solid(fr_opacity(a2, 0.6));
    GpBrush *fills[2];
    GpPen *pen = fr_pen(line, 1.4), *fine = fr_pen(fine_b, 0.6);
    GpPath *shapes[5];
    const double ys[4] = { top, bottom, h - bottom, h - top };
    const double ys2[4] = { top + 2.5, bottom - 2.5, h - bottom + 2.5, h - top - 2.5 };
    double inner = w - 2 * (top + 14), x0, mid = (top + bottom) / 2;
    int n = (int)floor(inner / step);
    if (n < 0) n = 0;

    // линии фризов, боковые и тонкие внутренние
    for (int i = 0; i < 4; i++) fr_line(g, pen, top, ys[i], w - top, ys[i]);
    for (int i = 0; i < 4; i++) fr_line(g, fine, top + 6, ys2[i], w - top - 6, ys2[i]);
    fr_line(g, pen, top, top, top, h - top);
    fr_line(g, pen, w - top, top, w - top, h - top);

    fr_glyph_shapes(fonts, 1, codes, 5, 18, shapes);
    fills[0] = fr_solid(t->c[WUI_C_ACCENT]);
    fills[1] = fr_solid(a2);
    x0 = (w - (n - 1) * step) / 2;
    for (int band = 0; band < 2; band++) {
        double cy = band == 0 ? mid : h - mid;
        // тонкие перегородки между знаками, как регистры на стене храма
        for (int i = 0; i + 1 < n; i++) {
            double dx = wui_round(x0 + (i + 0.5) * step) + 0.5;
            fr_line(g, fine, dx, cy - 8, dx, cy + 8);
        }
        for (int i = 0; i < n; i++)
            fr_place(g, fills[i % 2], shapes[i % 5], x0 + i * step, cy);
    }
    fr_free_paths(shapes, 5);

    // угловые блоки: золотой квадрат с лазуритовой серединой
    {
        const double cx[4] = { top, w - top, top, w - top }, cy[4] = { mid, mid, h - mid, h - mid };
        for (int i = 0; i < 4; i++) {
            fr_rect(g, fills[1], NULL, cx[i] - 7, cy[i] - 7, 14, 14);
            fr_rect(g, fills[0], NULL, cx[i] - 3.5, cy[i] - 3.5, 7, 7);
        }
    }
    GdipDeleteBrush(fills[1]);
    GdipDeleteBrush(fills[0]);
    GdipDeletePen(fine);
    GdipDeletePen(pen);
    GdipDeleteBrush(fine_b);
    GdipDeleteBrush(line);
}

typedef struct { double x, y; WCHAR text[12]; int len; } Label;

// Целое неотрицательное число — строкой.
static int int_text(int v, WCHAR *out)
{
    WCHAR tmp[12];
    int n = 0, len = 0;
    do { tmp[n++] = (WCHAR)(L'0' + v % 10); v /= 10; } while (v > 0 && n < 11);
    while (n > 0) out[len++] = tmp[--n];
    out[len] = 0;
    return len;
}

// Гравировка астролябии: две линии со шкалой градусов между ними и редкими числами.
static void frame_engraved(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    static const WCHAR *const fonts[2] = { L"Palatino Linotype", L"Georgia" };
    const double outer = 4.5, inner = 22.5, gap = 24;
    WuiColor c = t->c[WUI_C_ACCENT2];
    GpBrush *rule_b = fr_solid(wui_alpha(c, 0xC8)), *tick_b = fr_solid(wui_alpha(c, 0xA8));
    GpBrush *text = fr_solid(wui_alpha(c, 0xD0)), *faint_b = fr_solid(wui_alpha(c, 0x60));
    GpPen *rule = fr_pen(rule_b, 1), *tick = fr_pen(tick_b, 0.7), *major_pen = fr_pen(rule_b, 0.9);
    GpPen *faint = fr_pen(faint_b, 0.5);
    GpPath *minor = NULL, *major = NULL;
    GpFontFamily *face;
    Label *labels;
    int nlabels = 0, k, index = 0;
    double perimeter = 0, spacing, carry = 0;
    // стороны между угловыми квадратами по часовой от левого верхнего: начало, конец, наружу
    const double edges[4][6] = {
        { gap, inner, w - gap, inner, 0, -1 },
        { w - inner, gap, w - inner, h - gap, 1, 0 },
        { w - gap, h - inner, gap, h - inner, 0, 1 },
        { inner, h - gap, inner, gap, -1, 0 }
    };

    fr_inset(g, rule, w, h, outer);
    fr_inset(g, rule, w, h, inner);
    fr_inset(g, faint, w, h, inner + 2.5);

    for (int e = 0; e < 4; e++) {
        double dx = edges[e][2] - edges[e][0], dy = edges[e][3] - edges[e][1];
        perimeter += sqrt(dx * dx + dy * dy);
    }
    k = (int)wui_round(perimeter / (4.0 * 360));
    if (k < 1) k = 1;
    spacing = perimeter / (360.0 * k);

    labels = (Label *)malloc(sizeof(Label) * (12 * k + 16));
    GdipCreatePath(FillModeAlternate, &minor);
    GdipCreatePath(FillModeAlternate, &major);
    // окно 48×48 DIP: стороны нулевые, шаг 0 — шкалы нет (в C# здесь вечный цикл)
    for (int e = 0; e < 4 && spacing > 0; e++) {
        double dx = edges[e][2] - edges[e][0], dy = edges[e][3] - edges[e][1];
        double len = sqrt(dx * dx + dy * dy), ox = edges[e][4], oy = edges[e][5];
        if (len <= 0) continue;
        dx /= len;
        dy /= len;
        for (double s = carry; s <= len; s += spacing, index++) {
            double px = edges[e][0] + dx * s, py = edges[e][1] + dy * s;
            int is_major = index % 10 == 0;
            double size = is_major ? 9 : index % 5 == 0 ? 5.5 : 3;
            GpPath *path = is_major ? major : minor;
            GdipStartPathFigure(path);
            GdipAddPathLine(path, (REAL)px, (REAL)py, (REAL)(px + ox * size), (REAL)(py + oy * size));
            // числа — в стороне от угловых розеток
            if (nlabels < 12 * k + 16 && index % 30 == 0 && index % k == 0 && (index == 0 || (s > 36 && s < len - 36))) {
                Label *l = &labels[nlabels++];
                l->x = px + ox * 13.5;
                l->y = py + oy * 13.5;
                l->len = int_text(index / k, l->text);
            }
            carry = s + spacing - len;
        }
    }
    fr_geo_free(g, NULL, tick, minor);
    fr_geo_free(g, NULL, major_pen, major);

    // числа у длинной риски: сверху и снизу прямо, по бокам — вдоль края
    face = fr_family(fonts, 2);
    if (face) {
        UINT16 em = 1, asc = 0, ls = 0;
        double height;
        GdipGetEmHeight(face, FontStyleRegular, &em);
        GdipGetCellAscent(face, FontStyleRegular, &asc);
        GdipGetLineSpacing(face, FontStyleRegular, &ls);
        height = 7.5 * ls / em;                     // FormattedText.Height
        for (int i = 0; i < nlabels; i++) {
            Label *l = &labels[i];
            double angle = l->y < inner || l->y > h - inner ? 0 : l->x > w - inner ? 90 : 270;
            UINT st = fr_push_rotate(g, angle, l->x, l->y);
            double top = l->y - height / 2 + 0.5;
            PointF base;
            GpMatrix *m = NULL;
            // строка текста в WPF встаёт основанием на целую точку экрана (поперёк строки)
            base.X = (REAL)(l->x + 2.5);
            base.Y = (REAL)(top + 7.5 * asc / em);
            GdipCreateMatrix(&m);
            GdipGetWorldTransform(g, m);
            GdipTransformMatrixPoints(m, &base, 1);
            GdipDeleteMatrix(m);
            if (angle == 0) GdipTranslateWorldTransform(g, 0, (REAL)(floor(base.Y + 0.5) - base.Y), MatrixOrderAppend);
            else GdipTranslateWorldTransform(g, (REAL)(floor(base.X + 0.5) - base.X), 0, MatrixOrderAppend);
            fr_geo_free(g, text, NULL, fr_text_path(face, l->text, l->len, 7.5, 0, l->x + 2.5, top));
            fr_pop(g, st);
        }
        GdipDeleteFontFamily(face);
    }
    free(labels);

    // угловые розетки
    for (int cc = 0; cc < 4; cc++) {
        double px, py;
        fr_toward(cc, w, h, (outer + inner) / 2, &px, &py);
        fr_ellipse(g, NULL, rule, px, py, 6.5, 6.5);
        fr_ellipse(g, NULL, tick, px, py, 3.5, 3.5);
        fr_ellipse(g, text, NULL, px, py, 1.2, 1.2);
        for (int a = 0; a < 8; a++) {
            double rad = a * PI / 4, dx = cos(rad), dy = sin(rad);
            fr_line(g, tick, px + dx * 3.5, py + dy * 3.5, px + dx * 6.5, py + dy * 6.5);
        }
    }

    GdipDeletePen(faint);
    GdipDeletePen(major_pen);
    GdipDeletePen(tick);
    GdipDeletePen(rule);
    GdipDeleteBrush(faint_b);
    GdipDeleteBrush(text);
    GdipDeleteBrush(tick_b);
    GdipDeleteBrush(rule_b);
}

// ---- печать ----

// Части печати в её координатах (0..size): неровный контур и вырезы (рамка внутри, знак, крапины).
static void seal_paths(double size, GpPath **outline, GpPath **ring, GpPath **glyph, GpPath **specks)
{
    static const WCHAR *const fonts[4] = { L"Yu Mincho", L"MS Mincho", L"SimSun", L"MingLiU" };
    WuiRandom rnd;
    PointF pts[56];
    int per = 14, n = 0;
    GpFontFamily *face;
    wui_random_init(&rnd, 88);
    // неровный край: много чуть сдвинутых точек по квадрату
    for (int side = 0; side < 4; side++)
        for (int i = 0; i < per; i++) {
            double f = (double)i / per * size;
            double j = (wui_random_double(&rnd) - 0.5) * 1.3;
            double x, y;
            switch (side) {
            case 0:  x = f; y = j; break;
            case 1:  x = size + j; y = f; break;
            case 2:  x = size - f; y = size + j; break;
            default: x = j; y = size - f; break;
            }
            pts[n].X = (REAL)x;
            pts[n].Y = (REAL)y;
            n++;
        }
    GdipCreatePath(FillModeAlternate, outline);
    GdipAddPathPolygon(*outline, pts, n);

    GdipCreatePath(FillModeAlternate, ring);
    GdipAddPathRectangle(*ring, 3.2f, 3.2f, (REAL)(size - 6.4), (REAL)(size - 6.4));
    GdipAddPathRectangle(*ring, 4.6f, 4.6f, (REAL)(size - 9.2), (REAL)(size - 9.2));

    *glyph = NULL;
    face = fr_family(fonts, 4);
    if (face) {
        RectF b;
        GpPath *p = fr_text_path(face, L"\x548C", 1, 30, 1, 0, 0);    // 和
        GdipFlattenPath(p, NULL, 0.02f);
        GdipGetPathWorldBounds(p, &b, NULL, NULL);
        if (b.Width > 0 && b.Height > 0) {
            double kx = (size - 16) / b.Width, ky = (size - 16) / b.Height, k = kx < ky ? kx : ky;
            GpMatrix *m = NULL;
            GdipCreateMatrix(&m);
            GdipTranslateMatrix(m, (REAL)(-(b.X + (double)b.X + b.Width) / 2), (REAL)(-(b.Y + (double)b.Y + b.Height) / 2), MatrixOrderAppend);
            GdipScaleMatrix(m, (REAL)k, (REAL)k, MatrixOrderAppend);
            GdipTranslateMatrix(m, (REAL)(size / 2), (REAL)(size / 2), MatrixOrderAppend);
            GdipTransformPath(p, m);
            GdipDeleteMatrix(m);
            *glyph = p;
        } else GdipDeletePath(p);
        GdipDeleteFontFamily(face);
    }

    // крапины без краски; каждая — своя фигура
    GdipCreatePath(FillModeWinding, specks);
    for (int i = 0; i < 14; i++) {
        double r = 0.3 + wui_random_double(&rnd) * 0.8;
        double x = 2 + wui_random_double(&rnd) * (size - 4);
        double y = 2 + wui_random_double(&rnd) * (size - 4);
        double ry = r * (0.6 + wui_random_double(&rnd) * 0.6);
        GdipAddPathEllipse(*specks, (REAL)(x - r), (REAL)(y - ry), (REAL)(2 * r), (REAL)(2 * ry));
    }
}

// Сглаженное покрытие фигур в точках экрана: битмап bw×bh, сдвиг (bx, by).
static GpBitmap *coverage(GpMatrix *m, int bx, int by, int bw, int bh, GpPath **paths, int n)
{
    GpBitmap *bmp = NULL;
    GpGraphics *bg = NULL;
    GpBrush *white = fr_solid(0xFFFFFFFF);
    GdipCreateBitmapFromScan0(bw, bh, 0, PixelFormat32bppARGB, NULL, &bmp);
    GdipGetImageGraphicsContext(bmp, &bg);
    GdipGraphicsClear(bg, 0);
    GdipSetSmoothingMode(bg, SmoothingModeAntiAlias8x8);
    GdipSetPixelOffsetMode(bg, PixelOffsetModeHalf);
    GdipSetWorldTransform(bg, m);
    GdipTranslateWorldTransform(bg, (REAL)-bx, (REAL)-by, MatrixOrderAppend);
    for (int i = 0; i < n; i++)
        if (paths[i]) GdipFillPath(bg, white, paths[i]);
    GdipDeleteGraphics(bg);
    GdipDeleteBrush(white);
    return bmp;
}

// Печать: контур минус вырезы (Geometry.Combine Exclude). У GDI+ нет вычитания путей со сглаживанием,
// поэтому покрытие контура и вырезов считается в точках экрана и вычитается по пикселям.
static void draw_seal(GpGraphics *g, WuiColor color, double x, double y, double size)
{
    GpPath *outline, *ring, *glyph, *specks, *cuts[3];
    GpMatrix *m = NULL;
    PointF box[4];
    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    int bx, by, bw, bh;
    GpBitmap *a, *b;
    Rect lr;
    BitmapData da, db;

    seal_paths(size, &outline, &ring, &glyph, &specks);
    // перевод в экран: мировое преобразование g, сдвиг в угол и наклон -1.5° вокруг середины
    GdipCreateMatrix(&m);
    GdipGetWorldTransform(g, m);
    GdipTranslateMatrix(m, (REAL)x, (REAL)y, MatrixOrderPrepend);
    GdipTranslateMatrix(m, (REAL)(size / 2), (REAL)(size / 2), MatrixOrderPrepend);
    GdipRotateMatrix(m, -1.5f, MatrixOrderPrepend);
    GdipTranslateMatrix(m, (REAL)(-size / 2), (REAL)(-size / 2), MatrixOrderPrepend);
    box[0].X = -2;                box[0].Y = -2;
    box[1].X = (REAL)(size + 2);  box[1].Y = -2;
    box[2].X = (REAL)(size + 2);  box[2].Y = (REAL)(size + 2);
    box[3].X = -2;                box[3].Y = (REAL)(size + 2);
    GdipTransformMatrixPoints(m, box, 4);
    for (int i = 0; i < 4; i++) {
        if (box[i].X < minx) minx = box[i].X;
        if (box[i].Y < miny) miny = box[i].Y;
        if (box[i].X > maxx) maxx = box[i].X;
        if (box[i].Y > maxy) maxy = box[i].Y;
    }
    bx = (int)floor(minx);
    by = (int)floor(miny);
    bw = (int)ceil(maxx) - bx;
    bh = (int)ceil(maxy) - by;
    if (bw > 0 && bh > 0 && bw < 8192 && bh < 8192) {
        cuts[0] = ring;
        cuts[1] = glyph;
        cuts[2] = specks;
        a = coverage(m, bx, by, bw, bh, &outline, 1);
        b = coverage(m, bx, by, bw, bh, cuts, 3);
        lr.X = 0; lr.Y = 0; lr.Width = bw; lr.Height = bh;
        if (GdipBitmapLockBits(a, &lr, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &da) == Ok) {
            if (GdipBitmapLockBits(b, &lr, ImageLockModeRead, PixelFormat32bppARGB, &db) == Ok) {
                double k = WUI_A(color) / 255.0 * 0.86;       // PushOpacity(0.86)
                for (int yy = 0; yy < bh; yy++) {
                    unsigned *pa = (unsigned *)((BYTE *)da.Scan0 + yy * da.Stride);
                    unsigned *pb = (unsigned *)((BYTE *)db.Scan0 + yy * db.Stride);
                    for (int xx = 0; xx < bw; xx++) {
                        double cov = (pa[xx] >> 24) / 255.0 * (1 - (pb[xx] >> 24) / 255.0);
                        int al = (int)(cov * k * 255 + 0.5);
                        pa[xx] = (color & 0x00FFFFFFu) | ((unsigned)al << 24);
                    }
                }
                GdipBitmapUnlockBits(b, &db);
            }
            GdipBitmapUnlockBits(a, &da);
        }
        {
            UINT st;
            GdipSaveGraphics(g, &st);
            GdipResetWorldTransform(g);
            GdipSetInterpolationMode(g, InterpolationModeNearestNeighbor);
            GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
            GdipDrawImageRectI(g, a, bx, by, bw, bh);
            GdipRestoreGraphics(g, st);
        }
        GdipDisposeImage(b);
        GdipDisposeImage(a);
    }
    GdipDeleteMatrix(m);
    GdipDeletePath(specks);
    if (glyph) GdipDeletePath(glyph);
    GdipDeletePath(ring);
    GdipDeletePath(outline);
}

// Тушевая линия и красная квадратная печать в правом нижнем углу с вырезанным знаком.
static void frame_seal(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double size = 46;
    WuiColor text = t->c[WUI_C_TEXT];
    line_rect(g, w, h, 10.5, text, 1, 0.85);
    line_rect(g, w, h, 13.5, text, 0.5, 0.3);
    draw_seal(g, t->c[WUI_C_ACCENT], w - 10 - 10 - size, h - 10 - 10 - size, size);
}

// ---- металл с винтами ----

static void fill_quad(GpGraphics *g, WuiColor c, double ax, double ay, double bx, double by,
                      double cx, double cy, double dx, double dy)
{
    PointF p[4];
    GpBrush *b = fr_solid(c);
    p[0].X = (REAL)ax; p[0].Y = (REAL)ay;
    p[1].X = (REAL)bx; p[1].Y = (REAL)by;
    p[2].X = (REAL)cx; p[2].Y = (REAL)cy;
    p[3].X = (REAL)dx; p[3].Y = (REAL)dy;
    GdipFillPolygon(g, b, p, 4, FillModeAlternate);
    GdipDeleteBrush(b);
}

// Радиальный градиент WPF на эллипсе: центр (gx, gy), радиусы (rx, ry), исток (ox, oy);
// stops — смещения 0..1 от истока к краю. В GDI+ — градиент пути: 0 у края, 1 в середине.
static GpBrush *radial(double gx, double gy, double rx, double ry, double ox, double oy,
                       const WuiColor *colors, const double *stops, int n)
{
    GpPath *p = NULL;
    GpPathGradient *b = NULL;
    ARGB c[8];
    REAL pos[8];
    PointF o;
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathEllipse(p, (REAL)(gx - rx), (REAL)(gy - ry), (REAL)(2 * rx), (REAL)(2 * ry));
    GdipCreatePathGradientFromPath(p, &b);
    GdipDeletePath(p);
    o.X = (REAL)ox;
    o.Y = (REAL)oy;
    GdipSetPathGradientCenterPoint(b, &o);
    for (int i = 0; i < n; i++) {
        c[i] = colors[n - 1 - i];
        pos[i] = (REAL)(1 - stops[n - 1 - i]);
    }
    GdipSetPathGradientPresetBlend(b, c, pos, n);
    return b;
}

static void screw(GpGraphics *g, double px, double py, double r, double angle, WuiColor metal)
{
    WuiColor sh[2] = { 0x70000000, 0x00000000 }, hc[3];
    double s2[2] = { 0, 1 }, s3[3] = { 0, 0.5, 1 }, d = 2 * r;
    double rad = angle * PI / 180, vx = cos(rad) * (r - 1.1), vy = sin(rad) * (r - 1.1);
    double ox = 0.4, oy = 0.9;
    GpBrush *shadow, *head, *edge_b, *slot_b, *gleam_b;
    GpPen *edge, *slot, *gleam;

    shadow = radial(px + 1, py + 1.4, r + 2.2, r + 2.2, px + 1, py + 1.4, sh, s2, 2);
    fr_ellipse(g, shadow, NULL, px + 1, py + 1.4, r + 2.2, r + 2.2);
    GdipDeleteBrush(shadow);

    // головка: свет слева сверху (доли рамки круга: центр 0.4/0.38, исток 0.32/0.28, радиус 0.75)
    hc[0] = wui_mix(metal, 0xFFFFFFFF, 0.7);
    hc[1] = metal;
    hc[2] = wui_mix(metal, 0xFF000000, 0.55);
    head = radial(px - r + 0.4 * d, py - r + 0.38 * d, 0.75 * d, 0.75 * d, px - r + 0.32 * d, py - r + 0.28 * d, hc, s3, 3);
    edge_b = fr_solid(wui_mix(metal, 0xFF000000, 0.7));
    edge = fr_pen(edge_b, 0.7);
    fr_ellipse(g, head, edge, px, py, r, r);

    slot_b = fr_solid(wui_mix(metal, 0xFF000000, 0.8));
    slot = fr_pen(slot_b, 1.5);
    GdipSetPenLineCap197819(slot, LineCapRound, LineCapRound, 0);
    fr_line(g, slot, px - vx, py - vy, px + vx, py + vy);
    gleam_b = fr_solid(0x70FFFFFF);
    gleam = fr_pen(gleam_b, 0.6);
    fr_line(g, gleam, px - vx * 0.85 + ox, py - vy * 0.85 + oy, px + vx * 0.85 + ox, py + vy * 0.85 + oy);

    GdipDeletePen(gleam);
    GdipDeleteBrush(gleam_b);
    GdipDeletePen(slot);
    GdipDeleteBrush(slot_b);
    GdipDeletePen(edge);
    GdipDeleteBrush(edge_b);
    GdipDeleteBrush(head);
}

// Металлическая пластина: скос со светом слева сверху и четыре винта с повёрнутыми шлицами.
static void frame_riveted(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double o = 6, i = 12;
    static const double slots[4] = { 25, 110, -40, 70 };
    WuiColor metal = wui_mix(t->is_dark ? t->c[WUI_C_BACKGROUND2] : t->c[WUI_C_PAPER], t->c[WUI_C_EDGE], 0.35);
    WuiColor light = wui_alpha(wui_mix(metal, 0xFFFFFFFF, 0.55), 0xD0);
    WuiColor dark = wui_alpha(wui_mix(metal, 0xFF000000, 0.6), 0xD0);

    // скос: четыре трапеции, светлые сверху и слева, тёмные снизу и справа
    fill_quad(g, light, o, o, w - o, o, w - i, i, i, i);
    fill_quad(g, wui_mix(light, wui_alpha(metal, 0xD0), 0.3), o, o, i, i, i, h - i, o, h - o);
    fill_quad(g, dark, o, h - o, i, h - i, w - i, h - i, w - o, h - o);
    fill_quad(g, wui_mix(dark, wui_alpha(metal, 0xD0), 0.3), w - o, o, w - o, h - o, w - i, h - i, w - i, i);
    line_rect(g, w, h, o - 0.4, 0x70000000, 0.8, 1);
    line_rect(g, w, h, i + 0.4, 0x40000000, 0.8, 1);
    line_rect(g, w, h, i + 1.2, 0x24FFFFFF, 0.8, 1);

    for (int c = 0; c < 4; c++) {
        double x, y;
        fr_toward(c, w, h, 18, &x, &y);
        screw(g, x, y, 5.2, slots[c], metal);
    }
}

// ================= вход =================

void wui_frame_draw(GpGraphics *g, const WuiTheme *t, float wf, float hf)
{
    double w = wf, h = hf;
    UINT st;
    if (!g || !t || w < 2 || h < 2 || t->frame == WUI_FRAME_NONE) return;
    GdipSaveGraphics(g, &st);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias8x8);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);     // как в WPF: точка (0,0) — угол пикселя
    // Отсчёты сглаживания 8×8 у WPF сдвинуты на 1/32 точки против GDI+ (подобрано сверкой по линиям
    // 0.5–1.6 DIP): тот же сдвиг рисунка даёт то же покрытие точек, что и в WPF
    GdipTranslateWorldTransform(g, -1.0f / 32, -1.0f / 32, MatrixOrderAppend);
    switch (t->frame) {
    case WUI_FRAME_TRIPLELINE:
        if (t->is_dark)
            triple_line(g, w, h, t->c[WUI_C_ACCENT], t->c[WUI_C_ACCENT2], t->c[WUI_C_ACCENT], t->c[WUI_C_ACCENT2],
                        t->c[WUI_C_BACKGROUND1]);
        else
            triple_line(g, w, h, t->c[WUI_C_TEXT], t->c[WUI_C_ACCENT], t->c[WUI_C_TEXTDIM], t->c[WUI_C_ACCENT],
                        t->c[WUI_C_TEXT]);
        break;
    case WUI_FRAME_DOUBLEGOLD: frame_double_gold(g, t, w, h); break;
    case WUI_FRAME_ARTNOUVEAU: frame_art_nouveau(g, t, w, h); break;
    case WUI_FRAME_EGYPTIAN:   frame_egyptian(g, t, w, h); break;
    case WUI_FRAME_ENGRAVED:   frame_engraved(g, t, w, h); break;
    case WUI_FRAME_SEAL:       frame_seal(g, t, w, h); break;
    case WUI_FRAME_RIVETED:    frame_riveted(g, t, w, h); break;
    case WUI_FRAME_MEANDER:    fr_meander(g, t, w, h); break;
    case WUI_FRAME_ARTDECO:    fr_artdeco(g, t, w, h); break;
    case WUI_FRAME_STITCHED:   fr_stitched(g, t, w, h); break;
    case WUI_FRAME_CELTIC:     fr_celtic(g, t, w, h); break;
    case WUI_FRAME_ARABESQUE:  fr_arabesque(g, t, w, h); break;
    case WUI_FRAME_RUNIC:      fr_runic(g, t, w, h); break;
    }
    GdipRestoreGraphics(g, st);
}
