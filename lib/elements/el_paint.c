// Отрисовка Elements на GDI+: фон/рамки/градиенты/тени/стекло, все элементы,
// трансформации (анимации) через слой, поверхность своего рисования (el_g_*).
#include "el_internal.h"

static int P(int d) { return px(d); }
static int eff_font(const Element *e) { return e->fsize > 0 ? e->fsize : g.def_font; }

static u32 eff_fg(const Element *e) {
    if (e->has_fg) return e->fg;
    return th(6);
}

static float radius_of(const Element *e) {
    int r = (e->radius >= 0) ? e->radius : g.corner;
    return (float)P(r);
}

static void draw_shadow(GpGraphics *gr, GpRectF r, float rad, const Element *e) {
    int layers = 6, i;
    float blur = (float)P(e->sh_blur);
    GpPath *p = NULL;
    if (e->sh_a <= 0) return;
    if (GdipCreatePath(GP_FILL_ALTERNATE, &p) != GP_OK) return;
    for (i = layers; i >= 1; i--) {
        float t = (float)i / layers;
        float grow = blur * t;
        int a = (int)(e->sh_a * (1.0f - t) * 0.5f / layers * 6);
        if (a <= 0) continue;
        if (a > 255) a = 255;
        round_rect_path(p, rectf(r.X - grow + P(e->sh_dx), r.Y - grow + P(e->sh_dy), r.Width + grow * 2, r.Height + grow * 2), rad + grow);
        gfx_fill_path(gr, argba(e->sh_col, a), p);
    }
    GdipDeletePath(p);
}

static void fill_shape(GpGraphics *gr, GpPath *path, GpRectF r, const Element *e, u32 bg) {
    int alpha = (e->bg_a > 0) ? e->bg_a : 255;
    if (e->grad) {
        int mode = (e->grad == 2) ? GP_LGM_HORIZONTAL : (e->grad == 3) ? GP_LGM_FDIAGONAL : GP_LGM_VERTICAL;
        GpBrush *br = NULL;
        if (GdipCreateLineBrushFromRect(&r, argba(e->bg, alpha), argba(e->bg2, alpha), mode, GP_WRAP_TILE, &br) == GP_OK) {
            GdipFillPath(gr, br, path);
            GdipDeleteBrush(br);
        }
    } else {
        gfx_fill_path(gr, argba(bg, alpha), path);
    }
}

// рисует фон+рамку в прямоугольнике r по стилю элемента, цвет фона bg
static void draw_box(GpGraphics *gr, GpRectF r, const Element *e, u32 bg, int hasbg, u32 bd, int hasbd) {
    float rad = radius_of(e);
    GpPath *path = gfx_rrect(r, rad);
    int bw = e->bw;
    if (!path) return;
    if (e->sh_a > 0) draw_shadow(gr, r, rad, e);
    if (hasbg || e->grad) fill_shape(gr, path, r, e, bg);
    if (bw < 0) bw = (hasbd ? 1 : 0);
    if (bw > 0 && hasbd) gfx_draw_path(gr, argb(bd), (REAL)P(bw), path);
    GdipDeletePath(path);
}

static void draw_wtext_in(GpGraphics *gr, GpRectF r, const WCHAR *s, u32 color, int size, int bold, int mono, int align, int wrapW, int vcenter) {
    GpStringFormat *sf;
    if (!s || !*s) return;
    sf = gfx_format(!wrapW, align == 1 ? GP_ALIGN_CENTER : align == 2 ? GP_ALIGN_FAR : GP_ALIGN_NEAR,
                    vcenter ? GP_ALIGN_CENTER : GP_ALIGN_NEAR, 1);
    gfx_text(gr, s, get_font(size, bold, mono), argb(color), r, sf);
    if (sf) GdipDeleteStringFormat(sf);
}

static void draw_text_in(GpGraphics *gr, GpRectF r, const char *s, u32 color, int size, int bold, int mono, int align, int wrapW, int vcenter) {
    WCHAR *w;
    if (!s || !*s) return;
    w = wdup(s);
    draw_wtext_in(gr, r, w, color, size, bold, mono, align, wrapW, vcenter);
    free(w);
}

// обрезать рисование прямоугольником (вложенно); вернуть состояние для restore
static GraphicsState clip_push(GpGraphics *gr, int x, int y, int w, int h) {
    GraphicsState st = 0;
    GdipSaveGraphics(gr, &st);
    GdipSetClipRectI(gr, x, y, w, h, GP_COMBINE_INTERSECT);
    return st;
}

// ---- трансформации (анимации) ----
static void draw_inner(int h);

static void draw_layered(int h, double tx, double ty, double sc, double rot, double op) {
    Element *e = E(h);
    int m = P(60);
    int lw = e->aw + 2 * m, lh = e->ah + 2 * m;
    int ax = e->ax, ay = e->ay, aw = e->aw, ah = e->ah;
    GpImage *layer = NULL;
    GpGraphics *lg = NULL, *save;
    GpImageAttributes *ia = NULL;
    GraphicsState st = 0;
    float cx, cy;
    if (lw < 1) lw = 1;
    if (lh < 1) lh = 1;
    if (GdipCreateBitmapFromScan0(lw, lh, 0, GP_PIXFMT_32BPP_PARGB, NULL, &layer) != GP_OK) return;
    if (GdipGetImageGraphicsContext(layer, &lg) != GP_OK) { GdipDisposeImage(layer); return; }
    GdipSetSmoothingMode(lg, GP_SMOOTH_ANTIALIAS);
    GdipSetTextRenderingHint(lg, GP_TEXT_ANTIALIAS);
    GdipSetPixelOffsetMode(lg, GP_PIXOFF_HALF);
    GdipSetInterpolationMode(lg, GP_INTERP_HQ_BICUBIC);
    save = g.g;
    g.g = lg;
    GdipTranslateWorldTransform(g.g, (REAL)(m - ax), (REAL)(m - ay), GP_ORDER_PREPEND);
    draw_inner(h);
    g.g = save;
    GdipDeleteGraphics(lg);

    cx = (float)(ax + aw / 2.0); cy = (float)(ay + ah / 2.0);
    GdipSaveGraphics(g.g, &st);
    GdipTranslateWorldTransform(g.g, (REAL)(cx + px((int)tx)), (REAL)(cy + px((int)ty)), GP_ORDER_PREPEND);
    if (rot != 0) GdipRotateWorldTransform(g.g, (REAL)rot, GP_ORDER_PREPEND);
    if (sc != 100) GdipScaleWorldTransform(g.g, (REAL)(sc / 100.0), (REAL)(sc / 100.0), GP_ORDER_PREPEND);
    GdipTranslateWorldTransform(g.g, -cx, -cy, GP_ORDER_PREPEND);
    GdipCreateImageAttributes(&ia);
    if (ia && op < 255) {
        GpColorMatrix cm;
        memset(&cm, 0, sizeof cm);
        cm.m[0][0] = 1; cm.m[1][1] = 1; cm.m[2][2] = 1; cm.m[3][3] = (REAL)(op / 255.0); cm.m[4][4] = 1;
        GdipSetImageAttributesColorMatrix(ia, GP_COLORADJUST_DEFAULT, TRUE, &cm, NULL, GP_COLORMATRIX_DEFAULT);
    }
    GdipDrawImageRectRect(g.g, layer, (REAL)(ax - m), (REAL)(ay - m), (REAL)lw, (REAL)lh,
                          0, 0, (REAL)lw, (REAL)lh, GP_UNIT_PIXEL, ia, NULL, NULL);
    GdipRestoreGraphics(g.g, st);
    if (ia) GdipDisposeImageAttributes(ia);
    GdipDisposeImage(layer);
}

void draw_element(int h) {
    double tx, ty, sc, rot, op;
    if (!valid(h)) return;
    if (E(h)->hidden) return;
    transform_of(h, &tx, &ty, &sc, &rot, &op);
    if (op >= 255 && sc == 100 && rot == 0) {
        if (tx || ty) {
            GraphicsState st = 0;
            GdipSaveGraphics(g.g, &st);
            GdipTranslateWorldTransform(g.g, (REAL)px((int)tx), (REAL)px((int)ty), GP_ORDER_PREPEND);
            draw_inner(h);
            GdipRestoreGraphics(g.g, st);
        } else draw_inner(h);
    } else {
        draw_layered(h, tx, ty, sc, rot, op);
    }
}

static void draw_children(int h) {
    int c = E(h)->first;
    while (c) { if (E(c)->alive && E(c)->kind != K_MENU && E(c)->kind != K_TAB) draw_element(c); c = E(c)->next; }
}

static void paint_surface(int h) {
    Element *e = E(h);
    GraphicsState st;
    int sp, ox, oy;
    if (!e->paint_fn) return;
    st = clip_push(g.g, e->ax, e->ay, e->aw, e->ah);
    sp = g.cur_paint; ox = g.paint_ox; oy = g.paint_oy;
    g.cur_paint = h; g.paint_ox = e->ax; g.paint_oy = e->ay;
    e->paint_fn(h, dip(e->aw), dip(e->ah), e->paint_arg);
    g.cur_paint = sp; g.paint_ox = ox; g.paint_oy = oy;
    GdipRestoreGraphics(g.g, st);
    E(h)->needs_redraw = 0;
}

static void draw_arrow(GpGraphics *gr, ARGB c, int x0, int y0, int x1, int y1, int x2, int y2) {
    GpPoint pts[3];
    pts[0].X = x0; pts[0].Y = y0; pts[1].X = x1; pts[1].Y = y1; pts[2].X = x2; pts[2].Y = y2;
    gfx_lines(gr, c, 1.6f, pts, 3);
}

static void draw_inner(int h) {
    Element *e = E(h);
    GpGraphics *gr = g.g;
    GpRectF r = rectf((REAL)e->ax, (REAL)e->ay, (REAL)e->aw, (REAL)e->ah);
    int fs = eff_font(e), bold = e->bold, mono = e->mono;
    int hover = e->hv > 0.01;
    int pressed = (g.active == h) && (g.buttons & 1);
    const char *t = e->live ? e->live : S(e->text);
    int i;

    switch (e->kind) {
    case K_WINDOW:
        // фон окна: градиент/картинка/цвет из стиля, иначе тема
        if (e->has_bg || e->grad) draw_box(gr, r, e, e->bg, 1, e->bd, 0);
        if (g.win_canvas && valid(g.win_canvas)) {
            Element *pc = E(g.win_canvas);
            pc->ax = e->ax; pc->ay = e->ay; pc->aw = e->aw; pc->ah = e->ah;
            paint_surface(g.win_canvas);
        }
        draw_children(h);
        break;
    case K_STACK: case K_DOCK: case K_WRAP: case K_CANVAS: case K_TAB: case K_TOOLBAR: case K_GRID:
        if (e->has_bg || e->grad) draw_box(gr, r, e, e->bg, 1, e->bd, e->has_bd);
        else if (e->has_bd) draw_box(gr, r, e, 0, 0, e->bd, 1);
        draw_children(h);
        break;
    case K_BORDER:
        draw_box(gr, r, e, e->bg, e->has_bg, e->has_bd ? e->bd : th(5), e->has_bd || (e->bw > 0));
        draw_children(h);
        break;
    case K_GROUP: {
        int w, hdrh;
        GpPath *p;
        text_size_u(S(e->text), fs, 1, 0, 0, 0, &w, &hdrh);
        p = gfx_rrect(rectf(r.X, r.Y + hdrh / 2.0f, r.Width, r.Height - hdrh / 2.0f), radius_of(e));
        if (p) { gfx_draw_path(gr, argb(th(5)), 1.0f, p); GdipDeletePath(p); }
        if (e->text) {
            WCHAR *wt = wdup(e->text);
            GpStringFormat *sf = gfx_format(1, GP_ALIGN_NEAR, GP_ALIGN_NEAR, 0);
            gfx_text(gr, wt, get_font(fs, 1, 0), argb(th(8)), rectf(r.X + P(10), r.Y, r.Width - P(20), (REAL)hdrh + 2), sf);
            if (sf) GdipDeleteStringFormat(sf);
            free(wt);
        }
        draw_children(h);
        break; }
    case K_SCROLL: {
        GraphicsState st = clip_push(gr, e->ax, e->ay, e->aw, e->ah);
        int c;
        draw_children(h);
        GdipRestoreGraphics(gr, st);
        e = E(h);
        c = e->first;
        if (c) {
            int contentH = E(c)->dh, ch = e->ah;
            if (contentH > ch) {
                int trackx = e->ax + e->aw - P(9);
                int knob = imax(P(24), ch * ch / contentH);
                int ky = e->ay + (e->scroll * (ch - knob)) / imax(1, contentH - ch);
                GpPath *p;
                gfx_fill_rect(gr, argba(th(8), 40), (REAL)trackx, (REAL)e->ay, (REAL)P(7), (REAL)e->ah);
                p = gfx_rrect(rectf((REAL)trackx, (REAL)ky, (REAL)P(7), (REAL)knob), (REAL)P(3));
                if (p) { gfx_fill_path(gr, argb(th(8)), p); GdipDeletePath(p); }
            }
        }
        break; }
    case K_TABS: {
        int stripH = e->strip_h, idx = 0, sel = e->ival, c = e->first;
        gfx_fill_rect(gr, argb(th(5)), (REAL)e->ax, (REAL)(e->ay + stripH - P(1)), (REAL)e->aw, (REAL)P(1));
        while (c) {
            if (E(c)->kind == K_TAB) {
                Element *tb = E(c);
                int on = (idx == sel), hot = (g.hot == c);
                if (on) {
                    gfx_fill_rect(gr, argb(th(1)), (REAL)tb->hdr_x, (REAL)e->ay, (REAL)tb->hdr_w, (REAL)stripH);
                    gfx_fill_rect(gr, argb(th(9)), (REAL)tb->hdr_x, (REAL)(e->ay + stripH - P(3)), (REAL)tb->hdr_w, (REAL)P(3));
                } else if (hot) gfx_fill_rect(gr, argba(th(6), 18), (REAL)tb->hdr_x, (REAL)e->ay, (REAL)tb->hdr_w, (REAL)stripH);
                draw_text_in(gr, rectf((REAL)tb->hdr_x, (REAL)e->ay, (REAL)tb->hdr_w, (REAL)stripH), tb->text, on ? th(7) : th(8), fs, 0, 0, 1, 0, 1);
                idx++;
            }
            c = E(c)->next;
        }
        idx = 0; c = e->first;
        while (c) { if (E(c)->kind == K_TAB) { if (idx == sel) draw_children(c); idx++; } c = E(c)->next; }
        break; }
    case K_MENUBAR: {
        int c = e->first;
        gfx_fill_rect(gr, argb(th(1)), (REAL)e->ax, (REAL)e->ay, (REAL)e->aw, (REAL)e->ah);
        while (c) {
            if (E(c)->kind == K_MENU) {
                Element *mi = E(c);
                int hot = g.hot == c || mi->menu_open;
                if (hot) gfx_fill_rect(gr, argb(mi->menu_open ? th(4) : th(3)), (REAL)mi->ax, (REAL)mi->ay, (REAL)mi->aw, (REAL)mi->ah);
                draw_text_in(gr, rectf((REAL)mi->ax, (REAL)mi->ay, (REAL)mi->aw, (REAL)mi->ah), mi->text, th(6), fs, 0, 0, 1, 0, 1);
            }
            c = E(c)->next;
        }
        break; }
    case K_TEXT:
        draw_text_in(gr, r, t, eff_fg(e), fs, bold, mono, e->talign, e->wrap, 0);
        break;
    case K_BUTTON: case K_TOOL: {
        u32 bg = e->has_bg ? e->bg : th(2);
        u32 base = bg;
        int drawbg, hasbd, cx = e->ax, cw = e->aw;
        u32 bd;
        if (pressed) base = e->has_pbg ? e->pbg : th(4);
        else if (hover) base = e->has_hbg ? e->hbg : mix(bg, g.dark ? 0xffffff : 0x000000, 0.08 + 0.12 * e->hv);
        drawbg = !(e->kind == K_TOOL && !e->has_bg && !hover && !pressed);   // плоская: без фона
        bd = e->has_bd ? e->bd : th(5);
        hasbd = e->has_bd || (e->bw > 0);
        if (hover && e->has_hbd) { bd = e->hbd; hasbd = 1; }
        if (drawbg) draw_box(gr, r, e, base, 1, bd, hasbd);
        // значок + текст
        if (e->icon_img) {
            int is = P(e->icon_size > 0 ? e->icon_size : 20);
            int iy = e->ay + (e->ah - is) / 2;
            int ix = !*t ? e->ax + (e->aw - is) / 2 : e->ax + P(e->pl);
            GdipDrawImageRectI(gr, e->icon_img, ix, iy, is, is);
            if (*t) { cx = ix + is + P(6); cw = e->aw - (cx - e->ax) - P(e->pr); }
        }
        if (*t) {
            u32 fg = e->has_fg ? e->fg : (e->kind == K_TOOL ? th(6) : th(7));
            if (hover && e->has_hfg) fg = e->hfg;
            draw_text_in(gr, rectf((REAL)cx, (REAL)e->ay, (REAL)cw, (REAL)e->ah), t, fg, fs, bold, mono, e->icon_img ? 0 : 1, 0, 1);
        }
        break; }
    case K_CHECK: case K_RADIO: {
        int box = P(18);
        int by = e->ay + (e->ah - box) / 2;
        int on = e->iv ? (e->kind == K_CHECK ? (*e->iv != 0) : (*e->iv == e->ival)) : 0;
        GpRectF br = rectf((REAL)e->ax, (REAL)by, (REAL)box, (REAL)box);
        if (e->kind == K_RADIO) {
            gfx_draw_ellipse(gr, argb(on ? th(9) : th(5)), 1.5f, br.X, br.Y, br.Width, br.Height);
            if (on) gfx_fill_ellipse(gr, argb(th(9)), br.X + P(4), br.Y + P(4), (REAL)(box - P(8)), (REAL)(box - P(8)));
        } else {
            GpPath *p = gfx_rrect(br, (REAL)P(3));
            if (p) {
                if (on) {
                    GpPoint chk[3];
                    gfx_fill_path(gr, argb(th(9)), p);
                    chk[0].X = e->ax + P(4); chk[0].Y = by + P(9);
                    chk[1].X = e->ax + P(8); chk[1].Y = by + P(13);
                    chk[2].X = e->ax + P(14); chk[2].Y = by + P(5);
                    gfx_lines(gr, argb(th(10)), 2.0f, chk, 3);
                } else gfx_draw_path(gr, argb(th(5)), 1.5f, p);
                GdipDeletePath(p);
            }
        }
        if (*t) draw_text_in(gr, rectf((REAL)(e->ax + box + P(8)), (REAL)e->ay, (REAL)(e->aw - box - P(8)), (REAL)e->ah), t, eff_fg(e), fs, bold, mono, 0, 0, 1);
        break; }
    case K_EDIT: case K_PASSWORD: case K_MEMO:
        edit_draw(h);
        break;
    case K_COMBO: case K_NUMBER: {
        u32 bg = e->has_bg ? e->bg : th(11);
        GpRectF vr = rectf((REAL)(e->ax + P(e->pl)), (REAL)e->ay, (REAL)(e->aw - P(e->pl) - P(e->pr)), (REAL)e->ah);
        draw_box(gr, r, e, bg, 1, th(5), 1);
        if (e->kind == K_COMBO) {
            int s = e->iv ? *e->iv : 0;
            if (e->items && s >= 0 && s < e->count) draw_text_in(gr, vr, e->items[s], eff_fg(e), fs, bold, mono, 0, 0, 1);
        } else {
            WCHAR b[24];
            wsprintfW(b, L"%d", e->iv ? *e->iv : 0);
            draw_wtext_in(gr, vr, b, eff_fg(e), fs, bold, mono, 0, 0, 1);
        }
        // стрелка(и)
        if (e->kind == K_COMBO) {
            int ax = e->ax + e->aw - P(18), ay = e->ay + e->ah / 2;
            draw_arrow(gr, argb(th(8)), ax - P(4), ay - P(2), ax, ay + P(3), ax + P(4), ay - P(2));
        } else {
            int ax = e->ax + e->aw - P(16), ay = e->ay + e->ah / 2;
            draw_arrow(gr, argb(th(8)), ax - P(4), ay - P(1), ax, ay - P(5), ax + P(4), ay - P(1));
            draw_arrow(gr, argb(th(8)), ax - P(4), ay + P(2), ax, ay + P(6), ax + P(4), ay + P(2));
        }
        break; }
    case K_LIST: {
        u32 bg = e->has_bg ? e->bg : th(11);
        GraphicsState st;
        int w, rh, y, sel;
        draw_box(gr, r, e, bg, 1, th(5), 1);
        st = clip_push(gr, e->ax, e->ay, e->aw, e->ah);
        text_size(L"Ag", fs, 0, 0, 0, 0, &w, &rh);
        rh += P(8);
        y = e->ay + P(e->pt) - e->scroll;
        sel = e->iv ? *e->iv : -1;
        for (i = 0; i < e->count; i++) {
            if (y + rh > e->ay && y < e->ay + e->ah) {
                if (i == sel) {
                    GpPath *p = gfx_rrect(rectf((REAL)(e->ax + P(3)), (REAL)y, (REAL)(e->aw - P(6)), (REAL)rh), (REAL)P(4));
                    if (p) { gfx_fill_path(gr, argb(th(9)), p); GdipDeletePath(p); }
                } else if (g.hot == h && g.my >= y && g.my < y + rh)
                    gfx_fill_rect(gr, argba(th(6), 24), (REAL)(e->ax + P(3)), (REAL)y, (REAL)(e->aw - P(6)), (REAL)rh);
                draw_text_in(gr, rectf((REAL)(e->ax + P(10)), (REAL)y, (REAL)(e->aw - P(16)), (REAL)rh), e->items[i], i == sel ? th(10) : th(6), fs, 0, mono, 0, 0, 1);
            }
            y += rh;
        }
        GdipRestoreGraphics(gr, st);
        break; }
    case K_SLIDER: {
        int cy = e->ay + e->ah / 2;
        int tl = e->ax + P(8), tr = e->ax + e->aw - P(8);
        int v = e->iv ? *e->iv : 0, kx;
        double tfrac;
        GpPath *p = gfx_rrect(rectf((REAL)tl, (REAL)(cy - P(2)), (REAL)(tr - tl), (REAL)P(4)), (REAL)P(2));
        if (p) { gfx_fill_path(gr, argb(th(5)), p); GdipDeletePath(p); }
        if (v < e->vmin) v = e->vmin;
        if (v > e->vmax) v = e->vmax;
        tfrac = (e->vmax > e->vmin) ? (double)(v - e->vmin) / (e->vmax - e->vmin) : 0;
        kx = tl + (int)((tr - tl) * tfrac);
        p = gfx_rrect(rectf((REAL)tl, (REAL)(cy - P(2)), (REAL)(kx - tl), (REAL)P(4)), (REAL)P(2));
        if (p) { gfx_fill_path(gr, argb(th(9)), p); GdipDeletePath(p); }
        gfx_fill_ellipse(gr, argb(th(9)), (REAL)(kx - P(8)), (REAL)(cy - P(8)), (REAL)P(16), (REAL)P(16));
        gfx_fill_ellipse(gr, argb(th(10)), (REAL)(kx - P(4)), (REAL)(cy - P(4)), (REAL)P(8), (REAL)P(8));
        break; }
    case K_PROGRESS: {
        u32 fg = e->has_fg ? e->fg : th(9);
        int v = e->iv ? *e->iv : 0;
        double f;
        GpPath *p = gfx_rrect(r, (REAL)(e->ah / 2.0f));
        if (p) { gfx_fill_path(gr, argb(th(5)), p); GdipDeletePath(p); }
        if (v < 0) v = 0;
        if (v > e->vmax) v = e->vmax;
        f = e->vmax > 0 ? (double)v / e->vmax : 0;
        if (f > 0) {
            p = gfx_rrect(rectf(r.X, r.Y, (REAL)(r.Width * f), r.Height), (REAL)(e->ah / 2.0f));
            if (p) { gfx_fill_path(gr, argb(fg), p); GdipDeletePath(p); }
        }
        break; }
    case K_SEP:
        if (e->aw > e->ah) gfx_fill_rect(gr, argb(th(5)), (REAL)e->ax, (REAL)(e->ay + e->ah / 2), (REAL)e->aw, (REAL)P(1));
        else gfx_fill_rect(gr, argb(th(5)), (REAL)(e->ax + e->aw / 2), (REAL)e->ay, (REAL)P(1), (REAL)e->ah);
        break;
    case K_IMAGE:
        if (e->img) {
            UINT iw = 0, ih = 0;
            GdipGetImageWidth(e->img, &iw); GdipGetImageHeight(e->img, &ih);
            if (iw && ih) {
                double sx = (double)e->aw / iw, sy = (double)e->ah / ih, s = sx < sy ? sx : sy;
                int dw = (int)(iw * s), dh = (int)(ih * s);
                GdipDrawImageRectI(gr, e->img, e->ax + (e->aw - dw) / 2, e->ay + (e->ah - dh) / 2, dw, dh);
            }
        }
        break;
    case K_PAINT:
        paint_surface(h);
        break;
    default: break;
    }

    e = E(h);
    if (e->disabled && e->kind != K_WINDOW)
        gfx_fill_rect(gr, argba(th(0), 110), (REAL)e->ax, (REAL)e->ay, (REAL)e->aw, (REAL)e->ah);
}

void draw_tree(void) {
    GpGraphics *gr = g.g;
    GdipResetWorldTransform(gr);
    GdipResetClip(gr);
    if (g.layered) GdipGraphicsClear(gr, 0);
    else GdipGraphicsClear(gr, argb(th(0)));
    draw_element(g.window);
}

// ----------------------------------------------------------- el_g_* --------

static int OX(void) { return g.paint_ox; }
static int OY(void) { return g.paint_oy; }
static float W1(int width) { return (float)px(width ? width : 1); }

void el_g_fill(int x, int y, int w, int h, int rgb) {
    gfx_fill_rect(g.g, argb((u32)rgb), (REAL)(OX() + px(x)), (REAL)(OY() + px(y)), (REAL)px(w), (REAL)px(h));
}
void el_g_round(int x, int y, int w, int h, int radius, int rgb) {
    GpPath *p = gfx_rrect(rectf((REAL)(OX() + px(x)), (REAL)(OY() + px(y)), (REAL)px(w), (REAL)px(h)), (REAL)px(radius));
    if (p) { gfx_fill_path(g.g, argb((u32)rgb), p); GdipDeletePath(p); }
}
void el_g_frame(int x, int y, int w, int h, int radius, int rgb, int width) {
    GpPath *p = gfx_rrect(rectf((REAL)(OX() + px(x)), (REAL)(OY() + px(y)), (REAL)px(w), (REAL)px(h)), (REAL)px(radius));
    if (p) { gfx_draw_path(g.g, argb((u32)rgb), W1(width), p); GdipDeletePath(p); }
}
void el_g_line(int x0, int y0, int x1, int y1, int rgb, int width) {
    gfx_line(g.g, argb((u32)rgb), W1(width), (REAL)(OX() + px(x0)), (REAL)(OY() + px(y0)), (REAL)(OX() + px(x1)), (REAL)(OY() + px(y1)), 1);
}
void el_g_disc(int cx, int cy, int rad, int rgb) {
    gfx_fill_ellipse(g.g, argb((u32)rgb), (REAL)(OX() + px(cx) - px(rad)), (REAL)(OY() + px(cy) - px(rad)), (REAL)(px(rad) * 2), (REAL)(px(rad) * 2));
}
void el_g_ring(int cx, int cy, int rad, int width, int rgb) {
    gfx_draw_ellipse(g.g, argb((u32)rgb), W1(width), (REAL)(OX() + px(cx) - px(rad)), (REAL)(OY() + px(cy) - px(rad)), (REAL)(px(rad) * 2), (REAL)(px(rad) * 2));
}
void el_g_shade(int x, int y, int w, int h, int rgb, int alpha) {
    gfx_fill_rect(g.g, argba((u32)rgb, alpha), (REAL)(OX() + px(x)), (REAL)(OY() + px(y)), (REAL)px(w), (REAL)px(h));
}
void el_g_text(int x, int y, const char *s, int rgb, int size, int bold) {
    WCHAR *w;
    if (!s || !*s) return;
    w = wdup(s);
    gfx_text_at(g.g, w, get_font(size > 0 ? size : g.def_font, bold, 0), argb((u32)rgb), (REAL)(OX() + px(x)), (REAL)(OY() + px(y)));
    free(w);
}
int el_g_text_w(const char *s, int size, int bold) { return dip(text_w_u(s, size > 0 ? size : g.def_font, bold, 0)); }

// точки xy (пары, в точках) -> PointF в пикселях кадра; освобождать free
static GpPointF *to_points(const int *xy, int count) {
    GpPointF *pts = (GpPointF *)malloc(sizeof(GpPointF) * (size_t)count);
    int i;
    if (!pts) return NULL;
    for (i = 0; i < count; i++) { pts[i].X = (REAL)(OX() + px(xy[i * 2])); pts[i].Y = (REAL)(OY() + px(xy[i * 2 + 1])); }
    return pts;
}

void el_g_poly(const int *xy, int count, int rgb) {
    GpPointF *pts;
    if (!xy || count < 3) return;
    pts = to_points(xy, count);
    if (!pts) return;
    gfx_fill_poly(g.g, argb((u32)rgb), pts, count);
    free(pts);
}
void el_g_poly_frame(const int *xy, int count, int rgb, int width) {
    GpPointF *pts;
    GpPath *p = NULL;
    GpPen *pen = NULL;
    if (!xy || count < 2) return;
    pts = to_points(xy, count);
    if (!pts) return;
    if (GdipCreatePath(GP_FILL_ALTERNATE, &p) == GP_OK) {
        GdipAddPathPolygon(p, pts, count);
        if (GdipCreatePen1(argb((u32)rgb), W1(width), GP_UNIT_PIXEL, &pen) == GP_OK) {
            GdipSetPenLineJoin(pen, GP_JOIN_ROUND);
            GdipDrawPath(g.g, pen, p);
            GdipDeletePen(pen);
        }
        GdipDeletePath(p);
    }
    free(pts);
}
void el_g_spline(const int *xy, int count, int rgb, int width) {
    GpPointF *pts;
    GpPen *pen = NULL;
    if (!xy || count < 2) return;
    pts = to_points(xy, count);
    if (!pts) return;
    if (GdipCreatePen1(argb((u32)rgb), W1(width), GP_UNIT_PIXEL, &pen) == GP_OK) {
        GdipSetPenStartCap(pen, GP_CAP_ROUND); GdipSetPenEndCap(pen, GP_CAP_ROUND);
        GdipDrawCurve2(g.g, pen, pts, count, 0.5f);
        GdipDeletePen(pen);
    }
    free(pts);
}
void el_g_blob(const int *xy, int count, int rgb) {
    GpPointF *pts;
    GpBrush *b = NULL;
    if (!xy || count < 3) return;
    pts = to_points(xy, count);
    if (!pts) return;
    if (GdipCreateSolidFill(argb((u32)rgb), &b) == GP_OK) {
        GdipFillClosedCurve(g.g, b, pts, count);
        GdipDeleteBrush(b);
    }
    free(pts);
}
void el_g_arc(int cx, int cy, int r, int start_deg, int sweep_deg, int rgb, int width) {
    GpPen *pen = NULL;
    if (GdipCreatePen1(argb((u32)rgb), W1(width), GP_UNIT_PIXEL, &pen) != GP_OK) return;
    GdipSetPenStartCap(pen, GP_CAP_ROUND); GdipSetPenEndCap(pen, GP_CAP_ROUND);
    GdipDrawArc(g.g, pen, (REAL)(OX() + px(cx) - px(r)), (REAL)(OY() + px(cy) - px(r)), (REAL)(px(r) * 2), (REAL)(px(r) * 2), (REAL)start_deg, (REAL)sweep_deg);
    GdipDeletePen(pen);
}

// ----------------------------------------------------- мышь на поверхности --
int el_mouse(el e, int *x, int *y, int *buttons) {
    Element *m;
    if (!valid(e)) return 0;
    m = E(e);
    if (x) *x = dip(g.mx - m->ax);
    if (y) *y = dip(g.my - m->ay);
    if (buttons) *buttons = g.buttons;
    return (g.mx >= m->ax && g.mx < m->ax + m->aw && g.my >= m->ay && g.my < m->ay + m->ah) ? 1 : 0;
}
int el_wheel(el e, int *mods) {
    Element *m;
    int over;
    if (!valid(e)) return 0;
    m = E(e);
    over = (g.mx >= m->ax && g.mx < m->ax + m->aw && g.my >= m->ay && g.my < m->ay + m->ah);
    if (mods) *mods = el_mods();
    if (over && g.wheel) { int w = g.wheel; g.wheel = 0; return w; }
    return 0;
}
