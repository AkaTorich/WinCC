#pragma once
#ifdef __WINCC__
#pragma comment(lib, "elements")                  // wcc собирает lib\elements с программой
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")       // окно без консоли, даже с main()
#endif
// Elements для Windows — нативная библиотека окон на Си в духе WPF (подключается и из C++).
// Тот же набор функций, что у <elements.h> в DynastyOS: дерево элементов,
// панели сами раскладывают детей, элементы привязаны к переменным программы.
// Рисование — на GDI+ (без сторонних зависимостей). Программа строит дерево
// один раз, потом el_run() (или свой цикл: while (el_frame()) { ... }).
//
//   #include <elements.h>
//   char name[64]; int agree;
//   void hello(el e, void *arg) { el_set_text(el_find("out"), name); }
//   int main(void) {
//       el w = el_window("Пример", 420, 240);
//       el s = el_stack(w, 0);  el_padding(s, 16, 16, 16, 16);
//       el_edit(s, name, sizeof(name));
//       el_on_click(el_button(s, "OK"), hello, 0);
//       el_name(el_text(s, ""), "out");
//       el_run();
//   }
//
// Элемент — число (0 — нет). parent = 0 — само окно. Размеры в точках экрана
// (масштаб HiDPI библиотека делает сама).
#ifdef __cplusplus
extern "C" {
#endif

typedef int el;
typedef void (*el_fn)(el e, void *arg);

#define EL_STRETCH 0
#define EL_START 1
#define EL_CENTER 2
#define EL_END 3
#define EL_LEFT 0
#define EL_TOP 1
#define EL_RIGHT 2
#define EL_BOTTOM 3

// --- окно и цикл ---
void el_title(const char *title);
el   el_window(const char *title, int w, int h);
int  el_frame(void);                  // один кадр: ввод, события, раскладка, рисование; 0 — окно закрыто
void el_run(void);                    // кадры, пока окно не закроют

// --- нестандартные окна (своя форма, размеры, пропорции, отрисовка) ---
void el_frameless(int on);            // убрать системную рамку: окно рисует программа (своя «шапка», кнопки, фон)
void el_window_round(int radius);     // скруглить углы окна (в точках; 0 — прямые)
void el_window_shape(const char *shape);   // "rect" | "round:24" | "ellipse" | "pixels" (форма по непрозрачности рисунка окна)
void el_window_region(const int *xy, int count);  // произвольный контур окна: count пар x,y в точках
void el_window_aspect(int w, int h);  // держать пропорции при изменении размера (0,0 — свободно)
void el_window_opacity(int alpha);    // прозрачность всего окна 0..255 (включает послойное окно)
void el_drag_region(el e);            // тянут за этот элемент — двигается всё окно (для своей «шапки»)
void el_window_minmax(int minw, int minh, int maxw, int maxh);  // пределы размера окна (-1 — без предела)
void el_window_resizable(int on);     // можно ли менять размер окна (по умолчанию да; для безрамочных — тянуть за края)
el   el_window_canvas(void (*fn)(el e, int w, int h, void *arg), void *arg);  // поверхность во весь фон окна: рисуйте любой дизайн (el_g_*)

// --- эффекты окна (постобработка готового кадра; можно включить несколько сразу) ---
void el_effect(const char *name, int amount);   // amount: сила; 0 — убрать. Имена:
    // "dispersion"/"chromatic" — расщепление каналов (дисперсия), "scanlines" — строки развёртки,
    // "noise" — шум, "vignette" — затемнение краёв, "glitch" — глитч (анимированный),
    // "bloom"/"glow" — свечение ярких мест, "pixelate" — пикселизация, "grayscale" — ч/б,
    // "sepia" — сепия, "invert" — инверсия
void el_effects_clear(void);

// --- панели (раскладка) ---
el el_stack(el parent, int horizontal);                      // дети друг за другом (0 сверху вниз, 1 слева направо)
el el_dock(el parent);                                        // детям el_dock_side, последний занимает остаток
el el_wrap(el parent);                                        // в строку с переносом
el el_grid(el parent, const char *cols, const char *rows);    // "Auto,*,2*,120"
el el_scroll(el parent);                                      // прокрутка колесом и ползунком
el el_canvas(el parent);                                      // дети там, где el_pos
el el_border(el parent);                                      // рамка и фон вокруг одного ребёнка
el el_group(el parent, const char *header);                   // рамка с заголовком
el el_tabs(el parent);                                        // вкладки
el el_tab(el tabs, const char *header);

// --- элементы управления ---
el el_text(el parent, const char *text);
el el_button(el parent, const char *text);
el el_check(el parent, const char *text, int *value);
el el_radio(el parent, const char *text, int *group, int value);
el el_edit(el parent, char *buf, int cap);
el el_memo(el parent, char *buf, int cap);
el el_password(el parent, char *buf, int cap);
el el_combo(el parent, const char *const *items, int count, int *sel);
el el_listbox(el parent, const char *const *items, int count, int *sel);
void el_items(el e, const char *const *items, int count);
el el_slider(el parent, int min, int max, int *value);
el el_progress(el parent, int max, int *value);
el el_image(el parent, const char *path);
el el_separator(el parent);

// --- меню ---
el el_menubar(el parent);
el el_menu(el parent, const char *text);                      // "Создать\tCtrl+N" — клавиши справа
void el_menu_check(el item, int *value);
el el_context_menu(el target);
el el_popup_menu(void);
void el_menu_popup(el menu, el at, int x, int y);

// --- свойства ---
void el_size(el e, int w, int h);                             // -1 — по содержимому
void el_min_size(el e, int w, int h);
void el_max_size(el e, int w, int h);
void el_margin(el e, int left, int top, int right, int bottom);
void el_padding(el e, int left, int top, int right, int bottom);
void el_align(el e, int horizontal, int vertical);
void el_cell(el e, int row, int col, int rowspan, int colspan);
void el_dock_side(el e, int side);
void el_pos(el e, int x, int y);
void el_gap(el e, int px);
void el_set_text(el e, const char *text);
const char *el_get_text(el e);
void el_text_bind(el e, const char *live);                    // показывать строку программы как есть, каждый кадр
void el_enable(el e, int on);
void el_show(el e, int on);
void el_tooltip(el e, const char *text);
void el_wrap_text(el e, int on);
void el_font(el e, int size, int bold);
// --- встраиваемые шрифты (из .ttf/.otf, файлом или из памяти) ---
int  el_font_add(const char *name, const char *path);                   // загрузить шрифт и дать ему имя; 1 — успех
int  el_font_add_memory(const char *name, const void *data, int size);  // встраивание из байтов шрифта
void el_font_face(el e, const char *family);                            // шрифт элемента по имени ("default", "mono" или загруженное имя)
void el_colors(el e, int fg, int bg);                         // 0xRRGGBB; -1 — как в теме
void el_frame_style(el e, int color, int width, int radius);
void el_style(el e, const char *css);
    // background:#223  color:#fff  border:#456  border-width:1  radius:6  padding:4 8  margin:0 0 8 0  width:120  height:32
    // gradient:#1a2a6c #b21f1f vertical|horizontal|diagonal   background-alpha:160 (стекло)   background-image:путь
    // shadow:#000 18 0 6 120 (цвет размытие x y сила; shadow:none)   hover-background press-background hover-color hover-border
    // font-size:16  font-weight:bold  font-family:mono  text-align:center|right
void el_name(el e, const char *name);
el   el_find(const char *name);
void el_select_tab(el tabs, int index);
int  el_selected_tab(el tabs);
void el_scroll_to(el e, int pos);
void el_focus(el e);
void el_clear(el e);
void el_remove(el e);

// --- тема ---
void el_theme(int dark);
void el_accent(int rgb);
void el_default_font(int size);
void el_theme_color(int slot, int rgb);   // 0 окно,1 панель,2 кнопка,3 наведение,4 нажатие,5 рамка,6 текст,
                                          // 7 яркий текст,8 тусклый,9 акцент,10 текст на акценте,11 поле,12 выделение,13 меню
void el_corner_radius(int r);
int  el_theme_named(const char *name);    // Ночь День Аметист Изумруд Сапфир Рубин Янтарь Обсидиан Пергамент Океан Лес Сакура Графит Неон Золото Лёд
const char *el_theme_name(int index);

// --- события ---
void el_on_click(el e, el_fn fn, void *arg);
void el_on_change(el e, el_fn fn, void *arg);
int  el_clicked(el e);
int  el_changed(el e);

// --- оформление строкой (CSS-таблица) ---
void el_stylesheet(const char *css);
int  el_stylesheet_file(const char *path);
void el_class(el e, const char *classes);

// --- анимации ---
#define EL_LINEAR 0
#define EL_EASE_IN_OUT 1
#define EL_EASE_OUT 2
#define EL_OVERSHOOT 3
#define EL_BOUNCE 4
#define EL_EASE_IN 5
void el_animate(el e, const char *prop, int from, int to, int ms, int easing, int loop);
void el_appear(el e, const char *how, int ms);
void el_transform(el e, int x, int y, int scale, int rotate, int opacity);
int  el_animating(el e);
void el_stop(el e);

// --- ещё элементы ---
el   el_number(el parent, int min, int max, int step, int *value);
el   el_toolbar(el parent);
el   el_tool(el toolbar, const char *icon, const char *text, const char *tip);
void el_icon(el button, const char *path, int size);

// --- готовые окна ---
void el_message(const char *title, const char *text);
void el_ask(const char *title, const char *text, void (*fn)(int yes, void *arg), void *arg);
void el_open_file(const char *title, const char *folder, const char *filter, void (*fn)(const char *path, void *arg), void *arg);
void el_save_file(const char *title, const char *folder, const char *name, void (*fn)(const char *path, void *arg), void *arg);
void el_pick_folder(const char *title, const char *folder, void (*fn)(const char *path, void *arg), void *arg);
void el_toast(const char *text, int ms);
el   el_dialog(const char *title, int width);
void el_dialog_close(el d);

// --- клавиши окна ---
#define EL_SHIFT 1
#define EL_CTRL 2
#define EL_ALT 4
int  el_hotkey(const char *combo, el_fn fn, void *arg);
int  el_mods(void);

// --- своё рисование ---
el   el_paint(el parent, void (*fn)(el e, int w, int h, void *arg), void *arg);
void el_redraw(el e);
int  el_mouse(el e, int *x, int *y, int *buttons);
int  el_wheel(el e, int *mods);
void el_g_fill(int x, int y, int w, int h, int rgb);
void el_g_round(int x, int y, int w, int h, int radius, int rgb);
void el_g_frame(int x, int y, int w, int h, int radius, int rgb, int width);
void el_g_line(int x0, int y0, int x1, int y1, int rgb, int width);
void el_g_disc(int cx, int cy, int r, int rgb);
void el_g_ring(int cx, int cy, int r, int width, int rgb);
void el_g_shade(int x, int y, int w, int h, int rgb, int alpha);
void el_g_text(int x, int y, const char *s, int rgb, int size, int bold);
int  el_g_text_w(const char *s, int size, int bold);
void el_g_poly(const int *xy, int count, int rgb);                  // залить многоугольник (count пар x,y в точках)
void el_g_poly_frame(const int *xy, int count, int rgb, int width); // обвести многоугольник
void el_g_spline(const int *xy, int count, int rgb, int width);     // гладкая кривая через точки (штрих)
void el_g_blob(const int *xy, int count, int rgb);                  // залить гладкую ЗАМКНУТУЮ кривую через точки — любая «кривая» форма
void el_g_arc(int cx, int cy, int r, int start_deg, int sweep_deg, int rgb, int width);   // дуга окружности

// --- таймеры, данные, закрытие ---
int  el_every(int ms, el_fn fn, void *arg);
void el_timer_stop(int id);
void el_data(el e, void *p);
void *el_get_data(el e);
el   el_focused(void);
void el_on_close(el_fn fn, void *arg);
void el_quit(void);

#ifdef __cplusplus
}
#endif
