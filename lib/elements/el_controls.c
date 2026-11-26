// Ввод и взаимодействие Elements: попадание мышью, наведение, щелчки,
// текстовый редактор (edit/memo/password), выпадающий список, подсказки.
#include "el_internal.h"
#include <math.h>

static int P(int d) { return px(d); }
static int eff_font(const Element *e) { return e->fsize > 0 ? e->fsize : g.def_font; }

int is_container(int kind) {
    return kind == K_WINDOW || kind == K_STACK || kind == K_DOCK || kind == K_WRAP || kind == K_GRID ||
           kind == K_SCROLL || kind == K_CANVAS || kind == K_BORDER || kind == K_GROUP || kind == K_TABS ||
           kind == K_TAB || kind == K_TOOLBAR;
}
int is_focusable(int kind) { return kind == K_EDIT || kind == K_MEMO || kind == K_PASSWORD; }

static int hover_kind(int k) {
    return k == K_BUTTON || k == K_TOOL || k == K_CHECK || k == K_RADIO || k == K_LIST || k == K_TAB ||
           k == K_MENU || k == K_COMBO || k == K_NUMBER || k == K_SLIDER;
}

void set_focus(int h) {
    if (g.focus == h) return;
    g.focus = h;
    // курсор в конец при фокусе щелчком устанавливается отдельно
    request_redraw();
}

// --- кто получает действие по щелчку (вверх по дереву) ---
static int actionable(int h) {
    while (valid(h)) {
        Element *e = E(h);
        if (hover_kind(e->kind) || is_focusable(e->kind) || e->kind == K_IMAGE || e->kind == K_PAINT || e->on_click) return h;
        if (h == g.window) break;
        h = e->parent;
    }
    return 0;
}

// --- попадание ---
static int hit_rec(int h, int x, int y) {
    Element *e = E(h);
    int inside, found = 0, c;
    if (!e->alive || e->hidden) return 0;
    inside = (h == g.window) || (x >= e->ax && x < e->ax + e->aw && y >= e->ay && y < e->ay + e->ah);
    if (!inside) return 0;
    if (e->kind == K_TABS) {
        int idx = 0, sel = e->ival;
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) {
            Element *tb = E(c);
            if (x >= tb->hdr_x && x < tb->hdr_x + tb->hdr_w && y >= e->ay && y < e->ay + e->strip_h) return c;
        }
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) {
            if (idx == sel) { int r = hit_rec(c, x, y); if (r) found = r; }
            idx++;
        }
        return found ? found : h;
    }
    if (e->kind == K_MENUBAR) {
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_MENU) {
            Element *mi = E(c);
            if (x >= mi->ax && x < mi->ax + mi->aw && y >= mi->ay && y < mi->ay + mi->ah) return c;
        }
        return h;
    }
    for (c = e->first; c; c = E(c)->next)
        if (E(c)->alive && E(c)->kind != K_MENU && E(c)->kind != K_TAB) { int r = hit_rec(c, x, y); if (r) found = r; }
    return found ? found : h;
}
int hit_test(int root, int x, int y) { return hit_rec(root, x, y); }

// --- строка UTF-16, которую правит редактор ---
typedef struct { WCHAR *s; int n, cap; } WStr;

static void ws_load(WStr *w, const char *utf8) { w->s = wdup(utf8); w->n = wlen(w->s); w->cap = w->n + 1; }
static void ws_reserve(WStr *w, int need) {
    if (need + 1 > w->cap) {
        int c = w->cap * 2;
        WCHAR *p;
        if (c < need + 1) c = need + 1;
        p = (WCHAR *)realloc(w->s, (size_t)c * sizeof(WCHAR));
        if (!p) abort();
        w->s = p; w->cap = c;
    }
}
static void ws_insert(WStr *w, int at, const WCHAR *src, int cnt) {
    if (cnt <= 0) return;
    ws_reserve(w, w->n + cnt);
    memmove(w->s + at + cnt, w->s + at, (size_t)(w->n - at + 1) * sizeof(WCHAR));
    memcpy(w->s + at, src, (size_t)cnt * sizeof(WCHAR));
    w->n += cnt;
}
static void ws_erase(WStr *w, int at, int cnt) {
    if (cnt <= 0) return;
    memmove(w->s + at, w->s + at + cnt, (size_t)(w->n - at - cnt + 1) * sizeof(WCHAR));
    w->n -= cnt;
}

// ширина первых n символов строки
static int prefix_w(const WCHAR *s, int n, int size, int bold, int mono) {
    WCHAR stackbuf[256], *tmp = stackbuf;
    int r;
    if (n <= 0) return 0;
    if (n >= 256) { tmp = (WCHAR *)malloc((size_t)(n + 1) * sizeof(WCHAR)); if (!tmp) return 0; }
    memcpy(tmp, s, (size_t)n * sizeof(WCHAR));
    tmp[n] = 0;
    r = text_w(tmp, size, bold, mono);
    if (tmp != stackbuf) free(tmp);
    return r;
}

// строка для показа: в поле пароля — точки
static WCHAR *display_of(const Element *e, const WCHAR *w, int n) {
    WCHAR *d;
    int i;
    if (e->kind != K_PASSWORD) return NULL;
    d = (WCHAR *)malloc((size_t)(n + 1) * sizeof(WCHAR));
    if (!d) abort();
    for (i = 0; i < n; i++) d[i] = 0x2022;
    d[n] = 0;
    return d;
}

static int line_height(int fs, int mono) { int tw, lh; text_size(L"Ag", fs, 0, mono, 0, 0, &tw, &lh); return lh; }

// начало строки, в которой стоит позиция pos
static int line_start(const WCHAR *w, int pos) { while (pos > 0 && w[pos - 1] != L'\n') pos--; return pos; }
static int line_end(const WCHAR *w, int n, int pos) { while (pos < n && w[pos] != L'\n') pos++; return pos; }

int caret_from_x(int h, int mx, int my) {
    Element *e = E(h);
    int fs = eff_font(e), mono = e->mono, i, r;
    WStr w;
    ws_load(&w, e->buf);
    if (e->kind == K_MEMO) {
        int lh = line_height(fs, mono);
        int top = e->ay + P(e->pt) - e->scroll;
        int li = (my - top) / imax(1, lh), cur = 0, ls = 0, le, x0, col;
        if (li < 0) li = 0;
        // найти начало строки li (или последней)
        while (cur < li) {
            int nl = line_end(w.s, w.n, ls);
            if (nl >= w.n) break;
            ls = nl + 1; cur++;
        }
        le = line_end(w.s, w.n, ls);
        x0 = e->ax + P(e->pl);
        col = le - ls;
        for (i = 0; i <= le - ls; i++) if (x0 + prefix_w(w.s + ls, i, fs, 0, mono) >= mx) { col = i; break; }
        r = ls + col;
    } else {
        WCHAR *pw = display_of(e, w.s, w.n);
        const WCHAR *disp = pw ? pw : w.s;
        int x0 = e->ax + P(e->pl) - e->hscroll;
        r = w.n;
        for (i = 0; i <= w.n; i++) if (x0 + prefix_w(disp, i, fs, 0, mono) >= mx) { r = i; break; }
        free(pw);
    }
    free(w.s);
    return r;
}

// --- буфер обмена ---
static WCHAR *clip_get(void) {
    WCHAR *r = NULL;
    HANDLE hd;
    if (!OpenClipboard(g.hwnd)) return NULL;
    hd = GetClipboardData(CF_UNICODETEXT);
    if (hd) {
        const WCHAR *p = (const WCHAR *)GlobalLock(hd);
        if (p) {
            int n = wlen(p);
            r = (WCHAR *)malloc((size_t)(n + 1) * sizeof(WCHAR));
            if (r) memcpy(r, p, (size_t)(n + 1) * sizeof(WCHAR));
            GlobalUnlock(hd);
        }
    }
    CloseClipboard();
    return r;
}
static void clip_set(const WCHAR *s, int n) {
    HGLOBAL hd;
    if (!OpenClipboard(g.hwnd)) return;
    EmptyClipboard();
    hd = GlobalAlloc(GMEM_MOVEABLE, (size_t)(n + 1) * sizeof(WCHAR));
    if (hd) {
        WCHAR *p = (WCHAR *)GlobalLock(hd);
        if (p) { memcpy(p, s, (size_t)n * sizeof(WCHAR)); p[n] = 0; GlobalUnlock(hd); SetClipboardData(CF_UNICODETEXT, hd); }
        else GlobalFree(hd);
    }
    CloseClipboard();
}

static void buf_set(int h, const WCHAR *w) {
    Element *e = E(h);
    char *u;
    size_t n;
    if (!e->buf || e->cap <= 0) return;
    u = udup(w);
    n = strlen(u);
    if ((int)n >= e->cap) {
        n = (size_t)(e->cap - 1);
        // не рвать UTF-8 на конце
        while (n > 0 && ((unsigned char)u[n] & 0xC0) == 0x80) n--;
    }
    memcpy(e->buf, u, n);
    e->buf[n] = 0;
    free(u);
}

// --- редактирование клавишами ---
void edit_input(int h, const KeyEv *keys, int nk) {
    Element *e = E(h);
    WStr w;
    int changed = 0, i;
    ws_load(&w, e->buf);
    if (e->caret > w.n) e->caret = w.n;
    if (e->anchor > w.n) e->anchor = w.n;

#define HAS_SEL (e->caret != e->anchor)
#define SEL_LO imin(e->caret, e->anchor)
#define SEL_HI imax(e->caret, e->anchor)
#define DEL_SEL do { int a_ = SEL_LO, b_ = SEL_HI; ws_erase(&w, a_, b_ - a_); e->caret = e->anchor = a_; } while (0)

    for (i = 0; i < nk; i++) {
        const KeyEv *k = &keys[i];
        int shift = (k->mods & EL_SHIFT) != 0, ctrl = (k->mods & EL_CTRL) != 0;
        if (k->vk) {
            switch (k->vk) {
            case VK_LEFT:
                if (HAS_SEL && !shift) e->caret = SEL_LO; else if (e->caret > 0) e->caret--;
                if (!shift) e->anchor = e->caret;
                break;
            case VK_RIGHT:
                if (HAS_SEL && !shift) e->caret = SEL_HI; else if (e->caret < w.n) e->caret++;
                if (!shift) e->anchor = e->caret;
                break;
            case VK_HOME: e->caret = line_start(w.s, e->caret); if (!shift) e->anchor = e->caret; break;
            case VK_END: e->caret = line_end(w.s, w.n, e->caret); if (!shift) e->anchor = e->caret; break;
            case VK_UP: case VK_DOWN:
                if (e->kind == K_MEMO) {
                    int ls = line_start(w.s, e->caret), col = e->caret - ls;
                    if (k->vk == VK_UP) {
                        if (ls > 0) { int ps = line_start(w.s, ls - 1), pe = ls - 1; e->caret = imin(ps + col, pe); }
                    } else {
                        int ne = line_end(w.s, w.n, e->caret);
                        if (ne < w.n) { int ns = ne + 1, nend = line_end(w.s, w.n, ns); e->caret = imin(ns + col, nend); }
                    }
                    if (!shift) e->anchor = e->caret;
                }
                break;
            case VK_BACK:
                if (HAS_SEL) { DEL_SEL; changed = 1; }
                else if (e->caret > 0) { ws_erase(&w, e->caret - 1, 1); e->caret--; e->anchor = e->caret; changed = 1; }
                break;
            case VK_DELETE:
                if (HAS_SEL) { DEL_SEL; changed = 1; }
                else if (e->caret < w.n) { ws_erase(&w, e->caret, 1); changed = 1; }
                break;
            case VK_RETURN:
                if (e->kind == K_MEMO) {
                    WCHAR nl = L'\n';
                    if (HAS_SEL) DEL_SEL;
                    ws_insert(&w, e->caret, &nl, 1); e->caret++; e->anchor = e->caret; changed = 1;
                } else {
                    buf_set(h, w.s); free(w.s);
                    fire_click(h); fire_change(h);
                    return;
                }
                break;
            case 'A': if (ctrl) { e->anchor = 0; e->caret = w.n; } break;
            case 'C': if (ctrl && HAS_SEL) clip_set(w.s + SEL_LO, SEL_HI - SEL_LO); break;
            case 'X': if (ctrl && HAS_SEL) { clip_set(w.s + SEL_LO, SEL_HI - SEL_LO); DEL_SEL; changed = 1; } break;
            case 'V':
                if (ctrl) {
                    WCHAR *c = clip_get();
                    if (HAS_SEL) DEL_SEL;
                    if (c) {
                        int cn = wlen(c);
                        if (e->kind != K_MEMO) { int j; for (j = 0; j < cn; j++) if (c[j] == L'\n' || c[j] == L'\r') { cn = j; break; } }
                        ws_insert(&w, e->caret, c, cn); e->caret += cn; e->anchor = e->caret; changed = 1;
                        free(c);
                    }
                }
                break;
            case 'Z': /* отмена — упрощённо не ведём историю */ break;
            case VK_ESCAPE: free(w.s); set_focus(0); return;
            default: break;
            }
        } else if (k->ch >= 32 && k->ch != 127) {
            WCHAR ch = k->ch;
            if (HAS_SEL) DEL_SEL;
            ws_insert(&w, e->caret, &ch, 1); e->caret++; e->anchor = e->caret; changed = 1;
        }
    }
#undef HAS_SEL
#undef SEL_LO
#undef SEL_HI
#undef DEL_SEL
    if (e->caret < 0) e->caret = 0;
    if (e->caret > w.n) e->caret = w.n;
    buf_set(h, w.s);
    free(w.s);
    if (changed) fire_change(h);
    request_redraw();
}

// --- рисование поля ---
void edit_draw(int h) {
    Element *e = E(h);
    GpGraphics *gr = g.g;
    int fs = eff_font(e), mono = e->mono;
    GpRectF r = rectf((REAL)e->ax, (REAL)e->ay, (REAL)e->aw, (REAL)e->ah);
    u32 bg = e->has_bg ? e->bg : th(11);
    int focused = (g.focus == h);
    GpPath *p = gfx_rrect(r, (REAL)((e->radius >= 0) ? P(e->radius) : P(g.corner)));
    GpFont *f = get_font(fs, e->bold, mono);
    ARGB tc = argb(e->has_fg ? e->fg : th(6)), selc = argb(th(12)), cc = argb(th(7));
    GraphicsState st = 0;
    int caret_on = focused && ((int)(g.now_ms / 530) % 2 == 0);
    int selLo = imin(e->caret, e->anchor), selHi = imax(e->caret, e->anchor);
    GpStringFormat *sf = gfx_format(1, GP_ALIGN_NEAR, GP_ALIGN_NEAR, 0);
    WStr w;

    if (p) {
        gfx_fill_path(gr, argb(bg), p);
        gfx_draw_path(gr, argb(focused ? th(9) : th(5)), focused ? 1.6f : 1.0f, p);
        GdipDeletePath(p);
    }
    ws_load(&w, e->buf);
    GdipSaveGraphics(gr, &st);
    GdipSetClipRectI(gr, e->ax + P(2), e->ay, e->aw - P(4), e->ah, GP_COMBINE_INTERSECT);

    if (e->kind == K_MEMO) {
        int lh = line_height(fs, mono);
        int x0 = e->ax + P(e->pl), y, ls, cl = 0, caretY, viewH;
        // автопрокрутка к курсору
        for (ls = 0; ls < e->caret && ls < w.n; ls++) if (w.s[ls] == L'\n') cl++;
        caretY = cl * lh;
        viewH = e->ah - P(e->pt) - P(e->pb);
        if (caretY - e->scroll < 0) e->scroll = caretY;
        if (caretY + lh - e->scroll > viewH) e->scroll = caretY + lh - viewH;
        y = e->ay + P(e->pt) - e->scroll;
        ls = 0;
        for (;;) {
            int le = line_end(w.s, w.n, ls);
            if (y + lh > e->ay && y < e->ay + e->ah) {
                if (selHi > selLo && selHi > ls && selLo < le + 1) {
                    int a = imax(selLo, ls) - ls, b = imin(selHi, le) - ls;
                    if (b >= a) {
                        int xa = x0 + prefix_w(w.s + ls, a, fs, 0, mono);
                        int xb = x0 + prefix_w(w.s + ls, b, fs, 0, mono);
                        if (selHi > le) xb += P(5);
                        gfx_fill_rect(gr, selc, (REAL)xa, (REAL)y, (REAL)imax(2, xb - xa), (REAL)lh);
                    }
                }
                if (le > ls) {
                    WCHAR save = w.s[le];
                    w.s[le] = 0;
                    gfx_text(gr, w.s + ls, f, tc, rectf((REAL)x0, (REAL)y, 0, 0), sf);
                    w.s[le] = save;
                }
                if (caret_on && e->caret >= ls && e->caret <= le) {
                    int cx = x0 + prefix_w(w.s + ls, e->caret - ls, fs, 0, mono);
                    gfx_line(gr, cc, 1.0f, (REAL)cx, (REAL)(y + 1), (REAL)cx, (REAL)(y + lh - 1), 0);
                }
            }
            y += lh;
            if (le >= w.n) break;
            ls = le + 1;
        }
    } else {
        WCHAR *pw = display_of(e, w.s, w.n);
        const WCHAR *disp = pw ? pw : w.s;
        int tw, thh, fieldw, caretx, x0, y;
        text_size(disp, fs, e->bold, mono, 0, 0, &tw, &thh);
        fieldw = e->aw - P(e->pl) - P(e->pr);
        caretx = prefix_w(disp, imin(w.n, e->caret), fs, e->bold, mono);
        if (caretx - e->hscroll > fieldw) e->hscroll = caretx - fieldw;
        if (caretx - e->hscroll < 0) e->hscroll = caretx;
        if (e->hscroll < 0) e->hscroll = 0;
        x0 = e->ax + P(e->pl) - e->hscroll;
        y = e->ay + (e->ah - thh) / 2;
        if (selHi > selLo) {
            int xa = x0 + prefix_w(disp, selLo, fs, e->bold, mono);
            int xb = x0 + prefix_w(disp, selHi, fs, e->bold, mono);
            gfx_fill_rect(gr, selc, (REAL)xa, (REAL)(e->ay + P(3)), (REAL)imax(2, xb - xa), (REAL)(e->ah - P(6)));
        }
        gfx_text(gr, disp, f, tc, rectf((REAL)x0, (REAL)y, 0, 0), sf);
        if (caret_on) gfx_line(gr, cc, 1.0f, (REAL)(x0 + caretx), (REAL)(e->ay + P(4)), (REAL)(x0 + caretx), (REAL)(e->ay + e->ah - P(4)), 0);
        free(pw);
    }
    GdipRestoreGraphics(gr, st);
    if (sf) GdipDeleteStringFormat(sf);
    free(w.s);
    if (focused) request_redraw();   // мигание курсора
}

// --- выпадающий список combo ---
static void combo_rect(int h, int *x, int *y, int *w, int *hh, int *rowh) {
    Element *e = E(h);
    Element *win = E(g.window);
    int rh = line_height(eff_font(e), 0) + P(8);
    int vis = imin(e->count, 10);
    int below = win->ay + win->ah - (e->ay + e->ah);
    int ht = vis * rh + P(8);
    *x = e->ax; *w = e->aw; *rowh = rh;
    if (ht <= below) *y = e->ay + e->ah + P(2);
    else *y = e->ay - ht - P(2);
    *hh = ht;
}

void combo_frame_input(void) {
    int h = g.combo_open, x, y, w, ht, rh, down, inside, maxv = 10, top, i;
    Element *e;
    if (!valid(h)) { g.combo_open = 0; return; }
    e = E(h);
    combo_rect(h, &x, &y, &w, &ht, &rh);
    down = (g.buttons & 1) && !(g.pbuttons & 1);
    inside = g.mx >= x && g.mx < x + w && g.my >= y && g.my < y + ht;
    top = g.combo_scroll;
    g.combo_hover = -1;
    if (inside) { int idx = top + (g.my - (y + P(4))) / rh; if (idx >= 0 && idx < e->count) g.combo_hover = idx; }
    if (g.wheel) {
        int maxs = imax(0, e->count - maxv);
        g.combo_scroll -= g.wheel;
        if (g.combo_scroll < 0) g.combo_scroll = 0;
        if (g.combo_scroll > maxs) g.combo_scroll = maxs;
        g.wheel = 0; request_redraw();
    }
    if (down) {
        if (inside && g.combo_hover >= 0) { if (e->iv) *e->iv = g.combo_hover; g.combo_open = 0; fire_change(h); }
        else g.combo_open = 0;
        request_redraw();
    }
    for (i = 0; i < g.keys.n; i++) if (g.keys.a[i].vk == VK_ESCAPE) g.combo_open = 0;
    request_redraw();
}

void combo_draw(void) {
    int h = g.combo_open, x, y, w, ht, rh, fs, top, vis, yy, i;
    Element *e;
    GpGraphics *gr = g.g;
    GpPath *p;
    GpStringFormat *sf;
    GpFont *f;
    if (!valid(h)) return;
    e = E(h);
    combo_rect(h, &x, &y, &w, &ht, &rh);
    p = gfx_rrect(rectf((REAL)x, (REAL)y, (REAL)w, (REAL)ht), (REAL)P(6));
    if (p) { gfx_fill_path(gr, argb(th(13)), p); gfx_draw_path(gr, argb(th(5)), 1.0f, p); GdipDeletePath(p); }
    fs = eff_font(e);
    f = get_font(fs, 0, 0);
    sf = gfx_format(1, GP_ALIGN_NEAR, GP_ALIGN_CENTER, 1);
    top = g.combo_scroll; vis = imin(e->count - top, 10);
    yy = y + P(4);
    for (i = 0; i < vis; i++) {
        int idx = top + i;
        WCHAR *wt;
        if (idx == g.combo_hover) gfx_fill_rect(gr, argb(th(3)), (REAL)(x + P(3)), (REAL)yy, (REAL)(w - P(6)), (REAL)rh);
        if (e->iv && idx == *e->iv) gfx_fill_rect(gr, argba(th(9), 60), (REAL)(x + P(3)), (REAL)yy, (REAL)(w - P(6)), (REAL)rh);
        wt = wdup(e->items[idx]);
        gfx_text(gr, wt, f, argb(th(6)), rectf((REAL)(x + P(10)), (REAL)yy, (REAL)(w - P(16)), (REAL)rh), sf);
        free(wt);
        yy += rh;
    }
    if (sf) GdipDeleteStringFormat(sf);
}

// --- подсказка ---
void draw_tooltip(void) {
    Element *e;
    GpGraphics *gr = g.g;
    WCHAR *t;
    int tw, thh, pad, bw, bh, x, y, cw, ch;
    GpPath *p;
    if (!g.tip_show || !valid(g.tip_el)) return;
    e = E(g.tip_el);
    if (!e->tip) return;
    t = wdup(e->tip);
    text_size(t, g.def_font - 1, 0, 0, 0, 0, &tw, &thh);
    pad = P(7);
    bw = tw + pad * 2; bh = thh + pad;
    x = g.mx + P(14); y = g.my + P(18);
    cw = E(g.window)->aw; ch = E(g.window)->ah;
    if (x + bw > cw) x = cw - bw - P(4);
    if (y + bh > ch) y = g.my - bh - P(6);
    p = gfx_rrect(rectf((REAL)x, (REAL)y, (REAL)bw, (REAL)bh), (REAL)P(5));
    if (p) { gfx_fill_path(gr, argb(g.dark ? 0x3a3a3d : 0x2b2b2b), p); gfx_draw_path(gr, argb(th(5)), 1.0f, p); GdipDeletePath(p); }
    gfx_text_at(gr, t, get_font(g.def_font - 1, 0, 0), argb(0xf0f0f0), (REAL)(x + pad), (REAL)(y + pad / 2));
    free(t);
}

// --- открыть контекстное меню, если есть ---
static int find_ctx(int h) {
    while (valid(h)) { if (E(h)->ctx) return E(h)->ctx; if (h == g.window) break; h = E(h)->parent; }
    return 0;
}

static void slider_drag(int a) {
    Element *e = E(a);
    int tl = e->ax + P(8), tr = e->ax + e->aw - P(8), nv;
    double f = tr > tl ? (double)(g.mx - tl) / (tr - tl) : 0;
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    nv = e->vmin + (int)(f * (e->vmax - e->vmin) + 0.5);
    if (e->iv && *e->iv != nv) { *e->iv = nv; fire_change(a); }
}

// --- главный разбор ввода кадра ---
void input_frame(void) {
    int root = valid(g.modal) ? g.modal : g.window;
    int hot = hit_test(root, g.mx, g.my);
    int hv_tgt, down, up, rdown, i;
    g.hot = hot;
    hv_tgt = actionable(hot);

    // наведение (плавно)
    for (i = 1; i < g.el.n; i++) {
        Element *e = E(i);
        double tgt, d;
        if (!e->alive) continue;
        tgt = (i == hv_tgt && hover_kind(e->kind)) ? 1.0 : 0.0;
        d = tgt - e->hv;
        if (fabs(d) > 0.02) { e->hv += d * 0.3; g.dirty = 1; } else e->hv = tgt;
    }

    // подсказка
    if (hot != g.tip_el) { g.tip_el = hot; g.tip_since = g.now_ms; g.tip_show = 0; }
    else if (!g.tip_show && valid(hot) && E(hot)->tip && g.now_ms - g.tip_since > 600) { g.tip_show = 1; g.dirty = 1; }
    if (g.mx != g.pmx || g.my != g.pmy) { g.tip_since = g.now_ms; if (g.tip_show) { g.tip_show = 0; g.dirty = 1; } }

    down = (g.buttons & 1) && !(g.pbuttons & 1);
    up = !(g.buttons & 1) && (g.pbuttons & 1);
    rdown = (g.buttons & 2) && !(g.pbuttons & 2);

    // колесо — прокрутка ближайшего списка/скролла/memo
    if (g.wheel) {
        int s = hot;
        while (valid(s)) {
            int k = E(s)->kind;
            if (k == K_SCROLL || k == K_LIST || k == K_MEMO) break;
            if (s == g.window) { s = 0; break; }
            s = E(s)->parent;
        }
        if (valid(s)) { E(s)->scroll -= g.wheel * P(40); if (E(s)->scroll < 0) E(s)->scroll = 0; g.dirty = 1; }
        g.wheel = 0;
    }

    if (rdown) {
        int m = find_ctx(hot);
        if (m) {
            Popup pu;
            pu.menu = m; pu.x = g.mx; pu.y = g.my; pu.hover = 0;
            menu_close_all();
            VPUSH(g.popups, pu);
            request_redraw();
            return;
        }
    }

    if (down) {
        int a = actionable(hot);
        g.active = a; g.press_x = g.mx; g.press_y = g.my;
        if (valid(a)) {
            Element *e = E(a);
            switch (e->kind) {
            case K_EDIT: case K_MEMO: case K_PASSWORD: {
                int c;
                set_focus(a);
                c = caret_from_x(a, g.mx, g.my);
                E(a)->caret = c; E(a)->anchor = c;
                break; }
            case K_SLIDER: slider_drag(a); set_focus(0); break;
            case K_NUMBER:
                if (g.mx > e->ax + e->aw - P(28)) {
                    int upper = g.my < e->ay + e->ah / 2;
                    int nv = (e->iv ? *e->iv : 0) + (upper ? e->step : -e->step);
                    if (nv < e->vmin) nv = e->vmin;
                    if (nv > e->vmax) nv = e->vmax;
                    if (e->iv) *e->iv = nv;
                    fire_change(a);
                }
                break;
            case K_COMBO: g.combo_open = a; g.combo_scroll = 0; g.combo_hover = e->iv ? *e->iv : 0; break;
            case K_LIST: {
                int rh = line_height(eff_font(e), 0) + P(8);
                int idx = (g.my - (e->ay + P(e->pt)) + e->scroll) / rh;
                if (idx >= 0 && idx < e->count) {
                    int prev = e->iv ? *e->iv : -1;
                    if (e->iv) *e->iv = idx;
                    if (idx == prev) fire_click(a); else fire_change(a);
                }
                set_focus(0);
                break; }
            case K_TAB: {
                int tabs = e->parent, idx = 0, c;
                for (c = E(tabs)->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) { if (c == a) break; idx++; }
                if (E(tabs)->ival != idx) { E(tabs)->ival = idx; fire_change(tabs); }
                break; }
            case K_MENU:
                if (e->parent == g.window || E(e->parent)->kind == K_MENUBAR) menu_open_bar(a);
                break;
            default: if (!is_focusable(e->kind)) set_focus(0); break;
            }
        } else set_focus(0);
    }

    // перетаскивание
    if ((g.buttons & 1) && valid(g.active)) {
        int k = E(g.active)->kind;
        if (k == K_SLIDER) slider_drag(g.active);
        else if (is_focusable(k)) { int c = caret_from_x(g.active, g.mx, g.my); E(g.active)->caret = c; g.dirty = 1; }
    }

    if (up) {
        int a = g.active;
        if (valid(a) && actionable(hit_test(g.window, g.mx, g.my)) == a) {
            Element *e = E(a);
            switch (e->kind) {
            case K_BUTTON: case K_TOOL: fire_click(a); break;
            case K_CHECK: if (e->iv) *e->iv = !*e->iv; fire_change(a); fire_click(a); break;
            case K_RADIO: if (e->iv && *e->iv != e->ival) { *e->iv = e->ival; fire_change(a); } fire_click(a); break;
            case K_TEXT: case K_IMAGE: case K_BORDER: case K_STACK: case K_PAINT: if (e->on_click) fire_click(a); break;
            default: break;
            }
        }
        g.active = 0;
    }

    // горячие клавиши окна
    for (i = 0; i < g.keys.n; i++) {
        KeyEv k = g.keys.a[i];
        int j;
        if (!k.vk) continue;
        if (valid(g.focus) && is_focusable(E(g.focus)->kind)) continue;
        for (j = 0; j < g.hotkeys.n; j++) {
            Hotkey hk = g.hotkeys.a[j];
            if (hk.vk == k.vk && hk.mods == (k.mods & (EL_SHIFT | EL_CTRL | EL_ALT))) { if (hk.fn) hk.fn(0, hk.arg); request_redraw(); }
        }
    }

    // клавиши в поле ввода
    if (valid(g.focus) && is_focusable(E(g.focus)->kind) && g.keys.n) edit_input(g.focus, g.keys.a, g.keys.n);
}
