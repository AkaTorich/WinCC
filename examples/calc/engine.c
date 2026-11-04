// Калькулятор: счёт, проценты, журнал и вид чисел.
#include "calc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_DIGITS 15

// ---- вид чисел: «12 345,678», большие и малые — «1,25e+20» ----

// группы по три цифры (узкий пробел) и запятая вместо точки
static void group(const char *raw, char *out, int cap)
{
    int k = 0;
    const char *p = raw;
    if (*p == '-') {
        if (k + 3 < cap) { memcpy(out + k, "\xE2\x88\x92", 3); k += 3; }      // настоящий минус
        p++;
    }
    int intlen = 0;
    while (p[intlen] >= '0' && p[intlen] <= '9') intlen++;
    for (int i = 0; i < intlen && k < cap - 4; i++) {
        out[k++] = p[i];
        int left = intlen - i - 1;
        if (left > 0 && left % 3 == 0 && intlen > 3) {
            memcpy(out + k, "\xE2\x80\x89", 3);      // тонкий пробел
            k += 3;
        }
    }
    for (p += intlen; *p && k < cap - 1; p++)
        out[k++] = *p == '.' ? ',' : *p;
    out[k] = 0;
}

void format_number(double v, char *out, int cap)
{
    if (v != v || v > 1.7e308 || v < -1.7e308) {
        snprintf(out, cap, "Ошибка");
        return;
    }
    if (v == 0) v = 0;                  // без «−0»
    double a = fabs(v);
    char raw[64];
    if (a != 0 && (a >= 1e15 || a < 1e-9)) {
        // 1.250000000e+20 → «1,25×10²⁰»: степень надстрочными цифрами
        snprintf(raw, sizeof(raw), "%.9e", v);
        char *e = strchr(raw, 'e');
        int power = atoi(e + 1);
        char *z = e - 1;
        while (*z == '0') z--;
        if (*z == '.') z--;
        z[1] = 0;
        group(raw, out, cap);
        static const char *sup[10] = { "\xE2\x81\xB0", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4",
                                       "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9" };
        char pw[40];
        int k = snprintf(pw, sizeof(pw), "\xC3\x97" "10%s", power < 0 ? "\xE2\x81\xBB" : "");
        char digits[8];
        snprintf(digits, sizeof(digits), "%d", power < 0 ? -power : power);
        for (char *d = digits; *d && k < 34; d++)
            k += snprintf(pw + k, sizeof(pw) - k, "%s", sup[*d - '0']);
        if ((int)(strlen(out) + strlen(pw)) < cap) strcat(out, pw);
        return;
    } else {
        snprintf(raw, sizeof(raw), "%.12g", v);
    }
    group(raw, out, cap);
}

int parse_number(const char *s, double *v)
{
    char buf[64];
    int k = 0;
    for (; *s && k < 62; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9') buf[k++] = (char)c;
        else if (c == ',' || c == '.') buf[k++] = '.';
        else if (c == '-' && k == 0) buf[k++] = '-';
        else if (c == 0xE2 && (unsigned char)s[1] == 0x88 && (unsigned char)s[2] == 0x92 && k == 0) { buf[k++] = '-'; s += 2; }
        else if (c == ' ' || c == 0xC2 || c == 0xA0 || c == 0xE2 || c == 0x80 || c == 0x89 || c == 0xAF) continue;
        else if (c == 'e' || c == 'E' || c == '+') buf[k++] = (char)c;
        else return 0;
    }
    buf[k] = 0;
    if (!k) return 0;
    char *end;
    *v = strtod(buf, &end);
    return *end == 0;
}

static double entry_value(const Calc *c)
{
    double v = 0;
    sscanf(c->entry, "%lf", &v);
    return v;
}

static void set_entry(Calc *c, double v)
{
    snprintf(c->entry, sizeof(c->entry), "%.15g", v);
    c->typing = 0;
}

static const char *op_text(char op)
{
    switch (op) {
    case '+': return "+";
    case '-': return "\xE2\x88\x92";    // −
    case '*': return "\xC3\x97";        // ×
    default:  return "\xC3\xB7";        // ÷
    }
}

// строка выражения: «12 + 3 ×»
static void build_line(Calc *c, int with_entry)
{
    int k = 0;
    c->line[0] = 0;
    for (int i = 0; i < c->n; i++) {
        char num[64];
        format_number(c->vals[i], num, sizeof(num));
        k += snprintf(c->line + k, sizeof(c->line) - k, "%s %s ", num, op_text(c->ops[i]));
        if (k >= (int)sizeof(c->line) - 70) break;
    }
    if (with_entry && c->entry[0]) {
        char num[64];
        format_number(entry_value(c), num, sizeof(num));
        snprintf(c->line + k, sizeof(c->line) - k, "%s =", num);
    }
}

// значение выражения: сначала × и ÷, потом + и −; 0 — деление на ноль
static int evaluate(const double *v, const char *o, int count, double *out)
{
    double sum = 0, term = v[0];
    char add = '+';
    for (int i = 0; i < count - 1; i++) {
        char op = o[i];
        if (op == '*') term *= v[i + 1];
        else if (op == '/') {
            if (v[i + 1] == 0) return 0;
            term /= v[i + 1];
        } else {
            sum = add == '+' ? sum + term : sum - term;
            add = op;
            term = v[i + 1];
        }
    }
    *out = add == '+' ? sum + term : sum - term;
    return 1;
}

// ---- журнал ----

typedef struct { char expr[400]; char result[64]; double value; } Record;
static Record g_hist[100];
static int g_hist_n;

static void history_add(const char *expr, double value)
{
    memmove(g_hist + 1, g_hist, sizeof(Record) * (g_hist_n < 99 ? g_hist_n : 99));
    if (g_hist_n < 100) g_hist_n++;
    snprintf(g_hist[0].expr, sizeof(g_hist[0].expr), "%s", expr);
    format_number(value, g_hist[0].result, sizeof(g_hist[0].result));
    g_hist[0].value = value;
}

int history_count(void) { return g_hist_n; }
const char *history_expr(int i) { return g_hist[i].expr; }
const char *history_result(int i) { return g_hist[i].result; }
double history_value(int i) { return g_hist[i].value; }
void history_clear(void) { g_hist_n = 0; }

// ---- кнопки ----

void calc_reset(Calc *c)
{
    memset(c, 0, sizeof(*c));
}

static void after_error(Calc *c)
{
    if (c->error) calc_reset(c);
}

static int digits_in(const char *s)
{
    int n = 0;
    for (; *s; s++) if (*s >= '0' && *s <= '9') n++;
    return n;
}

static void start_typing(Calc *c)
{
    if (c->done) {
        c->n = 0;
        c->line[0] = 0;
        c->done = 0;
    }
    if (!c->typing) {
        c->entry[0] = 0;
        c->typing = 1;
    }
}

void calc_digit(Calc *c, int d)
{
    after_error(c);
    start_typing(c);
    if (digits_in(c->entry) >= MAX_DIGITS) return;
    size_t len = strlen(c->entry);
    if (!strcmp(c->entry, "0")) len = 0;                      // «0» → «7»
    else if (!strcmp(c->entry, "-0")) len = 1;
    c->entry[len] = (char)('0' + d);
    c->entry[len + 1] = 0;
}

void calc_comma(Calc *c)
{
    after_error(c);
    start_typing(c);
    if (strchr(c->entry, '.')) return;
    if (!c->entry[0] || !strcmp(c->entry, "-")) strcat(c->entry, "0");
    strcat(c->entry, ".");
}

void calc_op(Calc *c, char op)
{
    after_error(c);
    if (op == '-' && !c->n && !c->entry[0] && !c->done) {     // минус в самом начале — отрицательное число
        strcpy(c->entry, "-");
        c->typing = 1;
        return;
    }
    if (c->done) {                      // итог — первое число нового выражения
        c->done = 0;
        c->n = 0;
    }
    if (c->entry[0]) {
        if (c->n < 63) {
            c->vals[c->n] = entry_value(c);
            c->ops[c->n] = op;
            c->n++;
        }
        c->entry[0] = 0;
        c->typing = 0;
    } else if (c->n > 0) {
        c->ops[c->n - 1] = op;          // передумали: другое действие
    } else {
        c->vals[0] = 0;
        c->ops[0] = op;
        c->n = 1;
    }
    build_line(c, 0);
}

void calc_equals(Calc *c)
{
    if (c->error) { calc_reset(c); return; }
    if (c->n == 0) return;
    double v[65];
    memcpy(v, c->vals, sizeof(double) * c->n);
    int count = c->n;
    v[count++] = c->entry[0] ? entry_value(c) : c->vals[c->n - 1];   // «5 + =» — это 5 + 5
    if (!c->entry[0]) set_entry(c, v[count - 1]);
    build_line(c, 1);
    double r;
    if (!evaluate(v, c->ops, count, &r)) {
        c->error = 1;
        snprintf(c->entry, sizeof(c->entry), "0");
        return;
    }
    history_add(c->line, r);
    set_entry(c, r);
    c->done = 1;
}

int calc_clear_all(const Calc *c)
{
    return !(c->entry[0] && c->typing) || c->error;
}

void calc_clear(Calc *c)
{
    if (!calc_clear_all(c)) {           // C — только набираемое число
        c->entry[0] = 0;
        c->typing = 0;
    } else {
        calc_reset(c);
    }
}

void calc_back(Calc *c)
{
    if (c->error) { calc_reset(c); return; }
    if (c->done) {                      // после «=» стирается строка выражения
        c->line[0] = 0;
        c->n = 0;
        c->done = 0;
        return;
    }
    if (!c->typing) return;
    size_t len = strlen(c->entry);
    if (len) c->entry[--len] = 0;
    if (!strcmp(c->entry, "-") || !strcmp(c->entry, "-0")) c->entry[0] = 0;
}

void calc_percent(Calc *c)
{
    after_error(c);
    double b = c->entry[0] ? entry_value(c) : (c->n ? c->vals[c->n - 1] : 0);
    double r = b / 100;
    char last = c->n ? c->ops[c->n - 1] : 0;
    if (!c->done && (last == '+' || last == '-')) {
        // 200 + 10 % = 200 + 20: процент от того, к чему прибавляют
        double base;
        if (evaluate(c->vals, c->ops, c->n, &base)) r = base * b / 100;
    }
    if (c->done) { c->n = 0; c->line[0] = 0; c->done = 0; }
    set_entry(c, r);
    c->typing = 1;
}

void calc_sign(Calc *c)
{
    after_error(c);
    if (c->typing && c->entry[0]) {
        if (c->entry[0] == '-') memmove(c->entry, c->entry + 1, strlen(c->entry));
        else if (strlen(c->entry) < sizeof(c->entry) - 2) {
            memmove(c->entry + 1, c->entry, strlen(c->entry) + 1);
            c->entry[0] = '-';
        }
        return;
    }
    double v = c->entry[0] ? entry_value(c) : (c->n ? c->vals[c->n - 1] : 0);
    if (c->done) { c->n = 0; c->line[0] = 0; c->done = 0; }
    set_entry(c, -v);
    c->typing = 1;
}

void calc_set(Calc *c, double v)
{
    if (c->error || c->done) calc_reset(c);
    set_entry(c, v);
    c->typing = 1;
}

char calc_active_op(const Calc *c)
{
    return c->n > 0 && !c->entry[0] && !c->done && !c->error ? c->ops[c->n - 1] : 0;
}

void calc_display(const Calc *c, char *out, int cap)
{
    if (c->error) {
        snprintf(out, cap, "Делить на ноль нельзя");
        return;
    }
    if (c->entry[0]) {
        if (c->typing) group(c->entry, out, cap);           // как набрано: «0,» и «1,50» остаются
        else format_number(entry_value(c), out, cap);
        return;
    }
    format_number(c->n ? c->vals[c->n - 1] : 0, out, cap);
}

int calc_value(const Calc *c, double *v)
{
    if (c->error) return 0;
    *v = c->entry[0] ? entry_value(c) : (c->n ? c->vals[c->n - 1] : 0);
    return 1;
}

const char *calc_line(const Calc *c)
{
    return c->line;
}
