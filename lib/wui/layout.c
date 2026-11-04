// WUI — дерево элементов и раскладка: столбец, строка, перенос, прокрутка, карточка.
// Размер элемента: заданный (wui_size), по содержимому или доля остатка (wui_stretch).
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

#define BIG 1e6f

WuiControl *wui_new(WuiControl *parent, int type, const WuiClass *cls)
{
    WuiControl *c = calloc(1, sizeof(WuiControl));
    c->type = type;
    c->cls = cls;
    c->visible = c->enabled = 1;
    c->vmax = 1;
    if (parent) {
        if (parent->type == WUI_T_TABS && parent->ext) parent = *(WuiControl **)parent->ext;    // страницы вкладок
        c->parent = parent;
        c->win = parent->win;
        if (parent->last) parent->last->next = c;
        else parent->first = c;
        parent->last = c;
        if (c->win) wui_relayout(c);
    }
    return c;
}

// ---- замер и раскладка ----

static float hor(WuiEdges e) { return e.left + e.right; }
static float ver(WuiEdges e) { return e.top + e.bottom; }

void wui_measure(WuiControl *c, float aw, float ah)
{
    if (c->fix_w > 0) aw = c->fix_w;
    if (c->fix_h > 0) ah = c->fix_h;
    c->mw = c->mh = 0;
    if (c->cls && c->cls->measure) c->cls->measure(c, aw, ah);
    if (c->fix_w > 0) c->mw = c->fix_w;
    if (c->fix_h > 0) c->mh = c->fix_h;
    if (c->mw < c->min_w) c->mw = c->min_w;
    if (c->mh < c->min_h) c->mh = c->min_h;
}

void wui_arrange(WuiControl *c, WuiRect r)
{
    c->r = r;
    if (c->cls && c->cls->arrange) c->cls->arrange(c);
}

// поперёк: растянуть или поставить по краю/середине
static void cross(WuiControl *k, float from, float span, float *pos, float *size, int vertical_stack)
{
    float want = vertical_stack ? k->mw : k->mh;
    float m1 = vertical_stack ? k->margin.left : k->margin.top, m2 = vertical_stack ? k->margin.right : k->margin.bottom;
    float room = span - m1 - m2;
    int fixed = vertical_stack ? k->fix_w > 0 : k->fix_h > 0;
    int align = k->align;
    if (align == WUI_ALIGN_STRETCH && !fixed) want = room;
    if (want > room) want = room;
    if (want < 0) want = 0;
    float at = from + m1;
    if (align == WUI_ALIGN_CENTER) at += (room - want) / 2;
    else if (align == WUI_ALIGN_END) at += room - want;
    *pos = at;
    *size = want;
}

// стопка: по вертикали (столбец) или горизонтали (строка)
static void stack_measure(WuiControl *c, float aw, float ah, int vertical)
{
    float iw = aw - hor(c->pad), ih = ah - ver(c->pad);
    float main = 0, side = 0, weights = 0;
    int n = 0;
    float room = vertical ? ih : iw;
    for (WuiControl *k = c->first; k; k = k->next) {
        if (!k->visible) continue;
        if (n++) main += c->gap;
        if (k->weight > 0) { weights += k->weight; continue; }
        if (vertical) {
            wui_measure(k, iw - hor(k->margin), BIG);
            main += k->mh + ver(k->margin);
            if (k->mw + hor(k->margin) > side) side = k->mw + hor(k->margin);
        }
        else {
            wui_measure(k, room - main - hor(k->margin) > 0 ? room - main - hor(k->margin) : 0, ih - ver(k->margin));
            main += k->mw + hor(k->margin);
            if (k->mh + ver(k->margin) > side) side = k->mh + ver(k->margin);
        }
    }
    float left = room - main;
    for (WuiControl *k = c->first; k; k = k->next) {
        if (!k->visible || k->weight <= 0) continue;
        float share = left > 0 && room < BIG / 2 ? left * k->weight / weights : 0;
        if (vertical) {
            wui_measure(k, iw - hor(k->margin), share > 0 ? share - ver(k->margin) : BIG);
            main += k->mh + ver(k->margin);
            if (k->mw + hor(k->margin) > side) side = k->mw + hor(k->margin);
        }
        else {
            wui_measure(k, share > 0 ? share - hor(k->margin) : 0, ih - ver(k->margin));
            main += k->mw + hor(k->margin);
            if (k->mh + ver(k->margin) > side) side = k->mh + ver(k->margin);
        }
    }
    c->mw = (vertical ? side : main) + hor(c->pad);
    c->mh = (vertical ? main : side) + ver(c->pad);
}

static void stack_arrange(WuiControl *c, WuiRect in, int vertical)
{
    float fixed = 0, weights = 0;
    int n = 0;
    for (WuiControl *k = c->first; k; k = k->next) {
        if (!k->visible) continue;
        if (n++) fixed += c->gap;
        if (k->weight > 0) { weights += k->weight; fixed += vertical ? ver(k->margin) : hor(k->margin); }
        else fixed += vertical ? k->mh + ver(k->margin) : k->mw + hor(k->margin);
    }
    float left = (vertical ? in.h : in.w) - fixed;
    float at = vertical ? in.y : in.x;
    for (WuiControl *k = c->first; k; k = k->next) {
        if (!k->visible) continue;
        float len = vertical ? k->mh : k->mw;
        if (k->weight > 0) {
            float share = left > 0 ? left * k->weight / weights : 0;
            int fixed_len = vertical ? k->fix_h > 0 : k->fix_w > 0;
            if (!fixed_len) len = share;
        }
        WuiRect r;
        if (vertical) {
            r.y = at + k->margin.top;
            r.h = len;
            cross(k, in.x, in.w, &r.x, &r.w, 1);
            at = r.y + r.h + k->margin.bottom + c->gap;
        }
        else {
            r.x = at + k->margin.left;
            r.w = len;
            cross(k, in.y, in.h, &r.y, &r.h, 0);
            at = r.x + r.w + k->margin.right + c->gap;
        }
        wui_arrange(k, r);
    }
}

static void column_measure(WuiControl *c, float aw, float ah) { stack_measure(c, aw, ah, 1); }
static void column_arrange(WuiControl *c) { stack_arrange(c, wui_deflate(c->r, c->pad), 1); }
static void row_measure(WuiControl *c, float aw, float ah) { stack_measure(c, aw, ah, 0); }
static void row_arrange(WuiControl *c) { stack_arrange(c, wui_deflate(c->r, c->pad), 0); }

// перенос: слева направо, не влезло — новая строка
static float flow(WuiControl *c, WuiRect in, int place)
{
    float x = in.x, y = in.y, line = 0;
    int first = 1;
    for (WuiControl *k = c->first; k; k = k->next) {
        if (!k->visible) continue;
        float w = k->mw + hor(k->margin), h = k->mh + ver(k->margin);
        if (!first && x + c->gap + w > in.x + in.w + 0.01f) {
            x = in.x;
            y += line + c->gap;
            line = 0;
            first = 1;
        }
        if (!first) x += c->gap;
        if (place) {
            WuiRect r = { x + k->margin.left, y + k->margin.top, k->mw, k->mh };
            wui_arrange(k, r);
        }
        x += w;
        if (h > line) line = h;
        first = 0;
    }
    return y + line - in.y;
}

static void wrap_measure(WuiControl *c, float aw, float ah)
{
    (void)ah;
    float iw = aw - hor(c->pad), widest = 0;
    for (WuiControl *k = c->first; k; k = k->next) {
        if (!k->visible) continue;
        wui_measure(k, iw - hor(k->margin), BIG);
        if (k->mw + hor(k->margin) > widest) widest = k->mw + hor(k->margin);
    }
    WuiRect in = { 0, 0, iw < BIG / 2 ? iw : widest, BIG };
    c->mw = (iw < BIG / 2 ? iw : widest) + hor(c->pad);
    c->mh = flow(c, in, 0) + ver(c->pad);
}

static void wrap_arrange(WuiControl *c) { flow(c, wui_deflate(c->r, c->pad), 1); }

const WuiClass wui_class_column = { column_measure, column_arrange };
const WuiClass wui_class_row = { row_measure, row_arrange };
const WuiClass wui_class_wrap = { wrap_measure, wrap_arrange };

WuiControl *wui_column(WuiControl *parent) { return wui_new(parent, WUI_T_COLUMN, &wui_class_column); }
WuiControl *wui_row(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_ROW, &wui_class_row);
    c->gap = 10;
    return c;
}
WuiControl *wui_wrap(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_WRAP, &wui_class_wrap);
    c->gap = 12;
    return c;
}

// ---- прокрутка: столбец, который сдвигается; полоса справа ----

static float bar_size(void) { float s = WUI_S->Scroll_Size; return s > 0 ? s : 8; }

static void scroll_measure(WuiControl *c, float aw, float ah)
{
    column_measure(c, aw, BIG);
    if (ah < BIG / 2 && c->mh > ah) c->mh = ah;
}

static void scroll_arrange(WuiControl *c)
{
    WuiRect in = wui_deflate(c->r, c->pad);
    // по содержимому: не влезает — место под полосу
    stack_measure(c, in.w + hor(c->pad), BIG, 1);
    float content = c->mh - ver(c->pad);
    if (content > in.h + 0.5f) {
        in.w -= bar_size() + 4;
        stack_measure(c, in.w + hor(c->pad), BIG, 1);
        content = c->mh - ver(c->pad);
    }
    c->scroll_max = content - in.h > 0 ? content - in.h : 0;
    if (c->scroll > c->scroll_max) c->scroll = c->scroll_max;
    if (c->scroll < 0) c->scroll = 0;
    WuiRect at = { in.x, in.y - c->scroll, in.w, content > in.h ? content : in.h };
    stack_arrange(c, at, 1);
}

static WuiRect thumb_rect(WuiControl *c)
{
    WuiRect in = wui_deflate(c->r, c->pad);
    float view = in.h, total = view + c->scroll_max;
    float th = view * view / total;
    if (th < 24) th = 24;
    float y = in.y + (view - th) * (c->scroll_max > 0 ? c->scroll / c->scroll_max : 0);
    WuiRect r = { c->r.x + c->r.w - c->pad.right - bar_size(), y, bar_size(), th };
    return r;
}

static void scroll_paint_over(WuiControl *c, GpGraphics *g)
{
    if (c->scroll_max <= 0) return;
    WuiRect in = wui_deflate(c->r, c->pad);
    WuiRect track = { c->r.x + c->r.w - c->pad.right - bar_size(), in.y, bar_size(), in.h };
    wui_fill(g, WUI_S->Scroll_Track, track, wui_corners(4), 1);
    WuiRect t = thumb_rect(c);
    t = wui_inset(t, 1, 0, 1, 0);
    int b = c->pressed ? WUI_S->Scroll_ThumbDrag : c->hot ? WUI_S->Scroll_ThumbHover : WUI_S->Scroll_Thumb;
    wui_fill(g, b, t, wui_corners(4), 1);
}

void wui_scroll_by(WuiControl *c, float dy)
{
    float old = c->scroll;
    c->scroll += dy;
    if (c->scroll > c->scroll_max) c->scroll = c->scroll_max;
    if (c->scroll < 0) c->scroll = 0;
    if (c->scroll != old) {
        WuiRect in = wui_deflate(c->r, c->pad);
        WuiRect at = { in.x, in.y - c->scroll, in.w, in.h + c->scroll_max };
        if (c->scroll_max > 0) at.w = in.w - bar_size() - 4;
        stack_arrange(c, at, 1);
        wui_invalidate(c);
    }
}

static float g_drag_from, g_drag_scroll;

static int scroll_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    if (ev == WUI_MOUSE_WHEEL) {
        if (c->scroll_max <= 0) return 0;
        wui_scroll_by(c, -extra / 120.0f * 48);
        return 1;
    }
    WuiRect t = thumb_rect(c);
    int on_bar = c->scroll_max > 0 && x >= t.x - 2;
    if (ev == WUI_MOUSE_DOWN && on_bar) {
        if (y >= t.y && y < t.y + t.h) {
            c->pressed = 1;
            g_drag_from = y;
            g_drag_scroll = c->scroll;
            wui_window_capture(c->win, c);
        }
        else wui_scroll_by(c, y < t.y ? -(c->r.h - 40) : c->r.h - 40);
        wui_invalidate(c);
        return 1;
    }
    if (ev == WUI_MOUSE_MOVE) {
        if (c->pressed) {
            WuiRect in = wui_deflate(c->r, c->pad);
            float room = in.h - t.h;
            if (room > 0) wui_scroll_by(c, g_drag_scroll + (y - g_drag_from) * c->scroll_max / room - c->scroll);
            return 1;
        }
        int hot = on_bar && y >= t.y && y < t.y + t.h;
        if (hot != c->hot) { c->hot = hot; wui_invalidate(c); }
        return on_bar;
    }
    if (ev == WUI_MOUSE_UP && c->pressed) {
        c->pressed = 0;
        wui_window_capture(c->win, 0);
        wui_invalidate(c);
        return 1;
    }
    if (ev == WUI_MOUSE_LEAVE && c->hot) { c->hot = 0; wui_invalidate(c); }
    return on_bar;
}

static const WuiClass scroll_class = { scroll_measure, scroll_arrange, 0, scroll_paint_over, scroll_mouse };

WuiControl *wui_scroll(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_SCROLL, &scroll_class);
    c->weight = 1;
    return c;
}

// ---- карточка: панель темы с тенью, бликом, рамкой и заголовком ----

static WuiFont card_font(void)
{
    WuiFont f = wui_font(WUI_S->Card_TitleFont, WUI_S->Card_TitleFontSize, WUI_S->Card_TitleFontWeight);
    f.caps = WUI_S->Card_TitleCapitals;
    return f;
}

static float card_header(WuiControl *c, float width)
{
    if (!c->text || !*c->text) return 0;
    WuiFont f = card_font();
    float w, h;
    wui_text_size(&f, c->text, width, &w, &h);
    return h + 12;
}

static void card_measure(WuiControl *c, float aw, float ah)
{
    if (!c->own_pad) c->pad = WUI_S->Card_Padding;
    float head = card_header(c, aw - hor(c->pad));
    c->pad.top += head;
    column_measure(c, aw, ah);
    c->pad.top -= head;
}

static void card_arrange(WuiControl *c)
{
    if (!c->own_pad) c->pad = WUI_S->Card_Padding;
    float head = card_header(c, c->r.w - hor(c->pad));
    WuiRect in = wui_deflate(c->r, c->pad);
    in.y += head;
    in.h -= head;
    stack_arrange(c, in, 1);
}

static void card_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    WuiCorners k = s->Card_CornerRadius;
    wui_effect(g, s->Card_Shadow, s->Card_Background, c->r, k, 1);
    wui_fill(g, s->Card_Background, c->r, k, 1);
    WuiRect in = wui_inset(c->r, 1, 1, 1, 1);
    wui_fill(g, s->Card_Gloss, in, k, 1);
    WuiEdges top = { 0, 1, 0, 0 };
    wui_stroke(g, s->Card_Highlight, in, k, top, 1);
    wui_stroke(g, s->Card_Border, c->r, k, s->Card_BorderThickness, 1);
    if (c->text && *c->text) {
        WuiFont f = card_font();
        WuiRect t = wui_deflate(c->r, s->Card_Padding);
        float w, h;
        wui_text_size(&f, c->text, t.w, &w, &h);
        t.h = h;
        wui_draw_text(g, &f, c->text, t, wui_brush_color(s->Card_TitleForeground), 0, 1);
    }
}

static const WuiClass card_class = { card_measure, card_arrange, card_paint };

WuiControl *wui_card(WuiControl *parent, const char *title)
{
    WuiControl *c = wui_new(parent, WUI_T_CARD, &card_class);
    c->text = title ? wui_strdup(title) : 0;
    c->gap = 10;
    return c;
}

// ---- пустое место и черта ----

static const WuiClass spacer_class = { 0 };

WuiControl *wui_spacer(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_SPACER, &spacer_class);
    c->weight = 1;
    return c;
}

static void separator_measure(WuiControl *c, float aw, float ah) { (void)aw; (void)ah; c->mw = c->mh = 1; }
static void separator_paint(WuiControl *c, GpGraphics *g) { wui_fill(g, WUI_S->Separator, c->r, wui_corners(0), 1); }
static const WuiClass separator_class = { separator_measure, 0, separator_paint };

WuiControl *wui_separator(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_SEPARATOR, &separator_class);
    c->margin.top = c->margin.bottom = 4;
    return c;
}

// ---- обход: рисование и попадание мышью ----

int wui_is_shown(WuiControl *c)
{
    for (; c; c = c->parent)
        if (!c->visible) return 0;
    return 1;
}

static int is_clip(WuiControl *c) { return c->type == WUI_T_SCROLL || c->type == WUI_T_LIST; }

WuiRect wui_visible_rect(WuiControl *c)
{
    WuiRect r = c->r;
    for (WuiControl *p = c->parent; p; p = p->parent) {
        if (!is_clip(p)) continue;
        WuiRect v = wui_deflate(p->r, p->pad);
        float x1 = r.x > v.x ? r.x : v.x, y1 = r.y > v.y ? r.y : v.y;
        float x2 = r.x + r.w < v.x + v.w ? r.x + r.w : v.x + v.w, y2 = r.y + r.h < v.y + v.h ? r.y + r.h : v.y + v.h;
        r.x = x1; r.y = y1; r.w = x2 > x1 ? x2 - x1 : 0; r.h = y2 > y1 ? y2 - y1 : 0;
    }
    return r;
}


static void paint_children(WuiControl *c, GpGraphics *g, RECT *clip, float s);

// выключенный элемент — прозрачнее (WPF: Opacity 0.45): рисуется в отдельную картинку
static void paint_faded(WuiControl *c, GpGraphics *g, float s)
{
    float reach = c->cls ? c->cls->reach : 0;
    WuiRect r = wui_inset(c->r, -reach, -reach, -reach, -reach);
    int pw = (int)(r.w * s) + 2, ph = (int)(r.h * s) + 2;
    if (pw < 1 || ph < 1) return;
    GpBitmap *bmp;
    GdipCreateBitmapFromScan0(pw, ph, 0, PixelFormat32bppPARGB, NULL, &bmp);
    GpGraphics *bg;
    GdipGetImageGraphicsContext(bmp, &bg);
    GdipSetSmoothingMode(bg, SmoothingModeAntiAlias);
    GdipSetTextRenderingHint(bg, TextRenderingHintAntiAliasGridFit);
    GdipScaleWorldTransform(bg, s, s, MatrixOrderAppend);
    GdipTranslateWorldTransform(bg, -r.x, -r.y, MatrixOrderPrepend);
    RECT all = { -100000, -100000, 100000, 100000 };
    if (c->cls && c->cls->paint) c->cls->paint(c, bg);
    paint_children(c, bg, &all, s);
    if (c->cls && c->cls->paint_over) c->cls->paint_over(c, bg);
    GdipDeleteGraphics(bg);
    float m[25] = { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0.45f, 0, 0, 0, 0, 0, 1 };
    GpImageAttributes *ia;
    GdipCreateImageAttributes(&ia);
    GdipSetImageAttributesColorMatrix(ia, 1, TRUE, m, NULL, 0);
    GdipDrawImageRectRect(g, bmp, r.x, r.y, pw / s, ph / s, 0, 0, (REAL)pw, (REAL)ph, UnitPixel, ia, NULL, NULL);
    GdipDisposeImageAttributes(ia);
    GdipDisposeImage(bmp);
}

static void paint_one(WuiControl *c, GpGraphics *g, RECT *clip, float s)
{
    if (!c->visible || c->r.w <= 0 || c->r.h <= 0) return;
    float reach = c->cls ? c->cls->reach : 0;
    if ((c->r.x - reach) * s > clip->right || (c->r.y - reach) * s > clip->bottom
        || (c->r.x + c->r.w + reach) * s < clip->left || (c->r.y + c->r.h + reach) * s < clip->top)
        return;
    if (!c->enabled) { paint_faded(c, g, s); return; }
    if (c->cls && c->cls->paint) c->cls->paint(c, g);
    paint_children(c, g, clip, s);
    if (c->cls && c->cls->paint_over) c->cls->paint_over(c, g);
}

static void paint_children(WuiControl *c, GpGraphics *g, RECT *clip, float s)
{
    if (!c->first) return;
    UINT st = 0;
    if (is_clip(c)) {
        GdipSaveGraphics(g, &st);
        WuiRect v = wui_deflate(c->r, c->pad);
        GdipSetClipRect(g, v.x, v.y, v.w, v.h, CombineModeIntersect);
    }
    for (WuiControl *k = c->first; k; k = k->next)
        paint_one(k, g, clip, s);
    if (is_clip(c)) GdipRestoreGraphics(g, st);
}

void wui_paint_tree(WuiControl *c, GpGraphics *g, RECT *clip_px, float scale)
{
    paint_one(c, g, clip_px, scale);
}

WuiControl *wui_hit(WuiControl *c, float x, float y)
{
    if (!c->visible || x < c->r.x || y < c->r.y || x >= c->r.x + c->r.w || y >= c->r.y + c->r.h) return 0;
    WuiRect v = is_clip(c) ? wui_deflate(c->r, c->pad) : c->r;
    if (x >= v.x && y >= v.y && x < v.x + v.w && y < v.y + v.h) {
        // сверху — последний ребёнок
        WuiControl *kids[256];
        int n = 0;
        for (WuiControl *k = c->first; k && n < 256; k = k->next) kids[n++] = k;
        for (int i = n - 1; i >= 0; i--) {
            WuiControl *h = wui_hit(kids[i], x, y);
            if (h) return h;
        }
    }
    return c;
}

// ---- свойства ----

void wui_size(WuiControl *c, float w, float h) { c->fix_w = w; c->fix_h = h; wui_relayout(c); }
void wui_min_size(WuiControl *c, float w, float h) { c->min_w = w; c->min_h = h; wui_relayout(c); }
void wui_stretch(WuiControl *c, float weight) { c->weight = weight; wui_relayout(c); }
void wui_gap(WuiControl *c, float gap) { c->gap = gap; wui_relayout(c); }
void wui_align(WuiControl *c, int align) { c->align = align; wui_relayout(c); }

void wui_margin(WuiControl *c, float l, float t, float r, float b)
{
    WuiEdges e = { l, t, r, b };
    c->margin = e;
    wui_relayout(c);
}

void wui_padding(WuiControl *c, float l, float t, float r, float b)
{
    WuiEdges e = { l, t, r, b };
    c->pad = e;
    c->own_pad = 1;
    wui_relayout(c);
}

void wui_show(WuiControl *c, int on) { if (c->visible != !!on) { c->visible = !!on; wui_relayout(c); } }
void wui_enable(WuiControl *c, int on) { if (c->enabled != !!on) { c->enabled = !!on; wui_invalidate(c); } }
void wui_on_click(WuiControl *c, WuiHandler fn, void *user) { c->on_click = fn; c->click_user = user; }
void wui_on_change(WuiControl *c, WuiHandler fn, void *user) { c->on_change = fn; c->change_user = user; }
void wui_set_data(WuiControl *c, void *data) { c->data = data; }
void *wui_get_data(WuiControl *c) { return c->data; }
void wui_redraw(WuiControl *c) { wui_invalidate(c); }

void wui_fire_click(WuiControl *c) { if (c->on_click) c->on_click(c, c->click_user); }
void wui_fire_change(WuiControl *c) { if (c->on_change) c->on_change(c, c->change_user); }

WuiControl *wui_window_of(WuiControl *c)
{
    return c && c->win ? c->win->root : 0;
}
