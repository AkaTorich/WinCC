// Tokenizer and small string helpers.
#include "cc.h"

const char *cc_file_name;

int cc_strlen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

int cc_streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

int cc_strneq(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i])
            return 0;
        if (!a[i])
            return 1;
    }
    return 1;
}

char *cc_strndup(const char *s, int n)
{
    char *p = cc_alloc((size_t)n + 1);
    for (int i = 0; i < n; i++)
        p[i] = s[i];
    return p;
}

int cc_vformat(char *buf, int cap, const char *fmt, va_list ap);

char *cc_format(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    cc_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return cc_strndup(buf, cc_strlen(buf));
}

// a + sep + b, of any length (cc_format holds 1 KB)
char *cc_join(const char *a, const char *sep, const char *b)
{
    int na = cc_strlen(a), ns = cc_strlen(sep), nb = cc_strlen(b);
    char *r = cc_alloc((size_t)na + (size_t)ns + (size_t)nb + 1);
    for (int i = 0; i < na; i++) r[i] = a[i];
    for (int i = 0; i < ns; i++) r[na + i] = sep[i];
    for (int i = 0; i < nb; i++) r[na + ns + i] = b[i];
    r[na + ns + nb] = 0;
    return r;
}

int cc_equal(Token *tok, const char *op)
{
    int n = cc_strlen(op);
    return tok->len == n && cc_strneq(tok->loc, op, n);
}

Token *cc_skip(Token *tok, const char *op)
{
    if (!cc_equal(tok, op))
        cc_error_tok(tok, "ожидалось '%s'", op);
    return tok->next;
}

int cc_consume(Token **rest, Token *tok, const char *op)
{
    if (cc_equal(tok, op)) {
        *rest = tok->next;
        return 1;
    }
    *rest = tok;
    return 0;
}

static Token *new_token(TokenKind kind, const char *start, const char *end)
{
    Token *t = cc_alloc(sizeof(Token));
    t->kind = kind;
    t->loc = start;
    t->len = (int)(end - start);
    return t;
}

static int is_ident1(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80; }
static int is_ident2(int c) { return is_ident1(c) || (c >= '0' && c <= '9'); }
static int is_digit(int c)  { return c >= '0' && c <= '9'; }
static int is_space(int c)  { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

static int from_hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

static int is_hex(int c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static const char *g_cur_file;
static int g_cur_line;

static int read_punct(const char *p)
{
    static const char *kw[] = {
        "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=",
        "++", "--", "%=", "&=", "|=", "^=", "&&", "||", "<<", ">>", "##",
    };
    for (int i = 0; i < (int)(sizeof(kw) / sizeof(kw[0])); i++) {
        int n = cc_strlen(kw[i]);
        if (cc_strneq(p, kw[i], n))
            return n;
    }
    return (*p >= '!' && *p <= '~' && !is_ident2((unsigned char)*p)) ? 1 : 0;
}

static int read_escaped(const char **new_pos, const char *p)
{
    if (*p >= '0' && *p <= '7') {
        int c = *p++ - '0';
        if (*p >= '0' && *p <= '7') {
            c = (c << 3) + (*p++ - '0');
            if (*p >= '0' && *p <= '7')
                c = (c << 3) + (*p++ - '0');
        }
        *new_pos = p;
        return c;
    }
    if (*p == 'x') {
        p++;
        if (!is_hex(*p))
            cc_error_at(g_cur_file, g_cur_line, "неверная шестнадцатеричная последовательность");
        int c = 0;
        for (; is_hex(*p); p++)
            c = (c << 4) + from_hex(*p);
        *new_pos = p;
        return c;
    }
    *new_pos = p + 1;
    switch (*p) {
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';
    case 'e': return 27;
    case '\\': case '\'': case '"': case '?':
        return *p;
    default: {
        // "Папка\письмо": as in MSVC the backslash goes and the letter stays -- said aloud,
        // a Windows path in a string is the usual cause
        int n = (u8)*p >= 0xF0 ? 4 : (u8)*p >= 0xE0 ? 3 : (u8)*p >= 0xC0 ? 2 : 1;
        cc_warning_at(g_cur_file, g_cur_line, "неизвестная последовательность \\%s в строке: обратная косая пропущена (в пути пишут \\\\)",
                      cc_strndup(p, n));
        return *p;
    }
    }
}

static const char *string_end(const char *p)
{
    const char *start = p;
    for (; *p != '"'; p++) {
        if (*p == '\n' || *p == 0)
            cc_error_at(g_cur_file, g_cur_line, "строка не закрыта");
        if (*p == '\\')
            p++;
    }
    (void)start;
    return p;
}

static u32 decode_utf8(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0) { *p += 2; return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((s[0] & 0xF0) == 0xE0) { *p += 3; return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    *p += 4;
    return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

// \uXXXX or \UXXXXXXXX (p at the 'u' / 'U'): a code point
static u32 read_ucn(const char **new_pos, const char *p)
{
    int digits = *p == 'u' ? 4 : 8;
    u32 c = 0;
    p++;
    for (int i = 0; i < digits; i++, p++) {
        if (!is_hex(*p))
            cc_error_at(g_cur_file, g_cur_line, "после \\%c нужно %d шестнадцатеричных цифр", digits == 4 ? 'u' : 'U', digits);
        c = (c << 4) + (u32)from_hex(*p);
    }
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
        cc_error_at(g_cur_file, g_cur_line, "\\u%04X: такого символа в Юникоде нет", c);
    *new_pos = p;
    return c;
}

int cc_str_elem_size(int kind)
{
    if (kind == STR_WIDE)
        return cc_target_win ? 2 : 4;                   // wchar_t: UTF-16 in Windows
    return kind == STR_U16 ? 2 : kind == STR_U32 ? 4 : 1;
}

// one element of a literal, stored little-endian
static void put_unit(char *buf, int *n, int size, u32 v)
{
    for (int i = 0; i < size; i++)
        buf[*n * size + i] = (char)(v >> (8 * i));
    (*n)++;
}

// a whole character: UTF-8 bytes in narrow literals, a surrogate pair in u""
static void put_code_point(char *buf, int *n, int kind, u32 c)
{
    int size = cc_str_elem_size(kind);
    if (size == 4) {
        put_unit(buf, n, 4, c);
    } else if (size == 2) {
        if (c >= 0x10000) {
            c -= 0x10000;
            put_unit(buf, n, 2, 0xD800 + (c >> 10));
            put_unit(buf, n, 2, 0xDC00 + (c & 0x3FF));
        } else {
            put_unit(buf, n, 2, c);
        }
    } else if (c < 0x80) {
        put_unit(buf, n, 1, c);
    } else if (c < 0x800) {
        put_unit(buf, n, 1, 0xC0 | c >> 6);
        put_unit(buf, n, 1, 0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        put_unit(buf, n, 1, 0xE0 | c >> 12);
        put_unit(buf, n, 1, 0x80 | (c >> 6 & 0x3F));
        put_unit(buf, n, 1, 0x80 | (c & 0x3F));
    } else {
        put_unit(buf, n, 1, 0xF0 | c >> 18);
        put_unit(buf, n, 1, 0x80 | (c >> 12 & 0x3F));
        put_unit(buf, n, 1, 0x80 | (c >> 6 & 0x3F));
        put_unit(buf, n, 1, 0x80 | (c & 0x3F));
    }
}

// The literal after `quote` as elements of its kind. Narrow literals keep the
// source's UTF-8 bytes; wide ones get code points (u"" as UTF-16). \x and
// octal escapes give one element as written, \u and \U a whole character.
static void decode_string(Token *t, const char *quote, int kind)
{
    const char *end = string_end(quote + 1);
    int size = cc_str_elem_size(kind);
    // never more elements than source bytes, plus the NUL
    char *buf = cc_alloc((size_t)(end - quote + 1) * (size_t)size);
    int n = 0;
    for (const char *p = quote + 1; p < end;) {
        if (p[0] == '\\' && (p[1] == 'u' || p[1] == 'U'))
            put_code_point(buf, &n, kind, read_ucn(&p, p + 1));
        else if (*p == '\\')
            put_unit(buf, &n, size, (u32)read_escaped(&p, p + 1));
        else if (size > 1 && (unsigned char)*p >= 0x80)
            put_code_point(buf, &n, kind, decode_utf8(&p));
        else
            put_unit(buf, &n, size, (unsigned char)*p++);
    }
    put_unit(buf, &n, size, 0);
    t->str = buf;
    t->str_len = n;
    t->str_kind = kind;
}

static Token *read_string(const char *start, const char *quote, int kind)
{
    Token *t = new_token(TK_STR, start, string_end(quote + 1) + 1);
    decode_string(t, quote, kind);
    return t;
}

void cc_widen_string(Token *t, int kind)
{
    const char *q = t->loc;
    while (*q != '"')
        q++;
    g_cur_file = t->file;
    g_cur_line = t->line;
    decode_string(t, q, kind);
}

// 'x', L'x', u'x', U'x', u8'x': a non-ASCII character or \u gives its code point
static Token *read_char(const char *start, const char *quote, int kind)
{
    const char *p = quote + 1;
    if (*p == 0)
        cc_error_at(g_cur_file, g_cur_line, "символ не закрыт");
    i64 c;
    int raw = 0;
    if (p[0] == '\\' && (p[1] == 'u' || p[1] == 'U'))
        c = read_ucn(&p, p + 1);
    else if (*p == '\\')
        c = read_escaped(&p, p + 1), raw = 1;
    else if ((unsigned char)*p >= 0x80)
        c = decode_utf8(&p);    // 'ж' gives its code point
    else
        c = (unsigned char)*p++;
    if (*p != '\'' && kind == STR_PLAIN && c < 0x100) {
        // 'AB', 'RIFF': several characters, an int of their bytes, the first highest (as MSVC and GCC)
        i64 v = c & 0xFF;
        int n = 1;
        while (*p != '\'' && *p && *p != '\n') {
            i64 d = *p == '\\' ? read_escaped(&p, p + 1) : (unsigned char)*p++;
            if (++n > 4)
                cc_error_at(g_cur_file, g_cur_line, "в символьной константе больше 4 знаков");
            v = (v << 8) | (d & 0xFF);
        }
        if (*p != '\'')
            cc_error_at(g_cur_file, g_cur_line, "символ не закрыт");
        Token *t = new_token(TK_NUM, start, p + 1);
        t->val = (int)(u32)v;
        return t;
    }
    if (*p != '\'')
        cc_error_at(g_cur_file, g_cur_line, "символ не закрыт");
    if (kind == STR_PLAIN && raw)
        c = (char)c;            // '\xFF' is a char: -1
    else if (kind == STR_U8 && c > 0x7F)
        cc_error_at(g_cur_file, g_cur_line, "в u8'' помещается только символ ASCII");
    else if (kind == STR_U16 && c > 0xFFFF)
        cc_error_at(g_cur_file, g_cur_line, "символ не помещается в char16_t: возьмите U''");
    else if (kind == STR_U16 && raw)
        c &= 0xFFFF;
    else if (kind == STR_WIDE && cc_target_win && raw)
        c &= 0xFFFF;
    Token *t = new_token(TK_NUM, start, p + 1);
    t->too_wide = kind == STR_WIDE && cc_target_win && c > 0xFFFF;     // the parser says so, if it is used
    t->val = c;
    t->is_unsigned = kind == STR_U32;   // char32_t is unsigned int; L'x' is wchar_t (int)
    return t;
}

// the kind of a prefixed literal at p (L" u" U" u8" and the same with '), and
// its length; 0 if p does not start one
static int literal_prefix(const char *p, int *kind)
{
    if (p[0] == 'u' && p[1] == '8' && (p[2] == '"' || p[2] == '\'')) { *kind = STR_U8; return 2; }
    if (p[1] != '"' && p[1] != '\'') return 0;
    if (p[0] == 'L') { *kind = STR_WIDE; return 1; }
    if (p[0] == 'u') { *kind = STR_U16; return 1; }
    if (p[0] == 'U') { *kind = STR_U32; return 1; }
    return 0;
}

double rt_strtod(const char *s, char **end);   // runtime.c: correctly rounded (big integers)

static Token *read_float(const char *start)
{
    // the literal's end is found here; its value comes from the library's
    // strtod, which rounds correctly every time -- "0.1" in a program is the
    // same double as in every other C compiler, long and huge literals too
    // 1'000.5 (C23): the separators go before strtod sees the digits
    char clean[400];
    int cn = 0;
    const char *p = start;
    while (is_digit((unsigned char)*p) || (*p == '\'' && is_digit((unsigned char)p[1]))) {
        if (*p != '\'' && cn < 390) clean[cn++] = *p;
        p++;
    }
    if (*p == '.') {
        if (cn < 390) clean[cn++] = *p;
        p++;
        while (is_digit((unsigned char)*p) || (*p == '\'' && is_digit((unsigned char)p[1]))) {
            if (*p != '\'' && cn < 390) clean[cn++] = *p;
            p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        if (cn < 390) clean[cn++] = *p;
        p++;
        if (*p == '+' || *p == '-') {
            if (cn < 390) clean[cn++] = *p;
            p++;
        }
        if (!is_digit((unsigned char)*p))
            cc_error_at(g_cur_file, g_cur_line, "неверная степень в числе");
        while (is_digit((unsigned char)*p)) {
            if (cn < 390) clean[cn++] = *p;
            p++;
        }
    }
    clean[cn] = 0;
    double v = rt_strtod(clean, 0);
    Type *ty = ty_double;
    if (*p == 'f' || *p == 'F') {
        ty = ty_float;
        p++;
    } else if (*p == 'l' || *p == 'L') {
        p++;
    }
    if (is_ident2((unsigned char)*p))
        cc_error_at(g_cur_file, g_cur_line, "неверное число");
    Token *t = new_token(TK_NUM, start, p);
    t->fval = v;
    t->num_ty = ty;
    return t;
}

// 0x1.8p3, 0xAp-2: hex digits (a point among them, maybe), then a power of two -- exact
static Token *read_hex_float(const char *start)
{
    const char *p = start + 2;
    u64 m = 0;
    int shift = 0, digits = 0, seen_point = 0;
    for (;; p++) {
        if (*p == '\'' && is_hex(p[1])) continue;
        if (*p == '.' && !seen_point) { seen_point = 1; continue; }
        if (!is_hex(*p)) break;
        if (digits < 15) {                  // 60 bits are more than a double keeps
            m = m * 16 + (u64)from_hex(*p);
            if (seen_point) shift -= 4;
            if (m) digits++;
        } else if (!seen_point) {
            shift += 4;
        }
    }
    if (*p != 'p' && *p != 'P')
        cc_error_at(g_cur_file, g_cur_line, "у шестнадцатеричной дроби нужна степень двойки: p");
    p++;
    int neg = 0, e = 0;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    if (!is_digit(*p))
        cc_error_at(g_cur_file, g_cur_line, "неверная степень в числе");
    while (is_digit(*p)) e = e * 10 + (*p++ - '0');
    e = (neg ? -e : e) + shift;
    double v = (double)m;                   // m is exact (< 2^53 for 13 digits; rounded once above)
    for (; e > 0; e--) v *= 2.0;
    for (; e < 0; e++) v *= 0.5;
    Type *ty = ty_double;
    if (*p == 'f' || *p == 'F') { ty = ty_float; p++; }
    else if (*p == 'l' || *p == 'L') p++;
    if (is_ident2((unsigned char)*p))
        cc_error_at(g_cur_file, g_cur_line, "неверное число");
    Token *t = new_token(TK_NUM, start, p);
    t->fval = v;
    t->num_ty = ty;
    return t;
}

static Token *read_number(const char *start)
{
    const char *p = start;
    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && (is_hex(p[2]) || (p[2] == '.' && is_hex(p[3])))) {
        p += 2;
        base = 16;
    } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B') && (p[2] == '0' || p[2] == '1')) {
        p += 2;
        base = 2;
    } else if (p[0] == '0') {
        base = 8;
    }
    u64 v = 0;
    for (;; p++) {
        int d;
        if (*p == '\'' && (base == 16 ? is_hex(p[1]) : is_digit(p[1])))
            continue;                       // 1'000'000, 0xFF'FF (C23)
        if (is_digit(*p)) d = *p - '0';
        else if (base == 16 && is_hex(*p)) d = from_hex(*p);
        else break;
        if (d >= base)
            cc_error_at(g_cur_file, g_cur_line, "неверная цифра в числе");
        v = v * (u64)base + (u64)d;
    }
    if (base == 16 && (*p == '.' || *p == 'p' || *p == 'P'))
        return read_hex_float(start);
    int is_u = 0, is_l = 0;
    for (;;) {
        if (*p == 'u' || *p == 'U') { is_u = 1; p++; }
        else if (*p == 'l' || *p == 'L') { is_l = 1; p++; }
        else if ((*p == 'i' || *p == 'I') && p[1] == '6' && p[2] == '4') { is_l = 1; p += 3; }     // MSVC: 5i64, 5ui64
        else if ((*p == 'i' || *p == 'I') && p[1] == '3' && p[2] == '2') p += 3;
        else if ((*p == 'i' || *p == 'I') && p[1] == '1' && p[2] == '6') p += 3;
        else if ((*p == 'i' || *p == 'I') && p[1] == '8') p += 2;
        else break;
    }
    if (base != 16 && (*p == '.' || *p == 'e' || *p == 'E' || *p == 'f' || *p == 'F') && !(is_u || is_l))
        return read_float(start);
    if (is_ident2((unsigned char)*p))
        cc_error_at(g_cur_file, g_cur_line, "неверное число");

    // C rules: pick the first type the value fits
    if (!is_l && !is_u) {
        if (v >> 31) {
            if (base != 10 && !(v >> 32)) is_u = 1;
            else is_l = 1;
            if (v >> 63) is_u = 1;
        }
    } else if (!is_l && is_u) {
        if (v >> 32) is_l = 1;
    } else if (is_l && !is_u) {
        if (v >> 63) is_u = 1;
    }

    Token *t = new_token(TK_NUM, start, p);
    t->val = (i64)v;
    t->is_unsigned = is_u;
    t->is_long = is_l;
    return t;
}

void cc_convert_keywords(Token *tok)
{
    static const char *kw[] = {
        "return", "if", "else", "for", "while", "int", "sizeof", "char", "struct", "union",
        "short", "long", "void", "typedef", "_Bool", "enum", "static", "goto", "break", "continue",
        "switch", "case", "default", "extern", "_Alignof", "_Alignas", "do", "signed", "unsigned",
        "const", "volatile", "auto", "register", "restrict", "__restrict", "inline", "_Noreturn",
        "asm", "__asm__", "float", "double",
    };
    for (Token *t = tok; t->kind != TK_EOF; t = t->next)
        if (t->kind == TK_IDENT)
            for (int i = 0; i < (int)(sizeof(kw) / sizeof(kw[0])); i++)
                if (cc_equal(t, kw[i])) {
                    t->kind = TK_KEYWORD;
                    break;
                }
}

Token *cc_tokenize(const char *file, const char *src)
{
    g_cur_file = file;
    g_cur_line = 1;
    Token head = { 0 };
    Token *cur = &head;
    const char *p = src;
    int at_bol = 1, has_space = 0;

    // skip UTF-8 BOM
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;

    while (*p) {
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;
            has_space = 1;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n')
                    g_cur_line++;
                p++;
            }
            if (!*p)
                cc_error_at(file, g_cur_line, "комментарий не закрыт");
            p += 2;
            has_space = 1;
            continue;
        }
        if (p[0] == '\\' && p[1] == '\n') {     // line continuation
            p += 2;
            g_cur_line++;
            continue;
        }
        if (p[0] == '\\' && p[1] == '\r' && p[2] == '\n') {
            p += 3;
            g_cur_line++;
            continue;
        }
        if (*p == '\n') {
            p++;
            g_cur_line++;
            at_bol = 1;
            has_space = 0;
            continue;
        }
        if (is_space((unsigned char)*p)) {
            p++;
            has_space = 1;
            continue;
        }

        Token *t;
        int kind = 0, plen = literal_prefix(p, &kind);
        if (is_digit((unsigned char)*p) || (*p == '.' && is_digit((unsigned char)p[1]))) {
            t = read_number(p);
        } else if (*p == '"') {
            t = read_string(p, p, STR_PLAIN);
        } else if (*p == '\'') {
            t = read_char(p, p, STR_PLAIN);
        } else if (plen && p[plen] == '"') {
            t = read_string(p, p + plen, kind);
        } else if (plen) {
            t = read_char(p, p + plen, kind);
        } else if (is_ident1((unsigned char)*p)) {
            const char *start = p;
            do {
                p++;
            } while (is_ident2((unsigned char)*p));
            t = new_token(TK_IDENT, start, p);
        } else {
            int n = read_punct(p);
            if (!n)
                cc_error_at(file, g_cur_line, "непонятный знак '%c'", (u32)(unsigned char)*p);
            t = new_token(TK_PUNCT, p, p + n);
        }
        p = t->loc + t->len;
        t->file = file;
        t->line = g_cur_line;
        t->at_bol = at_bol;
        t->has_space = has_space;
        at_bol = has_space = 0;
        cur = cur->next = t;
    }
    Token *eof = new_token(TK_EOF, p, p);
    eof->file = file;
    eof->line = g_cur_line;
    eof->at_bol = 1;
    cur->next = eof;
    return head.next;
}
