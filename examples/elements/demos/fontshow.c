// Нестандартное окно-витрина 30 собственных шрифтов Elements: без системной рамки,
// своей формы (скруглённая градиентная панель, форма — по непрозрачности рисунка),
// своя шапка (тянем за неё; заголовок набран нашей «дидоной»), стеклянные карточки,
// в каждой — образец своим шрифтом. Размер окна меняется за края, Esc или ✕ — закрыть.
#include "elm_fonts.h"
#include <stdio.h>

static int ok[64];

static void on_close(el e, void *a) { el_quit(); }

int main(void)
{
    el w = el_window("Elements Fonts", 1000, 740);
    el_window_shape("pixels");
    el_window_minmax(560, 420, 0, 0);
    int loaded = elm_load_fonts(ok);

    el bg = el_border(w);
    el_style(bg, "gradient:#1e1244 #08192f diagonal; radius:28; border:#7a66e0; border-width:1; padding:0");
    el root = el_dock(bg);

    // своя шапка
    el head = el_border(root);
    el_dock_side(head, EL_TOP);
    el_style(head, "background:#00000000; padding:18 22 10 30");
    el_drag_region(head);
    el hg = el_grid(head, "*,Auto", "Auto,Auto");
    el title = el_text(hg, "30 Elements fonts");
    el_style(title, "font-size:34; color:#f4eeff; font-family:Elm Didone");
    char sub_text[160];
    _snprintf(sub_text, sizeof sub_text, "Drawn by a program: letter skeletons \xe2\x86\x92 pen \xe2\x86\x92 TrueType  \xc2\xb7  %d of %d loaded", loaded, ELM_FONT_COUNT);
    el sub = el_text(hg, sub_text);
    el_cell(sub, 1, 0, 1, 1);
    el_style(sub, "font-size:14; color:#a99bd8; font-family:Elm Humanist");
    el x = el_button(hg, "\xe2\x9c\x95");
    el_cell(x, 0, 1, 2, 1);
    el_align(x, EL_CENTER, EL_START);
    el_style(x, "background:#ffffff; background-alpha:34; color:#ffffff; radius:17; width:34; height:34; padding:0 0; hover-background:#ff5d8f");
    el_on_click(x, on_close, 0);

    // список шрифтов
    el sc = el_scroll(root);
    el_margin(sc, 20, 4, 20, 24);
    el list = el_stack(sc, 0);
    el_gap(list, 8);
    const int pal[] = { 0xffd27a, 0x7af0ff, 0xd29bff, 0xff8ec0, 0x8affc1, 0xffae7a };
    char label[64];
    for (int i = 0; i < ELM_FONT_COUNT; i++) {
        el card = el_border(list);
        el_style(card, "background:#ffffff; background-alpha:13; radius:16; padding:10 20 12 20; hover-background:#ffffff; hover-border:#9d8cff; border:#ffffff; border-width:1");
        el cs = el_stack(card, 0);
        el_gap(cs, 0);
        _snprintf(label, sizeof label, "%02d  %s", i + 1, ELM_FONTS[i].name);
        el nm = el_text(cs, label);
        el_style(nm, "font-size:12; color:#9d92c9; font-family:Elm Grotesk");
        el smp = el_text(cs, ok[i] ? "Съешь же ещё этих мягких булок \xe2\x80\x94 Quick Brown Fox 0123" : "(font not found)");
        if (ok[i]) { el_font(smp, 30, 0); el_font_face(smp, ELM_FONTS[i].name); el_colors(smp, pal[i % 6], -1); }
        else el_colors(smp, 0xe06c75, -1);
    }

    el_hotkey("Esc", on_close, 0);
    el_run();
    return 0;
}
