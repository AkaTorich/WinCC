// Готовые окна Elements: сообщение, вопрос, уведомление-тост, выбор файла и
// папки (родные диалоги Windows), своё модальное окно.
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include "el_internal.h"

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

static int P(int d) { return px(d); }
static double dmin(double a, double b) { return a < b ? a : b; }

// ------------------------------------------------- сообщение / вопрос ------

void el_message(const char *title, const char *text) {
    g.msg_kind = 1; sset(&g.msg_title, title); sset(&g.msg_text, text);
    g.ask_fn = NULL;
    request_redraw();
}
void el_ask(const char *title, const char *text, void (*fn)(int, void *), void *arg) {
    g.msg_kind = 2; sset(&g.msg_title, title); sset(&g.msg_text, text);
    g.ask_fn = fn; g.ask_arg = arg;
    request_redraw();
}

static void set_rect(RECT *r, int l, int t, int w, int h) { r->left = l; r->top = t; r->right = l + w; r->bottom = t + h; }

// геометрия окна сообщения + кнопок
static void msg_layout(int *x, int *y, int *w, int *h, RECT *b1, RECT *b2) {
    int tw, thh, titw, tith, cw, ch, winW, winH, cx, cy, bw, bh, by;
    text_size_u(S(g.msg_text), g.def_font, 0, 0, 1, 360, &tw, &thh);
    text_size_u(S(g.msg_title), g.def_font + 3, 1, 0, 0, 0, &titw, &tith);
    cw = imax(imax(tw, titw) + P(44), P(300));
    ch = tith + P(14) + thh + P(24) + P(40);
    winW = E(g.window)->aw; winH = E(g.window)->ah;
    cx = E(g.window)->ax + (winW - cw) / 2; cy = E(g.window)->ay + (winH - ch) / 2;
    *x = cx; *y = cy; *w = cw; *h = ch;
    bw = P(96); bh = P(34); by = cy + ch - P(18) - bh;
    set_rect(b1, cx + cw - P(18) - bw, by, bw, bh);                          // Yes / OK
    if (g.msg_kind == 2) set_rect(b2, b1->left - P(10) - bw, by, bw, bh);   // No
    else set_rect(b2, 0, 0, 0, 0);
}

static int in_rect(const RECT *r) { return g.mx >= r->left && g.mx < r->right && g.my >= r->top && g.my < r->bottom; }

void overlay_frame_input(void) {
    int x, y, w, h, down, i;
    RECT b1, b2;
    if (!g.msg_kind) return;
    msg_layout(&x, &y, &w, &h, &b1, &b2);
    down = (g.buttons & 1) && !(g.pbuttons & 1);
    for (i = 0; i < g.keys.n; i++) {
        int vk = g.keys.a[i].vk;
        if (vk == VK_ESCAPE) { int kind = g.msg_kind; g.msg_kind = 0; if (kind == 2 && g.ask_fn) g.ask_fn(0, g.ask_arg); return; }
        if (vk == VK_RETURN) { int kind = g.msg_kind; g.msg_kind = 0; if (kind == 2 && g.ask_fn) g.ask_fn(1, g.ask_arg); return; }
    }
    if (down) {
        if (in_rect(&b1)) { int kind = g.msg_kind; g.msg_kind = 0; if (kind == 2 && g.ask_fn) g.ask_fn(1, g.ask_arg); }
        else if (g.msg_kind == 2 && in_rect(&b2)) { g.msg_kind = 0; if (g.ask_fn) g.ask_fn(0, g.ask_arg); }
    }
    request_redraw();
}

static void draw_button(GpGraphics *gr, RECT r, const WCHAR *label, int primary) {
    GpRectF rr = rectf((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
    int hot = in_rect(&r);
    u32 bg = primary ? th(9) : th(2);
    GpPath *p = gfx_rrect(rr, (REAL)P(6));
    GpStringFormat *sf = gfx_format(1, GP_ALIGN_CENTER, GP_ALIGN_CENTER, 0);
    if (hot) bg = mix(bg, g.dark ? 0xffffff : 0x000000, 0.1);
    if (p) {
        gfx_fill_path(gr, argb(bg), p);
        if (!primary) gfx_draw_path(gr, argb(th(5)), 1.0f, p);
        GdipDeletePath(p);
    }
    gfx_text(gr, label, get_font(g.def_font, 0, 0), argb(primary ? th(10) : th(6)), rr, sf);
    if (sf) GdipDeleteStringFormat(sf);
}

void overlay_draw(void) {
    GpGraphics *gr = g.g;
    Element *win = E(g.window);
    // модальное окно программы
    if (valid(g.modal)) {
        gfx_fill_rect(gr, argba(0x000000, 120), (REAL)win->ax, (REAL)win->ay, (REAL)win->aw, (REAL)win->ah);
        draw_element(g.modal);
        win = E(g.window);
    }
    // сообщение / вопрос
    if (g.msg_kind) {
        int x, y, w, h, i, titw, tith;
        RECT b1, b2;
        GpRectF r;
        GpPath *p;
        WCHAR *wt;
        GpStringFormat *wf;
        gfx_fill_rect(gr, argba(0x000000, 120), (REAL)win->ax, (REAL)win->ay, (REAL)win->aw, (REAL)win->ah);
        msg_layout(&x, &y, &w, &h, &b1, &b2);
        r = rectf((REAL)x, (REAL)y, (REAL)w, (REAL)h);
        for (i = 6; i >= 1; i--) {
            p = gfx_rrect(rectf(r.X - i, r.Y - i + 4, r.Width + 2 * i, r.Height + 2 * i), (REAL)P(12));
            if (p) { gfx_fill_path(gr, argba(0x000000, 12), p); GdipDeletePath(p); }
        }
        p = gfx_rrect(r, (REAL)P(10));
        if (p) { gfx_fill_path(gr, argb(th(1)), p); gfx_draw_path(gr, argb(th(5)), 1.0f, p); GdipDeletePath(p); }
        text_size_u(S(g.msg_title), g.def_font + 3, 1, 0, 0, 0, &titw, &tith);
        wt = wdup(g.msg_title);
        gfx_text_at(gr, wt, get_font(g.def_font + 3, 1, 0), argb(th(7)), (REAL)(x + P(22)), (REAL)(y + P(18)));
        free(wt);
        wt = wdup(g.msg_text);
        wf = gfx_format(0, GP_ALIGN_NEAR, GP_ALIGN_NEAR, 0);
        gfx_text(gr, wt, get_font(g.def_font, 0, 0), argb(th(6)),
                 rectf((REAL)(x + P(22)), (REAL)(y + P(18) + tith + P(10)), (REAL)(w - P(44)), (REAL)(h - tith - P(90))), wf);
        if (wf) GdipDeleteStringFormat(wf);
        free(wt);
        if (g.msg_kind == 2) { draw_button(gr, b1, L"Yes", 1); draw_button(gr, b2, L"No", 0); }
        else draw_button(gr, b1, L"OK", 1);
    }
    // тост
    if (g.toast_until > g.now_ms && g.toast_text) {
        double age = g.now_ms - g.toast_t0;
        double appear = dmin(1.0, age / 200.0);
        double fade = dmin(1.0, (g.toast_until - g.now_ms) / 300.0);
        double a = dmin(appear, fade);
        int tw, thh, bw, bh, cx, cy;
        GpRectF r;
        GpPath *p;
        WCHAR *wt;
        GpStringFormat *sf;
        text_size_u(g.toast_text, g.def_font, 0, 0, 0, 0, &tw, &thh);
        bw = tw + P(28); bh = thh + P(16);
        cx = win->ax + (win->aw - bw) / 2;
        cy = win->ay + win->ah - bh - P(24) - (int)((1 - appear) * P(20));
        r = rectf((REAL)cx, (REAL)cy, (REAL)bw, (REAL)bh);
        p = gfx_rrect(r, (REAL)(bh / 2.0f));
        if (p) { gfx_fill_path(gr, argba(g.dark ? 0x202024 : 0x303030, (int)(230 * a)), p); GdipDeletePath(p); }
        wt = wdup(g.toast_text);
        sf = gfx_format(1, GP_ALIGN_CENTER, GP_ALIGN_CENTER, 0);
        gfx_text(gr, wt, get_font(g.def_font, 0, 0), argba(0xffffff, (int)(255 * a)), r, sf);
        if (sf) GdipDeleteStringFormat(sf);
        free(wt);
        request_redraw();
    }
}

void el_toast(const char *text, int ms) {
    sset(&g.toast_text, text); g.toast_t0 = g.now_ms; g.toast_until = g.now_ms + (ms <= 0 ? 2500 : ms);
    request_redraw();
}

// ------------------------------------------------- модальное окно ----------

el el_dialog(const char *title, int width) {
    int outer = el_alloc(K_BORDER);           // не добавляем в дерево окна — раскладывается отдельно
    int stack;
    Element *o = E(outer);
    o->w = width > 0 ? width : 320;
    o->has_bg = 1; o->bg = th(1); o->has_bd = 1; o->bd = th(5); o->bw = 1; o->radius = 10;
    o->pl = o->pr = 18; o->pt = 16; o->pb = 16;
    o->sh_col = 0x000000; o->sh_blur = 24; o->sh_dx = 0; o->sh_dy = 10; o->sh_a = 120;
    stack = el_alloc(K_STACK);
    E(stack)->horiz = 0;
    el_add_child(outer, stack);
    if (title && *title) {
        int t = el_alloc(K_TEXT);
        sset(&E(t)->text, title); E(t)->fsize = g.def_font + 3; E(t)->bold = 1; E(t)->mb = 10;
        el_add_child(stack, t);
    }
    g.modal = outer;
    request_redraw();
    return stack;
}
void el_dialog_close(el d) {
    int outer;
    if (!valid(d)) { g.modal = 0; return; }
    outer = E(d)->parent ? E(d)->parent : d;
    if (g.modal == outer || g.modal == d) g.modal = 0;
    // освободить поддерево
    el_remove(outer);
    request_redraw();
}

// ------------------------------------------------- файлы -------------------

// ".txt;.c" -> "Files (*.txt;*.c)\0*.txt;*.c\0All files (*.*)\0*.*\0\0"
static WCHAR *make_filter(const char *filter) {
    WCHAR *f = wdup(filter), *pats, *res, *o;
    int n = wlen(f), i, pn = 0, start = 0;
    pats = (WCHAR *)malloc((size_t)(n * 2 + 4) * sizeof(WCHAR));
    res = (WCHAR *)malloc((size_t)(n * 4 + 96) * sizeof(WCHAR));
    if (!pats || !res) abort();
    for (i = 0; i <= n; i++) {
        if (i == n || f[i] == L';') {
            if (i > start) {
                if (pn) pats[pn++] = L';';
                pats[pn++] = L'*';
                memcpy(pats + pn, f + start, (size_t)(i - start) * sizeof(WCHAR));
                pn += i - start;
            }
            start = i + 1;
        }
    }
    pats[pn] = 0;
    o = res;
    if (pn) {
        o += wsprintfW(o, L"Files (%s)", pats) + 1;
        memcpy(o, pats, (size_t)(pn + 1) * sizeof(WCHAR)); o += pn + 1;
    }
    memcpy(o, L"All files (*.*)", 16 * sizeof(WCHAR)); o += 16;
    memcpy(o, L"*.*", 4 * sizeof(WCHAR)); o += 4;
    *o = 0;
    free(f); free(pats);
    return res;
}

static void ofn_done(const WCHAR *path, void (*fn)(const char *, void *), void *arg) {
    char *u;
    if (!fn) return;
    u = udup(path);
    fn(u, arg);
    free(u);
}

void el_open_file(const char *title, const char *folder, const char *filter, void (*fn)(const char *, void *), void *arg) {
    WCHAR buf[1024];
    WCHAR *flt = make_filter(filter), *dir = wdup(folder), *ttl = wdup(title);
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof ofn);
    buf[0] = 0;
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = g.hwnd; ofn.lpstrFilter = flt; ofn.lpstrFile = buf; ofn.nMaxFile = 1024;
    ofn.lpstrInitialDir = *dir ? dir : NULL; ofn.lpstrTitle = *ttl ? ttl : NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn)) ofn_done(buf, fn, arg);
    free(flt); free(dir); free(ttl);
}
void el_save_file(const char *title, const char *folder, const char *name, void (*fn)(const char *, void *), void *arg) {
    WCHAR buf[1024];
    WCHAR *nm = wdup(name), *dir = wdup(folder), *ttl = wdup(title);
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof ofn);
    lstrcpynW(buf, nm, 1024);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = g.hwnd; ofn.lpstrFile = buf; ofn.nMaxFile = 1024;
    ofn.lpstrInitialDir = *dir ? dir : NULL; ofn.lpstrTitle = *ttl ? ttl : NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetSaveFileNameW(&ofn)) ofn_done(buf, fn, arg);
    free(nm); free(dir); free(ttl);
}
void el_pick_folder(const char *title, const char *folder, void (*fn)(const char *, void *), void *arg) {
    BROWSEINFOW bi;
    WCHAR *ttl = wdup(title);
    LPITEMIDLIST pidl;
    (void)folder;
    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = g.hwnd; bi.lpszTitle = ttl; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        WCHAR path[MAX_PATH + 2];
        if (SHGetPathFromIDListW(pidl, path)) {
            int n = wlen(path);
            if (n && path[n - 1] != L'\\') { path[n] = L'\\'; path[n + 1] = 0; }
            ofn_done(path, fn, arg);
        }
        CoTaskMemFree(pidl);
    }
    free(ttl);
}
