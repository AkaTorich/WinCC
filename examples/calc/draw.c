// Калькулятор: расстановка кнопок и всё рисование (GDI+, сглаживание, ClearType).
#include "calc.h"
#include <gdiplus.h>
#include <string.h>

const Button g_buttons[BUTTONS] = {
    { L"AC", K_CLEAR, 0, LOOK_FUNC },   { L"\x232B", K_BACK, 0, LOOK_FUNC }, { L"%", K_PERCENT, 0, LOOK_FUNC }, { L"\xF7", K_OP, '/', LOOK_OP },
    { L"7", K_DIGIT, 7, LOOK_DIGIT },   { L"8", K_DIGIT, 8, LOOK_DIGIT },    { L"9", K_DIGIT, 9, LOOK_DIGIT },    { L"\xD7", K_OP, '*', LOOK_OP },
    { L"4", K_DIGIT, 4, LOOK_DIGIT },   { L"5", K_DIGIT, 5, LOOK_DIGIT },    { L"6", K_DIGIT, 6, LOOK_DIGIT },    { L"\x2212", K_OP, '-', LOOK_OP },
    { L"1", K_DIGIT, 1, LOOK_DIGIT },   { L"2", K_DIGIT, 2, LOOK_DIGIT },    { L"3", K_DIGIT, 3, LOOK_DIGIT },    { L"+", K_OP, '+', LOOK_OP },
    { L"\xB1", K_SIGN, 0, LOOK_DIGIT }, { L"0", K_DIGIT, 0, LOOK_DIGIT },    { L",", K_COMMA, 0, LOOK_DIGIT },    { L"=", K_EQ, 0, LOOK_EQ },
};

// ---- цвета ----
#define C_BG_TOP     0xFF1E2129
#define C_BG_BOTTOM  0xFF131519
#define C_TEXT       0xFFF3F5F9
#define C_DIM        0xFF8B92A3
#define C_DIGIT      0xFF2C3039
#define C_FUNC       0xFF3B404D
#define C_OP         0xFF3A2F25
#define C_OP_TEXT    0xFFFFAE45
#define C_ACCENT_1   0xFFFFB54D
#define C_ACCENT_2   0xFFF0861F
#define C_PANEL      0xFF1C1F26
#define C_PANEL_LINE 0xFF2C3039
#define C_ITEM_HOT   0xFF272B34

static GpFontFamily *g_ui, *g_light, *g_symbol, *g_semibold;
static GpStringFormat *g_right, *g_center, *g_left;

static GpFontFamily *family(const WCHAR *name, GpFontFamily *fallback)
{
    GpFontFamily *f = 0;
    if (GdipCreateFontFamilyFromName(name, 0, &f) != 0) return fallback;
    return f;
}

static GpStringFormat *format(int align)
{
    GpStringFormat *f;
    GdipCreateStringFormat(StringFormatFlagsNoWrap, 0, &f);
    GdipSetStringFormatAlign(f, align);
    GdipSetStringFormatLineAlign(f, StringAlignmentCenter);
    return f;
}

void draw_start(void)
{
    g_ui = family(L"Segoe UI", 0);
    g_light = family(L"Segoe UI Light", g_ui);
    g_semibold = family(L"Segoe UI Semibold", g_ui);
    g_symbol = family(L"Segoe UI Symbol", g_ui);
    g_right = format(StringAlignmentFar);
    g_center = format(StringAlignmentCenter);
    g_left = format(StringAlignmentNear);
}

void draw_stop(void)
{
    GdipDeleteStringFormat(g_right);
    GdipDeleteStringFormat(g_center);
    GdipDeleteStringFormat(g_left);
}

// ---- расстановка ----

static Box box(float x, float y, float w, float h) { Box b = { x, y, w, h }; return b; }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }

void layout(View *v)
{
    float s = v->scale, W = (float)v->width, H = (float)v->height;
    float pad = 16 * s, gap = 9 * s;
    v->show_history = W >= 620 * s;
    float calcw = v->show_history ? W - minf(330 * s, W * 0.42f) : W;

    float disp_h = maxf(130 * s, (H - 2 * pad) * 0.29f);
    float top = pad + disp_h + 10 * s;
    float bw = (calcw - 2 * pad - 3 * gap) / 4;
    float bh = (H - pad - top - 4 * gap) / 5;
    for (int i = 0; i < BUTTONS; i++)
        v->btn[i] = box(pad + (i % 4) * (bw + gap), top + (i / 4) * (bh + gap), bw, bh);

    if (v->show_history) {
        v->hist_panel = box(calcw, pad, W - calcw - pad, H - 2 * pad);
        v->hist_clear = box(v->hist_panel.x + v->hist_panel.w - 104 * s, v->hist_panel.y + 12 * s, 90 * s, 30 * s);
    }
}

// ---- рисование ----

static ARGB mix(ARGB a, ARGB b, float t)
{
    if (t <= 0) return a;
    if (t >= 1) return b;
    ARGB r = 0;
    for (int sh = 0; sh < 32; sh += 8) {
        int ca = (int)((a >> sh) & 255), cb = (int)((b >> sh) & 255);
        r |= (ARGB)(ca + (int)((cb - ca) * t)) << sh;
    }
    return r;
}

static ARGB lighter(ARGB c, float t) { return mix(c, (c & 0xFF000000) | 0xFFFFFF, t); }
static ARGB darker(ARGB c, float t) { return mix(c, c & 0xFF000000, t); }

static void round_rect(GpPath *p, float x, float y, float w, float h, float r)
{
    float d = 2 * r;
    GdipResetPath(p);
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
}

static void fill_round(GpGraphics *g, GpPath *p, Box b, float r, ARGB top, ARGB bottom)
{
    RectF rf = { b.x, b.y - 1, b.w, b.h + 2 };
    GpLineGradient *br;
    GdipCreateLineBrushFromRect(&rf, top, bottom, LinearGradientModeVertical, WrapModeTileFlipXY, &br);
    round_rect(p, b.x, b.y, b.w, b.h, r);
    GdipFillPath(g, br, p);
    GdipDeleteBrush(br);
}

static void fill_solid(GpGraphics *g, GpPath *p, Box b, float r, ARGB c)
{
    GpSolidFill *br;
    GdipCreateSolidFill(c, &br);
    round_rect(p, b.x, b.y, b.w, b.h, r);
    GdipFillPath(g, br, p);
    GdipDeleteBrush(br);
}

static void text(GpGraphics *g, const WCHAR *s, GpFontFamily *fam, float size, int style, ARGB color,
                 Box b, GpStringFormat *fmt)
{
    GpFont *font;
    GpSolidFill *br;
    GdipCreateFont(fam, size, style, UnitPixel, &font);
    GdipCreateSolidFill(color, &br);
    RectF r = { b.x, b.y, b.w, b.h };
    GdipDrawString(g, s, -1, font, &r, fmt, br);
    GdipDeleteBrush(br);
    GdipDeleteFont(font);
}

static float text_width(GpGraphics *g, const WCHAR *s, GpFontFamily *fam, float size, int style)
{
    GpFont *font;
    GdipCreateFont(fam, size, style, UnitPixel, &font);
    RectF lay = { 0, 0, 100000, 10000 }, out = { 0, 0, 0, 0 };
    GdipMeasureString(g, s, -1, font, &lay, g_left, &out, 0, 0);
    GdipDeleteFont(font);
    return out.Width;
}

static void wide(const char *utf8, WCHAR *out, int cap)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap);
    if (n <= 0) out[0] = 0;
}

// длинная строка: видно её конец, начало уходит под «…»
static void fit_tail(GpGraphics *g, WCHAR *s, GpFontFamily *fam, float size, float maxw)
{
    if (text_width(g, s, fam, size, 0) <= maxw) return;
    int len = (int)wcslen(s);
    for (int cut = 1; cut < len; cut++) {
        WCHAR buf[512];
        buf[0] = 0x2026;
        wcscpy(buf + 1, s + cut);
        if (text_width(g, buf, fam, size, 0) <= maxw) {
            wcscpy(s, buf);
            return;
        }
    }
}

static void paint_display(View *v, GpGraphics *g)
{
    float s = v->scale, pad = 16 * s;
    float w = v->btn[3].x + v->btn[3].w - pad;
    float disp_h = v->btn[0].y - 10 * s - pad;
    WCHAR line[512], big[128];
    char buf[400];

    // строка выражения
    wide(calc_line(&v->calc), line, 512);
    float ls = 19 * s;
    fit_tail(g, line, g_ui, ls, w - 8 * s);
    text(g, line, g_ui, ls, 0, C_DIM, box(pad, pad + 4 * s, w - 4 * s, ls * 1.6f), g_right);

    // большое число: крупно, а длинное — мельче, чтобы влезло
    calc_display(&v->calc, buf, sizeof(buf));
    wide(buf, big, 128);
    float size = v->calc.error ? disp_h * 0.22f : disp_h * 0.56f;
    while (size > 14 * s && text_width(g, big, g_light, size, 0) > w - 6 * s)
        size *= 0.94f;
    float bh = size * 1.45f;
    text(g, big, g_light, size, 0, v->calc.error ? 0xFFFF7A6B : C_TEXT,
         box(pad, pad + disp_h - bh + size * 0.12f, w - 2 * s, bh), g_right);
}

static void paint_buttons(View *v, GpGraphics *g, GpPath *p)
{
    float s = v->scale;
    char active = calc_active_op(&v->calc);
    for (int i = 0; i < BUTTONS; i++) {
        const Button *b = &g_buttons[i];
        Box r = v->btn[i];
        float radius = minf(r.w, r.h) * 0.24f;
        float hv = v->hover[i], pr = v->press[i];
        int is_active = b->look == LOOK_OP && b->arg == active;

        ARGB face, label = C_TEXT;
        switch (b->look) {
        case LOOK_FUNC: face = C_FUNC; break;
        case LOOK_OP: face = C_OP; label = C_OP_TEXT; break;
        case LOOK_EQ: face = C_ACCENT_2; label = 0xFF1A1206; break;
        default: face = C_DIGIT;
        }
        if (is_active) { face = C_ACCENT_2; label = 0xFF1A1206; }

        // тень — чуть ниже кнопки, кнопка при нажатии в неё «опускается»
        float sink = pr * 2 * s;
        fill_solid(g, p, box(r.x, r.y + 3 * s, r.w, r.h), radius, 0x55000000);
        Box face_box = box(r.x, r.y + sink, r.w, r.h - sink * 0.5f);

        ARGB top, bottom;
        if (b->look == LOOK_EQ || is_active) {
            top = mix(C_ACCENT_1, 0xFFFFD08A, hv * 0.35f);
            bottom = mix(C_ACCENT_2, C_ACCENT_1, hv * 0.4f);
        } else {
            ARGB f = lighter(face, hv * 0.10f);
            top = lighter(f, 0.06f);
            bottom = darker(f, 0.06f);
        }
        top = darker(top, pr * 0.18f);
        bottom = darker(bottom, pr * 0.18f);
        fill_round(g, p, face_box, radius, top, bottom);
        // тонкий блик по верхнему краю
        fill_solid(g, p, box(face_box.x + radius * 0.6f, face_box.y + 1 * s, face_box.w - radius * 1.2f, 1.2f * s),
                   0.5f * s, b->look == LOOK_EQ || is_active ? 0x55FFFFFF : 0x22FFFFFF);

        const WCHAR *lab = b->label;
        if (b->key == K_CLEAR && !calc_clear_all(&v->calc)) lab = L"C";
        float fs = r.h * (b->look == LOOK_DIGIT ? 0.36f : 0.34f);
        GpFontFamily *fam = g_ui;
        if (b->key == K_BACK) { fam = g_symbol; fs = r.h * 0.32f; }
        else if (b->key == K_SIGN) { fam = g_ui; fs = r.h * 0.42f; }
        else if (b->look == LOOK_OP || b->look == LOOK_EQ) { fam = g_ui; fs = r.h * 0.42f; }
        else if (b->look == LOOK_FUNC) { fam = g_semibold; fs = r.h * 0.30f; }
        text(g, lab, fam, fs, 0, label, box(face_box.x, face_box.y - fs * 0.04f, face_box.w, face_box.h), g_center);
    }
}

static void paint_history(View *v, GpGraphics *g, GpPath *p)
{
    float s = v->scale;
    Box pn = v->hist_panel;
    fill_solid(g, p, pn, 16 * s, C_PANEL);

    text(g, L"Журнал", g_semibold, 17 * s, 0, C_TEXT, box(pn.x + 18 * s, pn.y + 12 * s, 200 * s, 30 * s), g_left);
    v->hist_shown = 0;
    int n = history_count();
    if (n > 0) {
        Box c = v->hist_clear;
        if (v->hist_clear_hot) fill_solid(g, p, c, 8 * s, C_ITEM_HOT);
        text(g, L"Очистить", g_ui, 14 * s, 0, v->hist_clear_hot ? C_OP_TEXT : C_DIM, c, g_center);
    }
    fill_solid(g, p, box(pn.x + 14 * s, pn.y + 52 * s, pn.w - 28 * s, 1 * s), 0, C_PANEL_LINE);

    if (n == 0) {
        text(g, L"Здесь появятся вычисления", g_ui, 15 * s, 0, C_DIM,
             box(pn.x, pn.y + 60 * s, pn.w, pn.h - 80 * s), g_center);
        return;
    }
    float item_h = 70 * s, y = pn.y + 60 * s;
    if (v->hist_scroll > n - 1) v->hist_scroll = n - 1;
    if (v->hist_scroll < 0) v->hist_scroll = 0;
    for (int i = v->hist_scroll; i < n && y + item_h <= pn.y + pn.h - 8 * s && v->hist_shown < 64; i++) {
        Box it = box(pn.x + 8 * s, y, pn.w - 16 * s, item_h);
        int k = v->hist_shown++;
        v->hist_item[k] = it;
        v->hist_index[k] = i;
        if (v->hist_hot == k) fill_solid(g, p, it, 10 * s, C_ITEM_HOT);

        WCHAR e[512], r[128];
        wide(history_expr(i), e, 512);
        wide(history_result(i), r, 128);
        fit_tail(g, e, g_ui, 14 * s, it.w - 24 * s);
        text(g, e, g_ui, 14 * s, 0, C_DIM, box(it.x + 12 * s, it.y + 8 * s, it.w - 24 * s, 22 * s), g_right);
        float rs = 24 * s;
        while (rs > 12 * s && text_width(g, r, g_semibold, rs, 0) > it.w - 24 * s) rs *= 0.92f;
        text(g, r, g_semibold, rs, 0, C_TEXT, box(it.x + 12 * s, it.y + 30 * s, it.w - 24 * s, 34 * s), g_right);
        y += item_h + 2 * s;
    }
}

void paint(View *v, HDC dc)
{
    // рисуется в память и выходит на экран одним куском: без мерцания
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, v->width, v->height);
    HGDIOBJ old = SelectObject(mem, bmp);

    GpGraphics *g;
    GdipCreateFromHDC(mem, &g);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipSetTextRenderingHint(g, TextRenderingHintClearTypeGridFit);

    Box all = box(0, 0, (float)v->width, (float)v->height);
    RectF rf = { 0, -1, all.w, all.h + 2 };
    GpLineGradient *bg;
    GdipCreateLineBrushFromRect(&rf, C_BG_TOP, C_BG_BOTTOM, LinearGradientModeVertical, WrapModeTileFlipXY, &bg);
    GdipFillRectangle(g, bg, 0, 0, all.w, all.h);
    GdipDeleteBrush(bg);

    GpPath *path;
    GdipCreatePath(FillModeAlternate, &path);
    paint_display(v, g);
    paint_buttons(v, g, path);
    if (v->show_history) paint_history(v, g, path);
    GdipDeletePath(path);
    GdipDeleteGraphics(g);

    BitBlt(dc, 0, 0, v->width, v->height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// ---- значок окна: тёмный корпус, оранжевое табло, кнопки ----

HICON make_icon(int size)
{
    GpBitmap *bmp;
    GpGraphics *g;
    if (GdipCreateBitmapFromScan0(size, size, 0, PixelFormat32bppARGB, 0, &bmp) != 0) return 0;
    GdipGetImageGraphicsContext(bmp, &g);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipGraphicsClear(g, 0);
    GpPath *p;
    GdipCreatePath(FillModeAlternate, &p);
    float f = (float)size;
    fill_round(g, p, box(f * 0.06f, f * 0.02f, f * 0.88f, f * 0.96f), f * 0.18f, 0xFF3A3F4C, 0xFF1E2128);
    fill_round(g, p, box(f * 0.17f, f * 0.12f, f * 0.66f, f * 0.24f), f * 0.06f, C_ACCENT_1, C_ACCENT_2);
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 3; c++) {
            ARGB col = r == 1 && c == 2 ? C_ACCENT_2 : 0xFF5A6070;
            fill_solid(g, p, box(f * (0.17f + c * 0.235f), f * (0.46f + r * 0.235f), f * 0.19f, f * 0.19f), f * 0.05f, col);
        }
    GdipDeletePath(p);
    GdipDeleteGraphics(g);
    HICON icon = 0;
    GdipCreateHICONFromBitmap(bmp, &icon);
    GdipDisposeImage(bmp);
    return icon;
}
