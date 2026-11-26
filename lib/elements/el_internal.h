#pragma once
// Внутреннее устройство Elements. Один заголовок на все модули: структура
// элемента, общий контекст (дерево, тема, ввод, GDI+), помощники рисования.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "el_gdip.h"
#include <elements.h>

// Internal names get the eli_ prefix (the C++ version kept them in namespace eli),
// so they never clash with names in a program linked against the static library.
#define g                   eli_g
#define vgrow               eli_vgrow
#define sdup                eli_sdup
#define sset                eli_sset
#define seq                 eli_seq
#define wdup                eli_wdup
#define udup                eli_udup
#define wlen                eli_wlen
#define theme_init          eli_theme_init
#define th                  eli_th
#define mix                 eli_mix
#define get_font            eli_get_font
#define font_face_by_name   eli_font_face_by_name
#define text_w              eli_text_w
#define text_size           eli_text_size
#define text_size_u         eli_text_size_u
#define text_w_u            eli_text_w_u
#define px                  eli_px
#define dip                 eli_dip
#define gfx_fill_rect       eli_gfx_fill_rect
#define gfx_fill_path       eli_gfx_fill_path
#define gfx_draw_path       eli_gfx_draw_path
#define gfx_fill_ellipse    eli_gfx_fill_ellipse
#define gfx_draw_ellipse    eli_gfx_draw_ellipse
#define gfx_line            eli_gfx_line
#define gfx_lines           eli_gfx_lines
#define gfx_fill_poly       eli_gfx_fill_poly
#define gfx_rrect           eli_gfx_rrect
#define round_rect_path     eli_round_rect_path
#define gfx_format          eli_gfx_format
#define gfx_text            eli_gfx_text
#define gfx_text_at         eli_gfx_text_at
#define measure             eli_measure
#define arrange             eli_arrange
#define layout_root         eli_layout_root
#define draw_tree           eli_draw_tree
#define draw_element        eli_draw_element
#define input_frame         eli_input_frame
#define fire_click          eli_fire_click
#define fire_change         eli_fire_change
#define hit_test            eli_hit_test
#define is_container        eli_is_container
#define is_focusable        eli_is_focusable
#define set_focus           eli_set_focus
#define menu_open_bar       eli_menu_open_bar
#define menu_close_all      eli_menu_close_all
#define menu_frame_input    eli_menu_frame_input
#define menu_draw           eli_menu_draw
#define menu_measure_width  eli_menu_measure_width
#define anim_frame          eli_anim_frame
#define anim_busy           eli_anim_busy
#define ease                eli_ease
#define transform_of        eli_transform_of
#define apply_style_string  eli_apply_style_string
#define restyle_all         eli_restyle_all
#define style_apply_sheet   eli_style_apply_sheet
#define apply_effects       eli_apply_effects
#define effects_animated    eli_effects_animated
#define overlay_frame_input eli_overlay_frame_input
#define overlay_draw        eli_overlay_draw
#define draw_tooltip        eli_draw_tooltip
#define combo_frame_input   eli_combo_frame_input
#define combo_draw          eli_combo_draw
#define edit_input          eli_edit_input
#define edit_draw           eli_edit_draw
#define caret_from_x        eli_caret_from_x
#define ensure_back         eli_ensure_back
#define present             eli_present
#define win_create          eli_win_create
#define win_pump            eli_win_pump
#define win_wait            eli_win_wait
#define win_set_title       eli_win_set_title
#define win_client_size     eli_win_client_size
#define gdip_init           eli_gdip_init
#define request_redraw      eli_request_redraw
#define win_apply_shape     eli_win_apply_shape
#define el_alloc            eli_alloc
#define el_add_child        eli_add_child

typedef uint32_t u32;
typedef uint8_t  u8;

// Виды элементов (порядок свой, но согласован внутри библиотеки).
enum Kind {
    K_FREE, K_WINDOW, K_STACK, K_DOCK, K_WRAP, K_GRID, K_SCROLL, K_CANVAS, K_BORDER, K_GROUP, K_TABS, K_TAB,
    K_TEXT, K_BUTTON, K_CHECK, K_RADIO, K_EDIT, K_MEMO, K_PASSWORD, K_COMBO, K_LIST, K_SLIDER, K_PROGRESS,
    K_IMAGE, K_SEP, K_MENUBAR, K_MENU, K_NUMBER, K_TOOLBAR, K_TOOL, K_PAINT, K_DIALOG, K_SPACER
};
enum { A_STRETCH, A_START, A_CENTER, A_END };
enum { D_LEFT, D_TOP, D_RIGHT, D_BOTTOM };

#define EL_INF (1 << 28)
#define GMAX 24

// growable array: VEC(T) v; VPUSH(v, x); v.a[i], v.n
#define VEC(T) struct { T *a; int n, cap; }
void *vgrow(void *a, int *cap, int need, size_t size);
#define VPUSH(v, x) do { if ((v).n >= (v).cap) (v).a = vgrow((v).a, &(v).cap, (v).n + 1, sizeof(*(v).a)); (v).a[(v).n++] = (x); } while (0)
#define VERASE(v, i) do { memmove(&(v).a[(i)], &(v).a[(i) + 1], (size_t)((v).n - (i) - 1) * sizeof(*(v).a)); (v).n--; } while (0)

typedef struct GridDef {
    int nc, nr;
    u8  ct[GMAX], rt[GMAX];   // 0 auto, 1 pixels, 2 star
    int cv[GMAX], rv[GMAX];   // пиксели или вес доли *100
    int cs[GMAX], rs[GMAX];   // посчитанные размеры
    int co[GMAX], ro[GMAX];   // смещения
} GridDef;

typedef struct Anim {
    int on;
    char prop[12];
    int from, to, ms, easing, loop;
    double t0;
    int dir;                  // для loop==2
} Anim;

// Стиль-правило из таблицы (el_stylesheet): селектор + свойства строкой.
typedef struct Rule {
    char *sel;                // "button", ".card", "#name", "button.card"
    int state;                // 0 обычное, 1 :hover, 2 :pressed
    char *css;
} Rule;

typedef struct Element {
    int kind;
    int parent, first, last, next;
    // раскладка
    int w, h, minw, minh, maxw, maxh;        // -1 — по содержимому
    int ml, mt, mr, mb, pl, pt, pr, pb;
    int ha, va, dock, row, col, rspan, cspan, px, py, gap;
    int hdr_x, hdr_w, strip_h;               // вкладки: прямоугольник ярлыка / высота полосы
    int hidden, disabled, wrap, horiz;
    int dw, dh;                              // желаемый размер с полями
    int ax, ay, aw, ah;                      // размещён (координаты клиента окна)
    // цвета/рамка/шрифт
    int has_bg, has_fg, has_bd;
    u32 bg, fg, bd;
    int bw, radius, fsize, bold;             // толщина рамки, скругление (-1 из темы), шрифт
    // текст и привязки (строки в UTF-8, свои копии; NULL — пусто)
    char *text, *tip, *name, *cls, *inl;
    char *accel;                             // часть после \t в пункте меню
    const char *live;                        // показывать строку программы как есть
    int *iv; int ival, vmin, vmax;           // привязанный int; значение радио; пределы; выбранная вкладка
    char *buf; int cap;                      // текст edit/memo
    const char *const *items; int count;     // combo/list
    int scroll, hscroll;                     // прокрутка
    int caret, anchor;                       // edit/memo (символы)
    // оформление
    u32 bg2; int grad, bg_a;                 // второй цвет; 1 верт 2 гор 3 диаг; альфа фона (0 — непрозрачно)
    int sh_blur, sh_dx, sh_dy, sh_a; u32 sh_col;
    u32 hbg, pbg, hfg, hbd; int has_hbg, has_pbg, has_hfg, has_hbd;
    double hv;                               // наведение 0..1 (плавно)
    int talign, mono;
    int styled_gen;
    GridDef *grid;
    int ctx;                                 // контекстное меню
    el_fn on_click; void *click_arg;
    el_fn on_change; void *change_arg;
    int fl_click, fl_change;
    // картинка
    GpImage *img;
    // трансформ (анимации)
    int tx, ty, sc, rot, op;
    Anim anim;
    // меню
    int *menu_val;                           // el_menu_check
    int menu_open;
    int is_bar;                              // элемент меню — пункт полосы
    // число
    int step;
    // своё рисование
    void (*paint_fn)(el, int, int, void *); void *paint_arg;
    int needs_redraw;
    // данные / панель инструментов
    void *data;
    int flat;
    char *icon; GpImage *icon_img; int icon_size;
    // живость
    int alive;
} Element;

typedef struct KeyEv { int vk; WCHAR ch; int mods; int down; } KeyEv;
typedef struct Timer { int id; int ms; double next; el_fn fn; void *arg; int dead; } Timer;
typedef struct Hotkey { int mods; int vk; el_fn fn; void *arg; } Hotkey;
typedef struct Popup { int menu; int x, y; int hover; } Popup;   // открытое выпадающее меню
typedef struct Effect { char *name; int amount; } Effect;        // постобработка кадра

typedef struct Ctx {
    VEC(Element) el;                         // индекс 0 не используется (0 = нет)
    int window;
    HWND hwnd;
    int quit, close_req, inited;
    el_fn on_close; void *on_close_arg;
    // тема
    int dark;
    u32 th[16];
    u32 accent;
    int def_font;
    int corner;
    int theme_idx;
    // ввод
    int mx, my, pmx, pmy;
    int buttons, pbuttons;                   // 1 левая, 2 правая
    int wheel;                               // накоплено за кадр
    int wheel_consumed_by;
    VEC(KeyEv) keys;
    int focus, hot, active;
    int press_x, press_y;                    // где нажали (для перетаскивания)
    int dragging;
    // DPI
    double dpi;                              // 1.0 при 96
    // GDI+ (буфер кадра — поверх DIB-секции, чтобы годился и для послойного окна)
    ULONG_PTR gdip_token;
    GpImage *back; GpGraphics *g; int back_w, back_h;
    HDC memdc; HBITMAP dib; void *dibbits;
    GpGraphics *measure_g; GpImage *measure_bmp;
    // кадр
    double now_ms, last_ms;
    int dirty;
    int close_guard;
    // всплывающие меню
    VEC(Popup) popups;
    // подсказка
    int tip_el; double tip_since; int tip_show;
    // таймеры/анимации/клавиши
    VEC(Timer) timers; int timer_seq;
    VEC(Hotkey) hotkeys;
    // таблица стилей
    VEC(Rule) rules; int rules_gen;
    // нестандартное окно
    int  frameless;
    int  win_shape;                          // 0 rect,1 round,2 ellipse,3 polygon,4 pixels(по альфе)
    int  win_round;                          // радиус скругления (точки)
    VEC(int) region_pts;                     // контур (пары x,y в точках)
    int  aspect_w, aspect_h;
    int  win_opacity;                        // 0..255
    int  layered;
    int  drag_region;                        // элемент-«шапка» для перетаскивания
    int  win_minw, win_minh, win_maxw, win_maxh;
    int  win_canvas;                         // поверхность во весь фон окна (K_PAINT)
    int  resizable;
    // диалоги/оверлей
    int modal;                               // активный диалог (0 — нет)
    char *msg_title, *msg_text;
    int msg_kind;                            // 0 нет,1 message,2 ask
    void (*ask_fn)(int, void *); void *ask_arg;
    // тост
    char *toast_text; double toast_until; double toast_t0;
    // своё рисование — активная поверхность при вызове el_g_*
    int cur_paint; int paint_ox, paint_oy;
    // выпадающий список combo
    int combo_open, combo_hover, combo_scroll;
    // эффекты (постобработка кадра)
    VEC(Effect) effects;
    VEC(u32) fx_tmp;
} Ctx;

extern Ctx g;

// --- доступ к элементам (указатель живёт до следующего el_alloc) ---
#define E(h) (&g.el.a[(h)])
static __inline int valid(int h) { return h > 0 && h < g.el.n && g.el.a[h].alive; }
int  el_alloc(int kind);
void el_add_child(int parent, int child);

// --- строки (UTF-8 и UTF-16) ---
static __inline const char *S(const char *s) { return s ? s : ""; }
char  *sdup(const char *s);                  // копия; NULL и "" -> NULL
void   sset(char **dst, const char *s);      // заменить свою копию
int    seq(const char *a, const char *b);    // равны (NULL == "")
WCHAR *wdup(const char *utf8);               // новая строка UTF-16 (освобождать free)
char  *udup(const WCHAR *w);                 // новая строка UTF-8 (освобождать free)
int    wlen(const WCHAR *w);

// --- тема ---
void theme_init(void);
u32  th(int slot);                           // цвет слота темы

// --- цвета ---
static __inline ARGB argb(u32 rgb) { return 0xFF000000u | (rgb & 0xFFFFFFu); }
static __inline ARGB argba(u32 rgb, int a) { return ((ARGB)(a & 255) << 24) | (rgb & 0xFFFFFFu); }
u32 mix(u32 a, u32 b, double t);

static __inline int imin(int a, int b) { return a < b ? a : b; }
static __inline int imax(int a, int b) { return a > b ? a : b; }

// --- шрифты и текст ---
GpFont *get_font(int size, int bold, int face);
int  font_face_by_name(const char *name);
int  text_w(const WCHAR *s, int size, int bold, int mono);
void text_size(const WCHAR *s, int size, int bold, int mono, int wrapW, int maxw, int *w, int *h);
void text_size_u(const char *s, int size, int bold, int mono, int wrapW, int maxw, int *w, int *h);   // строка UTF-8
int  text_w_u(const char *s, int size, int bold, int mono);
int  px(int dip);                            // точки интерфейса -> пиксели (HiDPI)
int  dip(int pxv);

// --- GDI+ помощники (el_gfx.c) ---
void gfx_fill_rect(GpGraphics *gr, ARGB c, float x, float y, float w, float h);
void gfx_fill_path(GpGraphics *gr, ARGB c, GpPath *p);
void gfx_draw_path(GpGraphics *gr, ARGB c, float width, GpPath *p);
void gfx_fill_ellipse(GpGraphics *gr, ARGB c, float x, float y, float w, float h);
void gfx_draw_ellipse(GpGraphics *gr, ARGB c, float width, float x, float y, float w, float h);
void gfx_line(GpGraphics *gr, ARGB c, float width, float x0, float y0, float x1, float y1, int round);
void gfx_lines(GpGraphics *gr, ARGB c, float width, const GpPoint *pts, int n);
void gfx_fill_poly(GpGraphics *gr, ARGB c, const GpPointF *pts, int n);
GpPath *gfx_rrect(GpRectF r, float rad);    // новый контур скруглённого прямоугольника
void round_rect_path(GpPath *p, GpRectF r, float rad);
GpStringFormat *gfx_format(int nowrap, int align, int valign, int ellipsis);   // освобождать GdipDeleteStringFormat
void gfx_text(GpGraphics *gr, const WCHAR *s, GpFont *f, ARGB c, GpRectF r, const GpStringFormat *sf);
void gfx_text_at(GpGraphics *gr, const WCHAR *s, GpFont *f, ARGB c, float x, float y);
static __inline GpRectF rectf(float x, float y, float w, float h) { GpRectF r; r.X = x; r.Y = y; r.Width = w; r.Height = h; return r; }

// --- раскладка ---
void measure(int h, int availw, int availh);
void arrange(int h, int x, int y, int w, int hh, int unused);
void layout_root(void);

// --- рисование дерева ---
void draw_tree(void);
void draw_element(int h);

// --- ввод и события ---
void input_frame(void);
void fire_click(int h);
void fire_change(int h);
int  hit_test(int root, int x, int y);
int  is_container(int kind);
int  is_focusable(int kind);
void set_focus(int h);

// --- меню ---
void menu_open_bar(int item);
void menu_close_all(void);
void menu_frame_input(void);
void menu_draw(void);
int  menu_measure_width(int menu);

// --- анимации/таймеры ---
void anim_frame(void);
int  anim_busy(void);
double ease(int kind, double t);
void transform_of(int h, double *tx, double *ty, double *sc, double *rot, double *op);

// --- стили ---
void apply_style_string(int h, const char *css);
void restyle_all(void);
void style_apply_sheet(int h);

// --- эффекты (постобработка) ---
void apply_effects(void);
int  effects_animated(void);

// --- диалоги/тосты ---
void overlay_frame_input(void);
void overlay_draw(void);
void draw_tooltip(void);
// --- combo ---
void combo_frame_input(void);
void combo_draw(void);

// --- edit/memo помощники ---
void edit_input(int h, const KeyEv *keys, int n);
void edit_draw(int h);
int  caret_from_x(int h, int mx, int my);

// --- win32 ---
void ensure_back(int w, int h);
void present(void);
void win_create(const char *title, int w, int h);
void win_pump(void);                          // обработать очередь сообщений Windows
void win_wait(int timeout_ms);                // уснуть до сообщения/таймаута (экономия ЦП)
void win_set_title(const char *t);
void win_client_size(int *w, int *h);
void gdip_init(void);
void request_redraw(void);                    // пометить кадр грязным
void win_apply_shape(void);                   // наложить форму окна (регион/слой)
