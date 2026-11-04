// WUI — фон окна: пятна света, звёзды, сетки, виньетка, строки развёртки.
// Те же слои и числа, что Backdrop.OnRender в ThemeLibrary; узоры — patterns.c, рамка — frames.c.
// Анимаций нет (в библиотеке они выключены): всё рисуется неподвижно, звёзды — все группы целиком.
#include "wui_int.h"

// Точек на DIP у текущего рисования (scale из wui_backdrop_draw).
static float dip_px = 1;

// Сплошное перо. Линию до полутора точек толщиной GDI+ рисует ровно в одну точку (0.6 — ярче,
// 1.3 — бледнее, чем у WPF); такое перо делается шириной в точку, а прозрачность умножается
// на толщину — линия закрывает столько же, сколько у WPF.
static GpPen *solid_pen(WuiColor c, float width)
{
    GpPen *pen;
    float px = width * dip_px;
    if (px <= 1.5f) {
        int a = (int)wui_round(WUI_A(c) * px);
        c = wui_alpha(c, a > 255 ? 255 : a);
        width = 1 / dip_px;
    }
    GdipCreatePen1(c, width, UnitWorld, &pen);
    return pen;
}

// ---- градиенты ----

// Цвет градиента WPF в точке t: между соседними остановками, за крайними — цвет крайней.
static WuiColor stop_at(const WuiStop *s, int n, double t)
{
    if (t <= s[0].offset) return s[0].color;
    if (t >= s[n - 1].offset) return s[n - 1].color;
    int i = 0;
    while (i + 2 < n && t > s[i + 1].offset) i++;
    double f = (t - s[i].offset) / (s[i + 1].offset - s[i].offset);
    return wui_mix(s[i].color, s[i + 1].color, f);
}

// Радиальная заливка как RadialGradientBrush WPF (GradientOrigin = Center): эллипс-путь с PathGradient.
// У GDI+ позиции перевёрнуты (0 — край, 1 — центр); остановки вне 0..1 (1.05 у виньетки)
// сведены к цвету, который WPF получает на краю отрезка. area — что залить (весь прямоугольник
// должен лежать внутри эллипса — у виньетки и углов развёртки так при любом размере);
// 0 — залить сам эллипс.
static void fill_radial(GpGraphics *g, float cx, float cy, float rx, float ry,
                        const WuiStop *s, int n, const RectF *area)
{
    ARGB colors[10];
    REAL pos[10];
    int k = 0;
    colors[k] = stop_at(s, n, 1);
    pos[k++] = 0;
    for (int i = n - 1; i >= 0; i--)
        if (s[i].offset > 0 && s[i].offset < 1) {
            colors[k] = s[i].color;
            pos[k++] = 1 - s[i].offset;
        }
    colors[k] = stop_at(s, n, 0);
    pos[k++] = 1;

    GpPath *path;
    GpPathGradient *brush;
    PointF c;
    c.X = cx;
    c.Y = cy;
    GdipCreatePath(FillModeAlternate, &path);
    GdipAddPathEllipse(path, cx - rx, cy - ry, 2 * rx, 2 * ry);
    GdipCreatePathGradientFromPath(path, &brush);
    GdipSetPathGradientCenterPoint(brush, &c);
    GdipSetPathGradientPresetBlend(brush, colors, pos, k);
    if (area) GdipFillRectangle(g, brush, area->X, area->Y, area->Width, area->Height);
    else GdipFillPath(g, brush, path);
    GdipDeleteBrush(brush);
    GdipDeletePath(path);
}

// Линейная кисть как LinearGradientBrush WPF от (x1, y1) к (x2, y2); за концами — отражение
// (у GDI+ нет «продлить крайний цвет», а отражение у самого края даёт тот же цвет).
static GpLineGradient *linear_brush(float x1, float y1, float x2, float y2, const WuiStop *s, int n)
{
    ARGB colors[10];
    REAL pos[10];
    int k = 0;
    colors[k] = stop_at(s, n, 0);
    pos[k++] = 0;
    for (int i = 0; i < n; i++)
        if (s[i].offset > 0 && s[i].offset < 1) {
            colors[k] = s[i].color;
            pos[k++] = s[i].offset;
        }
    colors[k] = stop_at(s, n, 1);
    pos[k++] = 1;

    PointF a, b;
    GpLineGradient *brush;
    a.X = x1;
    a.Y = y1;
    b.X = x2;
    b.Y = y2;
    GdipCreateLineBrush(&a, &b, colors[0], colors[k - 1], WrapModeTileFlipXY, &brush);
    GdipSetLinePresetBlend(brush, colors, pos, k);
    return brush;
}

static void stop_set(WuiStop *s, float offset, WuiColor color)
{
    s->offset = offset;
    s->color = color;
}

static void fill_solid_ellipse(GpGraphics *g, WuiColor color, double cx, double cy, double r)
{
    GpSolidFill *brush;
    GdipCreateSolidFill(color, &brush);
    GdipFillEllipse(g, brush, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r));
    GdipDeleteBrush(brush);
}

// ---- пятна света ----

static void orb(GpGraphics *g, WuiColor c, double opacity, double cx, double cy, double r)
{
    WuiColor core = wui_alpha(c, (int)(WUI_A(c) * opacity));
    WuiStop s[3];
    stop_set(&s[0], 0, core);
    stop_set(&s[1], 0.5f, wui_alpha(core, (int)(WUI_A(core) * 0.45)));
    stop_set(&s[2], 1, wui_alpha(core, 0));
    fill_radial(g, (float)cx, (float)cy, (float)r, (float)r, s, 3, 0);
}

static void draw_orbs(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    double m = w > h ? w : h;
    orb(g, t->c[WUI_C_ORB1], 0.5, w * 0.12, h * 0.12, m * 0.30);
    orb(g, t->c[WUI_C_ORB2], 0.5, w * 0.95, h * 0.85, m * 0.36);
    orb(g, t->c[WUI_C_ORB3], 0.35, w * 0.56, h * 0.08, m * 0.22);
}

// ---- звёзды ----

// Россыпь Random(7): сначала ровные звёзды, потом три группы мерцающих (здесь — в полную силу).
static void draw_stars(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    int count = (int)(w * h / 4200);
    WuiColor tint = wui_mix(0xFFFFFFFFu, t->c[WUI_C_ACCENT], 0.5);
    for (int pass = -1; pass < 3; pass++) {
        WuiRandom rnd;
        wui_random_init(&rnd, 7);
        for (int i = 0; i < count; i++) {
            // порядок вызовов — как в C#
            double x = wui_random_double(&rnd) * w;
            double y = wui_random_double(&rnd) * h;
            double r = 0.35 + pow(wui_random_double(&rnd), 3) * 1.1;
            int a = 60 + wui_random_below(&rnd, 170);
            int tinted = wui_random_double(&rnd) < 0.25;
            int group = i % 9 < 3 ? i % 9 : -1;
            if (group != pass) continue;
            WuiColor c = tinted ? tint : 0xFFFFFFFFu;
            fill_solid_ellipse(g, wui_alpha(c, a), x, y, r);
            if (i % 37 == 0) {
                WuiStop s[2];
                stop_set(&s[0], 0, wui_alpha(c, 0x70));
                stop_set(&s[1], 1, wui_alpha(c, 0));
                fill_radial(g, (float)x, (float)y, 5, 5, s, 2, 0);
            }
        }
    }
}

// ---- виньетка ----

static void draw_vignette(GpGraphics *g, double w, double h, WuiColor c, int mid, int edge)
{
    WuiStop s[3];
    RectF area;
    stop_set(&s[0], 0.5f, wui_alpha(c, 0));
    stop_set(&s[1], 0.86f, wui_alpha(c, mid));
    stop_set(&s[2], 1.05f, wui_alpha(c, edge));
    area.X = 0;
    area.Y = 0;
    area.Width = (REAL)w;
    area.Height = (REAL)h;
    fill_radial(g, (float)(w * 0.5), (float)(h * 0.45), (float)(w * 0.72), (float)(h * 0.78), s, 3, &area);
}

// ---- неоновая сетка ----

// Сетка в перспективе на нижних 40 % окна и светящийся горизонт.
static void draw_neon_grid(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    double horizon = wui_round(h * 0.6) + 0.5;
    double depth = h - horizon;
    WuiColor c = t->c[WUI_C_ACCENT];
    WuiStop s[3];
    GpLineGradient *brush;

    // слабое свечение пола
    stop_set(&s[0], 0, wui_alpha(c, 0x1C));
    stop_set(&s[1], 1, wui_alpha(c, 0x06));
    brush = linear_brush(0, (float)horizon, 0, (float)h, s, 2);
    GdipFillRectangle(g, brush, 0, (REAL)horizon, (REAL)w, (REAL)depth);
    GdipDeleteBrush(brush);

    GpPath *grid;
    GdipCreatePath(FillModeWinding, &grid);
    // лучи из точки схода
    double vx = w / 2, spread = 84;
    int count = (int)ceil(w * 2.5 / spread);
    for (int i = -count; i <= count; i++) {
        double dx = i * spread, dy = depth;
        GdipStartPathFigure(grid);
        GdipAddPathLine(grid, (REAL)(vx + dx * 0.015), (REAL)(horizon + dy * 0.015),
                        (REAL)(vx + dx * 4), (REAL)(horizon + dy * 4));
    }
    // поперечные линии, к горизонту всё теснее
    for (int k = 0; k < 40; k++) {
        double z = 1 + k * 0.55;
        double y = horizon + depth / z;
        if (y - horizon < 2.5) break;
        GdipStartPathFigure(grid);
        GdipAddPathLine(grid, 0, (REAL)y, (REAL)w, (REAL)y);
    }

    GpLineGradient *fade, *glow;
    GpPen *pen;
    stop_set(&s[0], 0, wui_alpha(c, 0x00));
    stop_set(&s[1], 0.35f, wui_alpha(c, 0x50));
    stop_set(&s[2], 1, wui_alpha(c, 0xB0));
    fade = linear_brush(0, (float)horizon, 0, (float)h, s, 3);
    stop_set(&s[0], 0, wui_alpha(c, 0x00));
    stop_set(&s[1], 1, wui_alpha(c, 0x28));
    glow = linear_brush(0, (float)horizon, 0, (float)h, s, 2);

    UINT state;
    GdipSaveGraphics(g, &state);
    GdipSetClipRect(g, 0, (REAL)horizon, (REAL)w, (REAL)depth, CombineModeIntersect);
    GdipCreatePen2(glow, 4, UnitWorld, &pen);
    GdipDrawPath(g, pen, grid);
    GdipDeletePen(pen);
    GdipCreatePen2(fade, 1, UnitWorld, &pen);
    GdipDrawPath(g, pen, grid);
    GdipDeletePen(pen);
    GdipRestoreGraphics(g, state);
    GdipDeleteBrush(fade);
    GdipDeleteBrush(glow);
    GdipDeletePath(grid);

    // горизонт: мягкое сияние вверх и вниз и яркая тонкая линия
    WuiColor a2 = t->c[WUI_C_ACCENT2];
    stop_set(&s[0], 0, wui_alpha(a2, 0x50));
    stop_set(&s[1], 0.35f, wui_alpha(a2, 0x14));
    stop_set(&s[2], 1, wui_alpha(a2, 0x00));
    brush = linear_brush(0, (float)horizon, 0, (float)(horizon - 60), s, 3);
    GdipFillRectangle(g, brush, 0, (REAL)(horizon - 60), (REAL)w, 60);
    GdipDeleteBrush(brush);

    stop_set(&s[0], 0, wui_alpha(a2, 0x38));
    stop_set(&s[1], 1, wui_alpha(a2, 0));
    brush = linear_brush(0, (float)horizon, 0, (float)(horizon + 14), s, 2);
    GdipFillRectangle(g, brush, 0, (REAL)horizon, (REAL)w, 14);
    GdipDeleteBrush(brush);

    stop_set(&s[0], 0, wui_alpha(a2, 0x30));
    stop_set(&s[1], 0.5f, wui_alpha(a2, 0xF0));
    stop_set(&s[2], 1, wui_alpha(a2, 0x30));
    brush = linear_brush(0, (float)horizon, (float)w, (float)horizon, s, 3);
    GdipCreatePen2(brush, 3, UnitWorld, &pen);
    GdipDrawLine(g, pen, 0, (REAL)horizon, (REAL)w, (REAL)horizon);
    GdipDeletePen(pen);
    GdipDeleteBrush(brush);

    pen = solid_pen(0x90FFFFFFu, 0.6f);
    GdipDrawLine(g, pen, (REAL)(w * 0.2), (REAL)horizon, (REAL)(w * 0.8), (REAL)horizon);
    GdipDeletePen(pen);
}

// ---- строки развёртки ----

// Тёмные строки через 3 DIP и чуть притенённые углы, как у кинескопа.
static void draw_scanlines(GpGraphics *g, double w, double h)
{
    GpPath *lines;
    GpSolidFill *black;
    GdipCreatePath(FillModeWinding, &lines);
    for (double y = 1; y < h; y += 3)
        GdipAddPathRectangle(lines, 0, (REAL)y, (REAL)w, 1);
    GdipCreateSolidFill(0x1C000000u, &black);
    GdipFillPath(g, black, lines);
    GdipDeleteBrush(black);
    GdipDeletePath(lines);

    WuiStop s[3];
    RectF area;
    stop_set(&s[0], 0.62f, 0x00000000u);
    stop_set(&s[1], 0.9f, 0x30000000u);
    stop_set(&s[2], 1.0f, 0x60000000u);
    area.X = 0;
    area.Y = 0;
    area.Width = (REAL)w;
    area.Height = (REAL)h;
    fill_radial(g, (float)(w * 0.5), (float)(h * 0.5), (float)(w * 0.75), (float)(h * 0.75), s, 3, &area);
}

// ---- полярная сетка ----

static void draw_circle(GpGraphics *g, GpPen *pen, double cx, double cy, double r)
{
    GdipDrawEllipse(g, pen, (REAL)(cx - r), (REAL)(cy - r), (REAL)(2 * r), (REAL)(2 * r));
}

// Подписи градусов: «0°» … «330°», по центру точки, повёрнуты вдоль круга.
static void draw_degree_labels(GpGraphics *g, WuiColor color, double cx, double cy, double radius)
{
    static const WCHAR palatino[] = { 'P', 'a', 'l', 'a', 't', 'i', 'n', 'o', ' ', 'L', 'i', 'n', 'o', 't', 'y', 'p', 'e', 0 };
    static const WCHAR georgia[] = { 'G', 'e', 'o', 'r', 'g', 'i', 'a', 0 };
    GpFontFamily *family = 0;
    GpFont *font;
    GpStringFormat *format;
    GpSolidFill *brush;
    if (GdipCreateFontFamilyFromName(palatino, 0, &family) != Ok &&
        GdipCreateFontFamilyFromName(georgia, 0, &family) != Ok &&
        GdipGetGenericFontFamilySerif(&family) != Ok)
        return;
    GdipCreateFont(family, 11, FontStyleRegular, UnitPixel, &font);
    // типографский вид, как FormattedText: без полей по бокам, строка в межстрочье шрифта
    GpStringFormat *typographic;
    GdipStringFormatGetGenericTypographic(&typographic);          // общий объект GDI+, не удаляется
    GdipCloneStringFormat(typographic, &format);
    GdipSetStringFormatFlags(format, StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    GdipSetStringFormatAlign(format, StringAlignmentCenter);
    GdipSetStringFormatLineAlign(format, StringAlignmentCenter);
    GdipCreateSolidFill(color, &brush);

    RectF box;
    box.X = -100;
    box.Y = -50;
    box.Width = 200;
    box.Height = 100;
    for (int a = 0; a < 360; a += 30) {
        WCHAR text[8];
        int n = 0;
        if (a >= 100) text[n++] = (WCHAR)('0' + a / 100);
        if (a >= 10) text[n++] = (WCHAR)('0' + a / 10 % 10);
        text[n++] = (WCHAR)('0' + a % 10);
        text[n++] = 0x00B0;                         // знак градуса
        text[n] = 0;
        double rad = a * 3.14159265358979323846 / 180;
        double px = cx + cos(rad) * radius, py = cy - sin(rad) * radius;
        UINT state;
        GdipSaveGraphics(g, &state);
        GdipTranslateWorldTransform(g, (REAL)px, (REAL)py, MatrixOrderPrepend);
        GdipRotateWorldTransform(g, (REAL)(90 - a), MatrixOrderPrepend);
        GdipDrawString(g, text, n, font, &box, format, brush);
        GdipRestoreGraphics(g, state);
    }
    GdipDeleteBrush(brush);
    GdipDeleteStringFormat(format);
    GdipDeleteFont(font);
    GdipDeleteFontFamily(family);
}

// Бледная астролябия: круги, лучи через 15°, шкала градусов и подписи через 30°.
static void draw_polar_grid(GpGraphics *g, const WuiTheme *t, double w, double h)
{
    const double pi = 3.14159265358979323846;
    WuiColor c = t->c[WUI_C_ACCENT2];
    double cx = w * 0.74, cy = h * 0.70;
    double fx = cx > w - cx ? cx : w - cx, fy = cy > h - cy ? cy : h - cy;
    double reach = sqrt(fx * fx + fy * fy);
    double unit = (w < h ? w : h) * 0.13;
    if (unit < 60) unit = 60;
    GpPen *ring = solid_pen(wui_alpha(c, 0x26), 0.8f);
    GpPen *faint = solid_pen(wui_alpha(c, 0x18), 0.6f);
    GpPen *tick = solid_pen(wui_alpha(c, 0x2C), 0.7f);

    UINT state;
    GdipSaveGraphics(g, &state);
    GdipSetClipRect(g, 0, 0, (REAL)w, (REAL)h, CombineModeIntersect);
    for (int i = 1; unit * i < reach; i++)
        draw_circle(g, i % 2 == 0 ? ring : faint, cx, cy, unit * i);
    draw_circle(g, ring, cx, cy, unit * 0.3);

    // лучи от второго круга наружу (по осям — от малого круга)
    GpPath *path;
    GdipCreatePath(FillModeWinding, &path);
    for (int a = 0; a < 360; a += 15) {
        double rad = a * pi / 180;
        double dx = cos(rad), dy = -sin(rad);
        double from = a % 90 == 0 ? unit * 0.3 : unit;
        GdipStartPathFigure(path);
        GdipAddPathLine(path, (REAL)(cx + dx * from), (REAL)(cy + dy * from),
                        (REAL)(cx + dx * reach), (REAL)(cy + dy * reach));
    }
    GdipDrawPath(g, faint, path);

    // шкала: штрих на каждый градус, длиннее через 5° и 10°
    double rs = unit * 3;
    GdipResetPath(path);
    for (int a = 0; a < 360; a++) {
        double rad = a * pi / 180;
        double dx = cos(rad), dy = -sin(rad);
        double len = a % 10 == 0 ? 9 : a % 5 == 0 ? 6 : 3.5;
        GdipStartPathFigure(path);
        GdipAddPathLine(path, (REAL)(cx + dx * rs), (REAL)(cy + dy * rs),
                        (REAL)(cx + dx * (rs + len)), (REAL)(cy + dy * (rs + len)));
    }
    GdipDrawPath(g, tick, path);
    GdipDeletePath(path);
    draw_circle(g, ring, cx, cy, rs + 9);

    draw_degree_labels(g, wui_alpha(c, 0x40), cx, cy, rs + 20);
    GdipRestoreGraphics(g, state);
    GdipDeletePen(ring);
    GdipDeletePen(faint);
    GdipDeletePen(tick);
}

// ---- весь слой ----

static const int pattern_order[] = {
    WUI_DECOR_CONTOURS, WUI_DECOR_FLOWEROFLIFE, WUI_DECOR_WAVES, WUI_DECOR_HEXAGONS, WUI_DECOR_CIRCUIT
};

void wui_backdrop_draw(GpGraphics *g, const WuiTheme *t, float w, float h, float scale)
{
    if (!t || w < 2 || h < 2) return;
    int decor = t->decor;
    UINT state;
    dip_px = scale > 0 ? scale : 1;
    // сглаживание и точки по центру пикселя, как у WPF; режимы вызывающего вернутся в конце
    GdipSaveGraphics(g, &state);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipSetTextRenderingHint(g, TextRenderingHintAntiAlias);
    GdipSetTextContrast(g, 1);                      // буквы такой же густоты, как у WPF
    GdipSetCompositingMode(g, CompositingModeSourceOver);

    if (decor & WUI_DECOR_ORBS) draw_orbs(g, t, w, h);
    if (decor & WUI_DECOR_POLARGRID) draw_polar_grid(g, t, w, h);
    if (decor & WUI_DECOR_GRID) draw_neon_grid(g, t, w, h);
    for (int i = 0; i < 5; i++)
        if (decor & pattern_order[i]) wui_pattern_draw(g, t, w, h, pattern_order[i], scale);
    if (decor & WUI_DECOR_STARS) draw_stars(g, t, w, h);
    if (decor & WUI_DECOR_VIGNETTE) {
        if (t->is_dark) draw_vignette(g, w, h, wui_darken(t->c[WUI_C_BACKGROUND1], 0.75), 0x40, 0x90);
        else draw_vignette(g, w, h, wui_lighten(t->c[WUI_C_TEXTDIM], 0.15), 0x2E, 0x70);
    }
    if (decor & WUI_DECOR_SCANLINES) draw_scanlines(g, w, h);
    GdipRestoreGraphics(g, state);

    wui_frame_draw(g, t, w, h);
}
