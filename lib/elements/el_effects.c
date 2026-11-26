// Эффекты окна: постобработка готового кадра (буфер BGRA g.dibbits).
// Включается через el_effect("имя", сила); несколько эффектов стопкой.
#include "el_internal.h"
#include <math.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int lum(u32 p) { int r = (p >> 16) & 255, g2 = (p >> 8) & 255, b = p & 255; return (r * 77 + g2 * 150 + b * 29) >> 8; }
static u32 pack(int a, int r, int g2, int b) { return ((u32)a << 24) | ((u32)clampi(r, 0, 255) << 16) | ((u32)clampi(g2, 0, 255) << 8) | (u32)clampi(b, 0, 255); }

static uint32_t s_rng = 0x1234567;
static uint32_t xr(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }

static u32 *copy_tmp(const u32 *buf, int w, int h) {
    int n = w * h;
    if (g.fx_tmp.cap < n) g.fx_tmp.a = (u32 *)vgrow(g.fx_tmp.a, &g.fx_tmp.cap, n, sizeof(u32));
    memcpy(g.fx_tmp.a, buf, (size_t)n * sizeof(u32));
    g.fx_tmp.n = n;
    return g.fx_tmp.a;
}

static void fx_chromatic(u32 *buf, int w, int h, int amt) {
    u32 *s;
    int x, y;
    if (amt < 1) return;
    s = copy_tmp(buf, w, h);
    for (y = 0; y < h; y++) {
        u32 *row = buf + (size_t)y * w, *sr = s + (size_t)y * w;
        for (x = 0; x < w; x++) {
            int r = (sr[clampi(x - amt, 0, w - 1)] >> 16) & 255;
            int b = sr[clampi(x + amt, 0, w - 1)] & 255;
            int g2 = (sr[x] >> 8) & 255, a = (sr[x] >> 24) & 255;
            row[x] = pack(a, r, g2, b);
        }
    }
}
static void fx_scanlines(u32 *buf, int w, int h, int amt) {
    double f = 1.0 - clampi(amt, 0, 100) / 100.0 * 0.85;
    int x, y;
    for (y = 0; y < h; y += 2) {
        u32 *row = buf + (size_t)y * w;
        for (x = 0; x < w; x++) {
            u32 p = row[x];
            int a = (p >> 24) & 255;
            row[x] = pack(a, (int)(((p >> 16) & 255) * f), (int)(((p >> 8) & 255) * f), (int)((p & 255) * f));
        }
    }
}
static void fx_noise(u32 *buf, int w, int h, int amt) {
    int i;
    s_rng ^= (uint32_t)(g.now_ms) * 2654435761u;
    for (i = 0; i < w * h; i++) {
        u32 p = buf[i];
        int a = (p >> 24) & 255, n;
        if (!a) continue;
        n = (int)(xr() % (uint32_t)(2 * amt + 1)) - amt;
        buf[i] = pack(a, (int)((p >> 16) & 255) + n, (int)((p >> 8) & 255) + n, (int)(p & 255) + n);
    }
}
static void fx_vignette(u32 *buf, int w, int h, int amt) {
    double cx = w / 2.0, cy = h / 2.0, md = sqrt(cx * cx + cy * cy), k = clampi(amt, 0, 100) / 100.0;
    int x, y;
    for (y = 0; y < h; y++) {
        u32 *row = buf + (size_t)y * w;
        for (x = 0; x < w; x++) {
            double d = sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / md;
            double f = 1.0 - k * d * d;
            u32 p = row[x];
            int a = (p >> 24) & 255;
            if (f < 0) f = 0;
            row[x] = pack(a, (int)(((p >> 16) & 255) * f), (int)(((p >> 8) & 255) * f), (int)((p & 255) * f));
        }
    }
}
static void fx_glitch(u32 *buf, int w, int h, int amt) {
    u32 *s = copy_tmp(buf, w, h);
    int bands, bi, x, y;
    s_rng = (uint32_t)(g.now_ms / 90) * 2246822519u + 1;      // новый набop каждые ~90мс
    bands = amt / 14 + 2;
    for (bi = 0; bi < bands; bi++) {
        int y0 = (int)(xr() % (uint32_t)h), bh = 6 + (int)(xr() % 26);
        int dx = (int)(xr() % (uint32_t)(amt * 2 + 1)) - amt;
        int cs = (int)(xr() % 7) - 3;                         // доп. расщепление канала в полосе
        for (y = y0; y < y0 + bh && y < h; y++) {
            u32 *row = buf + (size_t)y * w, *sr = s + (size_t)y * w;
            for (x = 0; x < w; x++) {
                int sx = clampi(x - dx, 0, w - 1);
                u32 p = sr[sx];
                int a = (p >> 24) & 255;
                int r = (sr[clampi(sx - cs, 0, w - 1)] >> 16) & 255, g2 = (p >> 8) & 255, b = sr[clampi(sx + cs, 0, w - 1)] & 255;
                row[x] = pack(a, r, g2, b);
            }
        }
    }
}
static void fx_mix_mono(u32 *buf, int w, int h, int amt, int mode) { // 0 grayscale,1 sepia,2 invert
    double k = clampi(amt, 0, 100) / 100.0;
    int i;
    for (i = 0; i < w * h; i++) {
        u32 p = buf[i];
        int a = (p >> 24) & 255, r, g2, b, nr, ng, nb;
        if (!a) continue;
        r = (p >> 16) & 255; g2 = (p >> 8) & 255; b = p & 255;
        if (mode == 0) { int y = lum(p); nr = ng = nb = y; }
        else if (mode == 1) { int y = lum(p); nr = y * 112 / 100; ng = y * 94 / 100; nb = y * 70 / 100; }
        else { nr = 255 - r; ng = 255 - g2; nb = 255 - b; }
        buf[i] = pack(a, (int)(r + (nr - r) * k), (int)(g2 + (ng - g2) * k), (int)(b + (nb - b) * k));
    }
}
static void fx_pixelate(u32 *buf, int w, int h, int amt) {
    int bs = clampi(amt, 2, 64), bx, by, x, y;
    for (by = 0; by < h; by += bs) for (bx = 0; bx < w; bx += bs) {
        long r = 0, g2 = 0, b = 0, a = 0, n = 0;
        u32 avg;
        for (y = by; y < by + bs && y < h; y++) for (x = bx; x < bx + bs && x < w; x++) {
            u32 p = buf[(size_t)y * w + x];
            r += (p >> 16) & 255; g2 += (p >> 8) & 255; b += p & 255; a += (p >> 24) & 255; n++;
        }
        if (!n) continue;
        avg = pack((int)(a / n), (int)(r / n), (int)(g2 / n), (int)(b / n));
        for (y = by; y < by + bs && y < h; y++) for (x = bx; x < bx + bs && x < w; x++) buf[(size_t)y * w + x] = avg;
    }
}

// раздельное размытие (гориз или верт) ярких точек
static void box_blur(const u32 *src, u32 *dst, int w, int h, int r, int horiz) {
    int x, y, k;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        long sr = 0, sg = 0, sb = 0;
        int n = 0;
        for (k = -r; k <= r; k++) {
            int xx = horiz ? clampi(x + k, 0, w - 1) : x, yy = horiz ? y : clampi(y + k, 0, h - 1);
            u32 p = src[(size_t)yy * w + xx];
            sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255; n++;
        }
        dst[(size_t)y * w + x] = pack(255, (int)(sr / n), (int)(sg / n), (int)(sb / n));
    }
}

static void fx_bloom(u32 *buf, int w, int h, int amt) {
    int r = clampi(amt / 10, 2, 10), i;
    double k;
    u32 *bright = (u32 *)malloc((size_t)w * h * sizeof(u32));
    u32 *tmp = (u32 *)malloc((size_t)w * h * sizeof(u32));
    if (!bright || !tmp) { free(bright); free(tmp); return; }
    for (i = 0; i < w * h; i++) { u32 p = buf[i]; bright[i] = (lum(p) > 170) ? p : 0xFF000000u; }
    box_blur(bright, tmp, w, h, r, 1);
    box_blur(tmp, bright, w, h, r, 0);
    k = clampi(amt, 0, 100) / 100.0 * 0.9;
    for (i = 0; i < w * h; i++) {
        u32 p = buf[i], gl = bright[i];
        int a = (p >> 24) & 255;
        buf[i] = pack(a, (int)((p >> 16) & 255) + (int)(((gl >> 16) & 255) * k),
                         (int)((p >> 8) & 255) + (int)(((gl >> 8) & 255) * k),
                         (int)(p & 255) + (int)((gl & 255) * k));
    }
    free(bright); free(tmp);
}

void apply_effects(void) {
    int w = g.back_w, h = g.back_h, i;
    u32 *buf = (u32 *)g.dibbits;
    if (!g.effects.n || !g.dibbits || w <= 0 || h <= 0) return;
    for (i = 0; i < g.effects.n; i++) {
        const char *n = S(g.effects.a[i].name);
        int a = g.effects.a[i].amount;
        if (!strcmp(n, "chromatic") || !strcmp(n, "dispersion")) fx_chromatic(buf, w, h, a);
        else if (!strcmp(n, "scanlines")) fx_scanlines(buf, w, h, a);
        else if (!strcmp(n, "noise")) fx_noise(buf, w, h, a);
        else if (!strcmp(n, "vignette")) fx_vignette(buf, w, h, a);
        else if (!strcmp(n, "glitch")) fx_glitch(buf, w, h, a);
        else if (!strcmp(n, "grayscale")) fx_mix_mono(buf, w, h, a, 0);
        else if (!strcmp(n, "sepia")) fx_mix_mono(buf, w, h, a, 1);
        else if (!strcmp(n, "invert")) fx_mix_mono(buf, w, h, a, 2);
        else if (!strcmp(n, "pixelate")) fx_pixelate(buf, w, h, a);
        else if (!strcmp(n, "bloom") || !strcmp(n, "glow")) fx_bloom(buf, w, h, a);
    }
    if (effects_animated()) request_redraw();
}

int effects_animated(void) {
    int i;
    for (i = 0; i < g.effects.n; i++) {
        const char *n = S(g.effects.a[i].name);
        if (!strcmp(n, "glitch") || !strcmp(n, "noise")) return 1;
    }
    return 0;
}

void el_effect(const char *name, int amount) {
    int found = 0, i;
    if (!name || !*name) return;
    for (i = 0; i < g.effects.n; i++) if (seq(g.effects.a[i].name, name)) { g.effects.a[i].amount = amount; found = 1; }
    if (!found && amount > 0) { Effect e; e.name = sdup(name); e.amount = amount; VPUSH(g.effects, e); }
    for (i = 0; i < g.effects.n;) {
        if (g.effects.a[i].amount <= 0) { free(g.effects.a[i].name); VERASE(g.effects, i); }
        else i++;
    }
    request_redraw();
}
void el_effects_clear(void) {
    int i;
    for (i = 0; i < g.effects.n; i++) free(g.effects.a[i].name);
    g.effects.n = 0;
    request_redraw();
}
