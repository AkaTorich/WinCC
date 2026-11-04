// printf-style formatting for the compiler: %s %c %d %i %u %x %X %p %%,
// flags '-' '+' '0', a width, length modifiers l / ll (64-bit).
#include "cc.h"

typedef struct {
    char *buf;
    int cap, len;
} Out;

static void put(Out *o, char c)
{
    if (o->len + 1 < o->cap)
        o->buf[o->len] = c;
    o->len++;
}

static int digits(char *tmp, u64 v, int base, int upper)
{
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char rev[24];
    int n = 0;
    do {
        rev[n++] = d[v % (u64)base];
        v /= (u64)base;
    } while (v);
    for (int i = 0; i < n; i++)
        tmp[i] = rev[n - 1 - i];
    return n;
}

int cc_vformat(char *buf, int cap, const char *fmt, va_list ap)
{
    Out o = { buf, cap, 0 };
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            put(&o, *f);
            continue;
        }
        f++;
        int left = 0, plus = 0;
        char pad = ' ';
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '+') plus = 1;
            else if (*f == '0') pad = '0';
            else break;
        }
        int width = 0;
        while (*f >= '0' && *f <= '9')
            width = width * 10 + (*f++ - '0');
        int wide = 0;
        while (*f == 'l') {
            wide = 1;
            f++;
        }
        char tmp[40];
        int n = 0;
        const char *s = tmp;
        switch (*f) {
        case 's':
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            n = cc_strlen(s);
            pad = ' ';
            break;
        case 'c': {
            u32 c = va_arg(ap, u32);
            if (c < 0x80) tmp[n++] = (char)c;
            else if (c < 0x800) { tmp[n++] = (char)(0xC0 | (c >> 6)); tmp[n++] = (char)(0x80 | (c & 0x3F)); }
            else { tmp[n++] = (char)(0xE0 | (c >> 12)); tmp[n++] = (char)(0x80 | ((c >> 6) & 0x3F)); tmp[n++] = (char)(0x80 | (c & 0x3F)); }
            break;
        }
        case 'd':
        case 'i': {
            i64 v = wide ? va_arg(ap, i64) : (i64)va_arg(ap, int);
            u64 mag = v < 0 ? (u64)(-(v + 1)) + 1 : (u64)v;
            if (v < 0) tmp[n++] = '-';
            else if (plus) tmp[n++] = '+';
            n += digits(tmp + n, mag, 10, 0);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            u64 v = wide ? va_arg(ap, u64) : (u64)va_arg(ap, u32);
            n = digits(tmp, v, *f == 'u' ? 10 : 16, *f == 'X');
            break;
        }
        case 'p': {
            u64 v = (u64)va_arg(ap, void *);
            tmp[n++] = '0';
            tmp[n++] = 'x';
            n += digits(tmp + n, v, 16, 1);
            break;
        }
        case '%':
            tmp[n++] = '%';
            break;
        case 0:
            f--;
            continue;
        default:
            tmp[n++] = '%';
            tmp[n++] = *f;
            break;
        }
        int fill = width > n ? width - n : 0;
        if (!left) {
            if (pad == '0' && n && (s[0] == '-' || s[0] == '+')) {
                put(&o, s[0]);
                s++;
                n--;
            }
            while (fill--)
                put(&o, pad);
        }
        for (int i = 0; i < n; i++)
            put(&o, s[i]);
        if (left)
            while (fill--)
                put(&o, ' ');
    }
    if (cap > 0)
        o.buf[o.len < cap ? o.len : cap - 1] = 0;
    return o.len;
}
