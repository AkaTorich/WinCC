// WUI — внутреннее: тема, кисти, цвета. Общее для всех файлов lib\wui.
#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <math.h>
#define WUI_INTERNAL                // файлы самой библиотеки не тянут её ещё раз
#include <wui.h>

typedef unsigned int WuiColor;              // 0xAARRGGBB

typedef struct { float left, top, right, bottom; } WuiEdges;
typedef struct { float top_left, top_right, bottom_right, bottom_left; } WuiCorners;

#include "style.h"

// ---- кисти и тени из ThemeLibrary (themes.c) ----
enum { WUI_BRUSH_NONE, WUI_BRUSH_SOLID, WUI_BRUSH_LINEAR, WUI_BRUSH_RADIAL, WUI_BRUSH_TEXTURE };
typedef struct { float offset; WuiColor color; } WuiStop;
typedef struct {
    int kind;
    float opacity;
    float x1, y1, x2, y2;       // линейная: начало и конец (доли рамки); радиальная: центр и исток
    float rx, ry;               // радиальная: радиусы (доли)
    int count;
    WuiStop stops[6];
} WuiBrushDef;
typedef struct { WuiColor color; float blur, depth, opacity, direction; } WuiEffectDef;

extern const WuiBrushDef wui_brushes[];
extern const WuiEffectDef wui_effects[];

// ---- тема: поля Theme из ThemeLibrary ----
enum { WUI_FAMILY_GLASS, WUI_FAMILY_STEEL, WUI_FAMILY_PARCHMENT, WUI_FAMILY_STONE, WUI_FAMILY_TERMINAL, WUI_FAMILY_FLAT };

enum {                          // TextureKind
    WUI_TEX_NONE, WUI_TEX_PARCHMENT, WUI_TEX_LAPIS, WUI_TEX_PAPYRUS, WUI_TEX_WASHI, WUI_TEX_MARBLE,
    WUI_TEX_MALACHITE, WUI_TEX_OBSIDIAN, WUI_TEX_BRASS, WUI_TEX_ALUMINIUM, WUI_TEX_WOOD, WUI_TEX_LEATHER,
    WUI_TEX_LINEN, WUI_TEX_GRANITE, WUI_TEX_SLATE, WUI_TEX_CARBON, WUI_TEX_FROST, WUI_TEX_SAND
};

enum {                          // DecorKind (флаги)
    WUI_DECOR_ORBS = 1, WUI_DECOR_STARS = 2, WUI_DECOR_VIGNETTE = 4, WUI_DECOR_GRID = 8,
    WUI_DECOR_SCANLINES = 16, WUI_DECOR_POLARGRID = 32, WUI_DECOR_HEXAGONS = 64, WUI_DECOR_CONTOURS = 128,
    WUI_DECOR_CIRCUIT = 256, WUI_DECOR_FLOWEROFLIFE = 512, WUI_DECOR_WAVES = 1024
};

enum {                          // FrameKind
    WUI_FRAME_NONE, WUI_FRAME_TRIPLELINE, WUI_FRAME_DOUBLEGOLD, WUI_FRAME_ARTNOUVEAU, WUI_FRAME_EGYPTIAN,
    WUI_FRAME_ENGRAVED, WUI_FRAME_SEAL, WUI_FRAME_RIVETED, WUI_FRAME_MEANDER, WUI_FRAME_ARTDECO,
    WUI_FRAME_STITCHED, WUI_FRAME_CELTIC, WUI_FRAME_ARABESQUE, WUI_FRAME_RUNIC
};

// Цвета палитры по порядку полей Theme.
enum {
    WUI_C_BACKGROUND1, WUI_C_BACKGROUND2, WUI_C_BACKGROUND3, WUI_C_TEXT, WUI_C_TEXTDIM, WUI_C_TEXTONACCENT,
    WUI_C_ACCENT, WUI_C_ACCENT2, WUI_C_ACCENTSTART, WUI_C_ACCENTEND, WUI_C_GLOW, WUI_C_SUCCESS, WUI_C_WARNING,
    WUI_C_DANGER, WUI_C_SURFACE, WUI_C_EDGE, WUI_C_FIELD, WUI_C_FIELDEDGE, WUI_C_POPUP, WUI_C_SELECTION,
    WUI_C_LCD, WUI_C_LCDTEXT, WUI_C_ORB1, WUI_C_ORB2, WUI_C_ORB3, WUI_C_PAPER,
    WUI_C_COUNT
};

typedef struct {
    const char *id, *name, *group, *name_en;
    int family, texture, decor, frame, is_dark;
    WuiColor c[WUI_C_COUNT];
    const char *font, *heading_font, *body_font, *mono_font;
    float font_size, window_radius, card_radius, control_radius;
    const WuiStyle *style;
} WuiTheme;

extern const WuiTheme wui_theme_list[];
extern const int wui_theme_total;              // сколько тем в таблице

// ---- цвета (color.c) — как ColorMath в ThemeLibrary ----
#define WUI_A(c) ((int)((c) >> 24) & 255)
#define WUI_R(c) ((int)((c) >> 16) & 255)
#define WUI_G(c) ((int)((c) >> 8) & 255)
#define WUI_B(c) ((int)(c) & 255)
#define WUI_ARGB(a, r, g, b) (((WuiColor)(a) << 24) | ((WuiColor)(r) << 16) | ((WuiColor)(g) << 8) | (WuiColor)(b))

double   wui_round(double v);                           // как Math.Round в .NET: половина — к чётному
WuiColor wui_alpha(WuiColor c, int alpha);              // WithAlpha
WuiColor wui_mix(WuiColor a, WuiColor b, double t);     // Mix
WuiColor wui_lighten(WuiColor c, double t);
WuiColor wui_darken(WuiColor c, double t);
WuiColor wui_from_hsl(double hue, double sat, double light, int alpha);
void     wui_to_hsl(WuiColor c, double *hue, double *sat, double *light);
WuiColor wui_with_lightness(WuiColor c, double light);

// ---- System.Random из .NET Framework (random.c): то же зерно — те же числа ----
typedef struct { int seed[56]; int next, nextp; } WuiRandom;
void   wui_random_init(WuiRandom *r, int seed);         // new Random(seed)
int    wui_random_next(WuiRandom *r);                   // Next()
int    wui_random_below(WuiRandom *r, int max);         // Next(max)
int    wui_random_range(WuiRandom *r, int min, int max);// Next(min, max)
double wui_random_double(WuiRandom *r);                 // NextDouble()

// ---- фактуры (texture.c) ----
// Плитка фактуры: 512·scale точек в сторону, точки 0xAARRGGBB (непрозрачные), как Textures.Get.
// Хранится в памяти до конца программы (кэш по виду, цвету и масштабу). 0 — у вида нет фактуры.
const unsigned *wui_texture(int kind, WuiColor base, int scale);

// ---- фон окна (backdrop.c, patterns.c, frames.c) ----
// Слой поверх градиента/фактуры: пятна света, звёзды, узоры, виньетка, рамка — как Backdrop.OnRender.
// w, h — размер в DIP; у g уже стоит масштаб (1 DIP = scale точек), scale — для плиток в точках.
void wui_backdrop_draw(GpGraphics *g, const WuiTheme *t, float w, float h, float scale);
// Узор-плитка во весь фон (Hexagons, Contours, Circuit, FlowerOfLife, Waves) — Backdrop.DrawPattern.
void wui_pattern_draw(GpGraphics *g, const WuiTheme *t, float w, float h, int decor_kind, float scale);
// Рамка темы (t->frame) — DrawTripleLine, DrawFrame* из Backdrop/BackdropArt/BackdropFrames.
void wui_frame_draw(GpGraphics *g, const WuiTheme *t, float w, float h);

// ======================================================================================
// Окна и элементы
// ======================================================================================

typedef struct { float x, y, w, h; } WuiRect;

// ---- текущая тема (core.c) ----
extern const WuiTheme *wui_t;                       // тема всех окон
#define WUI_S (wui_t->style)                        // её оформление: WUI_S->Card_Background …
void wui_init(void);                                // GDI+, масштаб экрана — один раз
void wui_theme_load(const char *key);               // запомненная тема программы
char *wui_strdup(const char *s);
wchar_t *wui_wide(const char *s);                   // UTF-8 → UTF-16 (malloc)
char *wui_utf8(const wchar_t *w, int len);          // UTF-16 → UTF-8 (malloc), len < 0 — до нуля

// ---- рисование (paint.c) ----
GpPath *wui_round_path(WuiRect r, WuiCorners k);    // скруглённый прямоугольник
WuiCorners wui_corners(float radius);
WuiRect wui_inset(WuiRect r, float l, float t, float rr, float b);
WuiRect wui_deflate(WuiRect r, WuiEdges e);
// кисть темы по номеру (wui_brushes) для рамки r; 0 — пусто. opacity умножает прозрачность
GpBrush *wui_brush(int index, WuiRect r, float opacity);
GpBrush *wui_solid(WuiColor c);
void wui_fill(GpGraphics *g, int brush, WuiRect r, WuiCorners k, float opacity);
void wui_stroke(GpGraphics *g, int brush, WuiRect r, WuiCorners k, WuiEdges thick, float opacity);
void wui_fill_color(GpGraphics *g, WuiColor c, WuiRect r, float radius);
void wui_line(GpGraphics *g, WuiColor c, float width, float x1, float y1, float x2, float y2);
// тень/свечение (wui_effects[effect]) от фигуры, залитой кистью brush
void wui_effect(GpGraphics *g, int effect, int brush, WuiRect r, WuiCorners k, float opacity);
float wui_effect_reach(int effect);                 // насколько тень выходит за фигуру (DIP)
extern float wui_paint_scale;                       // точек в DIP у окна, которое сейчас рисуется
int wui_effect_pad(int effect);                     // поле вокруг фигуры для размытия, точки
GpBitmap *wui_effect_bitmap(GpBitmap *shape, int effect);   // альфа фигуры → размытая тень её цветом
WuiColor wui_brush_color(int brush);                // первый цвет кисти (для текста, значков)

// ---- текст (text.c) ----
typedef struct {
    const char *family;     // список шрифтов через запятую, как в теме
    float size;
    int weight;             // 400 — обычный, 600 — полужирный…
    int italic, caps;       // caps — заглавные помельче (Typography.Capitals)
} WuiFont;
WuiFont wui_font(const char *family, float size, int weight);
// размер текста; max_w > 0 — с переносом по словам
void wui_text_size(const WuiFont *f, const char *text, float max_w, float *w, float *h);
// align: 0 — влево, 1 — по центру, 2 — вправо; wrap — переносить; по вертикали — по центру r
void wui_draw_text(GpGraphics *g, const WuiFont *f, const char *text, WuiRect r, WuiColor color, int align, int wrap);
// то же для строки UTF-16 (поле ввода): x каждого знака
void wui_draw_wtext(GpGraphics *g, const WuiFont *f, const wchar_t *s, int n, float x, float y, WuiColor color);
float wui_wtext_width(const WuiFont *f, const wchar_t *s, int n);
void wui_text_glow(GpGraphics *g, const WuiFont *f, const char *text, WuiRect r, int effect, int align);
float wui_line_height(const WuiFont *f);

// ---- элементы (layout.c, controls.c, …) ----
enum {
    WUI_T_ROOT, WUI_T_COLUMN, WUI_T_ROW, WUI_T_WRAP, WUI_T_SCROLL, WUI_T_CARD, WUI_T_SPACER, WUI_T_SEPARATOR,
    WUI_T_LABEL, WUI_T_BUTTON, WUI_T_CAPTION, WUI_T_NAV_SECTION, WUI_T_NAV_ITEM, WUI_T_TEXTBOX, WUI_T_CHECK,
    WUI_T_SWITCH, WUI_T_RADIO, WUI_T_SLIDER, WUI_T_PROGRESS, WUI_T_COMBO, WUI_T_LIST, WUI_T_ICON,
    WUI_T_CANVAS, WUI_T_TABS, WUI_T_TAB, WUI_T_TITLE
};

// события мыши для класса элемента
enum { WUI_MOUSE_DOWN, WUI_MOUSE_UP, WUI_MOUSE_MOVE, WUI_MOUSE_DOUBLE, WUI_MOUSE_WHEEL, WUI_MOUSE_LEAVE };

typedef struct WuiClass {
    void (*measure)(WuiControl *c, float avail_w, float avail_h);   // → c->mw, c->mh
    void (*arrange)(WuiControl *c);                                 // c->r уже стоит: детей по местам
    void (*paint)(WuiControl *c, GpGraphics *g);
    void (*paint_over)(WuiControl *c, GpGraphics *g);               // поверх детей (рамка карточки)
    int  (*mouse)(WuiControl *c, int event, float x, float y, int extra);
    int  (*key)(WuiControl *c, int vk, int ch);                     // ch — набранный знак или 0
    void (*focus)(WuiControl *c, int on);
    int focusable;
    int interactive;        // ловит мышь (не даёт тащить окно)
    float reach;            // насколько рисунок выходит за рамку (тень, свечение)
} WuiClass;

struct WuiWin;

struct WuiControl {
    const WuiClass *cls;
    int type;
    WuiControl *parent, *first, *last, *next;
    struct WuiWin *win;
    WuiRect r;                  // место в окне, DIP
    float mw, mh;               // желаемый размер без полей
    float fix_w, fix_h, min_w, min_h, weight;
    int align;
    WuiEdges margin, pad;
    int own_pad;                // wui_padding: свои поля (карточка тогда не берёт поля темы)
    float gap;
    int visible, enabled;
    int hot, pressed, focused, checked;
    int style;                  // вид надписи, кнопки
    char *text;
    double value, vmin, vmax;
    const WuiIconData *icon;
    WuiHandler on_click, on_change;
    void *click_user, *change_user, *data;
    void *ext;                  // своё у каждого вида
    float scroll, scroll_max;   // прокрутка столбца
};

typedef struct WuiWin {
    HWND hwnd;
    WuiControl *root, *content, *titlebar, *user_bar, *title, *logo;
    char *subtitle;
    char *key;
    int extend, maximized, active, shown;
    float scale;                // точек в DIP
    int pw, ph;                 // размер окна в точках
    float min_w, min_h;
    float title_min;            // уже этого окно не сжимается: строка заголовка без текста заголовка
    HDC dc;
    HBITMAP dib, old_bmp;
    void *bits;
    GpBitmap *surface;
    GpGraphics *g;
    GpBitmap *back;             // фон окна (градиент/фактура + слой Backdrop), без элементов
    int back_valid, layout_valid;
    RECT dirty;                 // что перерисовать (точки), пусто — ничего
    WuiControl *hot, *pressed, *focus, *capture;
    char *toast;
    DWORD toast_until;
    struct WuiWin *next;
} WuiWin;

extern WuiWin *wui_windows;

WuiControl *wui_new(WuiControl *parent, int type, const WuiClass *cls);
void wui_measure(WuiControl *c, float avail_w, float avail_h);
void wui_arrange(WuiControl *c, WuiRect r);
void wui_paint_tree(WuiControl *c, GpGraphics *g, RECT *clip_px, float scale);
WuiControl *wui_hit(WuiControl *c, float x, float y);
void wui_invalidate(WuiControl *c);                 // перерисовать место элемента
void wui_invalidate_rect(WuiWin *w, WuiRect r);
void wui_relayout(WuiControl *c);                   // поменялся размер — переложить окно
void wui_fire_click(WuiControl *c);
void wui_fire_change(WuiControl *c);
int  wui_is_shown(WuiControl *c);                   // виден и все родители видны
WuiRect wui_visible_rect(WuiControl *c);            // часть, не срезанная прокруткой
extern const WuiClass wui_class_column, wui_class_row, wui_class_wrap;

// окно (window.c)
void wui_window_set_cursor(int idc);
void wui_window_capture(WuiWin *w, WuiControl *c);
void wui_window_focus(WuiWin *w, WuiControl *c);
void wui_window_retheme(WuiWin *w);
void wui_caret_restart(void);
extern int wui_caret_on;

// всплывающий список (popup.c): у раскрывающегося списка и выбора темы
typedef struct {
    const char *text;
    int header;                 // подпись группы, не выбирается
} WuiPopupItem;
typedef void (*WuiPopupDone)(WuiControl *owner, int index);
void wui_popup_open(WuiControl *owner, const WuiPopupItem *items, int count, int selected, WuiPopupDone done);
void wui_popup_close(void);
int  wui_popup_is_open(WuiControl *owner);
int  wui_popup_key(int vk);                          // клавиши хозяина, пока список открыт
int  wui_popup_wheel(int delta);

// разное между файлами
WuiControl *wui_caption_button(WuiControl *parent, int which);   // 0 — свернуть, 1 — развернуть, 2 — закрыть
void wui_scroll_by(WuiControl *c, float dy);
void wui_animate(WuiControl *c);                    // перерисовывать, пока идёт (бегущая полоса)
