// Окно в духе Cyberpunk 2077: нестандартная угловатая форма со срезанными
// углами, дисперсия (RGB-расщепление каналов), неон жёлтый/голубой/розовый,
// строки развёртки, глитч-блоки. Кнопки — угловатые панели с неоном.
#include <elements.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define YEL 0xfcee0a
#define CYA 0x00f0ff
#define MAG 0xff2e6e
#define BG  0x0a0a14

int inHack, inScan, inExit;
char line1[48] = "ICE: breached";
char line2[48] = "RAM: 64%   NET: 7 nodes";

// дисперсию теперь даёт встроенный эффект окна el_effect("chromatic") — рисуем просто
static void disp_text(int x, int y, const char *s, int rgb, int size, int bold)
{
    el_g_text(x, y, s, rgb, size, bold);
}

// угловатая панель со срезанными углами (6 вершин) в прямоугольнике w×h от (ox,oy)
static void bevel(int ox, int oy, int w, int h, int bev, int *out)
{
    int p[12] = { ox + bev, oy, ox + w, oy, ox + w, oy + h - bev, ox + w - bev, oy + h, ox, oy + h, ox, oy + bev };
    for (int i = 0; i < 12; i++) out[i] = p[i];
}

void bg_draw(el e, int w, int h, void *arg)
{
    int pan[12]; bevel(0, 0, w, h, 30, pan);
    el_g_poly(pan, 6, BG);                               // форма окна — угловатая панель (ровные срезы)
    el_g_poly_frame(pan, 6, YEL, 2);                     // жёлтый неон-край; дисперсию/развёртку/свечение даёт движок эффектов

    // верхняя жёлтая полоса-заголовок
    el_g_fill(30, 0, w - 30, 38, YEL);
    el_g_text(44, 8, "CYBERDECK // 2077", 0x0a0a14, 16, 1);
    el_g_fill(w - 150, 10, 120, 18, 0x0a0a14);
    el_g_text(w - 142, 10, "NETRUNNER", YEL, 13, 1);

    // глитч-блоки
    el_g_fill(40, 70, 90, 10, MAG);
    el_g_fill(44, 70, 90, 10, CYA);
    el_g_fill(42, 70, 90, 10, YEL);

    // неоновые акценты по бокам
    el_g_fill(16, 60, 3, h - 150, CYA);
    el_g_fill(w - 19, 60, 3, h - 150, MAG);
    el_g_line(30, 100, w - 30, 100, MAG, 1);

    // дисперсные надписи
    disp_text(44, 120, "// ACCESS GRANTED", YEL, 22, 1);
    disp_text(44, 160, line1, CYA, 16, 1);
    disp_text(44, 184, line2, 0xffffff, 15, 0);

    // «спектр дисперсии» — полоса из цветных линий
    for (int i = 0; i < 60; i++) {
        double t = i / 60.0;
        int r = (int)(255 * fabs(sin(t * 3.1)));
        int g = (int)(255 * fabs(sin(t * 3.1 + 2.0)));
        int b = (int)(255 * fabs(sin(t * 3.1 + 4.0)));
        el_g_fill(44 + i * ((w - 90) / 60), 212, (w - 90) / 60 + 1, 10, (r << 16) | (g << 8) | b);
    }
    el_g_text(44, 228, "SIGNAL DISPERSION", MAG, 12, 1);
}

static void cyber_button(el e, int side_w, int side_h, int neon, const char *label, int *inside)
{
    int mx, my, b; int over = el_mouse(e, &mx, &my, &b);
    *inside = over && mx >= 0 && my >= 0 && mx < side_w && my < side_h;
    int bv[12]; bevel(0, 0, side_w, side_h, 9, bv);
    el_g_poly(bv, 6, *inside ? 0x1a1a2e : 0x101020);
    if (*inside) { int s1[12]; bevel(-2, 0, side_w, side_h, 9, s1); el_g_poly_frame(s1, 6, MAG, 1);
                   int s2[12]; bevel(2, 0, side_w, side_h, 9, s2); el_g_poly_frame(s2, 6, CYA, 1); }
    el_g_poly_frame(bv, 6, neon, 2);
    int tw = el_g_text_w(label, 14, 1);
    if (*inside) disp_text(side_w / 2 - tw / 2, side_h / 2 - 10, label, neon, 14, 1);
    else el_g_text(side_w / 2 - tw / 2, side_h / 2 - 10, label, neon, 14, 1);
}

void p_hack(el e, int w, int h, void *a) { cyber_button(e, w, h, CYA, "HACK", &inHack); }
void p_scan(el e, int w, int h, void *a) { cyber_button(e, w, h, YEL, "SCAN", &inScan); }
void p_exit(el e, int w, int h, void *a) { cyber_button(e, w, h, MAG, "EXIT", &inExit); }

void on_hack(el e, void *a) { if (inHack) { strcpy(line1, "ICE: core breach"); strcpy(line2, "ACCESS: root   TRACE: 0%"); } }
void on_scan(el e, void *a) { if (inScan) { strcpy(line1, "SCAN: 3 daemons"); strcpy(line2, "RAM: 51%   NET: 12 nodes"); } }
void on_exit(el e, void *a) { if (inExit) el_quit(); }
void on_esc(el e, void *a) { el_quit(); }

int main(void)
{
    el w = el_window("Cyberpunk", 560, 420);
    el_window_shape("pixels");            // форма окна — по непрозрачности рисунка (угловатая панель)
    el_window_canvas(bg_draw, 0);

    // встроенные эффекты окна (мягко, чтобы надписи оставались читаемыми)
    el_effect("chromatic", 2);
    el_effect("scanlines", 22);
    el_effect("bloom", 42);
    el_effect("vignette", 20);

    el root = el_dock(w);
    el_drag_region(root);

    el bar = el_stack(root, 1);
    el_dock_side(bar, EL_BOTTOM);
    el_align(bar, EL_CENTER, EL_END);
    el_margin(bar, 0, 0, 0, 30);
    el_gap(bar, 18);
    el bh = el_paint(bar, p_hack, 0); el_size(bh, 150, 52); el_on_click(bh, on_hack, 0);
    el bs = el_paint(bar, p_scan, 0); el_size(bs, 130, 52); el_on_click(bs, on_scan, 0);
    el be = el_paint(bar, p_exit, 0); el_size(be, 130, 52); el_on_click(be, on_exit, 0);

    el_hotkey("Esc", on_esc, 0);
    el_run();
    return 0;
}
