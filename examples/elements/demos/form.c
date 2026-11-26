// Пример окна как в WPF: меню, вкладки, сетка формы, список и многострочное
// поле, раскладка с переносом, подсказки, тема. (Перенос примера DyOS.)
#include <elements.h>
#include <stdio.h>
#include <string.h>

char name[64] = "Anna";
char pass[64];
char notes[2048] = "Multi-line field.\nSelect with the mouse and Shift+arrows,\nCtrl+A, Ctrl+C, Ctrl+X, Ctrl+V.";
int city, agree = 1, sz = 1, volume = 40, progress = 40, item, dark = 1;
char status[160] = "Ready.";
const char *cities[] = { "Moscow", "Jerusalem", "Tel Aviv", "Saint Petersburg", "Haifa", "Kazan", "Eilat" };
const char *planets[] = { "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune",
                          "Pluto", "Ceres", "Eris", "Haumea", "Makemake" };
el g_tabs;

void say_hello(el e, void *arg) { sprintf(status, "Hello, %s! City: %s, size: %d.", name, cities[city], sz); }
void on_volume(el e, void *arg) { progress = volume; sprintf(status, "Volume %d", volume); }
void on_planet(el e, void *arg) { sprintf(status, "Planet selected: %s", planets[item]); }
void on_theme(el e, void *arg) { el_theme(dark); }
void on_quit(el e, void *arg) { strcpy(status, "Close the window with the cross."); }
void on_tab(el e, void *arg) { sprintf(status, "Tab %d", el_selected_tab(g_tabs) + 1); }
void on_about(el e, void *arg) { el_select_tab(g_tabs, 2); strcpy(status, "Elements: windows like in WPF."); }

int main(void)
{
    el w = el_window("Elements for Windows", 800, 580);

    el bar = el_menubar(w);
    el file = el_menu(bar, "File");
    el_on_click(el_menu(file, "Say hello\tCtrl+H"), say_hello, 0);
    el_separator(file);
    el_on_click(el_menu(file, "Exit"), on_quit, 0);
    el view = el_menu(bar, "View");
    el th = el_menu(view, "Dark theme");
    el_menu_check(th, &dark);
    el_on_click(th, on_theme, 0);
    el sub = el_menu(view, "Go to");
    el_on_click(el_menu(sub, "About"), on_about, 0);
    el_on_click(el_menu(bar, "Help"), on_about, 0);
    el_hotkey("Ctrl+H", say_hello, 0);

    el st = el_border(w);
    el_dock_side(st, EL_BOTTOM);
    el_style(st, "background:#10161f; padding:6 12");
    el stt = el_text(st, "");
    el_text_bind(stt, status);
    el_style(stt, "color:#c9d1d9");

    g_tabs = el_tabs(w);
    el_margin(g_tabs, 12, 12, 12, 12);
    el_on_change(g_tabs, on_tab, 0);

    // 1: форма на сетке
    el t1 = el_tab(g_tabs, "Form");
    el grid = el_grid(t1, "Auto,*", "Auto,Auto,Auto,Auto,Auto,Auto,Auto");
    el_gap(grid, 8);
    el_text(grid, "Name:");
    el e = el_edit(grid, name, sizeof(name));
    el_cell(e, 0, 1, 1, 1);
    el_tooltip(e, "Type a name and it will appear in the greeting");
    el_cell(el_text(grid, "Password:"), 1, 0, 1, 1);
    el_cell(el_password(grid, pass, sizeof(pass)), 1, 1, 1, 1);
    el_cell(el_text(grid, "City:"), 2, 0, 1, 1);
    el_cell(el_combo(grid, cities, 7, &city), 2, 1, 1, 1);
    el_cell(el_text(grid, "Size:"), 3, 0, 1, 1);
    el rs = el_stack(grid, 1);
    el_cell(rs, 3, 1, 1, 1);
    el_radio(rs, "Small", &sz, 0);
    el_radio(rs, "Medium", &sz, 1);
    el_radio(rs, "Large", &sz, 2);
    el_cell(el_text(grid, "Volume:"), 4, 0, 1, 1);
    el sl = el_slider(grid, 0, 100, &volume);
    el_cell(sl, 4, 1, 1, 1);
    el_on_change(sl, on_volume, 0);
    el pr = el_progress(grid, 100, &progress);
    el_cell(pr, 5, 1, 1, 1);
    el_cell(el_check(grid, "I agree to the terms", &agree), 6, 1, 1, 1);
    el btns = el_stack(t1, 1);
    el_align(btns, EL_END, EL_START);
    el_gap(btns, 8);
    el_on_click(el_button(btns, "Hello"), say_hello, 0);
    el_button(btns, "Cancel");

    // 2: список и заметки
    el t2 = el_tab(g_tabs, "List");
    el d = el_grid(t2, "220,*", "300");
    el_gap(d, 10);
    el lb = el_listbox(d, planets, 13, &item);
    el_on_change(lb, on_planet, 0);
    el m = el_memo(d, notes, sizeof(notes));
    el_cell(m, 0, 1, 1, 1);
    el cm = el_context_menu(lb);
    el_on_click(el_menu(cm, "Say hello"), say_hello, 0);

    // 3: раскладка с переносом и прокруткой
    el t3 = el_tab(g_tabs, "Layout");
    el gr = el_group(t3, "WrapPanel in a ScrollViewer");
    el sc = el_scroll(gr);
    el_size(sc, -1, 220);
    el wp = el_wrap(sc);
    char lab[32];
    for (int i = 1; i <= 40; i++) { sprintf(lab, "Button %d", i); el_button(wp, lab); }
    el info = el_text(t3, "The window rearranges itself at any size: panels measure their children and lay them out, as in WPF.");
    el_wrap_text(info, 1);
    el_style(info, "color:#8fa3b8; font-size:13");

    el_run();
    return 0;
}
