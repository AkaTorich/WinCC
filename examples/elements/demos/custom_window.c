// Нестандартное окно: без системной рамки, со скруглёнными углами, своей
// «шапкой» (тянем за неё), своей кнопкой закрытия, фоном, который рисуем сами,
// и заблокированными пропорциями 16:10.
#include <elements.h>
#include <math.h>

int level = 55, mode;
const char *modes[] = { "Ether", "Flame", "Stream", "Stone" };

void on_close(el e, void *arg) { el_quit(); }

// фон во весь размер окна рисуем сами (el_g_* в точках, (0,0) — угол окна)
void bg_draw(el e, int w, int h, void *arg)
{
    el_g_round(0, 0, w, h, 22, 0x101a30);                 // основа со скруглением
    // мягкая «шапка» градиентной полосой
    for (int y = 0; y < 72; y++) { int a = 150 - y * 2; if (a < 0) a = 0; el_g_shade(0, y, w, 1, 0x3a6ff0, a / 3); }
    // декоративные кольца в углу и диск внизу
    for (int i = 0; i < 6; i++) el_g_ring(w - 54, 84, 30 + i * 16, 1, 0x24406e);
    el_g_disc(64, h - 54, 40, 0x172a4c);
}

int main(void)
{
    el w = el_window("Custom Window", 600, 375);
    el_frameless(1);                 // без системной рамки — рисуем всё сами
    el_window_round(22);             // скруглённые углы окна
    el_window_aspect(16, 10);        // держать пропорции при изменении размера
    el_window_minmax(400, 250, 0, 0);
    el_window_canvas(bg_draw, 0);    // фон окна — своя отрисовка

    el root = el_dock(w);

    // своя «шапка»: за неё тянем окно
    el head = el_border(root);
    el_dock_side(head, EL_TOP);
    el_size(head, -1, 52);
    el_style(head, "background:#00000000; padding:10 16");
    el_drag_region(head);
    el hg = el_grid(head, "*,Auto", "Auto");
    el ttl = el_text(hg, "Elements \xc2\xb7 custom window");
    el_style(ttl, "color:#eaf0ff; font-size:16; font-weight:bold");
    el_align(ttl, EL_START, EL_CENTER);
    el x = el_button(hg, "\xe2\x9c\x95");
    el_cell(x, 0, 1, 1, 1);
    el_align(x, EL_CENTER, EL_CENTER);
    el_style(x, "background:#e8536a; color:#ffffff; radius:14; width:28; height:28; padding:0 0; hover-background:#ff6a7f");
    el_on_click(x, on_close, 0);

    // содержимое
    el body = el_stack(root, 0);
    el_padding(body, 22, 10, 22, 22);
    el_gap(body, 14);

    el sub = el_text(body, "A frameless window: its own corners, title bar, background and 16:10 proportions.");
    el_wrap_text(sub, 1);
    el_style(sub, "color:#aab8d6; font-size:13");

    el card = el_border(body);
    el_style(card, "background:#ffffff; background-alpha:16; radius:14; padding:16 18; shadow:#000000 20 0 6 90");
    el cg = el_grid(card, "Auto,*,Auto", "Auto,Auto");
    el_gap(cg, 12);
    el lt = el_text(cg, "Mode:");
    el_style(lt, "color:#eaf0ff; font-weight:bold");
    el cb = el_combo(cg, modes, 4, &mode);
    el_cell(cb, 0, 1, 1, 2);
    el_align(cb, EL_START, EL_CENTER);
    el lv = el_text(cg, "Power:");
    el_cell(lv, 1, 0, 1, 1);
    el_style(lv, "color:#eaf0ff; font-weight:bold");
    el sl = el_slider(cg, 0, 100, &level);
    el_cell(sl, 1, 1, 1, 1);
    el pr = el_progress(cg, 100, &level);
    el_cell(pr, 1, 2, 1, 1);
    el_style(pr, "color:#3a6ff0; width:60");

    el hint = el_text(body, "Drag the title bar to move the window. Drag an edge to resize it; the proportions are kept.");
    el_wrap_text(hint, 1);
    el_style(hint, "color:#6f82a8; font-size:12");

    el_hotkey("Esc", on_close, 0);
    el_run();
    return 0;
}
