// Окно произвольной формы: форма задаётся непрозрачностью содержимого
// (el_window_shape("pixels")). Скруглённый градиентный фон даёт «пилюлю» с
// гладкими краями — её можно таскать за фон, а закрыть крестиком или Esc.
#include <elements.h>

void on_close(el e, void *arg) { el_quit(); }

int main(void)
{
    el w = el_window("Shape", 520, 120);
    el_window_shape("pixels");           // форма окна — по непрозрачности рисунка

    // фон во всё окно: скруглённый градиент. Его скругление и есть форма окна.
    el bg = el_border(w);
    el_style(bg, "gradient:#6a3cc0 #2a8bff horizontal; radius:60; padding:0 16");
    el_drag_region(bg);                  // тянем окно за фон

    el row = el_grid(bg, "*,Auto", "*");
    el t = el_text(row, "A shaped window: drag it by the background");
    el_style(t, "color:#ffffff; font-size:16; font-weight:bold");
    el_align(t, EL_START, EL_CENTER);
    el x = el_button(row, "\xe2\x9c\x95");
    el_cell(x, 0, 1, 1, 1);
    el_align(x, EL_CENTER, EL_CENTER);
    el_style(x, "background:#ffffff; background-alpha:55; color:#ffffff; radius:16; width:32; height:32; padding:0 0; hover-background:#ffffff");
    el_on_click(x, on_close, 0);

    el_hotkey("Esc", on_close, 0);
    el_run();
    return 0;
}
