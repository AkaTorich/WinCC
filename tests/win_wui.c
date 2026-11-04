// wui.h of WinCC: the library comes in by itself (lib\wui), the themes, the values of the controls.
#include "test.h"
#ifndef _WIN32                          // DynastyOS (cctest): no Windows there
int main(void) { return 0; }
#else
#include <wui.h>
#include <string.h>
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")      // тест — консольный: его ждут и читают код выхода

static int clicks;
static void count(WuiControl *c, void *user) { (void)c; (void)user; clicks++; }

int main(void)
{
    // темы: 61, первая — «Изумруд и золото», смена по Id
    ASSERT(61, wui_theme_count());
    ASSERT(0, strcmp(wui_theme_id(0), "Emerald"));
    ASSERT(0, strcmp(wui_theme_name(0), "Изумруд и золото"));
    ASSERT(0, strcmp(wui_theme_group(0), "Стекло"));

    WuiControl *win = wui_window("Тест", 600, 400, 0);
    wui_theme_set("Parchment");
    ASSERT(0, strcmp(wui_theme_id(wui_theme_current()), "Parchment"));
    wui_theme_set("нет такой");
    ASSERT(0, strcmp(wui_theme_id(wui_theme_current()), "Parchment"));
    ASSERT(1, wui_argb(WUI_TEXT) >> 24 == 0xFF);

    WuiControl *c = wui_content(win);
    WuiControl *name = wui_textbox(c, "Имя");
    wui_text(name, "Пётр Ильич");
    ASSERT(0, strcmp(wui_get_text(name), "Пётр Ильич"));

    WuiControl *flag = wui_checkbox(c, "Флажок");
    ASSERT(0, (int)wui_get_value(flag));
    wui_value(flag, 1);
    ASSERT(1, (int)wui_get_value(flag));

    // соседние кнопки выбора — одна группа: первая выбрана, выбор другой снимает её
    WuiControl *r1 = wui_radio(c, "Один"), *r2 = wui_radio(c, "Два");
    ASSERT(1, (int)wui_get_value(r1));
    wui_value(r2, 1);
    ASSERT(0, (int)wui_get_value(r1));
    ASSERT(1, (int)wui_get_value(r2));

    WuiControl *s = wui_slider(c, 0, 10, 4);
    wui_value(s, 25);
    ASSERT(10, (int)wui_get_value(s));

    WuiControl *combo = wui_combo(c);
    ASSERT(0, wui_combo_add(combo, "Овен"));
    ASSERT(1, wui_combo_add(combo, "Телец"));
    ASSERT(0, strcmp(wui_get_text(combo), "Овен"));
    wui_value(combo, 1);
    ASSERT(0, strcmp(wui_get_text(combo), "Телец"));

    WuiControl *list = wui_list(c);
    wui_list_add(list, "Альфа");
    wui_list_add(list, "Бета");
    ASSERT(-1, (int)wui_get_value(list));
    wui_value(list, 1);
    ASSERT(0, strcmp(wui_get_text(list), "Бета"));

    WuiControl *label = wui_label(c, "Надпись", WUI_LABEL_BODY);
    wui_text(label, "Другая");
    ASSERT(0, strcmp(wui_get_text(label), "Другая"));

    WuiControl *b = wui_button(c, "Кнопка", WUI_BUTTON_ACCENT);
    wui_on_click(b, count, 0);
    ASSERT(win, wui_window_of(b));
    ASSERT(1, wui_hwnd(win) != 0);

    // значки — из ThemeLibrary, на сетке 24×24
    ASSERT(1, WUI_ICON_HOME->stroke_count > 0);
    wui_window_close(win);
    ASSERT(0, wui_run());
    return g_fail;
}
#endif
