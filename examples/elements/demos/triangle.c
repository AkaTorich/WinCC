// Треугольное окно с треугольными кнопками. Форма окна задаётся тем, что мы
// рисуем (el_window_shape("pixels")) — непрозрачный треугольник и есть окно.
// Кнопки — поверхности el_paint: рисуем треугольник, попадание проверяем точно
// (точка в треугольнике), фон окна можно таскать, прозрачные углы «сквозные».
#include <elements.h>
#include <stdio.h>

int val = 50;
int inMinus, inPlus, inClose;
char info[64] = "Value: 50";

// знак ориентации и проверка «точка в треугольнике»
static long cr(int ax, int ay, int bx, int by, int cx, int cy) { return (long)(ax - cx) * (by - cy) - (long)(bx - cx) * (ay - cy); }
static int in_tri(int px, int py, const int *t) {
    long d1 = cr(px, py, t[0], t[1], t[2], t[3]);
    long d2 = cr(px, py, t[2], t[3], t[4], t[5]);
    long d3 = cr(px, py, t[4], t[5], t[0], t[1]);
    int neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

// фон во всё окно: сам треугольник + заголовок и значение
void bg_draw(el e, int w, int h, void *arg)
{
    int tri[6] = { w / 2, 8, 10, h - 10, w - 10, h - 10 };
    el_g_poly(tri, 3, 0x1b2550);
    int topt[6] = { w / 2, 8, w / 2 - 78, 150, w / 2 + 78, 150 };
    el_g_poly(topt, 3, 0x27387f);
    el_g_poly_frame(tri, 3, 0x5a7ee0, 2);
    el_g_text(w / 2 - el_g_text_w("Triangle window", 17, 1) / 2, 64, "Triangle window", 0xeaf0ff, 17, 1);
    el_g_text(w / 2 - el_g_text_w(info, 16, 1) / 2, h - 150, info, 0xbfe0ff, 16, 1);
}

void tri_button(el e, const int *t, int *inside, int base, int hot, const char *label)
{
    int mx, my, b;
    int over = el_mouse(e, &mx, &my, &b);
    *inside = over && in_tri(mx, my, t);
    el_g_poly(t, 3, *inside ? hot : base);
    el_g_poly_frame(t, 3, 0xdfe8ff, 2);
    if (label && *label) {
        int cx = (t[0] + t[2] + t[4]) / 3, cy = (t[1] + t[3] + t[5]) / 3;
        el_g_text(cx - el_g_text_w(label, 18, 1) / 2, cy - 13, label, 0xffffff, 18, 1);
    }
}

void p_minus(el e, int w, int h, void *a) { int t[6] = { w - 8, 8, w - 8, h - 8, 8, h / 2 }; tri_button(e, t, &inMinus, 0x37509c, 0x4f73d6, "\xe2\x88\x92"); }
void p_plus(el e, int w, int h, void *a)  { int t[6] = { 8, 8, 8, h - 8, w - 8, h / 2 };     tri_button(e, t, &inPlus,  0x37509c, 0x4f73d6, "+"); }
void p_close(el e, int w, int h, void *a) { int t[6] = { w / 2, 8, w - 8, h - 8, 8, h - 8 }; tri_button(e, t, &inClose, 0xb23a4a, 0xff5d6c, "\xe2\x9c\x95"); }

void on_minus(el e, void *a) { if (inMinus) { val -= 5; if (val < 0) val = 0; sprintf(info, "Value: %d", val); } }
void on_plus(el e, void *a)  { if (inPlus)  { val += 5; if (val > 100) val = 100; sprintf(info, "Value: %d", val); } }
void on_close(el e, void *a) { if (inClose) el_quit(); }
void on_esc(el e, void *a) { el_quit(); }

int main(void)
{
    el w = el_window("Triangle", 480, 430);
    el_window_shape("pixels");           // форма окна — по непрозрачности рисунка (треугольник)
    el_window_canvas(bg_draw, 0);

    el root = el_dock(w);
    el_drag_region(root);                // тянем окно за сам треугольник

    el bottom = el_stack(root, 1);
    el_dock_side(bottom, EL_BOTTOM);
    el_align(bottom, EL_CENTER, EL_END);
    el_margin(bottom, 0, 0, 0, 24);
    el_gap(bottom, 14);
    el bm = el_paint(bottom, p_minus, 0); el_size(bm, 96, 74); el_on_click(bm, on_minus, 0);
    el bc = el_paint(bottom, p_close, 0); el_size(bc, 74, 74); el_on_click(bc, on_close, 0);
    el bp = el_paint(bottom, p_plus, 0);  el_size(bp, 96, 74); el_on_click(bp, on_plus, 0);

    el_hotkey("Esc", on_esc, 0);
    el_run();
    return 0;
}
