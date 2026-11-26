// Раскладка Elements, как в WPF: measure (желаемый размер) и arrange
// (размещение). Вся геометрия — в пикселях; размеры из API (точки) переводятся
// px() при чтении. availw/availh == EL_INF — без ограничения.
#include "el_internal.h"

static int P(int d) { return px(d); }

static int eff_font(const Element *e) { return e->fsize > 0 ? e->fsize : g.def_font; }

static const char *disp_text(const Element *e) { return e->live ? e->live : S(e->text); }

// дети, участвующие в раскладке (меню и вкладки раскладывают свои хозяева)
static int kid_ok(int c) {
    const Element *ce = E(c);
    return ce->alive && !ce->hidden && ce->kind != K_MENU && ce->kind != K_TAB;
}
static int kid_first(int h) { int c = E(h)->first; while (c && !kid_ok(c)) c = E(c)->next; return c; }
static int kid_next(int c) { c = E(c)->next; while (c && !kid_ok(c)) c = E(c)->next; return c; }

// --- желаемый размер содержимого (без padding) для элемента-контрола ---
static void control_content(int h, int availw, int *cw, int *ch) {
    Element *e = E(h);
    int fs = eff_font(e), bold = e->bold, mono = e->mono;
    const char *t = disp_text(e);
    int w = 0, hh = 0, i;
    switch (e->kind) {
    case K_TEXT:
        if (e->wrap && availw < EL_INF) {
            int innerw = availw - P(e->pl) - P(e->pr);
            text_size_u(t, fs, bold, mono, 1, innerw < 10 ? 10 : innerw, &w, &hh);
        } else text_size_u(t, fs, bold, mono, 0, 0, &w, &hh);
        *cw = w; *ch = hh;
        break;
    case K_BUTTON: case K_TOOL: {
        int iw = 0, isz = P(e->icon_size > 0 ? e->icon_size : 20);
        text_size_u(t, fs, bold, mono, 0, 0, &w, &hh);
        if (e->icon_img) iw = isz + (*t ? P(6) : 0);
        *cw = w + iw; *ch = imax(hh, e->icon_img ? isz : 0);
        if (e->kind == K_TOOL && !*t) *cw = iw ? P(e->icon_size) : P(20);
        break; }
    case K_CHECK: case K_RADIO: {
        int box = P(18);
        text_size_u(t, fs, bold, mono, 0, 0, &w, &hh);
        *cw = box + (*t ? P(8) + w : 0); *ch = imax(box, hh);
        break; }
    case K_EDIT: case K_PASSWORD:
        text_size(L"Ag", fs, bold, mono, 0, 0, &w, &hh);
        *cw = P(120); *ch = hh;
        break;
    case K_MEMO:
        text_size(L"Ag", fs, bold, mono, 0, 0, &w, &hh);
        *cw = P(160); *ch = hh * 4;
        break;
    case K_COMBO: case K_NUMBER: {
        int mw = P(40);
        if (e->items) for (i = 0; i < e->count; i++) { int iw; text_size_u(e->items[i], fs, bold, mono, 0, 0, &iw, &hh); mw = imax(mw, iw); }
        else text_size(L"0000", fs, bold, mono, 0, 0, &mw, &hh);
        if (!hh) { int t2; text_size(L"Ag", fs, bold, mono, 0, 0, &t2, &hh); }
        *cw = mw; *ch = hh;
        break; }
    case K_LIST: {
        int mw = P(60), rh = 0, th2;
        text_size(L"Ag", fs, bold, mono, 0, 0, &th2, &rh);
        if (e->items) for (i = 0; i < e->count; i++) { int iw, ih; text_size_u(e->items[i], fs, bold, mono, 0, 0, &iw, &ih); mw = imax(mw, iw); }
        *cw = mw; *ch = (rh + P(8)) * imax(1, e->count);
        break; }
    case K_SLIDER: *cw = P(120); *ch = P(22); break;
    case K_PROGRESS: *cw = P(100); *ch = P(14); break;
    case K_SEP: *cw = P(1); *ch = P(9); break;
    case K_IMAGE:
        if (e->img) { UINT iw = 0, ih = 0; GdipGetImageWidth(e->img, &iw); GdipGetImageHeight(e->img, &ih); *cw = (int)iw; *ch = (int)ih; }
        else { *cw = P(60); *ch = P(40); }
        break;
    case K_PAINT: *cw = P(80); *ch = P(60); break;
    default: *cw = 0; *ch = 0; break;
    }
}

static int border_inset(const Element *e) {
    if (e->kind == K_BORDER || e->kind == K_GROUP)
        return e->bw > 0 ? P(e->bw) : (e->has_bd ? P(1) : 0);
    return 0;
}

static int group_header(const Element *e) {
    int w, hh;
    if (e->kind != K_GROUP) return 0;
    text_size_u(S(e->text), eff_font(e), 1, 0, 0, 0, &w, &hh);
    return hh + P(4);
}

void measure(int h, int availw, int availh) {
    Element *e = E(h);
    int ml = P(e->ml), mt = P(e->mt), mr = P(e->mr), mb = P(e->mb);
    int cavw = (availw >= EL_INF) ? EL_INF : imax(0, availw - ml - mr);
    int cavh = (availh >= EL_INF) ? EL_INF : imax(0, availh - mt - mb);
    int fixW = e->w >= 0, fixH = e->h >= 0;
    int pl = P(e->pl), pt = P(e->pt), pr = P(e->pr), pb = P(e->pb);
    int ins = border_inset(e);
    int hdr = group_header(e);
    int boxW = fixW ? P(e->w) : cavw;
    int boxH = fixH ? P(e->h) : cavh;
    int innerAvW = (boxW >= EL_INF) ? EL_INF : imax(0, boxW - pl - pr - 2 * ins);
    int innerAvH = (boxH >= EL_INF) ? EL_INF : imax(0, boxH - pt - pb - 2 * ins - hdr);
    int cw = 0, ch = 0;   // желаемое содержимое (без padding/border)
    int k, i, fullw, fullh;

    switch (e->kind) {
    case K_WINDOW: case K_DOCK: {
        int remW = innerAvW, remH = innerAvH;
        for (k = kid_first(h); k; k = kid_next(k)) {
            if (!kid_next(k)) { measure(k, remW, remH); break; }   // последний занимает остаток
            measure(k, remW, remH);
            if (E(k)->dock == D_LEFT || E(k)->dock == D_RIGHT) { if (remW < EL_INF) remW = imax(0, remW - E(k)->dw); }
            else { if (remH < EL_INF) remH = imax(0, remH - E(k)->dh); }
        }
        cw = (innerAvW >= EL_INF) ? 0 : innerAvW;
        ch = (innerAvH >= EL_INF) ? 0 : innerAvH;
        break; }
    case K_STACK: {
        int gap = e->gap >= 0 ? P(e->gap) : P(8), n = 0;
        if (!e->horiz) {
            for (k = kid_first(h); k; k = kid_next(k)) { measure(k, innerAvW, EL_INF); cw = imax(cw, E(k)->dw); ch += E(k)->dh; n++; }
            if (n > 1) ch += gap * (n - 1);
        } else {
            for (k = kid_first(h); k; k = kid_next(k)) { measure(k, EL_INF, innerAvH); ch = imax(ch, E(k)->dh); cw += E(k)->dw; n++; }
            if (n > 1) cw += gap * (n - 1);
        }
        break; }
    case K_WRAP: {
        int gap = e->gap >= 0 ? P(e->gap) : P(8);
        int limit = (innerAvW >= EL_INF) ? EL_INF : innerAvW;
        int x = 0, rowh = 0, maxw = 0;
        for (k = kid_first(h); k; k = kid_next(k)) {
            int w, hh;
            measure(k, EL_INF, EL_INF);
            w = E(k)->dw; hh = E(k)->dh;
            if (x > 0 && limit < EL_INF && x + w > limit) { ch += rowh + gap; maxw = imax(maxw, x - gap); x = 0; rowh = 0; }
            x += w + gap; rowh = imax(rowh, hh);
        }
        ch += rowh; maxw = imax(maxw, x > 0 ? x - gap : 0);
        cw = (limit < EL_INF) ? limit : maxw;
        break; }
    case K_GRID: {
        GridDef *gd = e->grid;
        int gap = e->gap >= 0 ? P(e->gap) : 0, fixedW = 0, fixedH = 0;
        for (i = 0; i < gd->nc; i++) gd->cs[i] = (gd->ct[i] == 1) ? P(gd->cv[i]) : 0;
        for (i = 0; i < gd->nr; i++) gd->rs[i] = (gd->rt[i] == 1) ? P(gd->rv[i]) : 0;
        for (k = kid_first(h); k; k = kid_next(k)) {
            Element *c = E(k);
            int col, row;
            measure(k, EL_INF, EL_INF);
            col = imin(imax(0, c->col), gd->nc - 1);
            row = imin(imax(0, c->row), gd->nr - 1);
            if (c->cspan <= 1 && gd->ct[col] == 0) gd->cs[col] = imax(gd->cs[col], c->dw);
            if (c->rspan <= 1 && gd->rt[row] == 0) gd->rs[row] = imax(gd->rs[row], c->dh);
        }
        for (i = 0; i < gd->nc; i++) if (gd->ct[i] != 2) fixedW += gd->cs[i];
        // known column widths (pixel columns; star columns when the width is known):
        // measure those cells again with their real width, so text wraps and wrap
        // panels break into rows; then take the auto row heights again
        if (innerAvW < EL_INF) {
            int starW = 0, remW;
            for (i = 0; i < gd->nc; i++) if (gd->ct[i] == 2) starW += gd->cv[i];
            remW = imax(0, innerAvW - fixedW - gap * imax(0, gd->nc - 1));
            for (i = 0; i < gd->nc; i++) if (gd->ct[i] == 2) gd->cs[i] = starW > 0 ? remW * gd->cv[i] / starW : 0;
        }
        for (k = kid_first(h); k; k = kid_next(k)) {
            Element *c = E(k);
            int col = imin(imax(0, c->col), gd->nc - 1), last = imin(gd->nc, col + imax(1, c->cspan)), spanW = 0, known = 1;
            for (i = col; i < last; i++) {
                if (gd->ct[i] == 0 || (gd->ct[i] == 2 && innerAvW >= EL_INF)) { known = 0; break; }
                spanW += gd->cs[i] + gap;
            }
            if (known) measure(k, imax(0, spanW - gap), EL_INF);
        }
        for (i = 0; i < gd->nr; i++) if (gd->rt[i] == 0) gd->rs[i] = 0;
        for (k = kid_first(h); k; k = kid_next(k)) {
            Element *c = E(k);
            int row = imin(imax(0, c->row), gd->nr - 1);
            if (c->rspan <= 1 && gd->rt[row] == 0) gd->rs[row] = imax(gd->rs[row], c->dh);
        }
        for (i = 0; i < gd->nr; i++) if (gd->rt[i] != 2) fixedH += gd->rs[i];
        cw = fixedW + gap * imax(0, gd->nc - 1);
        ch = fixedH + gap * imax(0, gd->nr - 1);
        if (innerAvW < EL_INF) cw = imax(cw, innerAvW);
        break; }
    case K_SCROLL:
        k = kid_first(h);
        if (k) { measure(k, innerAvW < EL_INF ? innerAvW - P(10) : EL_INF, EL_INF); cw = E(k)->dw; ch = E(k)->dh; }
        if (innerAvH < EL_INF) ch = imin(ch, innerAvH);
        break;
    case K_CANVAS:
        for (k = kid_first(h); k; k = kid_next(k)) {
            measure(k, EL_INF, EL_INF);
            cw = imax(cw, P(E(k)->px) + E(k)->dw); ch = imax(ch, P(E(k)->py) + E(k)->dh);
        }
        break;
    case K_BORDER:
        k = kid_first(h);
        if (k) { measure(k, innerAvW, innerAvH); cw = E(k)->dw; ch = E(k)->dh; }
        break;
    case K_GROUP: {
        int w, hh;
        k = kid_first(h);
        if (k) { measure(k, innerAvW, innerAvH); cw = E(k)->dw; ch = E(k)->dh; }
        text_size_u(S(e->text), eff_font(e), 1, 0, 0, 0, &w, &hh);
        cw = imax(cw, w);
        break; }
    case K_TABS: {
        int stripH = 0, stripW = 0, sel = e->ival, idx = 0, body = 0, bodyW = 0, c;
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) {
            int w, hh;
            text_size_u(S(E(c)->text), eff_font(e), 0, 0, 0, 0, &w, &hh);
            stripH = imax(stripH, hh + P(14)); stripW += w + P(28);
        }
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) {
            if (idx == sel) { measure(c, innerAvW, innerAvH >= EL_INF ? EL_INF : innerAvH - stripH); body = E(c)->dh; bodyW = E(c)->dw; }
            idx++;
        }
        cw = imax(stripW, bodyW);
        ch = stripH + body;
        break; }
    case K_TAB:
        for (k = kid_first(h); k; k = kid_next(k)) { measure(k, innerAvW, EL_INF); cw = imax(cw, E(k)->dw); ch += E(k)->dh; }
        break;
    case K_MENUBAR: {
        int c;
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_MENU) {
            int w, hh;
            text_size_u(S(E(c)->text), eff_font(e), 0, 0, 0, 0, &w, &hh);
            cw += w + P(20); ch = imax(ch, hh + P(8));
        }
        break; }
    case K_TOOLBAR: {
        int gap = e->gap >= 0 ? P(e->gap) : P(4), n = 0;
        for (k = kid_first(h); k; k = kid_next(k)) { measure(k, EL_INF, innerAvH); ch = imax(ch, E(k)->dh); cw += E(k)->dw; n++; }
        if (n > 1) cw += gap * (n - 1);
        break; }
    default:
        control_content(h, boxW, &cw, &ch);
        break;
    }

    fullw = fixW ? P(e->w) : (cw + pl + pr + 2 * ins);
    fullh = fixH ? P(e->h) : (ch + pt + pb + 2 * ins + hdr);
    if (e->minw >= 0) fullw = imax(fullw, P(e->minw));
    if (e->minh >= 0) fullh = imax(fullh, P(e->minh));
    if (e->maxw >= 0) fullw = imin(fullw, P(e->maxw));
    if (e->maxh >= 0) fullh = imin(fullh, P(e->maxh));
    e->dw = fullw + ml + mr;
    e->dh = fullh + mt + mb;
}

// ----------------------------------------------------------------- arrange -

static void place(int h, int sx, int sy, int sw, int sh) {
    Element *e = E(h);
    int ml = P(e->ml), mt = P(e->mt), mr = P(e->mr), mb = P(e->mb);
    int avw = imax(0, sw - ml - mr), avh = imax(0, sh - mt - mb);
    int w = e->dw - ml - mr, hh = e->dh - mt - mb;
    int x, y;
    if (e->w >= 0) w = P(e->w);
    if (e->h >= 0) hh = P(e->h);

    if (e->ha == A_STRETCH && e->w < 0) { x = sx + ml; w = avw; }
    else if (e->ha == A_END) x = sx + sw - mr - w;
    else if (e->ha == A_CENTER) x = sx + ml + (avw - w) / 2;
    else x = sx + ml;
    if (e->va == A_STRETCH && e->h < 0) { y = sy + mt; hh = avh; }
    else if (e->va == A_END) y = sy + sh - mb - hh;
    else if (e->va == A_CENTER) y = sy + mt + (avh - hh) / 2;
    else y = sy + mt;

    e->ax = x; e->ay = y; e->aw = w; e->ah = hh;
}

void arrange(int h, int sx, int sy, int sw, int sh, int unused) {
    Element *e;
    int ins, hdr, cx, cy, cw, ch, k, i;
    (void)unused;
    place(h, sx, sy, sw, sh);
    e = E(h);
    ins = border_inset(e);
    hdr = group_header(e);
    cx = e->ax + P(e->pl) + ins; cy = e->ay + P(e->pt) + ins + hdr;
    cw = imax(0, e->aw - P(e->pl) - P(e->pr) - 2 * ins);
    ch = imax(0, e->ah - P(e->pt) - P(e->pb) - 2 * ins - hdr);

    switch (e->kind) {
    case K_WINDOW: case K_DOCK: {
        int l = cx, t = cy, r = cx + cw, b = cy + ch;
        for (k = kid_first(h); k; k = kid_next(k)) {
            Element *c = E(k);
            if (!kid_next(k)) { arrange(k, l, t, r - l, b - t, 0); break; }
            if (c->dock == D_LEFT) { arrange(k, l, t, c->dw, b - t, 0); l += c->dw; }
            else if (c->dock == D_RIGHT) { arrange(k, r - c->dw, t, c->dw, b - t, 0); r -= c->dw; }
            else if (c->dock == D_TOP) { arrange(k, l, t, r - l, c->dh, 0); t += c->dh; }
            else { arrange(k, l, b - c->dh, r - l, c->dh, 0); b -= c->dh; }
        }
        break; }
    case K_STACK: {
        int gap = e->gap >= 0 ? P(e->gap) : P(8);
        if (!e->horiz) { int y = cy; for (k = kid_first(h); k; k = kid_next(k)) { arrange(k, cx, y, cw, E(k)->dh, 0); y += E(k)->dh + gap; } }
        else { int x = cx; for (k = kid_first(h); k; k = kid_next(k)) { arrange(k, x, cy, E(k)->dw, ch, 0); x += E(k)->dw + gap; } }
        break; }
    case K_WRAP: {
        int gap = e->gap >= 0 ? P(e->gap) : P(8);
        int x = cx, y = cy, rowh = 0;
        for (k = kid_first(h); k; k = kid_next(k)) {
            int w = E(k)->dw, hh = E(k)->dh;
            if (x > cx && x + w > cx + cw) { x = cx; y += rowh + gap; rowh = 0; }
            arrange(k, x, y, w, hh, 0); x += w + gap; rowh = imax(rowh, hh);
        }
        break; }
    case K_GRID: {
        GridDef *gd = e->grid;
        int gap = e->gap >= 0 ? P(e->gap) : 0;
        int fixedW = 0, starW = 0, fixedH = 0, starH = 0, remW, remH, off;
        for (i = 0; i < gd->nc; i++) { if (gd->ct[i] == 2) starW += gd->cv[i]; else fixedW += gd->cs[i]; }
        for (i = 0; i < gd->nr; i++) { if (gd->rt[i] == 2) starH += gd->rv[i]; else fixedH += gd->rs[i]; }
        remW = imax(0, cw - fixedW - gap * imax(0, gd->nc - 1));
        remH = imax(0, ch - fixedH - gap * imax(0, gd->nr - 1));
        for (i = 0; i < gd->nc; i++) if (gd->ct[i] == 2) gd->cs[i] = starW > 0 ? remW * gd->cv[i] / starW : 0;
        for (i = 0; i < gd->nr; i++) if (gd->rt[i] == 2) gd->rs[i] = starH > 0 ? remH * gd->rv[i] / starH : 0;
        off = cx; for (i = 0; i < gd->nc; i++) { gd->co[i] = off; off += gd->cs[i] + gap; }
        off = cy; for (i = 0; i < gd->nr; i++) { gd->ro[i] = off; off += gd->rs[i] + gap; }
        for (k = kid_first(h); k; k = kid_next(k)) {
            Element *c = E(k);
            int col = imin(imax(0, c->col), gd->nc - 1);
            int row = imin(imax(0, c->row), gd->nr - 1);
            int spanW = 0, spanH = 0;
            for (i = col; i < imin(gd->nc, col + c->cspan); i++) spanW += gd->cs[i] + gap;
            spanW -= gap;
            for (i = row; i < imin(gd->nr, row + c->rspan); i++) spanH += gd->rs[i] + gap;
            spanH -= gap;
            arrange(k, gd->co[col], gd->ro[row], imax(0, spanW), imax(0, spanH), 0);
        }
        break; }
    case K_SCROLL:
        k = kid_first(h);
        if (k) {
            int contentH = E(k)->dh;
            int maxs = imax(0, contentH - ch);
            int sbw = contentH > ch ? P(10) : 0;
            if (e->scroll < 0) e->scroll = 0;
            if (e->scroll > maxs) e->scroll = maxs;
            arrange(k, cx, cy - e->scroll, cw - sbw, contentH, 0);
        }
        break;
    case K_CANVAS:
        for (k = kid_first(h); k; k = kid_next(k)) arrange(k, cx + P(E(k)->px), cy + P(E(k)->py), E(k)->dw, E(k)->dh, 0);
        break;
    case K_BORDER: case K_GROUP:
        k = kid_first(h);
        if (k) arrange(k, cx, cy, cw, ch, 0);
        break;
    case K_TABS: {
        int stripH = 0, sel = e->ival, idx = 0, hx = e->ax, c;
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) {
            int w, hh;
            text_size_u(S(E(c)->text), eff_font(e), 0, 0, 0, 0, &w, &hh);
            stripH = imax(stripH, hh + P(14));
        }
        e->strip_h = stripH;
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_TAB) {
            int w, hh, iw;
            text_size_u(S(E(c)->text), eff_font(e), 0, 0, 0, 0, &w, &hh);
            iw = w + P(28);
            E(c)->hdr_x = hx; E(c)->hdr_w = iw; hx += iw;
            if (idx == sel) arrange(c, e->ax + P(e->pl), e->ay + stripH + P(e->pt), e->aw - P(e->pl) - P(e->pr), e->ah - stripH - P(e->pt) - P(e->pb), 0);
            idx++;
        }
        break; }
    case K_TAB: {
        int y = cy;
        for (k = kid_first(h); k; k = kid_next(k)) { arrange(k, cx, y, cw, E(k)->dh, 0); y += E(k)->dh; }
        break; }
    case K_MENUBAR: {
        int x = e->ax, c;
        for (c = e->first; c; c = E(c)->next) if (E(c)->kind == K_MENU) {
            int w, hh, iw;
            Element *mc = E(c);
            text_size_u(S(mc->text), eff_font(e), 0, 0, 0, 0, &w, &hh);
            iw = w + P(20);
            mc->ax = x; mc->ay = e->ay; mc->aw = iw; mc->ah = e->ah; x += iw;
        }
        break; }
    case K_TOOLBAR: {
        int gap = e->gap >= 0 ? P(e->gap) : P(4), x = cx;
        for (k = kid_first(h); k; k = kid_next(k)) { arrange(k, x, cy, E(k)->dw, ch, 0); x += E(k)->dw + gap; }
        break; }
    default:
        break;
    }
}

void layout_root(void) {
    int w = 0, h = 0;
    win_client_size(&w, &h);
    ensure_back(w, h);
    if (!valid(g.window)) return;
    // применить активную таблицу стилей
    style_apply_sheet(g.window);
    measure(g.window, w, h);
    place(g.window, 0, 0, w, h);
    // окно всегда занимает весь клиент
    E(g.window)->ax = 0; E(g.window)->ay = 0; E(g.window)->aw = w; E(g.window)->ah = h;
    arrange(g.window, 0, 0, w, h, 0);
    // модальное окно программы (вне обычного дерева) — по центру
    if (valid(g.modal)) {
        int dw, dh;
        measure(g.modal, w, h);
        dw = E(g.modal)->dw; dh = E(g.modal)->dh;
        arrange(g.modal, (w - dw) / 2, (h - dh) / 2, dw, dh, 0);
    }
}
