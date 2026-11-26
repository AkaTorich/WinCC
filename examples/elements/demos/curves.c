// Окно-«капля» со скруглёнными кривыми краями и круглыми (кривыми) кнопками.
// Форма окна — гладкая замкнутая кривая (el_g_blob); украшения — синусоида
// (el_g_spline) и дуга (el_g_arc). Кнопки — круги с точной проверкой попадания.
#include <elements.h>
#include <stdio.h>
#include <math.h>

int val = 50;
int inMinus, inClose, inPlus;
char info[64] = "Value: 50";

static void blob_points(int w, int h, int *out, int n)
{
    double cx = w / 2.0, cy = h / 2.0;
    double rx = w * 0.46, ry = h * 0.44;
    for (int i = 0; i < n; i++) {
        double a = 2.0 * 3.14159265 * i / n;
        double k = 1.0 + 0.07 * sin(3 * a) + 0.05 * cos(2 * a);
        out[i * 2] = (int)(cx + rx * k * cos(a));
        out[i * 2 + 1] = (int)(cy + ry * k * sin(a));
    }
}

void bg_draw(el e, int w, int h, void *arg)
{
    int pts[2 * 14]; blob_points(w, h, pts, 14);
    el_g_blob(pts, 14, 0x241a40);               // сама «капля» — она же форма окна
    int inner[2 * 14]; for (int i = 0; i < 14; i++) { inner[i * 2] = (pts[i * 2] - w / 2) * 92 / 100 + w / 2; inner[i * 2 + 1] = (pts[i * 2 + 1] - h / 2) * 92 / 100 + h / 2; }
    el_g_blob(inner, 14, 0x301f52);
    // синусоида поперёк (гладкая кривая через точки)
    int wave[2 * 13];
    for (int i = 0; i < 13; i++) { wave[i * 2] = 60 + i * (w - 120) / 12; wave[i * 2 + 1] = h / 2 + (int)(26 * sin(i * 0.9)); }
    el_g_spline(wave, 13, 0x8a6cff, 3);
    // дуга-кольцо вокруг центра
    el_g_arc(w / 2, h / 2 - 6, 120, 200, 140, 0x6f5ad0, 3);
    el_g_text(w / 2 - el_g_text_w("Curved shapes", 18, 1) / 2, h / 2 - 80, "Curved shapes", 0xeae2ff, 18, 1);
    el_g_text(w / 2 - el_g_text_w(info, 15, 1) / 2, h / 2 - 48, info, 0xc9bcff, 15, 1);
}

// круглая кнопка: размер поверхности side, центр (side/2), точная проверка по радиусу
static void circle_button(el e, int side, int base, int hot, const char *label, int *inside)
{
    int mx, my, b; int over = el_mouse(e, &mx, &my, &b);
    int c = side / 2, r = side / 2 - 4;
    long dx = mx - c, dy = my - c;
    *inside = over && (dx * dx + dy * dy <= (long)r * r);
    el_g_disc(c, c, r, *inside ? hot : base);
    el_g_ring(c, c, r, 2, 0xe8e0ff);
    if (label && *label) el_g_text(c - el_g_text_w(label, 19, 1) / 2, c - 13, label, 0xffffff, 19, 1);
}

void p_minus(el e, int w, int h, void *a) { circle_button(e, w, 0x4a3b86, 0x6f5ad0, "\xe2\x88\x92", &inMinus); }
void p_close(el e, int w, int h, void *a) { circle_button(e, w, 0xb23a66, 0xff5d8f, "\xe2\x9c\x95", &inClose); }
void p_plus(el e, int w, int h, void *a)  { circle_button(e, w, 0x4a3b86, 0x6f5ad0, "+", &inPlus); }

void on_minus(el e, void *a) { if (inMinus) { val -= 5; if (val < 0) val = 0; sprintf(info, "Value: %d", val); } }
void on_plus(el e, void *a)  { if (inPlus)  { val += 5; if (val > 100) val = 100; sprintf(info, "Value: %d", val); } }
void on_close(el e, void *a) { if (inClose) el_quit(); }
void on_esc(el e, void *a) { el_quit(); }

int main(void)
{
    el w = el_window("Curves", 520, 430);
    el_window_shape("pixels");
    el_window_canvas(bg_draw, 0);

    el root = el_dock(w);
    el_drag_region(root);

    el bottom = el_stack(root, 1);
    el_dock_side(bottom, EL_BOTTOM);
    el_align(bottom, EL_CENTER, EL_END);
    el_margin(bottom, 0, 0, 0, 40);
    el_gap(bottom, 20);
    el bm = el_paint(bottom, p_minus, 0); el_size(bm, 72, 72); el_on_click(bm, on_minus, 0);
    el bc = el_paint(bottom, p_close, 0); el_size(bc, 72, 72); el_on_click(bc, on_close, 0);
    el bp = el_paint(bottom, p_plus, 0);  el_size(bp, 72, 72); el_on_click(bp, on_plus, 0);

    el_hotkey("Esc", on_esc, 0);
    el_run();
    return 0;
}
