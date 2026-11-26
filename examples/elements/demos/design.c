// Пример оформления строкой (как CSS): градиентный фон, стеклянные карточки с
// тенью, градиентные кнопки с плавным наведением, крупные заголовки, плитки.
#include <elements.h>
#include <stdio.h>

int level = 64, power = 1;
char query[64];
char status[96] = "Hover the mouse over the tiles and buttons.";

void go(el e, void *arg) { sprintf(status, "Started: \"%s\", level %d", query[0] ? query : "untitled", level); }

el card(el parent, const char *title, const char *text, const char *grad)
{
    el c = el_border(parent);
    el_style(c, "background:#ffffff; background-alpha:28; border:#5d6f9e; border-width:1; radius:14; padding:16 18;"
                "shadow:#000000 22 0 8 110; hover-background:#ffffff; hover-border:#ffd27a");
    el_size(c, 230, -1);
    el s = el_stack(c, 0);
    el_gap(s, 6);
    el dot = el_border(s);
    el_style(dot, grad);
    el_size(dot, 40, 6);
    el_align(dot, EL_START, EL_START);
    el t = el_text(s, title);
    el_style(t, "font-size:18; font-weight:bold; color:#ffffff");
    el dd = el_text(s, text);
    el_wrap_text(dd, 1);
    el_style(dd, "color:#dbe4f3; font-size:13");
    return c;
}

int main(void)
{
    el w = el_window("Elements Design", 860, 600);
    el_style(w, "gradient:#1b1446 #0e5a74 diagonal");
    el_accent(0xffb347);
    el_corner_radius(10);

    el page = el_stack(w, 0);
    el_padding(page, 32, 28, 32, 24);
    el_gap(page, 18);

    el title = el_text(page, "Control Panel");
    el_style(title, "font-size:30; font-weight:bold; color:#ffffff");
    el sub = el_text(page, "Gradients, glass, shadows and smooth hover, all styled with a string, like CSS.");
    el_style(sub, "color:#b9c6dd; font-size:14");

    el bar = el_border(page);
    el_style(bar, "background:#000000; background-alpha:70; radius:12; padding:10 12; shadow:#000000 16 0 4 90");
    el row = el_grid(bar, "*,Auto,Auto", "Auto");
    el_gap(row, 10);
    el q = el_edit(row, query, sizeof(query));
    el_style(q, "radius:8");
    el_tooltip(q, "Task name");
    el_on_click(q, go, 0);
    el chk = el_check(row, "Power", &power);
    el_cell(chk, 0, 1, 1, 1);
    el_align(chk, EL_START, EL_CENTER);
    el_style(chk, "color:#ffffff");
    el b = el_button(row, "Run");
    el_cell(b, 0, 2, 1, 1);
    el_style(b, "gradient:#ff8a3d #ff3d77 horizontal; color:#ffffff; font-weight:bold; radius:10; padding:8 22;"
                "shadow:#ff3d77 18 0 6 120; hover-border:#ffffff; border-width:1; border:#ff9a6a");
    el_on_click(b, go, 0);

    el tiles = el_wrap(page);
    el_gap(tiles, 18);
    card(tiles, "Tablet", "Ciphers: AES-GCM, XChaCha20 and an install seal.", "gradient:#ffd27a #ff8a3d horizontal; radius:3");
    card(tiles, "Ankh", "Hieroglyphic archiver, seals and imprints.", "gradient:#7af0ff #3d7bff horizontal; radius:3");
    card(tiles, "Tarot", "Decks, spreads and interpretations.", "gradient:#c77dff #ff5ea8 horizontal; radius:3");

    el lv = el_border(page);
    el_style(lv, "background:#ffffff; background-alpha:22; radius:12; padding:14 18");
    el lg = el_grid(lv, "Auto,*,60", "Auto,Auto");
    el_gap(lg, 10);
    el lt = el_text(lg, "Level");
    el_style(lt, "color:#ffffff; font-weight:bold");
    el sl = el_slider(lg, 0, 100, &level);
    el_cell(sl, 0, 1, 1, 1);
    el pr = el_progress(lg, 100, &level);
    el_cell(pr, 1, 1, 1, 1);
    el_style(pr, "color:#ff8a3d");

    el stt = el_text(page, "");
    el_text_bind(stt, status);
    el_style(stt, "color:#ffd27a; font-family:mono; font-size:13");

    el_run();
    return 0;
}
