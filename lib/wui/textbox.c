// WUI — поле ввода в одну строку (и поле пароля): курсор, выделение мышью и клавишами,
// слово двойным щелчком, Ctrl+A/C/X/V/Z, подсказка в пустом поле. Вид — шаблон TextBox ThemeLibrary.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    wchar_t *buf;
    int len, cap;
    int caret, anchor;          // выделено между anchor и caret
    float shift;                // сдвиг текста влево, когда он длиннее поля
    int password;
    char *hint;
    char *utf8;                 // для wui_get_text
    wchar_t *undo;              // одна отмена
    int undo_len, undo_caret;
} Edit;

static Edit *E(WuiControl *c) { return c->ext; }

static WuiFont field_font(void) { return wui_font(WUI_S->Field_FontFamily, WUI_S->Field_FontSize, 400); }

// показанный текст: у пароля — точки
static const wchar_t *shown(Edit *e)
{
    static wchar_t *dots;
    static int cap;
    if (!e->password) return e->buf;
    if (cap < e->len + 1) {
        free(dots);
        cap = e->len + 64;
        dots = malloc(sizeof(wchar_t) * (size_t)cap);
    }
    for (int i = 0; i < e->len; i++) dots[i] = 0x25CF;
    dots[e->len] = 0;
    return dots;
}

static WuiRect text_area(WuiControl *c)
{
    WuiRect r = wui_deflate(c->r, WUI_S->Field_BorderThickness);
    r = wui_deflate(r, WUI_S->Field_Padding);
    r.x += 2;
    r.w -= 2;
    return r;
}

static float x_of(WuiControl *c, int pos)
{
    WuiFont f = field_font();
    return wui_wtext_width(&f, shown(E(c)), pos);
}

static int pos_at(WuiControl *c, float x)
{
    Edit *e = E(c);
    WuiRect a = text_area(c);
    float want = x - a.x + e->shift, prev = 0;
    for (int i = 1; i <= e->len; i++) {
        float w = x_of(c, i);
        if (want < (prev + w) / 2) return i - 1;
        prev = w;
    }
    return e->len;
}

// курсор всегда виден: текст сдвигается
static void keep_caret(WuiControl *c)
{
    Edit *e = E(c);
    WuiRect a = text_area(c);
    if (a.w <= 4) {             // not laid out yet (wui_focus before the window is shown): from the start
        e->shift = 0;
        return;
    }
    float x = x_of(c, e->caret), total = x_of(c, e->len);
    if (x - e->shift > a.w - 2) e->shift = x - a.w + 2;
    if (x - e->shift < 0) e->shift = x;
    if (total - e->shift < a.w - 2) e->shift = total - a.w + 2;
    if (e->shift < 0) e->shift = 0;
}

static void remember(Edit *e)
{
    free(e->undo);
    e->undo = malloc(sizeof(wchar_t) * (size_t)(e->len + 1));
    memcpy(e->undo, e->buf, sizeof(wchar_t) * (size_t)(e->len + 1));
    e->undo_len = e->len;
    e->undo_caret = e->caret;
}

static void replace(WuiControl *c, int from, int to, const wchar_t *s, int n)
{
    Edit *e = E(c);
    if (e->len - (to - from) + n + 1 > e->cap) {
        e->cap = e->len - (to - from) + n + 64;
        e->buf = realloc(e->buf, sizeof(wchar_t) * (size_t)e->cap);
    }
    memmove(e->buf + from + n, e->buf + to, sizeof(wchar_t) * (size_t)(e->len - to + 1));
    memcpy(e->buf + from, s, sizeof(wchar_t) * (size_t)n);
    e->len += n - (to - from);
    e->caret = e->anchor = from + n;
}

static void typed(WuiControl *c, const wchar_t *s, int n)
{
    Edit *e = E(c);
    remember(e);
    int a = e->anchor < e->caret ? e->anchor : e->caret, b = e->anchor < e->caret ? e->caret : e->anchor;
    replace(c, a, b, s, n);
    keep_caret(c);
    wui_invalidate(c);
    wui_fire_change(c);
}

static int is_word(wchar_t ch) { return IsCharAlphaNumericW(ch) || ch == '_'; }

static int word_left(Edit *e, int p)
{
    while (p > 0 && !is_word(e->buf[p - 1])) p--;
    while (p > 0 && is_word(e->buf[p - 1])) p--;
    return p;
}

static int word_right(Edit *e, int p)
{
    while (p < e->len && is_word(e->buf[p])) p++;
    while (p < e->len && !is_word(e->buf[p])) p++;
    return p;
}

// ---- буфер обмена ----

static void copy(WuiControl *c)
{
    Edit *e = E(c);
    int a = e->anchor < e->caret ? e->anchor : e->caret, b = e->anchor < e->caret ? e->caret : e->anchor;
    if (a == b || e->password || !OpenClipboard(c->win->hwnd)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (size_t)(b - a + 1));
    wchar_t *p = GlobalLock(h);
    memcpy(p, e->buf + a, sizeof(wchar_t) * (size_t)(b - a));
    p[b - a] = 0;
    GlobalUnlock(h);
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
}

static void paste(WuiControl *c)
{
    if (!OpenClipboard(c->win->hwnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        wchar_t *p = GlobalLock(h);
        if (p) {
            int n = 0;                      // одна строка: до перевода строки
            while (p[n] && p[n] != '\r' && p[n] != '\n') n++;
            typed(c, p, n);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}

// ---- вид ----

static void edit_measure(WuiControl *c, float aw, float ah)
{
    (void)aw; (void)ah;
    WuiFont f = field_font();
    WuiEdges p = WUI_S->Field_Padding, b = WUI_S->Field_BorderThickness;
    c->mw = 160;
    c->mh = wui_line_height(&f) + p.top + p.bottom + b.top + b.bottom;
}

static void edit_paint(WuiControl *c, GpGraphics *g)
{
    const WuiStyle *s = WUI_S;
    Edit *e = E(c);
    WuiCorners k = s->Field_CornerRadius;
    if (c->focused) wui_effect(g, s->Field_FocusEffect, s->Field_Background ? s->Field_Background : s->Field_BorderFocus, c->r, k, 1);
    wui_fill(g, s->Field_Background, c->r, k, 1);
    wui_stroke(g, c->focused || c->hot ? s->Field_BorderFocus : s->Field_Border, c->r, k, s->Field_BorderThickness, 1);

    WuiRect a = text_area(c);
    WuiFont f = field_font();
    float lh = wui_line_height(&f), y = a.y + (a.h - lh) / 2;
    if (!e->len) {
        if (e->hint && *e->hint) {
            WuiColor dim = wui_brush_color(s->TextDim);
            wui_draw_text(g, &f, e->hint, a, wui_alpha(dim, (int)(WUI_A(dim) * 0.8f)), 0, 0);
        }
    }
    UINT st;
    GdipSaveGraphics(g, &st);
    GdipSetClipRect(g, a.x - 1, c->r.y, a.w + 2, c->r.h, CombineModeIntersect);
    int sa = e->anchor < e->caret ? e->anchor : e->caret, sb = e->anchor < e->caret ? e->caret : e->anchor;
    if (sa != sb && c->focused) {
        float x1 = a.x + x_of(c, sa) - e->shift, x2 = a.x + x_of(c, sb) - e->shift;
        WuiColor sel = wui_brush_color(s->Field_Selection);
        WuiRect r = { x1, y, x2 - x1, lh };
        wui_fill_color(g, wui_alpha(sel, (int)(WUI_A(sel) * 0.6f)), r, 0);
    }
    wui_draw_wtext(g, &f, shown(e), e->len, a.x - e->shift, y, wui_brush_color(s->Field_Foreground));
    if (c->focused && wui_caret_on && sa == sb) {
        float x = a.x + x_of(c, e->caret) - e->shift;
        WuiRect r = { (float)(int)(x * wui_paint_scale) / wui_paint_scale, y + 1, 1, lh - 2 };
        wui_fill_color(g, wui_brush_color(s->Field_Foreground), r, 0);
    }
    GdipRestoreGraphics(g, st);
}

// ---- мышь и клавиши ----

static int edit_mouse(WuiControl *c, int ev, float x, float y, int extra)
{
    (void)y;
    Edit *e = E(c);
    switch (ev) {
    case WUI_MOUSE_DOWN:
        wui_window_focus(c->win, c);
        e->caret = pos_at(c, x);
        if (!(extra & MK_SHIFT)) e->anchor = e->caret;
        c->pressed = 1;
        wui_window_capture(c->win, c);
        wui_caret_restart();
        wui_invalidate(c);
        return 1;
    case WUI_MOUSE_DOUBLE: {
        int p = pos_at(c, x), a = p, b = p;
        while (a > 0 && is_word(e->buf[a - 1])) a--;
        while (b < e->len && is_word(e->buf[b])) b++;
        e->anchor = a;
        e->caret = b;
        wui_invalidate(c);
        return 1;
    }
    case WUI_MOUSE_MOVE:
        wui_window_set_cursor(32513);               // IDC_IBEAM
        if (c->pressed) {
            int p = pos_at(c, x);
            if (p != e->caret) { e->caret = p; keep_caret(c); wui_invalidate(c); }
        }
        else if (!c->hot) { c->hot = 1; wui_invalidate(c); }
        return 1;
    case WUI_MOUSE_UP:
        c->pressed = 0;
        wui_window_capture(c->win, 0);
        return 1;
    case WUI_MOUSE_LEAVE:
        if (c->hot) { c->hot = 0; wui_invalidate(c); }
        return 1;
    }
    return 0;
}

static int edit_key(WuiControl *c, int vk, int ch)
{
    Edit *e = E(c);
    int shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
    if (ch) {
        if (ch < 32 || ch == 127) return 0;     // управляющие — через vk
        wchar_t w = (wchar_t)ch;
        typed(c, &w, 1);
        wui_caret_restart();
        return 1;
    }
    int p = e->caret;
    int sa = e->anchor < e->caret ? e->anchor : e->caret, sb = e->anchor < e->caret ? e->caret : e->anchor;
    switch (vk) {
    case VK_LEFT: p = ctrl ? word_left(e, p) : (sa != sb && !shift ? sa : p > 0 ? p - 1 : 0); break;
    case VK_RIGHT: p = ctrl ? word_right(e, p) : (sa != sb && !shift ? sb : p < e->len ? p + 1 : p); break;
    case VK_HOME: p = 0; break;
    case VK_END: p = e->len; break;
    case VK_BACK:
        if (sa == sb && sa > 0) e->anchor = ctrl ? word_left(e, sa) : sa - 1;
        if (e->anchor != e->caret) typed(c, L"", 0);
        wui_caret_restart();
        return 1;
    case VK_DELETE:
        if (sa == sb && sb < e->len) e->anchor = ctrl ? word_right(e, sb) : sb + 1;
        if (e->anchor != e->caret) typed(c, L"", 0);
        wui_caret_restart();
        return 1;
    case VK_RETURN:
        wui_fire_click(c);
        return 1;
    default:
        if (!ctrl) return 0;
        if (vk == 'A') { e->anchor = 0; e->caret = e->len; wui_invalidate(c); return 1; }
        if (vk == 'C' || vk == VK_INSERT) { copy(c); return 1; }
        if (vk == 'X') { copy(c); if (sa != sb && !e->password) typed(c, L"", 0); return 1; }
        if (vk == 'V') { paste(c); return 1; }
        if (vk == 'Z' && e->undo) {
            wchar_t *u = e->undo;
            int n = e->undo_len, at = e->undo_caret;
            e->undo = 0;
            e->anchor = 0;
            e->caret = e->len;
            replace(c, 0, e->len, u, n);
            e->caret = e->anchor = at <= e->len ? at : e->len;
            free(u);
            keep_caret(c);
            wui_invalidate(c);
            wui_fire_change(c);
            return 1;
        }
        return 0;
    }
    e->caret = p;
    if (!shift) e->anchor = p;
    keep_caret(c);
    wui_caret_restart();
    wui_invalidate(c);
    return 1;
}

static void edit_focus(WuiControl *c, int on)
{
    Edit *e = E(c);
    if (on && !c->pressed) { e->anchor = 0; e->caret = e->len; keep_caret(c); }   // вошли клавишей — всё выделено
    wui_invalidate(c);
}

static const WuiClass edit_class = { edit_measure, 0, edit_paint, 0, edit_mouse, edit_key, edit_focus, 1, 1, 14 };

static WuiControl *make(WuiControl *parent, const char *hint, int password)
{
    WuiControl *c = wui_new(parent, WUI_T_TEXTBOX, &edit_class);
    Edit *e = calloc(1, sizeof(Edit));
    e->cap = 64;
    e->buf = calloc((size_t)e->cap, sizeof(wchar_t));
    e->hint = wui_strdup(hint);
    e->password = password;
    c->ext = e;
    return c;
}

WuiControl *wui_textbox(WuiControl *parent, const char *placeholder) { return make(parent, placeholder, 0); }
WuiControl *wui_password(WuiControl *parent, const char *placeholder) { return make(parent, placeholder, 1); }

void wui_textbox_set(WuiControl *c, const char *text)
{
    Edit *e = E(c);
    wchar_t *w = wui_wide(text);
    e->anchor = 0;
    e->caret = e->len;
    replace(c, 0, e->len, w, (int)wcslen(w));
    e->caret = e->anchor = e->len;
    e->shift = 0;
    free(w);
    wui_invalidate(c);
}

const char *wui_textbox_get(WuiControl *c)
{
    // строка живёт до правки поля: пока текст тот же, отдаётся прежняя
    Edit *e = E(c);
    char *now = wui_utf8(e->buf, e->len);
    if (e->utf8 && strcmp(e->utf8, now) == 0) {
        free(now);
        return e->utf8;
    }
    free(e->utf8);
    e->utf8 = now;
    return e->utf8;
}
