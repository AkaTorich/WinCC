// WUI — текст: шрифты темы (первый из списка, что есть в Windows; полужирный и тонкий — своими
// семействами Segoe UI Semibold/Light), замер и вывод, перенос по словам, свечение крупных чисел.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>


WuiFont wui_font(const char *family, float size, int weight)
{
    WuiFont f = { family ? family : wui_t->font, size > 0 ? size : wui_t->font_size, weight ? weight : 400, 0, 0 };
    return f;
}

// ---- шрифты: кэш по семейству, размеру и начертанию ----

typedef struct {
    const char *family;     // строка темы (указатель живёт всю программу)
    float size;
    int weight, italic;
    GpFont *font;
} Font;

#define FONTS 48
static Font g_fonts[FONTS];
static int g_font_next;

static GpFontFamily *family_named(const wchar_t *name)
{
    GpFontFamily *fam = 0;
    if (GdipCreateFontFamilyFromName(name, NULL, &fam) != Ok) return 0;
    return fam;
}

// "Segoe UI Light, Segoe UI, Segoe UI Symbol" → первое семейство, что есть; вес — именем начертания
static GpFontFamily *pick_family(const char *list, int weight, int *style)
{
    wchar_t *all = wui_wide(list);
    GpFontFamily *fam = 0;
    *style = 0;
    for (wchar_t *p = all; *p && !fam;) {
        while (*p == ' ' || *p == ',') p++;
        wchar_t *e = p;
        while (*e && *e != ',') e++;
        wchar_t *end = e;
        while (end > p && end[-1] == ' ') end--;
        wchar_t name[96];
        int n = (int)(end - p);
        if (n > 80) n = 80;
        wcsncpy(name, p, (size_t)n);
        name[n] = 0;
        p = *e ? e + 1 : e;
        if (!n) continue;
        wchar_t with[112];
        if (weight >= 550 && weight < 650) {
            swprintf(with, 112, L"%ls Semibold", name);
            if ((fam = family_named(with))) break;
        }
        if (weight >= 330 && weight < 380) {
            swprintf(with, 112, L"%ls Semilight", name);
            if ((fam = family_named(with))) break;
        }
        if (weight < 330) {
            swprintf(with, 112, L"%ls Light", name);
            if ((fam = family_named(with))) break;
        }
        fam = family_named(name);
        if (fam && weight >= 550) *style = 1;       // FontStyleBold
    }
    free(all);
    if (!fam) GdipGetGenericFontFamilySansSerif(&fam);
    return fam;
}

static GpFont *gp_font(const WuiFont *f)
{
    float size = f->caps ? f->size * 0.8f : f->size;
    for (int i = 0; i < FONTS; i++)
        if (g_fonts[i].font && g_fonts[i].family == f->family && g_fonts[i].size == size
            && g_fonts[i].weight == f->weight && g_fonts[i].italic == f->italic)
            return g_fonts[i].font;
    Font *slot = &g_fonts[g_font_next];
    g_font_next = (g_font_next + 1) % FONTS;
    if (slot->font) GdipDeleteFont(slot->font);
    int style;
    GpFontFamily *fam = pick_family(f->family, f->weight, &style);
    if (f->italic) style |= 2;                      // FontStyleItalic
    slot->family = f->family;
    slot->size = size;
    slot->weight = f->weight;
    slot->italic = f->italic;
    GdipCreateFont(fam, size, style, UnitPixel, &slot->font);
    GdipDeleteFontFamily(fam);
    return slot->font;
}

// ---- оформление строки ----

static GpStringFormat *format(int align, int wrap, int middle)
{
    static GpStringFormat *cache[3][2][2];
    GpStringFormat **slot = &cache[align][wrap ? 1 : 0][middle ? 1 : 0];
    if (*slot) return *slot;
    GpStringFormat *base;
    GdipStringFormatGetGenericTypographic(&base);
    GdipCloneStringFormat(base, slot);
    int flags = StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoClip;
    if (!wrap) flags |= StringFormatFlagsNoWrap;
    GdipSetStringFormatFlags(*slot, flags);
    GdipSetStringFormatAlign(*slot, align == 1 ? StringAlignmentCenter : align == 2 ? StringAlignmentFar : StringAlignmentNear);
    GdipSetStringFormatLineAlign(*slot, middle ? StringAlignmentCenter : StringAlignmentNear);
    GdipSetStringFormatTrimming(*slot, wrap ? StringTrimmingWord : StringTrimmingEllipsisCharacter);
    return *slot;
}

// замер — на картинке 1×1 с тем же масштабом и сглаживанием, что и окно: ширины совпадают с выводом
static GpGraphics *measurer(void)
{
    static GpBitmap *bmp;
    static GpGraphics *g;
    static float scale;
    if (!g) {
        GdipCreateBitmapFromScan0(1, 1, 0, PixelFormat32bppPARGB, NULL, &bmp);
        GdipGetImageGraphicsContext(bmp, &g);
        GdipSetTextRenderingHint(g, TextRenderingHintClearTypeGridFit);
    }
    if (scale != wui_paint_scale) {
        scale = wui_paint_scale;
        GdipResetWorldTransform(g);
        GdipScaleWorldTransform(g, scale, scale, MatrixOrderAppend);
    }
    return g;
}

static wchar_t *prepare(const WuiFont *f, const char *text)
{
    wchar_t *w = wui_wide(text);
    if (f->caps) CharUpperW(w);
    return w;
}

void wui_text_size(const WuiFont *f, const char *text, float max_w, float *w, float *h)
{
    wchar_t *s = prepare(f, text);
    RectF lay = { 0, 0, max_w > 0 ? max_w : 100000, 100000 }, box = { 0 };
    GdipMeasureString(measurer(), s, -1, gp_font(f), &lay, format(0, max_w > 0, 0), &box, NULL, NULL);
    if (!*s) box.Height = wui_line_height(f);
    *w = box.Width;
    *h = box.Height;
    free(s);
}

float wui_line_height(const WuiFont *f)
{
    REAL h = 0;
    GdipGetFontHeight(gp_font(f), measurer(), &h);
    return h;
}

void wui_draw_text(GpGraphics *g, const WuiFont *f, const char *text, WuiRect r, WuiColor color, int align, int wrap)
{
    if (!text || !*text || !WUI_A(color)) return;
    wchar_t *s = prepare(f, text);
    GpBrush *b = wui_solid(color);
    RectF lay = { r.x, r.y, r.w, r.h };
    GdipDrawString(g, s, -1, gp_font(f), &lay, format(align, wrap, !wrap), b);
    GdipDeleteBrush(b);
    free(s);
}

void wui_draw_wtext(GpGraphics *g, const WuiFont *f, const wchar_t *s, int n, float x, float y, WuiColor color)
{
    if (n <= 0) return;
    GpBrush *b = wui_solid(color);
    RectF lay = { x, y, 100000, wui_line_height(f) * 2 };
    GdipDrawString(g, s, n, gp_font(f), &lay, format(0, 0, 0), b);
    GdipDeleteBrush(b);
}

float wui_wtext_width(const WuiFont *f, const wchar_t *s, int n)
{
    if (n <= 0) return 0;
    RectF lay = { 0, 0, 100000, 1000 }, box = { 0 };
    GdipMeasureString(measurer(), s, n, gp_font(f), &lay, format(0, 0, 0), &box, NULL, NULL);
    return box.Width;
}

// ---- свечение текста (Theme.Big.Effect): буквы в отдельную картинку, её альфа размывается ----

typedef struct {
    char *text;
    const char *family;
    float size, w, h, scale;
    int effect, align;
    GpBitmap *bmp;
    int pad;
} Glow;

#define GLOWS 32
static Glow g_glows[GLOWS];
static int g_glow_next;

void wui_text_glow(GpGraphics *g, const WuiFont *f, const char *text, WuiRect r, int effect, int align)
{
    if (!effect || !text || !*text) return;
    float s = wui_paint_scale;
    Glow *c = 0;
    for (int i = 0; i < GLOWS; i++) {
        Glow *x = &g_glows[i];
        if (x->bmp && x->family == f->family && x->size == f->size && x->effect == effect && x->scale == s
            && x->w == r.w && x->h == r.h && x->align == align && strcmp(x->text, text) == 0) { c = x; break; }
    }
    if (!c) {
        c = &g_glows[g_glow_next];
        g_glow_next = (g_glow_next + 1) % GLOWS;
        if (c->bmp) { GdipDisposeImage(c->bmp); free(c->text); }
        c->text = wui_strdup(text);
        c->family = f->family; c->size = f->size; c->effect = effect; c->scale = s;
        c->w = r.w; c->h = r.h; c->align = align;
        c->pad = wui_effect_pad(effect);
        int pw = (int)(r.w * s) + 2 * c->pad, ph = (int)(r.h * s) + 2 * c->pad;
        GpBitmap *shape;
        GdipCreateBitmapFromScan0(pw, ph, 0, PixelFormat32bppPARGB, NULL, &shape);
        GpGraphics *sg;
        GdipGetImageGraphicsContext(shape, &sg);
        GdipSetTextRenderingHint(sg, TextRenderingHintAntiAlias);
        GdipScaleWorldTransform(sg, s, s, MatrixOrderAppend);
        WuiRect at = { c->pad / s, c->pad / s, r.w, r.h };
        wui_draw_text(sg, f, text, at, 0xFFFFFFFF, align, 0);
        GdipDeleteGraphics(sg);
        c->bmp = wui_effect_bitmap(shape, effect);
        GdipDisposeImage(shape);
    }
    const WuiEffectDef *e = &wui_effects[effect];
    float rad = e->direction * 3.14159265f / 180;
    UINT w, h;
    GdipGetImageWidth(c->bmp, &w);
    GdipGetImageHeight(c->bmp, &h);
    GdipDrawImageRect(g, c->bmp, r.x + e->depth * cosf(rad) - c->pad / s, r.y - e->depth * sinf(rad) - c->pad / s,
                      w / s, h / s);
}
