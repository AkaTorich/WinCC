// WUI — раскрывающийся список, выбор темы (все темы по группам) и список с прокруткой.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    int count, cap;
    int reveal;                 // выбранную строку показать при следующей отрисовке (wui_value)
} Items;

static int items_add(Items *it, const char *text)
{
    if (it->count == it->cap) {
        it->cap = it->cap ? it->cap * 2 : 16;
        it->items = realloc(it->items, sizeof(char *) * (size_t)it->cap);
    }
    it->items[it->count] = wui_strdup(text);
    return it->count++;
}

static void items_clear(Items *it)
{
    for (int i = 0; i < it->count; i++) free(it->items[i]);
    it->count = 0;
}

// ---- раскрывающийся список ----

static WuiFont field_font(void) { return wui_font(WUI_S->Field_FontFamily, WUI_S->Field_FontSize, 400); }

static const char *combo_text(WuiControl *c)
{
    if (c->type == WUI_T_TITLE) return wui_t->name;            // выбор темы
    Items *it = c->ext;
    int i = (int)c->value;
    return i >= 0 && i < it->count ? it->items[i] : "";
}

static void combo_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    WuiFont f = field_font();
    WuiEdges p = WUI_S->Field_Padding, b = WUI_S->Field_BorderThickness;
    float widest = 60, w, h;
    if (c->type != WUI_T_TITLE) {
        Items *it = c->ext;
        for (int i = 0; i < it->count; i++) {
            wui_text_size(&f, it->items[i], 0, &w, &h);
            if (w > widest) widest = w;
        }
    }
    c->mw = widest + p.left + 30 + b.left + b.right;
    c->mh = wui_line_height(&f) + p.top + p.bottom + b.top + b.bottom;
}

static void combo_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    WuiCorners k = s->Field_CornerRadius;
    int open = wui_popup_is_open(c);
    wui_fill(g, s->Field_Background, c->r, k, 1);
    wui_stroke(g, open || c->hot || c->focused ? s->Field_BorderFocus : s->Field_Border, c->r, k, s->Field_BorderThickness, 1);
    WuiFont f = field_font();
    WuiRect t = wui_deflate(wui_deflate(c->r, s->Field_BorderThickness), s->Field_Padding);
    t.w = c->r.x + c->r.w - 30 - t.x;
    wui_draw_text(g, &f, combo_text(c), t, wui_brush_color(s->Field_Foreground), 0, 0);
    // стрелка «M0,0 L4.5,5 L9,0» посередине правой колонки в 30
    float ax = c->r.x + c->r.w - 15 - 4.5f, ay = c->r.y + c->r.h / 2 - 2.5f;
    GpPen *pen;
    GdipCreatePen1(wui_brush_color(s->Combo_Arrow), 1.6f, UnitPixel, &pen);
    GdipSetPenLineCap197819(pen, LineCapRound, LineCapRound, 2);
    GdipSetPenLineJoin(pen, LineJoinRound);
    PointF pts[3] = { { ax, ay }, { ax + 4.5f, ay + 5 }, { ax + 9, ay } };
    if (open) { pts[0].Y = pts[2].Y = ay + 5; pts[1].Y = ay; }
    GdipDrawLines(g, pen, pts, 3);
    GdipDeletePen(pen);
}

static void combo_done(WuiControl *c, int index)
{
    if (c->type == WUI_T_TITLE) {
        // в выпадающем списке темы идут по группам: номер строки → номер темы
        int *map = c->ext;
        if (map[index] >= 0 && map[index] != wui_theme_current()) {
            wui_theme_set(wui_theme_list[map[index]].id);
            wui_fire_change(c);         // программа узнаёт о смене темы (номер — wui_theme_current)
        }
        return;
    }
    if (index != (int)c->value) {
        c->value = index;
        wui_invalidate(c);
        wui_fire_change(c);
    }
}

static void combo_open(WuiControl *c)
{
    if (wui_popup_is_open(c)) { wui_popup_close(); return; }
    if (c->type == WUI_T_TITLE) {
        // группы по порядку первого появления, внутри — как в библиотеке
        int n = wui_theme_total;
        const char *groups[64];
        int ng = 0;
        for (int i = 0; i < n; i++) {
            int seen = 0;
            for (int j = 0; j < ng; j++) if (strcmp(groups[j], wui_theme_list[i].group) == 0) seen = 1;
            if (!seen && ng < 64) groups[ng++] = wui_theme_list[i].group;
        }
        WuiPopupItem *items = malloc(sizeof(WuiPopupItem) * (size_t)(n + ng));
        int *map = c->ext, count = 0, selected = -1;
        for (int j = 0; j < ng; j++) {
            items[count].text = groups[j];
            items[count].header = 1;
            map[count++] = -1;
            for (int i = 0; i < n; i++) {
                if (strcmp(wui_theme_list[i].group, groups[j]) != 0) continue;
                if (&wui_theme_list[i] == wui_t) selected = count;
                items[count].text = wui_theme_list[i].name;
                items[count].header = 0;
                map[count++] = i;
            }
        }
        wui_popup_open(c, items, count, selected, combo_done);
        free(items);
        return;
    }
    Items *it = c->ext;
    WuiPopupItem *items = malloc(sizeof(WuiPopupItem) * (size_t)(it->count ? it->count : 1));
    for (int i = 0; i < it->count; i++) { items[i].text = it->items[i]; items[i].header = 0; }
    wui_popup_open(c, items, it->count, (int)c->value, combo_done);
    free(items);
}

static int combo_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)x; (void)y; (void)extra;
    switch (ev) {
    case WUI_MOUSE_DOWN:
        wui_window_focus(c->win, c);
        combo_open(c);
        return 1;
    case WUI_MOUSE_MOVE:
        if (!c->hot) { c->hot = 1; wui_invalidate(c); }
        return 1;
    case WUI_MOUSE_LEAVE:
        if (c->hot) { c->hot = 0; wui_invalidate(c); }
        return 1;
    }
    return 1;
}

static int combo_key(WuiControl *c, int vk, int ch)
{
    (void)ch;
    if (vk == VK_SPACE || vk == VK_RETURN || vk == VK_F4 || (vk == VK_DOWN && GetKeyState(VK_MENU) < 0)) {
        combo_open(c);
        return 1;
    }
    if (c->type == WUI_T_TITLE) return 0;
    Items *it = c->ext;
    int i = (int)c->value;
    if (vk == VK_DOWN && i + 1 < it->count) i++;
    else if (vk == VK_UP && i > 0) i--;
    else return 0;
    combo_done(c, i);
    return 1;
}

static void combo_focus(WuiControl *c, int on) { (void)on; wui_invalidate(c); }

static const WuiClass combo_class = { combo_measure, 0, combo_paint, 0, combo_mouse, combo_key, combo_focus, 1, 1, 2 };

WuiControl *wui_combo(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_COMBO, &combo_class);
    c->ext = calloc(1, sizeof(Items));
    c->value = -1;
    if (parent && parent->type == WUI_T_ROW) c->align = WUI_ALIGN_CENTER;
    return c;
}

int wui_combo_add(WuiControl *c, const char *text)
{
    int i = items_add(c->ext, text);
    if (c->value < 0) c->value = 0;
    wui_relayout(c);
    return i;
}

void wui_combo_clear(WuiControl *c)
{
    items_clear(c->ext);
    c->value = -1;
    wui_relayout(c);
}

WuiControl *wui_theme_picker(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_TITLE, &combo_class);
    c->ext = calloc((size_t)wui_theme_total + 64, sizeof(int));
    c->fix_w = 230;
    c->align = WUI_ALIGN_CENTER;
    return c;
}

// ---- список (ListBox): строки с прокруткой ----

static float row_h(void) { WuiFont f = field_font(); return wui_line_height(&f) + 12; }

static void list_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    Items *it = c->ext;
    int rows = it->count < 3 ? 3 : it->count > 8 ? 8 : it->count;
    c->mw = 160;
    c->mh = rows * (row_h() + 2) + 10;
}

static void list_limits(WuiControl *c)
{
    Items *it = c->ext;
    float content = it->count * (row_h() + 2), view = c->r.h - 10;
    c->scroll_max = content - view > 0 ? content - view : 0;
    if (c->scroll > c->scroll_max) c->scroll = c->scroll_max;
    if (c->scroll < 0) c->scroll = 0;
}

// выбранная строка видна: прокрутка до неё
static void list_reveal(WuiControl *c)
{
    float h = row_h() + 2, view = c->r.h - 10;
    int i = (int)c->value;
    if (i < 0) return;
    if (i * h < c->scroll) c->scroll = i * h;
    if ((i + 1) * h > c->scroll + view) c->scroll = (i + 1) * h - view;
}

void wui_list_reveal(WuiControl *c)     // wui_value списка: строка видна, как только список разложен
{
    ((Items *)c->ext)->reveal = 1;
}

static void list_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    Items *it = c->ext;
    if (it->reveal) { list_reveal(c); it->reveal = 0; }
    list_limits(c);
    WuiCorners k = s->Field_CornerRadius;
    WuiEdges one = { 1, 1, 1, 1 };
    wui_fill(g, s->Grid_Background, c->r, k, 1);
    WuiRect in = wui_inset(c->r, 5, 5, 5, 5);
    UINT st;
    GdipSaveGraphics(g, &st);
    GdipSetClipRect(g, in.x, in.y, in.w, in.h, CombineModeIntersect);
    float h = row_h(), y = in.y + 1 - c->scroll, right = c->scroll_max > 0 ? 10 : 0;
    WuiFont f = field_font();
    int hot = c->hot - 1;
    for (int i = 0; i < it->count; i++, y += h + 2) {
        if (y + h < in.y || y > in.y + in.h) continue;
        WuiRect r = { in.x, y, in.w - right, h };
        WuiColor fore = wui_brush_color(s->Text);
        if (i == (int)c->value) {
            wui_fill(g, s->Item_Selected, r, s->Item_CornerRadius, 1);
            wui_stroke(g, s->Item_SelectedBorder, r, s->Item_CornerRadius, s->Item_SelectedBorderThickness, 1);
            fore = wui_brush_color(s->Item_SelectedForeground);
        }
        else if (i == hot) wui_fill(g, s->Item_Hover, r, s->Item_CornerRadius, 1);
        wui_draw_text(g, &f, it->items[i], wui_inset(r, 10, 0, 10, 0), fore, 0, 0);
    }
    GdipRestoreGraphics(g, st);
    if (c->scroll_max > 0) {
        float view = in.h, th = view * view / (view + c->scroll_max);
        if (th < 24) th = 24;
        WuiRect t = { in.x + in.w - 7, in.y + (view - th) * c->scroll / c->scroll_max, 6, th };
        wui_fill(g, s->Scroll_Thumb, t, wui_corners(3), 1);
    }
    wui_stroke(g, s->List_Border, c->r, k, one, 1);
}

static int list_row(WuiControl *c, float y)
{
    Items *it = c->ext;
    int i = (int)((y - c->r.y - 6 + c->scroll) / (row_h() + 2));
    return i >= 0 && i < it->count ? i : -1;
}

static void list_select(WuiControl *c, int i)
{
    if (i < 0 || i == (int)c->value) return;
    c->value = i;
    list_reveal(c);
    wui_invalidate(c);
    wui_fire_change(c);
}

static int list_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)x;
    switch (ev) {
    case WUI_MOUSE_DOWN:
        wui_window_focus(c->win, c);
        list_select(c, list_row(c, y));
        return 1;
    case WUI_MOUSE_DOUBLE:
        if (list_row(c, y) >= 0) wui_fire_click(c);
        return 1;
    case WUI_MOUSE_MOVE: {
        int h = list_row(c, y) + 1;
        if (h != c->hot) { c->hot = h; wui_invalidate(c); }
        return 1;
    }
    case WUI_MOUSE_LEAVE:
        if (c->hot) { c->hot = 0; wui_invalidate(c); }
        return 1;
    case WUI_MOUSE_WHEEL:
        list_limits(c);
        c->scroll -= extra / 120.0f * (row_h() + 2) * 3;
        list_limits(c);
        wui_invalidate(c);
        return 1;
    }
    return 1;
}

static int list_key(WuiControl *c, int vk, int ch)
{
    (void)ch;
    Items *it = c->ext;
    int i = (int)c->value;
    if (vk == VK_DOWN) i++;
    else if (vk == VK_UP) i--;
    else if (vk == VK_HOME) i = 0;
    else if (vk == VK_END) i = it->count - 1;
    else if (vk == VK_RETURN) { wui_fire_click(c); return 1; }
    else return 0;
    if (i < 0) i = 0;
    if (i >= it->count) i = it->count - 1;
    list_select(c, i);
    return 1;
}

static const WuiClass list_class = { list_measure, 0, list_paint, 0, list_mouse, list_key, combo_focus, 1, 1, 0 };

WuiControl *wui_list(WuiControl *parent)
{
    WuiControl *c = wui_new(parent, WUI_T_LIST, &list_class);
    c->ext = calloc(1, sizeof(Items));
    c->value = -1;
    return c;
}

int wui_list_add(WuiControl *c, const char *text)
{
    int i = items_add(c->ext, text);
    wui_relayout(c);
    return i;
}

void wui_list_clear(WuiControl *c)
{
    items_clear(c->ext);
    c->value = -1;
    c->scroll = 0;
    wui_relayout(c);
}

const char *wui_items_text(WuiControl *c)
{
    Items *it = c->ext;
    int i = (int)c->value;
    return i >= 0 && i < it->count ? it->items[i] : "";
}
