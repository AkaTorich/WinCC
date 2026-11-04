// Показ WUI: страницы. Каждая — столбец с прокруткой: заголовок, пояснение, карточки.
#include "demo.h"
#include <stdio.h>
#include <math.h>

static WuiControl *page(WuiControl *parent, const char *title, const char *about)
{
    WuiControl *p = wui_scroll(parent);
    wui_gap(p, 14);
    wui_padding(p, 0, 0, 6, 0);
    wui_label(p, title, WUI_LABEL_H1);
    wui_label(p, about, WUI_LABEL_DIM);
    return p;
}

// ---- кнопки и поля ----

static WuiControl *g_name, *g_level, *g_bar, *g_level_text;

static void greet(WuiControl *b, void *u)
{
    (void)u;
    char text[300];
    const char *name = wui_get_text(g_name);
    snprintf(text, sizeof text, *name ? "Здравствуйте, %s!" : "Сначала впишите имя", name);
    wui_toast(wui_window_of(b), text);
}

static void level_changed(WuiControl *s, void *u)
{
    (void)u;
    char text[32];
    double v = wui_get_value(s);
    snprintf(text, sizeof text, "%d %%", (int)(v + 0.5));
    wui_text(g_level_text, text);
    wui_value(g_bar, v / 100);
}

WuiControl *page_controls(WuiControl *parent)
{
    WuiControl *p = page(parent, "Кнопки и поля", "Всё рисуется темой: стекло, металл, бумага или экран — по выбору сверху.");

    WuiControl *c = wui_card(p, "Кнопки");
    WuiControl *row = wui_wrap(c);
    wui_button(row, "Главное действие", WUI_BUTTON_ACCENT);
    wui_button(row, "Обычная", WUI_BUTTON_NORMAL);
    wui_button(row, "Сохранить", WUI_BUTTON_SUCCESS);
    wui_button(row, "Удалить", WUI_BUTTON_DANGER);
    wui_button(row, "Тихая", WUI_BUTTON_GHOST);
    WuiControl *ib = wui_button(row, "Обновить", WUI_BUTTON_NORMAL);
    wui_button_icon(ib, WUI_ICON_REFRESH);
    WuiControl *off = wui_button(row, "Недоступна", WUI_BUTTON_NORMAL);
    wui_enable(off, 0);

    c = wui_card(p, "Поля ввода");
    WuiControl *r = wui_row(c);
    g_name = wui_textbox(r, "Как вас зовут?");
    wui_stretch(g_name, 1);
    wui_on_click(g_name, greet, 0);                 // Enter в поле
    wui_on_click(wui_button(r, "Поздороваться", WUI_BUTTON_ACCENT), greet, 0);
    wui_password(c, "Пароль");

    c = wui_card(p, "Выбор");
    WuiControl *cols = wui_row(c);
    wui_gap(cols, 40);
    WuiControl *a = wui_column(cols);
    wui_gap(a, 12);
    wui_value(wui_checkbox(a, "Показывать подсказки"), 1);
    wui_checkbox(a, "Тёмные рисунки");
    wui_value(wui_switch(a, "Звук"), 1);
    wui_switch(a, "Обновляться само");
    WuiControl *b = wui_column(cols);
    wui_gap(b, 12);
    wui_radio(b, "Латиница");
    wui_radio(b, "Кириллица");
    wui_radio(b, "Иврит");

    WuiControl *lr = wui_row(c);
    wui_label(lr, "Уровень", WUI_LABEL_TEXT);
    g_level = wui_slider(lr, 0, 100, 40);
    wui_stretch(g_level, 1);
    g_level_text = wui_label(lr, "40 %", WUI_LABEL_ACCENT);
    wui_size(g_level_text, 48, 0);
    wui_on_change(g_level, level_changed, 0);
    g_bar = wui_progress(c);
    wui_value(g_bar, 0.4);
    wui_label(c, "Идёт работа…", WUI_LABEL_DIM);
    wui_value(wui_progress(c), -1);
    WuiControl *combo = wui_combo(c);
    wui_combo_add(combo, "Пифагорейская");
    wui_combo_add(combo, "Халдейская");
    wui_combo_add(combo, "Квадрат Пифагора");
    wui_align(combo, WUI_ALIGN_START);
    return p;
}

// ---- списки и вкладки ----

static WuiControl *g_list_text;

static void list_changed(WuiControl *l, void *u)
{
    (void)u;
    char text[200];
    snprintf(text, sizeof text, "Выбрано: %s", wui_get_text(l));
    wui_text(g_list_text, text);
}

static void ask(WuiControl *b, void *u)
{
    (void)u;
    int yes = wui_message(wui_window_of(b), "Вопрос", "Сохранить расчёт перед выходом? Несохранённые числа пропадут.",
                          WUI_MSG_YES_NO);
    wui_toast(wui_window_of(b), yes ? "Ответ: да" : "Ответ: нет");
}

static void inform(WuiControl *b, void *u)
{
    (void)u;
    wui_message(wui_window_of(b), "Готово", "Все семь чисел посчитаны.", WUI_MSG_OK);
}

WuiControl *page_lists(WuiControl *parent)
{
    WuiControl *p = page(parent, "Списки и вкладки", "Список с прокруткой, раскрывающийся список, вкладки и сообщения.");
    WuiControl *c = wui_card(p, 0);
    WuiControl *tabs = wui_tabs(c);
    wui_stretch(tabs, 0);

    WuiControl *t1 = wui_tab(tabs, "Список");
    static const char *signs[] = { "Овен", "Телец", "Близнецы", "Рак", "Лев", "Дева", "Весы", "Скорпион", "Стрелец",
                                   "Козерог", "Водолей", "Рыбы" };
    WuiControl *list = wui_list(t1);
    for (int i = 0; i < 12; i++) wui_list_add(list, signs[i]);
    wui_size(list, 0, 220);
    g_list_text = wui_label(t1, "Выбрано: —", WUI_LABEL_DIM);
    wui_on_change(list, list_changed, 0);

    WuiControl *t2 = wui_tab(tabs, "Раскрывающийся");
    WuiControl *combo = wui_combo(t2);
    for (int i = 0; i < 12; i++) wui_combo_add(combo, signs[i]);
    wui_size(combo, 260, 0);
    wui_label(t2, "Список раскрывается над окном и уходит вверх, если внизу нет места.", WUI_LABEL_DIM);

    WuiControl *t3 = wui_tab(tabs, "Сообщения");
    WuiControl *row = wui_row(t3);
    wui_on_click(wui_button(row, "Спросить", WUI_BUTTON_ACCENT), ask, 0);
    wui_on_click(wui_button(row, "Сообщить", WUI_BUTTON_NORMAL), inform, 0);
    return p;
}

// ---- карточки: плитки с крупными числами ----

WuiControl *page_cards(WuiControl *parent)
{
    WuiControl *p = page(parent, "Карточки", "Плитки переносятся по ширине окна, крупные числа светятся цветом темы.");
    WuiControl *wrap = wui_wrap(p);
    static const struct { const char *name, *number, *meaning; } tiles[] = {
        { "Путь", "1", "Монада, начало" },
        { "Выражение", "4", "Тетрада, основание" },
        { "Душа", "11/2", "Первое мастер-число" },
        { "Личность", "11/2", "Первое мастер-число" },
        { "День рождения", "1", "Монада, начало" },
        { "Зрелость", "5", "Пентада, свобода" },
        { "Отношение", "8", "Огдоада, сила" },
    };
    for (int i = 0; i < 7; i++) {
        WuiControl *t = wui_card(wrap, 0);
        wui_gap(t, 2);
        wui_min_size(t, 150, 0);
        wui_label(t, tiles[i].name, WUI_LABEL_DIM);
        wui_label(t, tiles[i].number, WUI_LABEL_BIG);
        wui_label(t, tiles[i].meaning, WUI_LABEL_DIM);
    }
    return p;
}

// ---- своё рисование: круг девяти чисел цветами темы ----

static void circle(WuiControl *c, GpGraphics *g, float w, float h, void *u)
{
    (void)c; (void)u;
    float cx = w / 2, cy = h / 2, r = (w < h ? w : h) / 2 - 40;
    GpPen *pen;
    GdipCreatePen1(wui_argb(WUI_ACCENT) & 0x80FFFFFF, 1.2f, UnitPixel, &pen);
    GdipDrawEllipse(g, pen, cx - r, cy - r, 2 * r, 2 * r);
    GdipDrawEllipse(g, pen, cx - r * 0.82f, cy - r * 0.82f, r * 1.64f, r * 1.64f);
    // девять узлов и звезда между ними
    PointF pt[9];
    for (int i = 0; i < 9; i++) {
        float a = (float)(-3.14159265 / 2 + i * 2 * 3.14159265 / 9);
        pt[i].X = cx + r * 0.82f * cosf(a);
        pt[i].Y = cy + r * 0.82f * sinf(a);
    }
    GpPen *thin;
    GdipCreatePen1(wui_argb(WUI_ACCENT2) & 0x90FFFFFF, 1, UnitPixel, &thin);
    for (int i = 0; i < 9; i++) GdipDrawLine(g, thin, pt[i].X, pt[i].Y, pt[(i + 4) % 9].X, pt[(i + 4) % 9].Y);
    GpFontFamily *fam;
    GdipCreateFontFamilyFromName(L"Segoe UI", NULL, &fam);
    GpFont *font;
    GdipCreateFont(fam, 16, 1, UnitPixel, &font);
    GpSolidFill *fill, *text;
    GdipCreateSolidFill(wui_argb(WUI_SURFACE) & 0x30FFFFFF, &fill);
    GdipCreateSolidFill(wui_argb(WUI_ACCENT), &text);
    GpStringFormat *fmt;
    GdipCreateStringFormat(0, 0, &fmt);
    GdipSetStringFormatAlign(fmt, StringAlignmentCenter);
    GdipSetStringFormatLineAlign(fmt, StringAlignmentCenter);
    for (int i = 0; i < 9; i++) {
        GdipFillEllipse(g, fill, pt[i].X - 17, pt[i].Y - 17, 34, 34);
        GdipDrawEllipse(g, pen, pt[i].X - 17, pt[i].Y - 17, 34, 34);
        wchar_t n[2] = { (wchar_t)(L'1' + i), 0 };
        RectF box = { pt[i].X - 17, pt[i].Y - 17, 34, 34 };
        GdipDrawString(g, n, 1, font, &box, fmt, text);
    }
    GdipDeleteStringFormat(fmt);
    GdipDeleteBrush(fill);
    GdipDeleteBrush(text);
    GdipDeleteFont(font);
    GdipDeleteFontFamily(fam);
    GdipDeletePen(thin);
    GdipDeletePen(pen);
}

WuiControl *page_canvas(WuiControl *parent)
{
    WuiControl *p = page(parent, "Своё рисование", "Холст рисует сама программа через GDI+, цвета берёт у темы.");
    WuiControl *c = wui_card(p, "Круг девяти чисел");
    WuiControl *cv = wui_canvas(c, circle, 0);
    wui_size(cv, 0, 420);
    return p;
}

// ---- о библиотеке ----

WuiControl *page_about(WuiControl *parent)
{
    WuiControl *p = page(parent, "О библиотеке", "wui.h — оформление ThemeLibrary для программ на Си.");
    WuiControl *c = wui_card(p, "Как подключить");
    wui_label(c, "Достаточно написать #include <wui.h> — компилятор WinCC сам добавит библиотеку из папки lib\\wui. "
                 "Окно создаёт wui_window, элементы кладутся в столбцы, строки и карточки, wui_run ждёт, пока окно "
                 "открыто.", WUI_LABEL_BODY);
    wui_label(c, "Тема выбирается из 61 готовой: стекло, приборы, пергамент, камень, самоцветы, древние миры, экраны. "
                 "Выбор запоминается под ключом программы и подхватывается при следующем запуске.", WUI_LABEL_BODY);
    c = wui_card(p, "Что внутри");
    wui_label(c, "Своё окно с тенью и скруглёнными углами, фактуры (мрамор, дерево, кожа, латунь), узоры и рамки, "
                 "значки ThemeLibrary, поле ввода с выделением и буфером обмена, списки, вкладки, сообщения.",
              WUI_LABEL_BODY);
    return p;
}
