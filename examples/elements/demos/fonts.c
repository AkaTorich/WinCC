// 30 собственных шрифтов Elements в обычном окне: имя и образец строки каждым шрифтом.
// Шрифты нарисованы программой tools/fontgen (оси букв -> перо -> TrueType).
#include "elm_fonts.h"
#include <stdio.h>

int main(void)
{
    el w = el_window("30 Elements fonts", 900, 680);
    el_window_minmax(480, 320, 0, 0);

    int ok[64];
    elm_load_fonts(ok);

    el page = el_stack(w, 0);
    el_padding(page, 20, 18, 20, 18); el_gap(page, 4);
    el head = el_text(page, "30 Elements fonts");
    el_style(head, "font-size:26; font-family:Elm Grotesk Bold");
    el sub = el_text(page, "Our own fonts: tools/fontgen draws letter skeletons with a pen into TrueType. Latin, Cyrillic, digits, symbols.");
    el_style(sub, "color:#8fa3b8; font-size:13; margin:0 0 8 0");

    el sc = el_scroll(page);
    el list = el_stack(sc, 0); el_gap(list, 2);

    char label[64];
    for (int i = 0; i < ELM_FONT_COUNT; i++) {
        el row = el_border(list);
        el_style(row, "background:#161b24; radius:8; padding:10 14; margin:0 0 2 0");
        el g = el_grid(row, "210,*", "Auto"); el_gap(g, 12);
        _snprintf(label, sizeof label, "%2d. %s", i + 1, ELM_FONTS[i].name);
        el nm = el_text(g, label);
        el_style(nm, "color:#c9d1d9; font-size:13");
        el_align(nm, EL_START, EL_CENTER);
        el smp = el_text(g, ok[i] ? "Съешь же ещё этих мягких булок — Quick Brown Fox 0123" : "(not found)");
        el_cell(smp, 0, 1, 1, 1);
        if (ok[i]) { el_font(smp, 22, 0); el_font_face(smp, ELM_FONTS[i].name); }
        else el_style(smp, "color:#e06c75");
    }

    el_run();
    return 0;
}
