// WUI — элементы: надпись, кнопка, пункт бокового меню, флажок, переключатель, ползунок,
// полоса хода, значок, холст. Вид — как шаблоны Controls.xaml библиотеки ThemeLibrary.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static WuiColor col(int brush) { return wui_brush_color(brush); }

void wui_draw_icon(GpGraphics *g, const WuiIconData *icon, float x, float y, float size, ARGB color)
{
    if (!icon) return;
    float s = size / 24;
    for (int part = 0; part < 2; part++) {
        const float *d = part ? icon->stroke : icon->fill;
        int n = part ? icon->stroke_count : icon->fill_count;
        if (!d || !n) continue;
        GpPath *p;
        GdipCreatePath(part || !icon->fill_nonzero ? FillModeAlternate : FillModeWinding, &p);
        float cx = 0, cy = 0;
        for (int i = 0; i < n;) {
            int op = (int)d[i++];
            if (op == 0) { GdipStartPathFigure(p); cx = x + d[i] * s; cy = y + d[i + 1] * s; i += 2; }
            else if (op == 1) {
                float nx = x + d[i] * s, ny = y + d[i + 1] * s;
                GdipAddPathLine(p, cx, cy, nx, ny);
                cx = nx; cy = ny; i += 2;
            }
            else if (op == 2) {
                float x3 = x + d[i + 4] * s, y3 = y + d[i + 5] * s;
                GdipAddPathBezier(p, cx, cy, x + d[i] * s, y + d[i + 1] * s, x + d[i + 2] * s, y + d[i + 3] * s, x3, y3);
                cx = x3; cy = y3; i += 6;
            }
            else GdipClosePathFigure(p);
        }
        if (part) {
            GpPen *pen;
            GdipCreatePen1(color, 1.8f * s, UnitPixel, &pen);
            GdipSetPenLineCap197819(pen, LineCapRound, LineCapRound, 2);
            GdipSetPenLineJoin(pen, LineJoinRound);
            GdipDrawPath(g, pen, p);
            GdipDeletePen(pen);
        }
        else {
            GpBrush *b = wui_solid(color);
            GdipFillPath(g, b, p);
            GdipDeleteBrush(b);
        }
        GdipDeletePath(p);
    }
}

// значок со свечением (Theme.Glow.Accent у выбранного пункта меню)
static void icon_glow(GpGraphics *g, const WuiIconData *icon, float x, float y, float size, int effect)
{
    if (!effect || !icon) return;
    float s = wui_paint_scale;
    int pad = wui_effect_pad(effect);
    int pw = (int)(size * s) + 2 * pad;
    GpBitmap *shape;
    GdipCreateBitmapFromScan0(pw, pw, 0, PixelFormat32bppPARGB, NULL, &shape);
    GpGraphics *sg;
    GdipGetImageGraphicsContext(shape, &sg);
    GdipSetSmoothingMode(sg, SmoothingModeAntiAlias);
    GdipScaleWorldTransform(sg, s, s, MatrixOrderAppend);
    wui_draw_icon(sg, icon, pad / s, pad / s, size, 0xFFFFFFFF);
    GdipDeleteGraphics(sg);
    GpBitmap *glow = wui_effect_bitmap(shape, effect);
    GdipDisposeImage(shape);
    GdipDrawImageRect(g, glow, x - pad / s, y - pad / s, pw / s, pw / s);
    GdipDisposeImage(glow);
}

// ---- надпись ----

static WuiFont label_font(int style, WuiColor *color, int *effect)
{
    const WuiStyle *s = WUI_S;
    WuiFont f = wui_font(s->Font, s->FontSize, 400);
    *color = col(s->Text);
    *effect = 0;
    switch (style) {
    case WUI_LABEL_BODY: f = wui_font(s->Font_Body, s->Body_Size, 400); break;
    case WUI_LABEL_DIM: f.size = s->FontSize_Small; *color = col(s->TextDim); break;
    case WUI_LABEL_H1:
        f = wui_font(s->H1_Font, s->H1_Size, s->H1_Weight);
        f.caps = s->H1_Capitals;
        *color = col(s->H1_Foreground);
        break;
    case WUI_LABEL_H2:
        f = wui_font(s->H2_Font, s->H2_Size, s->H2_Weight);
        f.caps = s->H2_Capitals;
        *color = col(s->H2_Foreground);
        break;
    case WUI_LABEL_TITLE:
        f = wui_font(s->Card_TitleFont, s->Card_TitleFontSize, s->Card_TitleFontWeight);
        f.caps = s->Card_TitleCapitals;
        *color = col(s->Card_TitleForeground);
        break;
    case WUI_LABEL_SECTION:
        f.size = s->FontSize_Small * 0.92f / 0.8f;
        f.weight = 600;
        f.caps = 1;
        *color = col(s->TextDim);
        break;
    case WUI_LABEL_MONO: f = wui_font(s->Font_Mono, s->FontSize, 400); break;
    case WUI_LABEL_BIG:
        f = wui_font(s->H1_Font, s->Big_Size, 300);
        *color = col(s->Accent);
        *effect = s->Big_Effect;
        break;
    case WUI_LABEL_ACCENT: *color = col(s->Accent); break;
    }
    return f;
}

static int label_wraps(int style) { return style != WUI_LABEL_BIG && style != WUI_LABEL_SECTION; }

static void label_measure(WuiControl *c, float aw, float ah)
{
    (void)ah;
    WuiColor color;
    int effect;
    WuiFont f = label_font(c->style, &color, &effect);
    wui_text_size(&f, c->text, label_wraps(c->style) && aw < 1e5f ? aw : 0, &c->mw, &c->mh);
    c->mw += 1;                     // запас против обрезки при округлении
}

static void label_paint(WuiControl *c, GpGraphics *g)
{
    WuiColor color;
    int effect;
    WuiFont f = label_font(c->style, &color, &effect);
    if (effect) wui_text_glow(g, &f, c->text, c->r, effect, 0);
    wui_draw_text(g, &f, c->text, c->r, color, 0, label_wraps(c->style));
}

static const WuiClass label_class = { label_measure, 0, label_paint };

WuiControl *wui_label(WuiControl *parent, const char *text, int style)
{
    WuiControl *c = wui_new(parent, WUI_T_LABEL, &label_class);
    c->text = wui_strdup(text);
    c->style = style;
    // с переносом — во всю ширину (переносится по ней), без переноса — по своему размеру
    c->align = label_wraps(style) ? WUI_ALIGN_STRETCH : WUI_ALIGN_START;
    if (parent && parent->type == WUI_T_ROW) c->align = WUI_ALIGN_CENTER;
    return c;
}

WuiControl *wui_nav_section(WuiControl *parent, const char *text)
{
    WuiControl *c = wui_label(parent, text, WUI_LABEL_SECTION);
    c->margin.left = 14;
    c->margin.top = 14;
    c->margin.bottom = 4;
    return c;
}

// ---- кнопка ----

typedef struct { int bg, border, hover_border, fore, gloss, inner, hover, pressed, effect, hover_effect; } Look;

static Look button_look(int kind)
{
    const WuiStyle *s = WUI_S;
    Look l = { s->Button_Background, s->Button_Border, s->Button_BorderHover, s->Button_Foreground, s->Button_Gloss,
               s->Button_InnerBorder, s->Button_HoverOverlay, s->Button_PressedOverlay, 0, s->Button_HoverEffect };
    switch (kind) {
    case WUI_BUTTON_ACCENT:
        l.bg = s->AccentButton_Background; l.border = s->AccentButton_Border; l.hover_border = s->AccentButton_Border;
        l.fore = s->AccentButton_Foreground; l.inner = s->AccentButton_InnerBorder;
        l.effect = s->AccentButton_Effect; l.hover_effect = s->AccentButton_HoverEffect;
        break;
    case WUI_BUTTON_DANGER:
        l.bg = s->DangerButton_Background; l.border = s->DangerButton_Border; l.hover_border = s->Danger;
        l.fore = s->DangerButton_Foreground; l.inner = 0; l.hover_effect = 0;
        break;
    case WUI_BUTTON_SUCCESS:
        l.bg = s->SuccessButton_Background; l.border = s->SuccessButton_Border; l.hover_border = s->Success;
        l.fore = s->SuccessButton_Foreground; l.inner = 0; l.hover_effect = 0;
        break;
    case WUI_BUTTON_GHOST:
        l.bg = 0; l.border = 0; l.hover_border = 0; l.gloss = 0; l.inner = 0; l.hover_effect = 0;
        break;
    }
    return l;
}

static WuiFont button_font(WuiControl *c)
{
    const WuiStyle *s = WUI_S;
    return wui_font(s->Button_FontFamily, s->Button_FontSize, c->style == WUI_BUTTON_ACCENT ? 600 : s->Button_FontWeight);
}

static void button_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    WuiFont f = button_font(c);
    float w = 0, h = wui_line_height(&f);
    if (c->text && *c->text) wui_text_size(&f, c->text, 0, &w, &h);
    if (c->icon) w += (w > 0 ? 26 : 18);
    WuiEdges p = WUI_S->Button_Padding;
    if (!c->text || !*c->text) { p.left = p.right = p.top = p.bottom = 8; }
    c->mw = w + p.left + p.right;
    c->mh = h + p.top + p.bottom;
}

// общий рисунок кнопки (ButtonTemplate): фон с тенью, блик, внутренняя рамка, подсветка, содержимое
static void button_body(WuiControl *c, GpGraphics *g, Look l, WuiCorners k)
{
    const WuiStyle *s = WUI_S;
    int hot = c->hot || c->pressed;
    int effect = hot ? l.hover_effect : l.effect;
    if (effect && l.bg) wui_effect(g, effect, l.bg, c->r, k, 1);
    wui_fill(g, l.bg, c->r, k, 1);
    wui_fill(g, l.gloss, wui_inset(c->r, 1, 1, 1, 1), k, 1);
    wui_stroke(g, l.inner, wui_inset(c->r, 3, 3, 3, 3), k, s->Button_InnerBorderThickness, 1);
    if (c->pressed && hot) wui_fill(g, l.pressed, c->r, k, 1);
    else if (hot) wui_fill(g, l.hover, c->r, k, 1);
    int border = hot || c->focused ? l.hover_border : l.border;
    wui_stroke(g, border, c->r, k, s->Button_BorderThickness, 1);
}

static void button_paint(WuiControl *c, GpGraphics *g)
{
    Look l = button_look(c->style);
    button_body(c, g, l, WUI_S->Button_CornerRadius);
    WuiFont f = button_font(c);
    WuiColor fore = col(l.fore);
    float tw = 0, th = 0;
    if (c->text && *c->text) wui_text_size(&f, c->text, 0, &tw, &th);
    float iw = c->icon ? 18 : 0, gap = c->icon && tw > 0 ? 8 : 0;
    float x = c->r.x + (c->r.w - tw - iw - gap) / 2;
    if (c->icon) wui_draw_icon(g, c->icon, x, c->r.y + (c->r.h - 18) / 2, 18, fore);
    WuiRect t = { x + iw + gap, c->r.y, tw + 2, c->r.h };
    wui_draw_text(g, &f, c->text, t, fore, 0, 0);
}

// мышь у всего, что нажимается: нажал — отпустил над ним — щелчок
static int press_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)extra;
    int inside = x >= c->r.x && y >= c->r.y && x < c->r.x + c->r.w && y < c->r.y + c->r.h;
    switch (ev) {
    case WUI_MOUSE_DOWN:
        c->pressed = 1;
        wui_window_capture(c->win, c);
        wui_invalidate(c);
        return 1;
    case WUI_MOUSE_UP:
        if (!c->pressed) return 1;
        c->pressed = 0;
        wui_window_capture(c->win, 0);
        wui_invalidate(c);
        if (inside) {
            if (c->type == WUI_T_CHECK || c->type == WUI_T_SWITCH) { c->checked = !c->checked; wui_fire_change(c); }
            else if ((c->type == WUI_T_RADIO || c->type == WUI_T_NAV_ITEM) && !c->checked) wui_value(c, 1);
            wui_fire_click(c);
        }
        return 1;
    case WUI_MOUSE_MOVE:
        if (c->hot != inside) { c->hot = inside; wui_invalidate(c); }
        return 1;
    case WUI_MOUSE_LEAVE:
        if (c->hot) { c->hot = 0; wui_invalidate(c); }
        return 1;
    }
    return 0;
}

static int press_key(WuiControl *c, int vk, int ch)
{
    (void)ch;
    if (vk == VK_SPACE || vk == VK_RETURN) {
        if (c->type == WUI_T_CHECK || c->type == WUI_T_SWITCH) { c->checked = !c->checked; wui_fire_change(c); }
        else if ((c->type == WUI_T_RADIO || c->type == WUI_T_NAV_ITEM) && !c->checked) wui_value(c, 1);
        wui_invalidate(c);
        wui_fire_click(c);
        return 1;
    }
    return 0;
}

static void press_focus(WuiControl *c, int on) { (void)on; wui_invalidate(c); }

static const WuiClass button_class = { button_measure, 0, button_paint, 0, press_mouse, press_key, press_focus, 1, 1, 26 };

WuiControl *wui_button(WuiControl *parent, const char *text, int kind)
{
    WuiControl *c = wui_new(parent, WUI_T_BUTTON, &button_class);
    c->text = wui_strdup(text);
    c->style = kind;
    if (parent && parent->type == WUI_T_ROW) c->align = WUI_ALIGN_CENTER;
    else if (parent && parent->type != WUI_T_WRAP) c->align = WUI_ALIGN_START;
    return c;
}

void wui_button_icon(WuiControl *b, const WuiIconData *icon)
{
    b->icon = icon;
    wui_relayout(b);
}

// ---- кнопки заголовка окна: свернуть, развернуть, закрыть ----

static void caption_measure(WuiControl *c, float aw, float ah) { (void)aw; (void)ah; c->mw = 40; c->mh = 30; }

static void caption_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    WuiCorners k = s->Caption_CornerRadius;
    int close = c->style == 2;
    if (c->hot || c->pressed) wui_fill(g, close ? s->Caption_CloseHover : s->Caption_Hover, c->r, k, 1);
    WuiColor fore = close && c->hot ? 0xFFFFFFFF : col(s->Title_Foreground);
    float x = (float)(int)(c->r.x + (c->r.w - 10) / 2), y = (float)(int)(c->r.y + (c->r.h - 10) / 2);
    GpPen *pen;
    GdipCreatePen1(fore, close ? 1.3f : 1.2f, UnitPixel, &pen);
    if (c->style == 0) GdipDrawLine(g, pen, x, y + 5.5f, x + 10, y + 5.5f);
    else if (c->style == 1) {
        if (c->win && c->win->maximized) {
            GdipDrawLine(g, pen, x + 2.5f, y + 0.5f, x + 9.5f, y + 0.5f);
            GdipDrawLine(g, pen, x + 9.5f, y + 0.5f, x + 9.5f, y + 7.5f);
            GdipDrawRectangle(g, pen, x + 0.5f, y + 2.5f, 7, 7);
        }
        else GdipDrawRectangle(g, pen, x + 0.5f, y + 0.5f, 9, 9);
    }
    else {
        GdipDrawLine(g, pen, x, y, x + 10, y + 10);
        GdipDrawLine(g, pen, x + 10, y, x, y + 10);
    }
    GdipDeletePen(pen);
}

static const WuiClass caption_class = { caption_measure, 0, caption_paint, 0, press_mouse, 0, 0, 0, 1 };

WuiControl *wui_caption_button(WuiControl *parent, int which)
{
    WuiControl *c = wui_new(parent, WUI_T_CAPTION, &caption_class);
    c->style = which;
    c->align = WUI_ALIGN_CENTER;
    return c;
}

// ---- пункт бокового меню (Theme.NavItem) ----

static void nav_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    WuiFont f = wui_font(WUI_S->Font, WUI_S->FontSize, 400);
    float w, h;
    wui_text_size(&f, c->text, 0, &w, &h);
    if (h < 18) h = 18;
    c->mw = 14 + (c->icon ? 30 : 0) + w + 14;
    c->mh = 9 + h + 9;
}

static void nav_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    WuiCorners k = s->Item_CornerRadius;
    WuiColor fore = col(s->TextDim);
    if (c->checked) {
        wui_fill(g, s->Item_Selected, c->r, k, 1);
        wui_stroke(g, s->Item_SelectedBorder, c->r, k, s->Item_SelectedBorderThickness, 1);
        fore = col(s->Item_SelectedForeground);
    }
    else if (c->hot) {
        wui_fill(g, s->Item_Hover, c->r, k, 1);
        fore = col(s->Text);
    }
    float x = c->r.x + 14;
    if (c->icon) {
        float y = c->r.y + (c->r.h - 18) / 2;
        if (c->checked) icon_glow(g, c->icon, x, y, 18, s->Glow_Accent);
        wui_draw_icon(g, c->icon, x, y, 18, fore);
        x += 30;
    }
    WuiFont f = wui_font(s->Font, s->FontSize, 400);
    WuiRect t = { x, c->r.y, c->r.x + c->r.w - 14 - x, c->r.h };
    wui_draw_text(g, &f, c->text, t, fore, 0, 0);
}

static const WuiClass nav_class = { nav_measure, 0, nav_paint, 0, press_mouse, press_key, press_focus, 1, 1, 12 };

WuiControl *wui_nav_item(WuiControl *parent, const WuiIconData *icon, const char *text)
{
    WuiControl *c = wui_new(parent, WUI_T_NAV_ITEM, &nav_class);
    c->text = wui_strdup(text);
    c->icon = icon;
    // первый пункт группы — выбран
    int others = 0;
    for (WuiControl *k = parent->first; k; k = k->next)
        if (k != c && k->type == WUI_T_NAV_ITEM) others = 1;
    c->checked = !others;
    return c;
}

// ---- флажок, переключатель, кнопка выбора ----

static float box_w(WuiControl *c) { return c->type == WUI_T_SWITCH ? 42 : 18; }

static void check_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    WuiFont f = wui_font(WUI_S->Font, WUI_S->FontSize, 400);
    float w = 0, h = 0;
    if (c->text && *c->text) { wui_text_size(&f, c->text, 0, &w, &h); w += 9; }
    float bh = c->type == WUI_T_SWITCH ? 23 : 18;
    c->mw = box_w(c) + w;
    c->mh = h > bh ? h : bh;
}

static void check_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    float bw = box_w(c), bh = c->type == WUI_T_SWITCH ? 23 : 18;
    WuiRect b = { c->r.x, c->r.y + (c->r.h - bh) / 2, bw, bh };
    WuiEdges one = { 1.2f, 1.2f, 1.2f, 1.2f };
    int focus_border = c->hot || c->focused;
    if (c->type == WUI_T_CHECK) {
        WuiCorners k = s->Check_CornerRadius;
        wui_fill(g, s->Check_Background, b, k, 1);
        wui_stroke(g, c->checked || focus_border ? s->Field_BorderFocus : s->Check_Border, b, k, one, 1);
        if (c->checked) {
            wui_fill(g, s->Check_CheckedBackground, b, k, 1);
            GpPen *pen;
            GdipCreatePen1(col(s->Check_Mark), 2.2f, UnitPixel, &pen);
            GdipSetPenLineCap197819(pen, LineCapRound, LineCapRound, 2);
            GdipSetPenLineJoin(pen, LineJoinRound);
            PointF pts[3] = { { b.x + 4, b.y + 9.2f }, { b.x + 7.6f, b.y + 12.8f }, { b.x + 14.2f, b.y + 5.4f } };
            GdipDrawLines(g, pen, pts, 3);
            GdipDeletePen(pen);
        }
    }
    else if (c->type == WUI_T_RADIO) {
        WuiCorners k = wui_corners(9);
        wui_fill(g, s->Check_Background, b, k, 1);
        wui_stroke(g, c->checked || focus_border ? s->Field_BorderFocus : s->Check_Border, b, k, one, 1);
        if (c->checked) {
            WuiRect d = { b.x + 5, b.y + 5, 8, 8 };
            wui_fill(g, s->Radio_Dot, d, wui_corners(4), 1);
        }
    }
    else {
        WuiCorners k = wui_corners(11.5f);
        WuiEdges e1 = { 1, 1, 1, 1 };
        wui_fill(g, s->Switch_Track, b, k, 1);
        wui_stroke(g, focus_border ? s->Field_BorderFocus : s->Switch_TrackBorder, b, k, e1, 1);
        if (c->checked) wui_fill(g, s->Switch_TrackOn, b, k, 1);
        WuiRect knob = { b.x + (c->checked ? 22 : 3), b.y + 3, 17, 17 };
        wui_fill(g, c->checked ? s->Switch_KnobOn : s->Switch_Knob, knob, wui_corners(8.5f), 1);
    }
    if (c->text && *c->text) {
        WuiFont f = wui_font(s->Font, s->FontSize, 400);
        WuiRect t = { b.x + bw + 9, c->r.y, c->r.w - bw - 9, c->r.h };
        wui_draw_text(g, &f, c->text, t, col(s->Text), 0, 0);
    }
}

static const WuiClass check_class = { check_measure, 0, check_paint, 0, press_mouse, press_key, press_focus, 1, 1, 2 };

static WuiControl *make_check(WuiControl *parent, const char *text, int type)
{
    WuiControl *c = wui_new(parent, type, &check_class);
    c->text = wui_strdup(text);
    c->align = WUI_ALIGN_START;
    if (parent && parent->type == WUI_T_ROW) c->align = WUI_ALIGN_CENTER;
    return c;
}

WuiControl *wui_checkbox(WuiControl *parent, const char *text) { return make_check(parent, text, WUI_T_CHECK); }
WuiControl *wui_switch(WuiControl *parent, const char *text) { return make_check(parent, text, WUI_T_SWITCH); }
WuiControl *wui_radio(WuiControl *parent, const char *text)
{
    WuiControl *c = make_check(parent, text, WUI_T_RADIO);
    int others = 0;
    for (WuiControl *k = parent->first; k; k = k->next)
        if (k != c && k->type == WUI_T_RADIO) others = 1;
    c->checked = !others;
    return c;
}

// ---- ползунок ----

static void slider_measure(WuiControl *c, float aw, float ah) { (void)aw; (void)ah; c->mw = 120; c->mh = 24; }

static float slider_x(WuiControl *c)
{
    float span = c->vmax - c->vmin > 0 ? (float)((c->value - c->vmin) / (c->vmax - c->vmin)) : 0;
    return c->r.x + 9 + (c->r.w - 18) * span;
}

static void slider_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    float cy = c->r.y + c->r.h / 2, x = slider_x(c);
    WuiRect track = { c->r.x + 9, cy - 2, c->r.w - 18, 4 };
    wui_fill(g, s->Slider_Track, track, wui_corners(2), 1);
    WuiRect fill = { c->r.x + 9, cy - 2, x - c->r.x - 9, 4 };
    wui_fill(g, s->Slider_Fill, fill, wui_corners(2), 1);
    float k = c->hot || c->pressed ? 1.12f : 1;
    WuiRect knob = { x - 9 * k, cy - 9 * k, 18 * k, 18 * k };
    WuiCorners round = wui_corners(9 * k);
    wui_effect(g, s->Slider_ThumbEffect, s->Slider_Thumb, knob, round, 1);
    wui_fill(g, s->Slider_Thumb, knob, round, 1);
    WuiEdges e = { 1.5f, 1.5f, 1.5f, 1.5f };
    wui_stroke(g, s->Slider_ThumbBorder, knob, round, e, 1);
}

static void slider_set(WuiControl *c, float x)
{
    double v = c->vmin + (x - c->r.x - 9) / (c->r.w - 18) * (c->vmax - c->vmin);
    if (v < c->vmin) v = c->vmin;
    if (v > c->vmax) v = c->vmax;
    if (v != c->value) {
        c->value = v;
        wui_invalidate(c);
        wui_fire_change(c);
    }
}

static int slider_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)y; (void)extra;
    switch (ev) {
    case WUI_MOUSE_DOWN:
        c->pressed = 1;
        wui_window_capture(c->win, c);
        slider_set(c, x);
        return 1;
    case WUI_MOUSE_MOVE:
        if (c->pressed) slider_set(c, x);
        else {
            int hot = fabsf(x - slider_x(c)) < 12;
            if (hot != c->hot) { c->hot = hot; wui_invalidate(c); }
        }
        return 1;
    case WUI_MOUSE_UP:
        c->pressed = 0;
        wui_window_capture(c->win, 0);
        wui_invalidate(c);
        return 1;
    case WUI_MOUSE_LEAVE:
        if (c->hot) { c->hot = 0; wui_invalidate(c); }
        return 1;
    }
    return 0;
}

static int slider_key(WuiControl *c, int vk, int ch)
{
    (void)ch;
    double step = (c->vmax - c->vmin) / 20, was = c->value;
    if (vk == VK_LEFT || vk == VK_DOWN) wui_value(c, c->value - step);
    else if (vk == VK_RIGHT || vk == VK_UP) wui_value(c, c->value + step);
    else return 0;
    if (c->value != was) wui_fire_change(c);       // на краю стрелка ничего не меняет — и не сообщает
    return 1;
}

static const WuiClass slider_class = { slider_measure, 0, slider_paint, 0, slider_mouse, slider_key, press_focus, 1, 1, 14 };

WuiControl *wui_slider(WuiControl *parent, double min, double max, double value)
{
    WuiControl *c = wui_new(parent, WUI_T_SLIDER, &slider_class);
    c->vmin = min;
    c->vmax = max > min ? max : min + 1;
    c->value = value < c->vmin ? c->vmin : value > c->vmax ? c->vmax : value;
    return c;
}

// ---- полоса хода ----

static void progress_measure(WuiControl *c, float aw, float ah) { (void)aw; (void)ah; c->mw = 100; c->mh = 8; }

static void progress_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    WuiCorners k = s->Progress_CornerRadius;
    WuiEdges e = { 1, 1, 1, 1 };
    wui_fill(g, s->Progress_Track, c->r, k, 1);
    wui_stroke(g, s->Progress_Border, c->r, k, e, 1);
    WuiRect in = wui_inset(c->r, 1, 1, 1, 1);
    if (c->value < 0) {                 // бежит, пока идёт работа
        float t = (float)(GetTickCount() % 2200) / 2200;
        WuiRect run = { in.x - 140 + (1540) * t, in.y, 140, in.h };
        UINT st;
        GdipSaveGraphics(g, &st);
        GdipSetClipRect(g, in.x, in.y, in.w, in.h, CombineModeIntersect);
        wui_fill(g, s->Progress_Runner, run, wui_corners(0), 1);
        GdipRestoreGraphics(g, st);
        return;
    }
    float v = c->value > 1 ? 1 : (float)c->value;
    WuiRect bar = { in.x, in.y, in.w * v, in.h };
    wui_fill(g, s->Progress_Fill, bar, k, 1);
}

static const WuiClass progress_class = { progress_measure, 0, progress_paint };

WuiControl *wui_progress(WuiControl *parent)
{
    return wui_new(parent, WUI_T_PROGRESS, &progress_class);
}

// ---- значок ----

static void icon_measure(WuiControl *c, float aw, float ah) { (void)aw; (void)ah; c->mw = c->mh = (float)c->vmax; }

static void icon_paint(WuiControl *c, GpGraphics *g)
{
    float size = c->r.w < c->r.h ? c->r.w : c->r.h;
    wui_draw_icon(g, c->icon, c->r.x + (c->r.w - size) / 2, c->r.y + (c->r.h - size) / 2, size,
                  col(c->style ? WUI_S->Text : WUI_S->Accent));
}

static const WuiClass icon_class = { icon_measure, 0, icon_paint };

WuiControl *wui_icon(WuiControl *parent, const WuiIconData *icon, float size)
{
    WuiControl *c = wui_new(parent, WUI_T_ICON, &icon_class);
    c->icon = icon;
    c->vmax = size > 0 ? size : 18;
    c->align = WUI_ALIGN_CENTER;
    return c;
}

// ---- холст: рисует сама программа ----

typedef struct { WuiPaint paint; void *user; } Canvas;

static void canvas_measure(WuiControl *c, float aw, float ah) { (void)aw; (void)ah; c->mw = 100; c->mh = 100; }

static void canvas_paint(WuiControl *c, GpGraphics *g)
{
    Canvas *cv = c->ext;
    if (!cv->paint) return;
    UINT st;
    GdipSaveGraphics(g, &st);
    GdipSetClipRect(g, c->r.x, c->r.y, c->r.w, c->r.h, CombineModeIntersect);
    GdipTranslateWorldTransform(g, c->r.x, c->r.y, MatrixOrderPrepend);
    cv->paint(c, g, c->r.w, c->r.h, cv->user);
    GdipRestoreGraphics(g, st);
}

static int canvas_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)x; (void)y; (void)extra;
    if (ev == WUI_MOUSE_UP) wui_fire_click(c);
    return c->on_click != 0;
}

static const WuiClass canvas_class = { canvas_measure, 0, canvas_paint, 0, canvas_mouse };

WuiControl *wui_canvas(WuiControl *parent, WuiPaint paint, void *user)
{
    WuiControl *c = wui_new(parent, WUI_T_CANVAS, &canvas_class);
    Canvas *cv = calloc(1, sizeof(Canvas));
    cv->paint = paint;
    cv->user = user;
    c->ext = cv;
    return c;
}
