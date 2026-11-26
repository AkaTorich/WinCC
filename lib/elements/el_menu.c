// Меню Elements: полоса меню, выпадающие и подменю, контекстное меню.
#include "el_internal.h"

static int P(int d) { return px(d); }
static int eff_font(void) { return g.def_font; }

// ------------------------------------------------------------- создание ----

el el_menubar(el parent) {
    int h;
    if (parent == 0) parent = g.window;
    h = el_alloc(K_MENUBAR);
    E(h)->dock = D_TOP;
    el_add_child(parent, h);
    request_redraw();
    return h;
}

el el_menu(el parent, const char *text) {
    int h = el_alloc(K_MENU);
    const char *tab = text ? strchr(text, '\t') : NULL;
    if (tab) {
        size_t n = (size_t)(tab - text);
        char *s = (char *)malloc(n + 1);
        if (!s) abort();
        memcpy(s, text, n); s[n] = 0;
        sset(&E(h)->text, s);
        sset(&E(h)->accel, tab + 1);
        free(s);
    } else sset(&E(h)->text, text);
    if (valid(parent)) { E(h)->is_bar = (E(parent)->kind == K_MENUBAR); el_add_child(parent, h); }
    request_redraw();
    return h;
}

void el_menu_check(el item, int *value) { if (valid(item)) E(item)->menu_val = value; }

el el_context_menu(el target) {
    int h;
    if (!valid(target)) return 0;
    h = el_alloc(K_MENU);
    el_add_child(target, h);          // раскладка его пропустит (K_MENU)
    E(target)->ctx = h;
    return h;
}

el el_popup_menu(void) {
    return el_alloc(K_MENU);           // отдельное, в дереве не висит
}

void el_menu_popup(el menu, el at, int x, int y) {
    int ox, oy;
    Popup pu;
    if (!valid(menu)) return;
    if (valid(at)) { ox = E(at)->ax; oy = E(at)->ay; } else { ox = E(g.window)->ax; oy = E(g.window)->ay; }
    menu_close_all();
    pu.menu = menu; pu.x = ox + px(x); pu.y = oy + px(y); pu.hover = 0;
    VPUSH(g.popups, pu);
    request_redraw();
}

// ------------------------------------------------------------- раскладка ---

static int has_sub(int item) {
    int c;
    if (!valid(item)) return 0;
    for (c = E(item)->first; c; c = E(c)->next) if (E(c)->kind == K_MENU) return 1;
    return 0;
}

int menu_measure_width(int menu) {
    int w = P(120), c;
    for (c = E(menu)->first; c; c = E(c)->next) {
        if (E(c)->kind == K_MENU) {
            int tw, th2, aw = 0, ah, iw;
            text_size_u(S(E(c)->text), eff_font(), 0, 0, 0, 0, &tw, &th2);
            if (E(c)->accel) text_size_u(E(c)->accel, eff_font(), 0, 0, 0, 0, &aw, &ah);
            iw = P(28) + tw + (aw ? P(24) + aw : 0) + (has_sub(c) ? P(18) : 0) + P(16);
            w = imax(w, iw);
        }
    }
    return w;
}

typedef struct Item { int h; int y, height; int sep; } Item;

// пункты меню (освобождать free); возвращает их число
static int popup_items(int menu, Item **out, int *width, int *height) {
    int tw, rh, y = P(6), c, n = 0, cap = 0;
    Item *items = NULL;
    text_size(L"Ag", eff_font(), 0, 0, 0, 0, &tw, &rh);
    rh += P(10);
    for (c = E(menu)->first; c; c = E(c)->next) {
        int k = E(c)->kind;
        if (k != K_MENU && k != K_SEP) continue;
        if (n >= cap) items = (Item *)vgrow(items, &cap, n + 1, sizeof(Item));
        items[n].h = c; items[n].y = y;
        items[n].sep = (k == K_SEP);
        items[n].height = items[n].sep ? P(9) : rh;
        y += items[n].height;
        n++;
    }
    *width = menu_measure_width(menu);
    *height = y + P(6);
    *out = items;
    return n;
}

static void popup_bounds(const Popup *pu, int *x, int *y, int *w, int *h) {
    Item *items;
    int ww, hh, px0 = pu->x, py0 = pu->y;
    int winR = E(g.window)->ax + E(g.window)->aw, winB = E(g.window)->ay + E(g.window)->ah;
    popup_items(pu->menu, &items, &ww, &hh);
    free(items);
    if (px0 + ww > winR) px0 = imax(0, winR - ww);
    if (py0 + hh > winB) py0 = imax(0, winB - hh);
    *x = px0; *y = py0; *w = ww; *h = hh;
}

// ------------------------------------------------------------- открытие ----

void menu_open_bar(int item) {
    Popup pu;
    if (!valid(item)) return;
    menu_close_all();
    E(item)->menu_open = 1;
    pu.menu = item; pu.x = E(item)->ax; pu.y = E(item)->ay + E(item)->ah; pu.hover = 0;
    VPUSH(g.popups, pu);
    request_redraw();
}

void menu_close_all(void) {
    int i;
    for (i = 0; i < g.popups.n; i++) if (valid(g.popups.a[i].menu)) E(g.popups.a[i].menu)->menu_open = 0;
    // снять подсветку у пунктов полосы
    for (i = 1; i < g.el.n; i++) if (E(i)->alive && E(i)->kind == K_MENU) E(i)->menu_open = 0;
    g.popups.n = 0;
    request_redraw();
}

static int bar_item_at(int mx, int my) {
    int i;
    for (i = 1; i < g.el.n; i++) {
        Element *e = E(i);
        if (e->alive && e->kind == K_MENU && e->is_bar)
            if (mx >= e->ax && mx < e->ax + e->aw && my >= e->ay && my < e->ay + e->ah) return i;
    }
    return 0;
}

static void pop_after(int keep) {
    while (g.popups.n > keep) {
        int m = g.popups.a[g.popups.n - 1].menu;
        if (valid(m)) E(m)->menu_open = 0;
        g.popups.n--;
    }
}

// ------------------------------------------------------------- ввод --------

void menu_frame_input(void) {
    int down, hpop = -1, hitem = 0, pi, i;
    for (i = 0; i < g.keys.n; i++) if (g.keys.a[i].vk == VK_ESCAPE) { menu_close_all(); return; }
    down = (g.buttons & 1) && !(g.pbuttons & 1);

    for (pi = g.popups.n - 1; pi >= 0; pi--) {
        int x, y, w, h;
        popup_bounds(&g.popups.a[pi], &x, &y, &w, &h);
        if (g.mx >= x && g.mx < x + w && g.my >= y && g.my < y + h) {
            Item *items;
            int ww, hh, n = popup_items(g.popups.a[pi].menu, &items, &ww, &hh);
            for (i = 0; i < n; i++) if (!items[i].sep && g.my >= y + items[i].y && g.my < y + items[i].y + items[i].height) { hitem = items[i].h; break; }
            free(items);
            g.popups.a[pi].hover = hitem;
            hpop = pi;
            break;
        } else g.popups.a[pi].hover = 0;
    }

    // переключение между пунктами полосы при наведении
    if (hpop < 0) {
        int bi = bar_item_at(g.mx, g.my);
        if (bi && g.popups.n && g.popups.a[0].menu != bi) { menu_open_bar(bi); return; }
    }

    // открыть/закрыть подменю по наведению
    if (hpop >= 0 && valid(hitem) && has_sub(hitem)) {
        int already = (g.popups.n > hpop + 1) && g.popups.a[hpop + 1].menu == hitem;
        if (!already) {
            int x, y, w, h, ww, hh, n, iy;
            Item *items;
            Popup sub;
            pop_after(hpop + 1);
            popup_bounds(&g.popups.a[hpop], &x, &y, &w, &h);
            n = popup_items(g.popups.a[hpop].menu, &items, &ww, &hh);
            iy = y;
            for (i = 0; i < n; i++) if (items[i].h == hitem) { iy = y + items[i].y; break; }
            free(items);
            sub.menu = hitem; sub.x = x + w - P(2); sub.y = iy; sub.hover = 0;
            VPUSH(g.popups, sub);
            request_redraw();
        }
    } else if (hpop >= 0) {
        pop_after(hpop + 1);
    }

    if (down) {
        if (hpop < 0) {
            if (!bar_item_at(g.mx, g.my)) menu_close_all();
        } else if (valid(hitem) && E(hitem)->kind == K_MENU && !has_sub(hitem)) {
            int act = hitem;
            if (E(hitem)->menu_val) *E(hitem)->menu_val = !*E(hitem)->menu_val;
            menu_close_all();
            fire_click(act);
        }
    }
    request_redraw();
}

// ------------------------------------------------------------- рисование ---

void menu_draw(void) {
    GpGraphics *gr = g.g;
    int pi, i;
    for (pi = 0; pi < g.popups.n; pi++) {
        Popup pu = g.popups.a[pi];
        int x, y, w, h, ww, hh, n, fs = eff_font();
        GpRectF r;
        GpPath *p;
        Item *items;
        GpFont *f;
        GpStringFormat *sf, *af;
        if (!valid(pu.menu)) continue;
        popup_bounds(&pu, &x, &y, &w, &h);
        r = rectf((REAL)x, (REAL)y, (REAL)w, (REAL)h);
        // тень
        for (i = 5; i >= 1; i--) {
            p = gfx_rrect(rectf(r.X - i, r.Y - i + 3, r.Width + 2 * i, r.Height + 2 * i), (REAL)P(8));
            if (p) { gfx_fill_path(gr, argba(0x000000, 10), p); GdipDeletePath(p); }
        }
        p = gfx_rrect(r, (REAL)P(7));
        if (p) { gfx_fill_path(gr, argb(th(13)), p); gfx_draw_path(gr, argb(th(5)), 1.0f, p); GdipDeletePath(p); }

        f = get_font(fs, 0, 0);
        sf = gfx_format(1, GP_ALIGN_NEAR, GP_ALIGN_CENTER, 0);
        af = gfx_format(1, GP_ALIGN_FAR, GP_ALIGN_CENTER, 0);
        n = popup_items(pu.menu, &items, &ww, &hh);
        for (i = 0; i < n; i++) {
            Item it = items[i];
            int iy = y + it.y, hot;
            Element *mi;
            if (it.sep) { gfx_fill_rect(gr, argb(th(5)), (REAL)(x + P(8)), (REAL)(iy + P(4)), (REAL)(w - P(16)), (REAL)P(1)); continue; }
            mi = E(it.h);
            hot = (pu.hover == it.h) || mi->menu_open;
            if (hot) {
                p = gfx_rrect(rectf((REAL)(x + P(4)), (REAL)iy, (REAL)(w - P(8)), (REAL)it.height), (REAL)P(5));
                if (p) { gfx_fill_path(gr, argb(th(3)), p); GdipDeletePath(p); }
            }
            // галочка
            if (mi->menu_val && *mi->menu_val) {
                GpPoint chk[3];
                chk[0].X = x + P(10); chk[0].Y = iy + it.height / 2;
                chk[1].X = x + P(14); chk[1].Y = iy + it.height / 2 + P(4);
                chk[2].X = x + P(20); chk[2].Y = iy + it.height / 2 - P(4);
                gfx_lines(gr, argb(th(9)), 2.0f, chk, 3);
            }
            if (mi->text) {
                WCHAR *wt = wdup(mi->text);
                gfx_text(gr, wt, f, argb(mi->disabled ? th(8) : th(6)), rectf((REAL)(x + P(28)), (REAL)iy, (REAL)(w - P(44)), (REAL)it.height), sf);
                free(wt);
            }
            if (mi->accel) {
                WCHAR *wa = wdup(mi->accel);
                gfx_text(gr, wa, f, argb(th(8)), rectf((REAL)(x + P(10)), (REAL)iy, (REAL)(w - P(26)), (REAL)it.height), af);
                free(wa);
            }
            if (has_sub(it.h)) {
                int ax = x + w - P(14), ay = iy + it.height / 2;
                GpPoint arr[3];
                arr[0].X = ax - P(2); arr[0].Y = ay - P(4);
                arr[1].X = ax + P(3); arr[1].Y = ay;
                arr[2].X = ax - P(2); arr[2].Y = ay + P(4);
                gfx_lines(gr, argb(th(8)), 1.6f, arr, 3);
            }
        }
        free(items);
        if (sf) GdipDeleteStringFormat(sf);
        if (af) GdipDeleteStringFormat(af);
    }
}
