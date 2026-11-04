// wui.h — красивые окна для программ на Си: 61 тема ThemeLibrary (стекло, приборы, пергамент,
// камень, экраны...), своё окно со скруглёнными углами и тенью, кнопки, поля, списки, меню сбоку.
// Тема меняется на лету и запоминается. Библиотека — исходниками в lib\wui, wcc подключает её сам.
//
//   #include <wui.h>
//   int main(void) {
//       WuiControl *win = wui_window("Привет", 800, 520, "Privet");   // ключ — где помнить тему
//       WuiControl *card = wui_card(wui_content(win), "Имя");
//       WuiControl *name = wui_textbox(card, "Как вас зовут?");
//       wui_button(card, "Поздороваться", WUI_BUTTON_ACCENT);
//       return wui_run();
//   }
//
// Размеры — в DIP (точках при 96 dpi): на экране с масштабом 150 % всё само крупнее.
// Тексты — UTF-8.
#pragma once
#include <windows.h>
#include <gdiplus.h>
#ifndef WUI_INTERNAL
#pragma comment(lib, "wui")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")       // окно без консоли, даже с main()
#endif

typedef struct WuiControl WuiControl;
typedef struct WuiIconData WuiIconData;
typedef void (*WuiHandler)(WuiControl *c, void *user);

// ---- окно ----
WuiControl *wui_window(const char *title, int width, int height, const char *key);
WuiControl *wui_content(WuiControl *window);        // столбец под заголовком: сюда кладут всё
WuiControl *wui_titlebar(WuiControl *window);       // строка справа в заголовке (выбор темы и т. п.)
void wui_window_subtitle(WuiControl *window, const char *text);
void wui_window_logo(WuiControl *window, const WuiIconData *icon);
void wui_window_extend(WuiControl *window, int on); // содержимое до самого верха (заголовок поверх)
void wui_window_min_size(WuiControl *window, int width, int height);
void wui_window_close(WuiControl *window);
HWND wui_hwnd(WuiControl *window);
int  wui_run(void);                                 // пока открыты окна; возвращает код выхода
void wui_quit(int code);

// ---- темы ----
int         wui_theme_count(void);
const char *wui_theme_id(int index);                // "Emerald"
const char *wui_theme_name(int index);              // "Изумруд и золото"
const char *wui_theme_group(int index);             // "Стекло"
int         wui_theme_current(void);
void        wui_theme_set(const char *id);          // сменить во всех окнах и запомнить
COLORREF    wui_color(int which);                   // цвет текущей темы для своего рисования
enum { WUI_TEXT, WUI_TEXT_DIM, WUI_ACCENT, WUI_ACCENT2, WUI_SUCCESS, WUI_WARNING, WUI_DANGER, WUI_SURFACE };
ARGB        wui_argb(int which);                    // то же с прозрачностью — для GDI+

// ---- раскладка ----
// Столбец кладёт детей сверху вниз, строка — слева направо, перенос — строками с переносом.
WuiControl *wui_column(WuiControl *parent);
WuiControl *wui_row(WuiControl *parent);
WuiControl *wui_wrap(WuiControl *parent);
WuiControl *wui_scroll(WuiControl *parent);         // столбец с прокруткой
WuiControl *wui_card(WuiControl *parent, const char *title);    // панель темы; title может быть 0
WuiControl *wui_spacer(WuiControl *parent);         // пустое место, забирает остаток
WuiControl *wui_separator(WuiControl *parent);

void wui_size(WuiControl *c, float width, float height);        // 0 — по содержимому
void wui_min_size(WuiControl *c, float width, float height);
void wui_stretch(WuiControl *c, float weight);      // доля остатка вдоль родителя (0 — нет)
void wui_margin(WuiControl *c, float left, float top, float right, float bottom);
void wui_padding(WuiControl *c, float left, float top, float right, float bottom);   // у карточки — вместо полей темы
void wui_gap(WuiControl *c, float gap);             // расстояние между детьми
enum { WUI_ALIGN_STRETCH, WUI_ALIGN_START, WUI_ALIGN_CENTER, WUI_ALIGN_END };
void wui_align(WuiControl *c, int align);           // поперёк родителя

// ---- элементы ----
enum { WUI_LABEL_BODY, WUI_LABEL_DIM, WUI_LABEL_H1, WUI_LABEL_H2, WUI_LABEL_TITLE, WUI_LABEL_SECTION,
       WUI_LABEL_MONO, WUI_LABEL_BIG, WUI_LABEL_ACCENT, WUI_LABEL_TEXT };
// BODY — книжный текст темы (абзацы), TEXT — обычный шрифт окна, DIM — мелкий бледный, SECTION — подпись группы
WuiControl *wui_label(WuiControl *parent, const char *text, int style);

enum { WUI_BUTTON_NORMAL, WUI_BUTTON_ACCENT, WUI_BUTTON_DANGER, WUI_BUTTON_SUCCESS, WUI_BUTTON_GHOST };
WuiControl *wui_button(WuiControl *parent, const char *text, int kind);
void wui_button_icon(WuiControl *button, const WuiIconData *icon);

WuiControl *wui_nav_section(WuiControl *parent, const char *text);  // подпись группы в боковом меню
WuiControl *wui_nav_item(WuiControl *parent, const WuiIconData *icon, const char *text);

WuiControl *wui_textbox(WuiControl *parent, const char *placeholder);
WuiControl *wui_password(WuiControl *parent, const char *placeholder);
WuiControl *wui_checkbox(WuiControl *parent, const char *text);
WuiControl *wui_switch(WuiControl *parent, const char *text);
WuiControl *wui_radio(WuiControl *parent, const char *text);        // соседи-переключатели — одна группа
WuiControl *wui_slider(WuiControl *parent, double min, double max, double value);
WuiControl *wui_progress(WuiControl *parent);                       // 0…1, меньше 0 — бегущая полоса
WuiControl *wui_combo(WuiControl *parent);
int         wui_combo_add(WuiControl *combo, const char *text);
void        wui_combo_clear(WuiControl *combo);
WuiControl *wui_list(WuiControl *parent);
int         wui_list_add(WuiControl *list, const char *text);
void        wui_list_clear(WuiControl *list);
WuiControl *wui_theme_picker(WuiControl *parent);                   // выбор из всех тем; сменили — wui_on_change
WuiControl *wui_icon(WuiControl *parent, const WuiIconData *icon, float size);
WuiControl *wui_tabs(WuiControl *parent);
WuiControl *wui_tab(WuiControl *tabs, const char *title);           // страница вкладки (столбец)

// Своё рисование: вызывается при каждой перерисовке; g уже в DIP, (0, 0) — левый верхний угол.
typedef void (*WuiPaint)(WuiControl *c, GpGraphics *g, float width, float height, void *user);
WuiControl *wui_canvas(WuiControl *parent, WuiPaint paint, void *user);

// ---- общее для всех элементов ----
void        wui_text(WuiControl *c, const char *text);
const char *wui_get_text(WuiControl *c);            // строка живёт до следующей правки элемента
void        wui_value(WuiControl *c, double value); // флажок 0/1, ползунок, полоса, выбор в списке
double      wui_get_value(WuiControl *c);
void        wui_enable(WuiControl *c, int on);   // выключенная панель выключает и всё, что в ней
void        wui_show(WuiControl *c, int on);
void        wui_on_click(WuiControl *c, WuiHandler fn, void *user);
void        wui_on_change(WuiControl *c, WuiHandler fn, void *user);
void        wui_focus(WuiControl *c);
void        wui_redraw(WuiControl *c);
void        wui_set_data(WuiControl *c, void *data);
void       *wui_get_data(WuiControl *c);
WuiControl *wui_window_of(WuiControl *c);

// ---- сообщения ----
void wui_toast(WuiControl *window, const char *text);      // всплывающая строка внизу окна
enum { WUI_MSG_OK, WUI_MSG_OK_CANCEL, WUI_MSG_YES_NO };
int  wui_message(WuiControl *window, const char *title, const char *text, int buttons);  // 1 — да/ок

// ---- значки ThemeLibrary (сетка 24×24) ----
struct WuiIconData {
    const float *stroke; int stroke_count;
    const float *fill; int fill_count;
    int fill_nonzero;
};
#include "wui_icons.h"
void wui_draw_icon(GpGraphics *g, const WuiIconData *icon, float x, float y, float size, ARGB color);
