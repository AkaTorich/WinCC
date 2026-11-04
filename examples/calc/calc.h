// Калькулятор на WinCC: общее для трёх частей.
//   engine.c — счёт (с приоритетом × ÷ над + −), проценты, журнал, вид чисел
//   draw.c   — всё рисование: GDI+ (<gdiplus.h> WinCC) со сглаживанием, расстановка кнопок, значок
//   main.c   — окно, мышь, клавиатура, буфер обмена, масштаб экрана
// Собрать:  wcc main.c engine.c draw.c -o Калькулятор.exe
#pragma once
#include <windows.h>

// ---- счёт ----
typedef struct {
    double vals[64];        // введённые числа
    char ops[64];           // действие после каждого: '+', '-', '*', '/'
    int n;                  // сколько чисел уже с действием
    char entry[40];         // число на экране: набирается или итог ('.' — дробная часть)
    int typing;             // число набирается (можно стирать по цифре)
    int done;               // только что нажато «=»
    int error;              // на экране сообщение об ошибке
    char line[400];         // строка выражения над числом (UTF-8)
} Calc;

void calc_reset(Calc *c);
void calc_digit(Calc *c, int d);
void calc_comma(Calc *c);
void calc_op(Calc *c, char op);
void calc_equals(Calc *c);
void calc_clear(Calc *c);           // C — стереть число, AC — всё
void calc_back(Calc *c);
void calc_percent(Calc *c);
void calc_sign(Calc *c);
void calc_set(Calc *c, double v);   // число из журнала или из буфера обмена
int  calc_clear_all(const Calc *c); // кнопка сейчас «AC» (иначе «C»)
char calc_active_op(const Calc *c); // действие, ждущее второе число, или 0
void calc_display(const Calc *c, char *out, int cap);      // большое число (UTF-8)
int  calc_value(const Calc *c, double *v);                 // число на табло; 0 — там ошибка
const char *calc_line(const Calc *c);                      // строка выражения
void format_number(double v, char *out, int cap);          // «1 234,5»
int  parse_number(const char *s, double *v);               // «1 234,5» / «1234.5» → число

// ---- журнал ----
int history_count(void);
const char *history_expr(int i);    // 0 — самое новое
const char *history_result(int i);
double history_value(int i);
void history_clear(void);

// ---- кнопки ----
enum { K_DIGIT, K_COMMA, K_OP, K_EQ, K_CLEAR, K_BACK, K_PERCENT, K_SIGN };
enum { LOOK_DIGIT, LOOK_FUNC, LOOK_OP, LOOK_EQ };
typedef struct {
    const wchar_t *label;
    int key, arg, look;
} Button;
#define BUTTONS 20
extern const Button g_buttons[BUTTONS];

// ---- что видно в окне ----
typedef struct { float x, y, w, h; } Box;
typedef struct {
    Calc calc;
    float scale;                    // масштаб экрана (1 = 96 dpi)
    int width, height;              // клиентская часть окна
    Box btn[BUTTONS];
    float hover[BUTTONS];           // подсветка при наведении, плавно 0…1
    float press[BUTTONS];           // вспышка нажатия 0…1
    int hot, held;                  // кнопка под мышью, нажатая мышью (-1 нет)
    int show_history;
    Box hist_panel, hist_clear;
    Box hist_item[64];
    int hist_index[64], hist_shown;
    int hist_hot, hist_scroll;
    int hist_clear_hot;
} View;

void layout(View *v);
void paint(View *v, HDC dc);
HICON make_icon(int size);
void draw_start(void);
void draw_stop(void);
