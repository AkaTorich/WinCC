// WUI — окно темы (ThemedWindow + WindowFrame ThemeLibrary): прозрачное по точкам (layered),
// скруглённые углы и тень, свой заголовок с кнопками, перетаскивание и растяжка за края,
// разворот в рабочую область своего монитора, масштаб экрана, клавиатура, мышь, курсор ввода.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

#define WM_WUI_RENDER (WM_APP + 1)
#define WM_WUI_SHOW (WM_APP + 2)
#define TIMER_CARET 1
#define TIMER_ANIMATE 2
#define TIMER_TOAST 3

WuiWin *wui_windows;
int wui_caret_on = 1;
static int g_cursor = 32512;                // IDC_ARROW
static int g_exit;

static WuiWin *win_of(HWND h) { return (WuiWin *)GetWindowLongPtrW(h, GWLP_USERDATA); }

static float outer(WuiWin *w) { return w->maximized ? 0 : WUI_S->Window_OuterMargin.left; }

// ---- раскладка окна: заголовок сверху, под ним содержимое ----

static void root_measure(WuiControl *c, float aw, float ah) { c->mw = aw; c->mh = ah; }

// сколько нужно строке заголовка: всё, что не растягивается
static float titlebar_need(WuiWin *w)
{
    float need = w->titlebar->pad.left + w->titlebar->pad.right;
    for (WuiControl *k = w->titlebar->first; k; k = k->next)
        if (k->visible && k->weight <= 0) need += k->mw + k->margin.left + k->margin.right;
    return need;
}

static void root_arrange(WuiControl *c)
{
    WuiWin *w = c->win;
    const WuiStyle *s = WUI_S;
    float m = outer(w);
    WuiRect r = { m, m, c->r.w - 2 * m, c->r.h - 2 * m };
    WuiEdges tm = s->TitleBar_Margin;
    WuiRect tb = { r.x + tm.left, r.y + tm.top, r.w - tm.left - tm.right, s->TitleBar_Height };
    w->logo->visible = w->logo->icon && !w->extend;
    w->title->visible = !w->extend;
    wui_measure(w->titlebar, tb.w, tb.h);
    // узкому окну строка заголовка не помещается: первым уходит текст заголовка; уже остального
    // (значок, выбор темы, кнопки окна) окно не сжимается — WM_GETMINMAXINFO
    float need = titlebar_need(w);
    if (need > tb.w && w->title->visible) {
        w->title->visible = 0;
        wui_measure(w->titlebar, tb.w, tb.h);
        need = titlebar_need(w);
    }
    w->title_min = need + tm.left + tm.right;
    if (w->extend && need < tb.w) {         // содержимое до верха: заголовок прижат вправо, без плашки
        tb.x += tb.w - need;
        tb.w = need;
    }
    wui_arrange(w->titlebar, tb);
    WuiRect in;
    if (w->extend) in = wui_deflate(r, s->Window_ExtendedContentMargin);
    else {
        float top = tm.top + s->TitleBar_Height + tm.bottom;
        in = wui_deflate(wui_inset(r, 0, top, 0, 0), s->Window_ContentMargin);
    }
    WuiRect cr = wui_deflate(in, w->content->margin);
    wui_measure(w->content, cr.w, cr.h);
    wui_arrange(w->content, cr);
}

static const WuiClass root_class = { root_measure, root_arrange };

static void titlebar_paint(WuiControl *c, GpGraphics *g)
{
    if (c->win->extend) return;             // прозрачный, как в WPF при ExtendContentIntoTitleBar
    const WuiStyle *s = WUI_S;
    WuiCorners k = s->TitleBar_CornerRadius;
    wui_fill(g, s->TitleBar_Background, c->r, k, 1);
    wui_fill(g, s->TitleBar_Gloss, c->r, k, 1);
    wui_stroke(g, s->TitleBar_Border, c->r, k, s->TitleBar_BorderThickness, 1);
}

static const WuiClass titlebar_class = { 0, 0, titlebar_paint };   // раскладка — как у строки

// название окна на табличке + подзаголовок
static WuiFont title_font(void)
{
    WuiFont f = wui_font(WUI_S->Title_Font, WUI_S->Title_FontSize, WUI_S->Title_FontWeight);
    f.caps = WUI_S->Title_Capitals;
    return f;
}

static WuiFont subtitle_font(void)
{
    WuiFont f = wui_font(WUI_S->Font, WUI_S->FontSize_Small, 400);
    f.italic = WUI_S->Subtitle_FontStyle;
    return f;
}

static void title_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    WuiFont f = title_font();
    float w, h, sw = 0, sh = 0;
    wui_text_size(&f, c->text, 0, &w, &h);
    if (c->win->subtitle && *c->win->subtitle) {
        WuiFont sf = subtitle_font();
        wui_text_size(&sf, c->win->subtitle, 0, &sw, &sh);
        sw += 14;
    }
    WuiEdges p = WUI_S->TitleText_Padding, b = WUI_S->TitleText_BorderThickness;
    c->mw = w + sw + p.left + p.right + b.left + b.right + 1;
    c->mh = h + p.top + p.bottom + b.top + b.bottom;
}

static void title_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    WuiCorners k = wui_corners(4);
    wui_fill(g, s->TitleText_Background, c->r, k, 1);
    wui_stroke(g, s->TitleText_Border, c->r, k, s->TitleText_BorderThickness, 1);
    WuiRect in = wui_deflate(wui_deflate(c->r, s->TitleText_BorderThickness), s->TitleText_Padding);
    WuiFont f = title_font();
    float w, h;
    wui_text_size(&f, c->text, 0, &w, &h);
    wui_draw_text(g, &f, c->text, in, wui_brush_color(s->Title_Foreground), 0, 0);
    if (c->win->subtitle && *c->win->subtitle) {
        WuiFont sf = subtitle_font();
        WuiRect sr = { in.x + w + 14, in.y, in.w - w - 14, in.h };
        wui_draw_text(g, &sf, c->win->subtitle, sr, wui_brush_color(s->TextDim), 0, 0);
    }
}

static const WuiClass title_class = { title_measure, 0, title_paint };

// ---- перерисовка ----

static void post_render(WuiWin *w)
{
    if (w->hwnd) PostMessageW(w->hwnd, WM_WUI_RENDER, 0, 0);
}

void wui_invalidate_rect(WuiWin *w, WuiRect r)
{
    if (!w || !w->hwnd) return;
    float s = w->scale;
    RECT px = { (LONG)floorf(r.x * s) - 1, (LONG)floorf(r.y * s) - 1, (LONG)ceilf((r.x + r.w) * s) + 1,
                (LONG)ceilf((r.y + r.h) * s) + 1 };
    int was_empty = IsRectEmpty(&w->dirty);
    if (was_empty) w->dirty = px;
    else UnionRect(&w->dirty, &w->dirty, &px);
    if (was_empty) post_render(w);
}

void wui_invalidate(WuiControl *c)
{
    if (!c || !c->win || !wui_is_shown(c)) return;
    float reach = c->cls ? c->cls->reach : 0;
    WuiRect r = wui_visible_rect(c);
    r = wui_inset(r, -reach, -reach, -reach, -reach);
    wui_invalidate_rect(c->win, r);
}

void wui_relayout(WuiControl *c)
{
    WuiWin *w = c ? c->win : 0;
    if (!w) return;
    w->layout_valid = 0;
    SetRect(&w->dirty, 0, 0, w->pw, w->ph);
    post_render(w);
}

static void layout(WuiWin *w)
{
    float s = w->scale;
    WuiRect all = { 0, 0, w->pw / s, w->ph / s };
    wui_measure(w->root, all.w, all.h);
    wui_arrange(w->root, all);
    w->layout_valid = 1;
}

static GpBitmap *texture_bitmap(void)
{
    static GpBitmap *bmp;
    static const WuiTheme *for_theme;
    static int for_k;
    int k = wui_paint_scale >= 1.5f ? 2 : 1;
    if (bmp && for_theme == wui_t && for_k == k) return bmp;
    if (bmp) GdipDisposeImage(bmp);
    bmp = 0;
    for_theme = wui_t;
    for_k = k;
    const unsigned *px = wui_texture(wui_t->texture, wui_t->c[WUI_C_PAPER], k);
    if (!px) return 0;
    int n = 512 * k;
    GpBitmap *view;
    GdipCreateBitmapFromScan0(n, n, n * 4, PixelFormat32bppARGB, (BYTE *)px, &view);
    GdipCloneImage(view, (GpImage **)&bmp);     // своя копия точек
    GdipDisposeImage(view);
    return bmp;
}


// фон окна без элементов: тень, градиент или фактура, слой Backdrop (пятна, звёзды, узоры, рамка)
static void build_back(WuiWin *w)
{
    float s = w->scale;
    const WuiStyle *st = WUI_S;
    if (w->back) GdipDisposeImage(w->back);
    GdipCreateBitmapFromScan0(w->pw, w->ph, 0, PixelFormat32bppPARGB, NULL, &w->back);
    GpGraphics *g;
    GdipGetImageGraphicsContext(w->back, &g);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipScaleWorldTransform(g, s, s, MatrixOrderAppend);
    float m = outer(w);
    WuiRect r = { m, m, w->pw / s - 2 * m, w->ph / s - 2 * m };
    WuiCorners k = w->maximized ? wui_corners(0) : st->Window_CornerRadius;
    if (!w->maximized) wui_effect(g, st->Window_Shadow, st->Window_ShadowFill, r, k, 1);

    // внутренность — отдельной картинкой, потом ею заливается скруглённая фигура (гладкие углы)
    int iw = (int)(r.w * s + 0.5f), ih = (int)(r.h * s + 0.5f);
    GpBitmap *inner;
    GdipCreateBitmapFromScan0(iw, ih, 0, PixelFormat32bppPARGB, NULL, &inner);
    GpGraphics *ig;
    GdipGetImageGraphicsContext(inner, &ig);
    GdipSetSmoothingMode(ig, SmoothingModeAntiAlias);
    GdipScaleWorldTransform(ig, s, s, MatrixOrderAppend);
    WuiRect ir = { 0, 0, r.w, r.h };
    GpBitmap *tex = wui_brushes[st->Window_Background].kind == WUI_BRUSH_TEXTURE ? texture_bitmap() : 0;
    if (tex) {
        GpTexture *tb;
        GdipCreateTexture(tex, WrapModeTile, &tb);
        int k2 = s >= 1.5f ? 2 : 1;
        GpMatrix *mx;                       // плитка — 512 DIP при любом масштабе
        GdipCreateMatrix2(1.0f / k2, 0, 0, 1.0f / k2, 0, 0, &mx);
        GdipSetTextureTransform(tb, mx);
        GdipDeleteMatrix(mx);
        GdipFillRectangle(ig, tb, 0, 0, r.w + 1, r.h + 1);
        GdipDeleteBrush(tb);
    }
    else wui_fill(ig, st->Window_Background, wui_inset(ir, -1, -1, -1, -1), wui_corners(0), 1);
    wui_backdrop_draw(ig, wui_t, r.w, r.h, s);
    GdipDeleteGraphics(ig);

    GpTexture *fill;
    GdipCreateTexture(inner, WrapModeClamp, &fill);
    GpMatrix *mx;
    GdipCreateMatrix2(1 / s, 0, 0, 1 / s, r.x, r.y, &mx);
    GdipSetTextureTransform(fill, mx);
    GdipDeleteMatrix(mx);
    GpPath *p = wui_round_path(r, k);
    GdipFillPath(g, fill, p);
    GdipDeletePath(p);
    GdipDeleteBrush(fill);
    GdipDisposeImage(inner);
    GdipDeleteGraphics(g);
    w->back_valid = 1;
}

static void ensure_surface(WuiWin *w)
{
    if (w->dib) {
        BITMAP bm;
        GetObjectW(w->dib, sizeof bm, &bm);
        if (bm.bmWidth == w->pw && bm.bmHeight == w->ph) return;
        GdipDeleteGraphics(w->g);
        GdipDisposeImage(w->surface);
        SelectObject(w->dc, w->old_bmp);
        DeleteObject(w->dib);
    }
    else w->dc = CreateCompatibleDC(0);
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), w->pw, -w->ph, 1, 32 } };
    w->dib = CreateDIBSection(0, &bi, DIB_RGB_COLORS, &w->bits, 0, 0);
    w->old_bmp = SelectObject(w->dc, w->dib);
    GdipCreateBitmapFromScan0(w->pw, w->ph, w->pw * 4, PixelFormat32bppPARGB, w->bits, &w->surface);
    GdipGetImageGraphicsContext(w->surface, &w->g);
    w->back_valid = 0;
}

static void draw_toast(WuiWin *w, GpGraphics *g)
{
    if (!w->toast) return;
    const WuiStyle *s = WUI_S;
    WuiFont f = wui_font(s->Font, s->FontSize, 400);
    float tw, th;
    wui_text_size(&f, w->toast, 0, &tw, &th);
    float W = w->pw / w->scale, H = w->ph / w->scale, m = outer(w);
    WuiRect r = { (W - tw - 36) / 2, H - m - 34 - th - 20, tw + 36, th + 20 };
    wui_fill(g, s->Popup_Background, r, s->Popup_CornerRadius, 1);
    WuiEdges one = { 1, 1, 1, 1 };
    wui_stroke(g, s->Popup_Border, r, s->Popup_CornerRadius, one, 1);
    wui_draw_text(g, &f, w->toast, r, wui_brush_color(s->Text), 1, 0);
}

static WuiRect toast_rect(WuiWin *w)
{
    float W = w->pw / w->scale, H = w->ph / w->scale;
    WuiRect r = { 0, H - 160, W, 160 };
    return r;
}

// Проверка без глаз: WUI_SNAPSHOT=файл.png — первое окно после отрисовки сохраняется картинкой,
// программа закрывается. WUI_THEME=Id — тема вместо запомненной, WUI_SIZE=ширина,высота (DIP), WUI_SCALE=2.25 —
// масштаб экрана.
// WUI_SNAPSHOT_WAIT=1 — окно показывается и живёт; снимок «файл-N.png» — по сообщению WM_APP+3 (N в wParam).
#define WM_WUI_SNAP (WM_APP + 3)

static int snap_wait(void) { return GetEnvironmentVariableW(L"WUI_SNAPSHOT_WAIT", 0, 0) > 0; }

static void snap_save(WuiWin *w, int n)
{
    wchar_t path[MAX_PATH], name[MAX_PATH + 16];
    if (!GetEnvironmentVariableW(L"WUI_SNAPSHOT", path, MAX_PATH)) return;
    int len = (int)wcslen(path);
    if (len > 4) path[len - 4] = 0;                 // без «.png»
    swprintf(name, MAX_PATH + 16, L"%ls-%d.png", path, n);
    GdipSaveImageToFile(w->surface, name, &GdipEncoderPng, NULL);
}

static void snapshot(WuiWin *w)
{
    static int done;
    wchar_t path[MAX_PATH];
    if (done || w->next || snap_wait() || !GetEnvironmentVariableW(L"WUI_SNAPSHOT", path, MAX_PATH)) return;
    done = 1;
    GdipSaveImageToFile(w->surface, path, &GdipEncoderPng, NULL);
    wui_quit(0);
}

static void render(WuiWin *w)
{
    if (!w->hwnd || !w->shown || IsIconic(w->hwnd)) return;     // до показа программа ещё кладёт элементы
    wui_paint_scale = w->scale;
    ensure_surface(w);
    if (!w->layout_valid) {
        layout(w);
        SetRect(&w->dirty, 0, 0, w->pw, w->ph);
    }
    if (!w->back_valid) {
        build_back(w);
        SetRect(&w->dirty, 0, 0, w->pw, w->ph);
    }
    RECT d = w->dirty, all = { 0, 0, w->pw, w->ph };
    SetRectEmpty(&w->dirty);
    IntersectRect(&d, &d, &all);
    if (IsRectEmpty(&d)) return;

    GpGraphics *g = w->g;
    float s = w->scale;
    GdipResetWorldTransform(g);
    GdipResetClip(g);
    GdipSetClipRect(g, (REAL)d.left, (REAL)d.top, (REAL)(d.right - d.left), (REAL)(d.bottom - d.top), CombineModeReplace);
    GdipSetCompositingMode(g, CompositingModeSourceCopy);
    GdipDrawImageRectRect(g, w->back, (REAL)d.left, (REAL)d.top, (REAL)(d.right - d.left), (REAL)(d.bottom - d.top),
                          (REAL)d.left, (REAL)d.top, (REAL)(d.right - d.left), (REAL)(d.bottom - d.top), UnitPixel, NULL, NULL, NULL);
    GdipSetCompositingMode(g, CompositingModeSourceOver);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetTextRenderingHint(g, TextRenderingHintClearTypeGridFit);
    GdipSetPixelOffsetMode(g, PixelOffsetModeHalf);
    GdipScaleWorldTransform(g, s, s, MatrixOrderAppend);
    wui_paint_tree(w->content, g, &d, s);
    wui_paint_tree(w->titlebar, g, &d, s);
    draw_toast(w, g);
    // кромка окна — поверх всего
    float m = outer(w);
    WuiRect r = { m, m, w->pw / s - 2 * m, w->ph / s - 2 * m };
    wui_stroke(g, WUI_S->Window_Edge, r, w->maximized ? wui_corners(0) : WUI_S->Window_CornerRadius,
               WUI_S->Window_EdgeThickness, 1);
    GdipFlush(g, 1);

    RECT wr;
    GetWindowRect(w->hwnd, &wr);
    POINT src = { 0, 0 }, at = { wr.left, wr.top };
    SIZE size = { w->pw, w->ph };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(w->hwnd, 0, &at, &size, w->dc, &src, 0, &bf, ULW_ALPHA);
    snapshot(w);
}

void wui_window_retheme(WuiWin *w)
{
    w->back_valid = 0;
    wui_relayout(w->root);
}

// ---- фокус, захват мыши, курсор ----

void wui_window_set_cursor(int idc) { g_cursor = idc; }

void wui_window_capture(WuiWin *w, WuiControl *c)
{
    w->capture = c;
    if (c) SetCapture(w->hwnd);
    else if (GetCapture() == w->hwnd) ReleaseCapture();
}

void wui_caret_restart(void)
{
    wui_caret_on = 1;
    for (WuiWin *w = wui_windows; w; w = w->next)
        if (w->focus && w->focus->type == WUI_T_TEXTBOX) {
            SetTimer(w->hwnd, TIMER_CARET, GetCaretBlinkTime(), 0);
            wui_invalidate(w->focus);
        }
}

void wui_window_focus(WuiWin *w, WuiControl *c)
{
    if (w->focus == c) return;
    WuiControl *old = w->focus;
    w->focus = c;
    if (old) {
        old->focused = 0;
        if (old->cls->focus) old->cls->focus(old, 0);
        wui_invalidate(old);
    }
    if (c) {
        c->focused = 1;
        if (c->cls->focus) c->cls->focus(c, 1);
        wui_caret_restart();
        wui_invalidate(c);
    }
}

void wui_focus(WuiControl *c) { if (c && c->win) wui_window_focus(c->win, c); }

// Tab: следующий элемент, что берёт фокус, по порядку дерева
static void collect(WuiControl *c, WuiControl **list, int *n, int cap)
{
    if (!c->visible || !c->enabled) return;
    if (c->cls && c->cls->focusable && *n < cap) list[(*n)++] = c;
    for (WuiControl *k = c->first; k; k = k->next) collect(k, list, n, cap);
}

static void tab_next(WuiWin *w, int back)
{
    WuiControl *list[512];
    int n = 0;
    collect(w->content, list, &n, 512);
    collect(w->user_bar, list, &n, 512);
    if (!n) return;
    int at = -1;
    for (int i = 0; i < n; i++) if (list[i] == w->focus) at = i;
    at = back ? (at <= 0 ? n - 1 : at - 1) : (at + 1) % n;
    wui_window_focus(w, list[at]);
}

// ---- мышь ----

static WuiControl *hit(WuiWin *w, float x, float y)
{
    WuiControl *h = wui_hit(w->titlebar, x, y);
    if (h && h->cls && h->cls->interactive) return h;
    WuiControl *c = wui_hit(w->content, x, y);
    return c ? c : h;
}

static int is_hand(WuiControl *c)
{
    switch (c->type) {
    case WUI_T_BUTTON: case WUI_T_CHECK: case WUI_T_SWITCH: case WUI_T_RADIO: case WUI_T_NAV_ITEM: return 1;
    }
    return 0;
}

// элемент живой, если включён он и все, в ком он лежит: выключенная карточка выключает и кнопки в ней
static int live(WuiControl *c)
{
    for (; c; c = c->parent)
        if (!c->enabled) return 0;
    return 1;
}

// до элемента, который отвечает на событие: сам или его родители (прокрутка, список)
static int send(WuiControl *c, int ev, float x, float y, int extra)
{
    for (; c; c = c->parent)
        if (c->enabled && c->cls && c->cls->mouse && c->cls->mouse(c, ev, x, y, extra)) return 1;
    return 0;
}

static void set_hot(WuiWin *w, WuiControl *c)
{
    if (w->hot == c) return;
    WuiControl *old = w->hot;
    w->hot = c;
    if (old && old->cls && old->cls->mouse) old->cls->mouse(old, WUI_MOUSE_LEAVE, 0, 0, 0);
    // прокрутка над которой ушла мышь — тоже гаснет
    for (WuiControl *p = old ? old->parent : 0; p; p = p->parent)
        if (p->type == WUI_T_SCROLL && p->hot) {
            int inside = 0;
            for (WuiControl *q = c; q; q = q->parent) if (q == p) inside = 1;
            if (!inside) { p->hot = 0; wui_invalidate(p); }
        }
}

static void mouse(WuiWin *w, int ev, LPARAM lp, WPARAM wp)
{
    float x = (short)LOWORD(lp) / w->scale, y = (short)HIWORD(lp) / w->scale;
    int keys = (int)wp;
    if (w->capture) {
        w->capture->cls->mouse(w->capture, ev, x, y, keys);
        return;
    }
    WuiControl *c = hit(w, x, y);
    if (c && !live(c)) c = 0;
    if (ev == WUI_MOUSE_MOVE) {
        g_cursor = 32512;
        set_hot(w, c);
        if (c) {
            if (is_hand(c)) g_cursor = 32649;          // IDC_HAND
            send(c, WUI_MOUSE_MOVE, x, y, keys);
        }
        return;
    }
    if (ev == WUI_MOUSE_DOWN) {
        if (wui_popup_is_open(0) && !(c && wui_popup_is_open(c))) wui_popup_close();
        if (c && c->cls && c->cls->focusable && c->type != WUI_T_TEXTBOX) wui_window_focus(w, c);
    }
    if (c) send(c, ev, x, y, keys);
}

// ---- рамка окна: где тащить и где тянуть ----

static LRESULT hit_test(WuiWin *w, LPARAM lp)
{
    RECT wr;
    GetWindowRect(w->hwnd, &wr);
    float s = w->scale;
    float x = ((short)LOWORD(lp) - wr.left) / s, y = ((short)HIWORD(lp) - wr.top) / s;
    float W = (wr.right - wr.left) / s, H = (wr.bottom - wr.top) / s;
    if (!w->maximized) {
        float m = outer(w), band = 6;
        int left = x < m + band, right = x > W - m - band, top = y < m + band, bottom = y > H - m - band;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }
    WuiRect tb = w->titlebar->r;
    if (y >= tb.y && y < tb.y + tb.h) {
        WuiControl *c = hit(w, x, y);
        if (!c || !c->cls || !c->cls->interactive) return HTCAPTION;
    }
    return HTCLIENT;
}

static void caption_click(WuiControl *c, void *user)
{
    WuiWin *w = c->win;
    (void)user;
    if (c->style == 0) ShowWindow(w->hwnd, SW_MINIMIZE);
    else if (c->style == 1) ShowWindow(w->hwnd, w->maximized ? SW_RESTORE : SW_MAXIMIZE);
    else PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
}

// ---- анимация бегущей полосы, всплывающая строка ----

static WuiControl *g_animated[16];

void wui_animate(WuiControl *c)
{
    for (int i = 0; i < 16; i++) if (g_animated[i] == c) return;
    for (int i = 0; i < 16; i++)
        if (!g_animated[i]) {
            g_animated[i] = c;
            if (c->win) SetTimer(c->win->hwnd, TIMER_ANIMATE, 33, 0);
            return;
        }
}

static void animate_tick(WuiWin *w)
{
    int any = 0;
    for (int i = 0; i < 16; i++) {
        WuiControl *c = g_animated[i];
        if (!c || c->win != w) continue;
        if (c->value >= 0) { g_animated[i] = 0; continue; }
        any = 1;
        if (wui_is_shown(c)) wui_invalidate(c);
    }
    if (!any) KillTimer(w->hwnd, TIMER_ANIMATE);
}

void wui_toast(WuiControl *window, const char *text)
{
    WuiWin *w = window->win;
    free(w->toast);
    w->toast = wui_strdup(text);
    wui_invalidate_rect(w, toast_rect(w));
    SetTimer(w->hwnd, TIMER_TOAST, 2600, 0);
}

// ---- размеры ----

static void fit_size(WuiWin *w)
{
    RECT r;
    GetWindowRect(w->hwnd, &r);
    w->pw = r.right - r.left;
    w->ph = r.bottom - r.top;
}

static UINT dpi_of(HWND h)
{
    typedef UINT (WINAPI *GetDpi)(HWND);
    static GetDpi get;
    static int tried;
    if (!tried) { tried = 1; get = (GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"); }
    UINT d = get ? get(h) : 0;
    if (!d) {
        HDC dc = GetDC(0);
        d = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(0, dc);
    }
    return d ? d : 96;
}

static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    WuiWin *w = win_of(h);
    if (!w) return DefWindowProcW(h, msg, wp, lp);
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp) return 0;                   // вся площадь — своя
        break;
    case WM_NCHITTEST:
        return hit_test(w, lp);
    case WM_NCACTIVATE:
        return 1;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        float s = w->scale, m = WUI_S->Window_OuterMargin.left * 2;
        float min_w = w->min_w > w->title_min ? w->min_w : w->title_min;
        mm->ptMinTrackSize.x = (LONG)((min_w + m) * s);
        mm->ptMinTrackSize.y = (LONG)((w->min_h + m) * s);
        MONITORINFO mi = { sizeof mi };
        GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
        // развёрнутое — ровно в рабочую область своего монитора, без тени
        mm->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
        mm->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
        mm->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
        mm->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        return 0;
    }
    case WM_SIZE: {
        int max = wp == SIZE_MAXIMIZED;
        if (max != w->maximized) { w->maximized = max; w->back_valid = 0; }
        fit_size(w);
        if (wp != SIZE_MINIMIZED) {
            w->back_valid = 0;
            w->layout_valid = 0;
            wui_popup_close();
            render(w);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        w->scale = HIWORD(wp) / 96.0f;
        RECT *r = (RECT *)lp;
        w->back_valid = w->layout_valid = 0;
        SetWindowPos(h, 0, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_MOVE:
        wui_popup_close();
        return 0;
    case WM_ACTIVATE:
        w->active = LOWORD(wp) != WA_INACTIVE;
        if (!w->active) wui_popup_close();
        return 0;
    case WM_WUI_SHOW:
        w->shown = 1;
        if (!GetEnvironmentVariableW(L"WUI_SNAPSHOT", 0, 0) || snap_wait()) ShowWindow(h, SW_SHOW);   // снимок — без показа
        render(w);
        return 0;
    case WM_WUI_RENDER:
        render(w);
        return 0;
    case WM_WUI_SNAP:
        render(w);
        snap_save(w, (int)wp);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursorW(0, MAKEINTRESOURCEW(g_cursor))); return 1; }
        break;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
        TrackMouseEvent(&t);
        mouse(w, WUI_MOUSE_MOVE, lp, wp);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (!w->capture) set_hot(w, 0);
        return 0;
    case WM_LBUTTONDOWN: mouse(w, WUI_MOUSE_DOWN, lp, wp); return 0;
    case WM_LBUTTONUP: mouse(w, WUI_MOUSE_UP, lp, wp); return 0;
    case WM_LBUTTONDBLCLK: mouse(w, WUI_MOUSE_DOUBLE, lp, wp); return 0;
    case WM_CAPTURECHANGED:
        if (w->capture && (HWND)lp != h) {
            WuiControl *c = w->capture;
            w->capture = 0;
            if (c->pressed) { c->pressed = 0; wui_invalidate(c); }
        }
        return 0;
    case WM_MOUSEWHEEL: {
        if (wui_popup_wheel((short)HIWORD(wp))) return 0;
        POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
        ScreenToClient(h, &p);
        float x = p.x / w->scale, y = p.y / w->scale;
        WuiControl *c = hit(w, x, y);
        if (c) send(c, WUI_MOUSE_WHEEL, x, y, (short)HIWORD(wp));
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wui_popup_key((int)wp)) return 0;
        if (wp == VK_TAB) { tab_next(w, GetKeyState(VK_SHIFT) < 0); return 0; }
        if (w->focus && live(w->focus) && w->focus->cls->key && w->focus->cls->key(w->focus, (int)wp, 0)) return 0;
        if (msg == WM_SYSKEYDOWN) break;
        return 0;
    case WM_CHAR:
        if (w->focus && live(w->focus) && w->focus->cls->key && wp >= 32) w->focus->cls->key(w->focus, 0, (int)wp);
        return 0;
    case WM_TIMER:
        if (wp == TIMER_CARET) {
            wui_caret_on = !wui_caret_on;
            if (w->focus && w->focus->type == WUI_T_TEXTBOX) wui_invalidate(w->focus);
            else KillTimer(h, TIMER_CARET);
        }
        else if (wp == TIMER_ANIMATE) animate_tick(w);
        else if (wp == TIMER_TOAST) {
            KillTimer(h, TIMER_TOAST);
            free(w->toast);
            w->toast = 0;
            wui_invalidate_rect(w, toast_rect(w));
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: {
        wui_popup_close();
        WuiWin **p = &wui_windows;
        while (*p && *p != w) p = &(*p)->next;
        if (*p) *p = w->next;
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        w->hwnd = 0;
        if (!wui_windows) PostQuitMessage(g_exit);
        return 0;
    }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---- создание ----

WuiControl *wui_window(const char *title, int width, int height, const char *key)
{
    wui_init();
    wui_theme_load(key);
    static int registered;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof wc };
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(0);
        wc.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW);
        wc.hIcon = LoadIconW(GetModuleHandleW(0), MAKEINTRESOURCEW(1));
        wc.lpszClassName = L"WuiWindow";
        RegisterClassExW(&wc);
        registered = 1;
    }
    WuiWin *w = calloc(1, sizeof(WuiWin));
    w->key = key ? wui_strdup(key) : 0;
    w->min_w = 360;
    w->min_h = 240;
    w->root = wui_new(0, WUI_T_ROOT, &root_class);
    w->root->win = w;
    w->content = wui_new(w->root, WUI_T_COLUMN, &wui_class_column);
    w->content->gap = 12;
    w->titlebar = wui_new(w->root, WUI_T_ROW, &titlebar_class);
    w->titlebar->pad.left = 10;
    w->titlebar->pad.right = 6;
    w->logo = wui_icon(w->titlebar, 0, 22);
    w->logo->margin.right = 12;
    w->logo->visible = 0;
    w->title = wui_new(w->titlebar, WUI_T_LABEL, &title_class);
    w->title->text = wui_strdup(title);
    w->title->align = WUI_ALIGN_CENTER;
    wui_spacer(w->titlebar);
    w->user_bar = wui_row(w->titlebar);
    w->user_bar->align = WUI_ALIGN_CENTER;
    w->user_bar->margin.right = 10;
    w->user_bar->gap = 8;
    for (int i = 0; i < 3; i++) wui_on_click(wui_caption_button(w->titlebar, i), caption_click, 0);
    // раскладка строки заголовка — как у обычной строки
    static WuiClass tb;
    tb = titlebar_class;
    tb.measure = wui_class_row.measure;
    tb.arrange = wui_class_row.arrange;
    w->titlebar->cls = &tb;
    w->titlebar->gap = 0;

    // окно по центру монитора, где мышь; не больше рабочей области
    POINT cur;
    GetCursorPos(&cur);
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromPoint(cur, MONITOR_DEFAULTTOPRIMARY), &mi);
    wchar_t *wt = wui_wide(title);
    w->hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_APPWINDOW, L"WuiWindow", wt,
                              WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
                              mi.rcWork.left, mi.rcWork.top, 100, 100, 0, 0, GetModuleHandleW(0), 0);
    free(wt);
    SetWindowLongPtrW(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);
    w->scale = dpi_of(w->hwnd) / 96.0f;
    char env[64];
    static int env_theme_done;                  // проверочная тема — только первому окну
    if (!env_theme_done++ && GetEnvironmentVariableA("WUI_THEME", env, sizeof env)) {
        for (int i = 0; i < wui_theme_total; i++)
            if (lstrcmpiA(wui_theme_list[i].id, env) == 0) wui_t = &wui_theme_list[i];
    }
    if (env_theme_done == 1 && GetEnvironmentVariableA("WUI_SCALE", env, sizeof env) && atof(env) > 0.5)
        w->scale = (float)atof(env);           // проверка вида при другом масштабе экрана
    if (env_theme_done == 1 && GetEnvironmentVariableA("WUI_SIZE", env, sizeof env)) {
        int sw = atoi(env), sh = 0;
        char *comma = strchr(env, ',');
        if (comma) sh = atoi(comma + 1);
        if (sw > 0 && sh > 0) { width = sw; height = sh; }
    }
    float m = WUI_S->Window_OuterMargin.left;
    int pw = (int)((width + 2 * m) * w->scale), ph = (int)((height + 2 * m) * w->scale);
    int aw = mi.rcWork.right - mi.rcWork.left, ah = mi.rcWork.bottom - mi.rcWork.top;
    if (pw > aw) pw = aw;
    if (ph > ah) ph = ah;
    SetWindowPos(w->hwnd, 0, mi.rcWork.left + (aw - pw) / 2, mi.rcWork.top + (ah - ph) / 2, pw, ph,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    fit_size(w);
    w->next = wui_windows;
    wui_windows = w;
    PostMessageW(w->hwnd, WM_WUI_SHOW, 0, 0);
    return w->root;
}

WuiControl *wui_content(WuiControl *window) { return window->win->content; }
WuiControl *wui_titlebar(WuiControl *window) { return window->win->user_bar; }
HWND wui_hwnd(WuiControl *window) { return window->win->hwnd; }

void wui_window_subtitle(WuiControl *window, const char *text)
{
    free(window->win->subtitle);
    window->win->subtitle = wui_strdup(text);
    wui_relayout(window);
}

void wui_window_logo(WuiControl *window, const WuiIconData *icon)
{
    window->win->logo->icon = icon;
    wui_relayout(window);
}

void wui_window_extend(WuiControl *window, int on)
{
    window->win->extend = on;
    wui_relayout(window);
}

void wui_window_min_size(WuiControl *window, int width, int height)
{
    window->win->min_w = (float)width;
    window->win->min_h = (float)height;
}

void wui_window_close(WuiControl *window)
{
    if (window->win->hwnd) PostMessageW(window->win->hwnd, WM_CLOSE, 0, 0);
}

int wui_run(void)
{
    MSG m;
    while (GetMessageW(&m, 0, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    wui_popup_close();
    return g_exit ? g_exit : (int)m.wParam;
}

void wui_quit(int code)
{
    g_exit = code;
    for (WuiWin *w = wui_windows; w; w = w->next) PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
    if (!wui_windows) PostQuitMessage(code);
}
