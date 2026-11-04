// WUI — вкладки и общие свойства элементов: текст и значение (флажок, выбор, ползунок, полоса).
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

// ---- вкладки: строка заголовков, под ней — выбранная страница ----

typedef struct {
    WuiControl *pages;          // столбец со страницами (WUI_T_TAB)
    float x[64], w[64];
    int count, hot;
} Tabs;

static WuiFont tab_font(void) { return wui_font(WUI_S->Font, WUI_S->FontSize, 400); }

static float header_h(void)
{
    WuiFont f = tab_font();
    return wui_line_height(&f) + 16;
}

static void show_selected(WuiControl *c)
{
    Tabs *t = c->ext;
    int i = 0;
    for (WuiControl *p = t->pages->first; p; p = p->next, i++)
        p->visible = i == (int)c->value;
}

static void tabs_measure(WuiControl *c, float aw, float ah)
{
    Tabs *t = c->ext;
    show_selected(c);
    WuiFont f = tab_font();
    float x = 0, w, h;
    t->count = 0;
    for (WuiControl *p = t->pages->first; p && t->count < 64; p = p->next) {
        wui_text_size(&f, p->text, 0, &w, &h);
        t->x[t->count] = x;
        t->w[t->count++] = w + 32;
        x += w + 32 + 4;
    }
    float head = header_h() + 1 + 14;
    wui_measure(t->pages, aw, ah - head);
    c->mw = x > t->pages->mw ? x : t->pages->mw;
    c->mh = head + t->pages->mh;
}

static void tabs_arrange(WuiControl *c)
{
    Tabs *t = c->ext;
    float head = header_h() + 1 + 14;
    WuiRect r = { c->r.x, c->r.y + head, c->r.w, c->r.h - head };
    if (r.h < 0) r.h = 0;
    wui_measure(t->pages, r.w, r.h);
    wui_arrange(t->pages, r);
}

static void tabs_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    Tabs *t = c->ext;
    float hh = header_h();
    WuiRect line = { c->r.x, c->r.y + hh, c->r.w, 1 };
    wui_fill(g, s->Separator, line, wui_corners(0), 1);
    WuiFont f = tab_font();
    int i = 0;
    for (WuiControl *p = t->pages->first; p && i < t->count; p = p->next, i++) {
        WuiRect r = { c->r.x + t->x[i], c->r.y, t->w[i], hh };
        int sel = i == (int)c->value;
        wui_fill(g, sel ? s->Tab_Selected : s->Tab_Background, r, s->Tab_CornerRadius, 1);
        wui_fill(g, s->Tab_Gloss, r, s->Tab_CornerRadius, 1);
        if (sel) {
            WuiRect ind = { r.x + 8, r.y + r.h - 2.5f, r.w - 16, 2.5f };
            wui_fill(g, s->Tab_Indicator, ind, wui_corners(0), 1);
        }
        WuiColor fore = wui_brush_color(sel ? s->Tab_SelectedForeground : i == t->hot ? s->Text : s->Tab_Foreground);
        wui_draw_text(g, &f, p->text, r, fore, 1, 0);
    }
}

static int tab_at(WuiControl *c, float x, float y)
{
    Tabs *t = c->ext;
    if (y < c->r.y || y >= c->r.y + header_h()) return -1;
    for (int i = 0; i < t->count; i++)
        if (x >= c->r.x + t->x[i] && x < c->r.x + t->x[i] + t->w[i]) return i;
    return -1;
}

static int tabs_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)extra;
    Tabs *t = c->ext;
    int i = tab_at(c, x, y);
    if (ev == WUI_MOUSE_DOWN && i >= 0) {
        wui_window_focus(c->win, c);
        if (i != (int)c->value) { wui_value(c, i); wui_fire_change(c); }
        return 1;
    }
    if (ev == WUI_MOUSE_MOVE || ev == WUI_MOUSE_LEAVE) {
        if (ev == WUI_MOUSE_LEAVE) i = -1;
        if (i != t->hot) { t->hot = i; wui_invalidate(c); }
        return i >= 0;
    }
    return i >= 0;
}

static int tabs_key(WuiControl *c, int vk, int ch)
{
    (void)ch;
    Tabs *t = c->ext;
    int i = (int)c->value;
    if (vk == VK_LEFT && i > 0) i--;
    else if (vk == VK_RIGHT && i + 1 < t->count) i++;
    else return 0;
    wui_value(c, i);
    wui_fire_change(c);
    return 1;
}

static void tabs_focus(WuiControl *c, int on) { (void)c; (void)on; }

static const WuiClass tabs_class = { tabs_measure, tabs_arrange, tabs_paint, 0, tabs_mouse, tabs_key, tabs_focus, 1, 0 };

WuiControl *wui_tabs(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_TABS, &tabs_class);
    Tabs *t = calloc(1, sizeof(Tabs));
    t->hot = -1;
    t->pages = wui_new(c, WUI_T_COLUMN, &wui_class_column);   // до c->ext: в саму вкладку, не в страницы
    c->ext = t;
    c->weight = 1;
    return c;
}

WuiControl *wui_tab(WuiControl *tabs, const char *title)
{
    WuiControl *p = wui_new(tabs, WUI_T_TAB, &wui_class_column);   // wui_new кладёт в столбец страниц
    p->text = wui_strdup(title);
    p->weight = 1;
    p->gap = 10;
    show_selected(tabs);
    return p;
}

// ---- текст ----

void wui_textbox_set(WuiControl *c, const char *text);
const char *wui_textbox_get(WuiControl *c);
void wui_list_reveal(WuiControl *c);
const char *wui_items_text(WuiControl *c);

void wui_text(WuiControl *c, const char *text)
{
    if (c->type == WUI_T_TEXTBOX) { wui_textbox_set(c, text); return; }
    if (c->type == WUI_T_ROOT) {
        free(c->win->title->text);
        c->win->title->text = wui_strdup(text);
        wchar_t *w = wui_wide(text);
        SetWindowTextW(c->win->hwnd, w);
        free(w);
        wui_relayout(c);
        return;
    }
    free(c->text);
    c->text = wui_strdup(text);
    wui_relayout(c);
}

const char *wui_get_text(WuiControl *c)
{
    switch (c->type) {
    case WUI_T_TEXTBOX: return wui_textbox_get(c);
    case WUI_T_COMBO: case WUI_T_LIST: return wui_items_text(c);
    case WUI_T_TITLE: return wui_t->name;
    case WUI_T_ROOT: return c->win->title->text;
    }
    return c->text ? c->text : "";
}

// ---- значение ----

void wui_animate(WuiControl *c);

void wui_value(WuiControl *c, double v)
{
    switch (c->type) {
    case WUI_T_CHECK: case WUI_T_SWITCH:
        c->checked = v != 0;
        break;
    case WUI_T_RADIO: case WUI_T_NAV_ITEM:
        if (v == 0) { c->checked = 0; break; }
        for (WuiControl *k = c->parent->first; k; k = k->next)
            if (k->type == c->type && k->checked && k != c) { k->checked = 0; wui_invalidate(k); }
        c->checked = 1;
        break;
    case WUI_T_SLIDER:
        if (v < c->vmin) v = c->vmin;
        if (v > c->vmax) v = c->vmax;
        c->value = v;
        break;
    case WUI_T_PROGRESS:
        c->value = v;
        if (v < 0) wui_animate(c);
        break;
    case WUI_T_LIST:
        c->value = v;
        wui_list_reveal(c);
        break;
    case WUI_T_TABS:
        c->value = v;
        show_selected(c);
        wui_relayout(c);
        return;
    default:
        c->value = v;
        break;
    }
    wui_invalidate(c);
}

double wui_get_value(WuiControl *c)
{
    switch (c->type) {
    case WUI_T_CHECK: case WUI_T_SWITCH: case WUI_T_RADIO: case WUI_T_NAV_ITEM:
        return c->checked;
    case WUI_T_TITLE:
        return wui_theme_current();
    }
    return c->value;
}
