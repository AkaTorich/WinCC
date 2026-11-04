// Показ библиотеки WUI: все элементы на страницах, боковое меню до самого верха окна
// (как в Нумерографе), выбор из всех тем в заголовке.
// Собрать:  wcc main.c pages.c -o "Показ WUI.exe"
#include "demo.h"
#include <stdlib.h>
#include <string.h>

static WuiControl *g_pages[PAGE_COUNT], *g_nav[PAGE_COUNT];

static void open_page(WuiControl *item, void *user)
{
    (void)item;
    int page = (int)(INT_PTR)user;
    for (int i = 0; i < PAGE_COUNT; i++) wui_show(g_pages[i], i == page);
}

int main(int argc, char **argv)
{
    WuiControl *win = wui_window("Показ WUI", 1180, 760, "WuiDemo");
    wui_window_extend(win, 1);
    wui_window_min_size(win, 760, 520);
    wui_theme_picker(wui_titlebar(win));

    WuiControl *body = wui_row(wui_content(win));
    wui_stretch(body, 1);
    wui_gap(body, 16);

    // боковое меню: значок, имя, пункты по группам
    WuiControl *side = wui_card(body, 0);
    wui_size(side, 232, 0);
    wui_gap(side, 2);
    WuiControl *head = wui_row(side);
    wui_icon(head, WUI_ICON_GRID, 28);
    wui_label(head, "Показ WUI", WUI_LABEL_H2);
    wui_margin(head, 4, 0, 0, 10);
    wui_nav_section(side, "Элементы");
    struct { const WuiIconData *icon; const char *text; } items[PAGE_COUNT] = {
        { WUI_ICON_SLIDERS, "Кнопки и поля" },
        { WUI_ICON_LIST, "Списки и вкладки" },
        { WUI_ICON_LAYERS, "Карточки" },
        { WUI_ICON_EDIT, "Своё рисование" },
        { WUI_ICON_INFO, "О библиотеке" },
    };
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == 3) wui_nav_section(side, "Ещё");
        g_nav[i] = wui_nav_item(side, items[i].icon, items[i].text);
        wui_on_click(g_nav[i], open_page, (void *)(INT_PTR)i);
    }
    wui_spacer(side);
    WuiControl *note = wui_label(side, "61 тема ThemeLibrary — смена на лету, выбор запоминается", WUI_LABEL_DIM);
    wui_margin(note, 6, 0, 6, 4);

    // страницы: видна одна
    WuiControl *pages = wui_column(body);
    wui_stretch(pages, 1);
    wui_margin(pages, 0, 52, 0, 0);            // под строкой заголовка с выбором темы
    g_pages[0] = page_controls(pages);
    g_pages[1] = page_lists(pages);
    g_pages[2] = page_cards(pages);
    g_pages[3] = page_canvas(pages);
    g_pages[4] = page_about(pages);
    // «--страница=2» — открыть сразу нужную (для снимков)
    int first = argc > 1 && strncmp(argv[1], "--страница=", strlen("--страница=")) == 0 ? atoi(argv[1] + strlen("--страница=")) : 0;
    if (first < 0 || first >= PAGE_COUNT) first = 0;
    wui_value(g_nav[first], 1);
    open_page(0, (void *)(INT_PTR)first);
    return wui_run();
}
