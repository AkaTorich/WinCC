// Resources of a Windows program: .rc scripts (as rc.exe reads them) and ready .res files.
// A .rc goes through the C preprocessor (#include, #define, #if; RC_INVOKED is defined);
// of an included .h or .c only the macros count, as with rc.exe. What it knows:
//   ICON, CURSOR, BITMAP, RCDATA, HTML, FONT, MESSAGETABLE, ANICURSOR, ANIICON, a numbered
//   type (24 = the manifest) or a named one -- from a file or as BEGIN data END;
//   STRINGTABLE, VERSIONINFO, MENU, MENUEX, ACCELERATORS, DIALOG and DIALOGEX; LANGUAGE.
// The result is a list of resources (CcResource) that asm.c writes into .rsrc.
#include "cc.h"

#define RT_CURSOR 1
#define RT_BITMAP 2
#define RT_ICON 3
#define RT_MENU 4
#define RT_DIALOG 5
#define RT_STRING 6
#define RT_FONT 8
#define RT_ACCELERATOR 9
#define RT_RCDATA 10
#define RT_MESSAGETABLE 11
#define RT_GROUP_CURSOR 12
#define RT_GROUP_ICON 14
#define RT_VERSION 16
#define RT_ANICURSOR 21
#define RT_ANIICON 22
#define RT_HTML 23
#define RT_MANIFEST 24

const u8 *(*cc_read_binary)(const char *name, int *size);

// ---- a growing byte buffer ----

typedef struct { u8 *p; int n, cap; } Buf;

static void need(Buf *b, int more)
{
    if (b->n + more <= b->cap)
        return;
    int cap = b->cap ? b->cap * 2 : 256;
    while (cap < b->n + more) cap *= 2;
    u8 *p = cc_alloc((size_t)cap);
    for (int i = 0; i < b->n; i++) p[i] = b->p[i];
    b->p = p;
    b->cap = cap;
}
static void put8(Buf *b, u32 v) { need(b, 1); b->p[b->n++] = (u8)v; }
static void put16(Buf *b, u32 v) { put8(b, v & 0xFF); put8(b, (v >> 8) & 0xFF); }
static void put32(Buf *b, u32 v) { put16(b, v & 0xFFFF); put16(b, v >> 16); }
static void put_bytes(Buf *b, const u8 *p, int n) { need(b, n); for (int i = 0; i < n; i++) b->p[b->n++] = p[i]; }
static void align4(Buf *b) { while (b->n & 3) put8(b, 0); }
static void set16(Buf *b, int at, u32 v) { b->p[at] = (u8)v; b->p[at + 1] = (u8)(v >> 8); }
static void set32(Buf *b, int at, u32 v) { set16(b, at, v & 0xFFFF); set16(b, at + 2, v >> 16); }
static u32 get16(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8; }
static u32 get32(const u8 *p) { return get16(p) | get16(p + 2) << 16; }

// ---- UTF-16 text ----

typedef struct { u16 *s; int n; } WStr;     // n units, no terminating 0 counted

static void wput(u16 **s, int *n, int *cap, u32 cp)
{
    if (*n + 2 >= *cap) {
        int c = *cap ? *cap * 2 : 32;
        u16 *t = cc_alloc(sizeof(u16) * (size_t)c);
        for (int i = 0; i < *n; i++) t[i] = (*s)[i];
        *s = t;
        *cap = c;
    }
    if (cp >= 0x10000) {
        cp -= 0x10000;
        (*s)[(*n)++] = (u16)(0xD800 + (cp >> 10));
        (*s)[(*n)++] = (u16)(0xDC00 + (cp & 0x3FF));
    } else
        (*s)[(*n)++] = (u16)cp;
    (*s)[*n] = 0;
}

static WStr wide_of_utf8(const char *p, int len)
{
    WStr w = { 0 };
    int cap = 0;
    wput(&w.s, &w.n, &cap, 0);
    w.n = 0;
    for (int i = 0; i < len;) {
        u32 c = (u8)p[i], cp = c;
        int k = 0;
        if (c >= 0xF0) { cp = c & 7; k = 3; }
        else if (c >= 0xE0) { cp = c & 15; k = 2; }
        else if (c >= 0xC0) { cp = c & 31; k = 1; }
        i++;
        for (; k > 0 && i < len; k--, i++)
            cp = cp << 6 | ((u8)p[i] & 63);
        wput(&w.s, &w.n, &cap, cp);
    }
    return w;
}

static WStr upper(WStr w)
{
    WStr r = { cc_alloc(sizeof(u16) * (size_t)(w.n + 1)), w.n };
    for (int i = 0; i < w.n; i++)
        r.s[i] = w.s[i] >= 'a' && w.s[i] <= 'z' ? (u16)(w.s[i] - 32) : w.s[i];
    return r;
}

static void put_wstr(Buf *b, WStr w) { for (int i = 0; i < w.n; i++) put16(b, w.s[i]); put16(b, 0); }

// ---- the resources found ----

static CcResource *g_res;
static int g_nres, g_capres;
static u32 g_next_image = 1;        // images of icons and cursors: one count for both, as in rc.exe
static u16 g_lang;

static void add(CcResId type, CcResId name, u16 lang, const u8 *data, int size)
{
    for (int i = 0; i < g_nres; i++) {
        CcResource *r = &g_res[i];
        if (cc_res_id_equal(r->type, type) && cc_res_id_equal(r->name, name) && r->lang == lang) {
            r->data = data;             // a later one with the same type, name and language wins
            r->size = size;
            return;
        }
    }
    if (g_nres == g_capres) {
        int cap = g_capres ? g_capres * 2 : 16;
        CcResource *t = cc_alloc(sizeof(CcResource) * (size_t)cap);
        for (int i = 0; i < g_nres; i++) t[i] = g_res[i];
        g_res = t;
        g_capres = cap;
    }
    CcResource *r = &g_res[g_nres++];
    r->type = type;
    r->name = name;
    r->lang = lang;
    r->data = data;
    r->size = size;
}

static CcResId num_id(u32 n) { CcResId id = { 0 }; id.num = n; return id; }
static CcResId str_id(WStr w) { CcResId id = { 0 }; id.str = w.s; id.len = w.n; return id; }

int cc_res_id_equal(CcResId a, CcResId b)
{
    if (!a.str || !b.str)
        return !a.str && !b.str && a.num == b.num;
    if (a.len != b.len)
        return 0;
    for (int i = 0; i < a.len; i++)
        if (a.str[i] != b.str[i]) return 0;
    return 1;
}

// ---- tokens: what an included .h or .c had is not the script (rc.exe takes only its macros) ----

static const char *g_rc_name;
static Token *g_eof;

static int header_tok(Token *t)
{
    if (!t->file || t->kind == TK_EOF) return 0;
    int n = cc_strlen(t->file);
    if (n < 2 || t->file[n - 2] != '.') return 0;
    char c = t->file[n - 1];
    return c == 'h' || c == 'H' || c == 'c' || c == 'C';
}

static Token *script_only(Token *t)
{
    Token head = { 0 }, *cur = &head;
    for (; t->kind != TK_EOF; t = t->next)
        if (!header_tok(t))
            cur = cur->next = t;
    cur->next = t;
    g_eof = t;
    return head.next;
}

static int word(Token *t, const char *w)
{
    if (t->kind != TK_IDENT && t->kind != TK_KEYWORD) return 0;
    int n = cc_strlen(w);
    if (t->len != n) return 0;
    for (int i = 0; i < n; i++) {
        char a = t->loc[i], b = w[i];
        if (a >= 'a' && a <= 'z') a = (char)(a - 32);
        if (a != b) return 0;
    }
    return 1;
}

static int is_begin(Token *t) { return word(t, "BEGIN") || cc_equal(t, "{"); }
static int is_end(Token *t) { return word(t, "END") || cc_equal(t, "}"); }

static Token *expect_begin(Token *t)
{
    if (!is_begin(t)) cc_error_tok(t, "ожидалось BEGIN или {");
    return t->next;
}

static Token *comma(Token *t) { return cc_equal(t, ",") ? t->next : t; }

// ---- numbers: | & + - * / ~ ( ) and NOT (in a style: these bits off) ----

static i64 expr(Token **rest, Token *t, u32 *off);

static i64 primary(Token **rest, Token *t, u32 *off)
{
    if (cc_equal(t, "(")) {
        i64 v = expr(&t, t->next, off);
        *rest = cc_skip(t, ")");
        return v;
    }
    if (cc_equal(t, "-")) { i64 v = primary(rest, t->next, off); return -v; }
    if (cc_equal(t, "+")) return primary(rest, t->next, off);
    if (cc_equal(t, "~")) { i64 v = primary(rest, t->next, off); return ~v; }
    if (t->kind == TK_NUM || t->kind == TK_CHAR) {
        *rest = t->next;
        return t->val;
    }
    cc_error_tok(t, "ожидалось число");
}

static i64 product(Token **rest, Token *t, u32 *off)
{
    i64 v = primary(&t, t, off);
    for (;;) {
        if (cc_equal(t, "*")) v *= primary(&t, t->next, off);
        else if (cc_equal(t, "/")) { i64 d = primary(&t, t->next, off); if (!d) cc_error_tok(t, "деление на ноль"); v /= d; }
        else break;
    }
    *rest = t;
    return v;
}

static i64 sum(Token **rest, Token *t, u32 *off)
{
    i64 v = product(&t, t, off);
    for (;;) {
        if (cc_equal(t, "+")) v += product(&t, t->next, off);
        else if (cc_equal(t, "-")) v -= product(&t, t->next, off);
        else break;
    }
    *rest = t;
    return v;
}

static i64 expr(Token **rest, Token *t, u32 *off)
{
    i64 v;
    if (word(t, "NOT")) { if (off) *off |= (u32)sum(&t, t->next, off); v = 0; }
    else v = sum(&t, t, off);
    for (;;) {
        if (cc_equal(t, "|")) {
            if (word(t->next, "NOT")) { u32 x = (u32)sum(&t, t->next->next, off); if (off) *off |= x; }
            else v |= sum(&t, t->next, off);
        } else if (cc_equal(t, "&")) v &= sum(&t, t->next, off);
        else break;
    }
    *rest = t;
    return v;
}

static u32 number(Token **rest, Token *t) { return (u32)expr(rest, t, 0); }

static int starts_number(Token *t)
{
    return t->kind == TK_NUM || t->kind == TK_CHAR || cc_equal(t, "(") || cc_equal(t, "-") ||
           cc_equal(t, "~") || cc_equal(t, "+") || word(t, "NOT");
}

// ---- strings ----

// A string token as UTF-16; "a""b" (no space between) is a"b, as in rc.exe.
static WStr string(Token **rest, Token *t)
{
    if (t->kind != TK_STR) cc_error_tok(t, "ожидалась строка в кавычках");
    WStr w = { 0 };
    int cap = 0;
    wput(&w.s, &w.n, &cap, 0);
    w.n = 0;
    for (int first = 1; t->kind == TK_STR && (first || !t->has_space); first = 0, t = t->next) {
        if (!first) wput(&w.s, &w.n, &cap, '"');
        int es = cc_str_elem_size(t->str_kind), n = t->str_len - 1;
        if (es == 1) {
            WStr part = wide_of_utf8(t->str, n);
            for (int i = 0; i < part.n; i++) wput(&w.s, &w.n, &cap, part.s[i]);
        } else
            for (int i = 0; i < n; i++) {
                const u8 *e = (const u8 *)t->str + i * es;
                wput(&w.s, &w.n, &cap, es == 2 ? get16(e) : get32(e));
            }
    }
    *rest = t;
    return w;
}

// A file name: the text between the quotes as written ("res\app.ico" and "res\\app.ico" both work),
// beside the .rc, then in the -I folders.
static const u8 *file_data(Token *t, int *size)
{
    if (t->kind != TK_STR) cc_error_tok(t, "ожидалось имя файла в кавычках");
    const char *p = t->loc;
    while (*p != '"') p++;
    p++;
    int n = 0;
    char *name = cc_alloc((size_t)t->len + 1);
    for (; *p && *p != '"'; p++) {
        if (p[0] == '\\' && p[1] == '\\') p++;
        name[n++] = *p;
    }
    name[n] = 0;
    const u8 *d = 0;
    int abs = n > 1 && (name[1] == ':' || name[0] == '\\' || name[0] == '/');
    if (!abs && g_rc_name) {
        int dl = 0;
        for (int i = 0; g_rc_name[i]; i++)
            if (g_rc_name[i] == '\\' || g_rc_name[i] == '/') dl = i + 1;
        d = cc_read_binary(cc_format("%s%s", cc_strndup(g_rc_name, dl), name), size);
    }
    if (!d) d = cc_read_binary(name, size);
    if (!d) cc_error_tok(t, "нет файла %s", name);
    return d;
}

// A name or a type: a number, or a word / string (kept in capitals, as rc.exe does).
static CcResId res_id(Token **rest, Token *t)
{
    if (starts_number(t)) {
        u32 n = number(rest, t);
        if (n > 0xFFFF) cc_error_tok(t, "номер ресурса больше 65535");
        return num_id(n);
    }
    if (t->kind == TK_STR) {           // rc.exe keeps the quotes in such a name: "NAME" is \"NAME\"
        WStr s = string(rest, t), q = { cc_alloc(sizeof(u16) * (size_t)(s.n + 3)), s.n + 2 };
        q.s[0] = q.s[s.n + 1] = '"';
        for (int i = 0; i < s.n; i++) q.s[i + 1] = s.s[i];
        return str_id(upper(q));
    }
    if (t->kind == TK_IDENT || t->kind == TK_KEYWORD) {
        *rest = t->next;
        return str_id(upper(wide_of_utf8(t->loc, t->len)));
    }
    cc_error_tok(t, "ожидалось имя или номер ресурса");
}

// memory options of old 16-bit Windows: allowed and ignored
static Token *skip_options(Token *t)
{
    static const char *opts[] = { "PRELOAD", "LOADONCALL", "MOVEABLE", "DISCARDABLE", "PURE", "IMPURE",
                                  "FIXED", "SHARED", "NONSHARED", 0 };
    for (;;) {
        int hit = 0;
        for (int i = 0; opts[i]; i++)
            if (word(t, opts[i])) { t = t->next; hit = 1; break; }
        if (!hit) return t;
    }
}

// LANGUAGE, CHARACTERISTICS, VERSION before BEGIN: the language counts for this resource only
static Token *common_statements(Token *t, u16 *lang)
{
    for (;;) {
        if (word(t, "LANGUAGE")) {
            u32 p = number(&t, t->next);
            t = cc_skip(t, ",");
            u32 s = number(&t, t);
            *lang = (u16)(s << 10 | p);
        } else if (word(t, "CHARACTERISTICS") || word(t, "VERSION"))
            number(&t, t->next);
        else
            return t;
    }
}

// ---- icons and cursors ----

static void icon_or_cursor(CcResId name, Token *ft, int cursor)
{
    int size;
    const u8 *f = file_data(ft, &size);
    if (size < 6 || get16(f) != 0 || get16(f + 2) != (cursor ? 2u : 1u))
        cc_error_tok(ft, cursor ? "это не файл курсора .cur" : "это не файл значка .ico");
    int count = (int)get16(f + 4);
    if (size < 6 + 16 * count) cc_error_tok(ft, "файл обрезан");
    Buf g = { 0 };
    put16(&g, 0);
    put16(&g, cursor ? 2 : 1);
    put16(&g, (u32)count);
    for (int i = 0; i < count; i++) {
        const u8 *e = f + 6 + 16 * i;
        u32 bytes = get32(e + 8), off = get32(e + 12);
        if (off + bytes > (u32)size) cc_error_tok(ft, "файл обрезан");
        const u8 *img = f + off;
        u32 planes = get16(e + 4), bits = get16(e + 6);
        int bmp = bytes >= 40 && get32(img) == 40;         // a BITMAPINFOHEADER (not PNG)
        if (bmp && (cursor || !planes || !bits)) { planes = get16(img + 12); bits = get16(img + 14); }
        if (!planes) planes = 1;                            // a PNG image: one plane, as rc.exe writes
        u32 id = g_next_image++;
        if (cursor) {
            Buf c = { 0 };
            put16(&c, get16(e + 4));                        // the hot spot
            put16(&c, get16(e + 6));
            put_bytes(&c, img, (int)bytes);
            add(num_id(RT_CURSOR), num_id(id), g_lang, c.p, c.n);
            u32 w = e[0] ? e[0] : 256, h = e[1] ? e[1] : 256;
            put16(&g, w);
            put16(&g, bmp ? h * 2 : h);
            put16(&g, planes);
            put16(&g, bits);
            put32(&g, bytes + 4);
        } else {
            add(num_id(RT_ICON), num_id(id), g_lang, img, (int)bytes);
            put8(&g, e[0]); put8(&g, e[1]); put8(&g, e[2]); put8(&g, 0);
            put16(&g, planes);
            put16(&g, bits);
            put32(&g, bytes);
        }
        put16(&g, id);
    }
    add(num_id(cursor ? RT_GROUP_CURSOR : RT_GROUP_ICON), name, g_lang, g.p, g.n);
}

// ---- raw data: a file, or BEGIN numbers and strings END ----

static Token *raw_block(Token *t, Buf *b)
{
    t = expect_begin(t);
    while (!is_end(t)) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END");
        if (t->kind == TK_STR) {
            int es = cc_str_elem_size(t->str_kind), n = t->str_len - 1;
            for (int i = 0; i < n * es; i++) put8(b, (u8)t->str[i]);    // as written: no 0 at the end
            t = t->next;
        } else {
            int is_long = t->kind == TK_NUM && t->is_long;
            u32 v = number(&t, t);
            if (is_long) put32(b, v); else put16(b, v);
        }
        t = comma(t);
    }
    return t->next;
}

// ---- STRINGTABLE: kept until the end, then cut into blocks of 16 ----

typedef struct { u32 id; u16 lang; WStr s; } StrEntry;
static StrEntry *g_str;
static int g_nstr, g_capstr;

static Token *stringtable(Token *t)
{
    u16 lang = g_lang;
    t = skip_options(t);
    t = common_statements(t, &lang);
    t = expect_begin(t);
    while (!is_end(t)) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END у STRINGTABLE");
        Token *at = t;
        u32 id = number(&t, t);
        if (id > 0xFFFF) cc_error_tok(at, "номер строки больше 65535");
        t = comma(t);
        WStr s = string(&t, t);
        for (int i = 0; i < g_nstr; i++)
            if (g_str[i].id == id && g_str[i].lang == lang) cc_error_tok(at, "строка %u уже есть", id);
        if (g_nstr == g_capstr) {
            int cap = g_capstr ? g_capstr * 2 : 64;
            StrEntry *n = cc_alloc(sizeof(StrEntry) * (size_t)cap);
            for (int i = 0; i < g_nstr; i++) n[i] = g_str[i];
            g_str = n;
            g_capstr = cap;
        }
        g_str[g_nstr].id = id;
        g_str[g_nstr].lang = lang;
        g_str[g_nstr++].s = s;
    }
    return t->next;
}

static void emit_strings(void)
{
    for (int i = 0; i < g_nstr; i++) {
        u32 block = g_str[i].id / 16;
        u16 lang = g_str[i].lang;
        int seen = 0;
        for (int k = 0; k < i; k++)
            if (g_str[k].id / 16 == block && g_str[k].lang == lang) seen = 1;
        if (seen) continue;
        Buf b = { 0 };
        for (u32 slot = 0; slot < 16; slot++) {
            WStr *s = 0;
            for (int k = 0; k < g_nstr; k++)
                if (g_str[k].id == block * 16 + slot && g_str[k].lang == lang) s = &g_str[k].s;
            put16(&b, s ? (u32)s->n : 0);
            for (int k = 0; s && k < s->n; k++) put16(&b, s->s[k]);
        }
        add(num_id(RT_STRING), num_id(block + 1), lang, b.p, b.n);
    }
}

// ---- VERSIONINFO ----

static void ver_key(Buf *b, int start, WStr key, int type)
{
    put16(b, 0);            // wLength, filled in later
    put16(b, 0);            // wValueLength
    put16(b, (u32)type);
    put_wstr(b, key);
    align4(b);
    (void)start;
}

static Token *ver_block(Token *t, Buf *b);

static Token *ver_value(Token *t, Buf *b)
{
    int start = b->n;
    WStr key = string(&t, t);
    t = comma(t);
    ver_key(b, start, key, 1);
    int text = 0, vlen = 0;
    while (!is_end(t) && !word(t, "VALUE") && !word(t, "BLOCK")) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END");
        if (t->kind == TK_STR) {
            WStr s = string(&t, t);
            for (int i = 0; i < s.n; i++) put16(b, s.s[i]);
            vlen += s.n;
            text = 1;
        } else {
            int is_long = t->kind == TK_NUM && t->is_long;
            u32 v = number(&t, t);
            if (is_long) { put32(b, v); vlen += 4; } else { put16(b, v); vlen += 2; }
        }
        t = comma(t);
    }
    if (text) { put16(b, 0); vlen++; }      // a text value: its length in characters with the 0
    set16(b, start, (u32)(b->n - start));
    set16(b, start + 2, (u32)vlen);
    set16(b, start + 4, text ? 1 : 0);
    align4(b);
    return t;
}

static Token *ver_block(Token *t, Buf *b)
{
    int start = b->n;
    WStr key = string(&t, t);
    ver_key(b, start, key, 1);
    t = expect_begin(t);
    while (!is_end(t)) {
        align4(b);
        if (word(t, "BLOCK")) t = ver_block(t->next, b);
        else if (word(t, "VALUE")) t = ver_value(t->next, b);
        else cc_error_tok(t, "в VERSIONINFO ожидалось BLOCK или VALUE");
    }
    set16(b, start, (u32)(b->n - start));
    align4(b);
    return t->next;
}

static Token *versioninfo(CcResId name, Token *t)
{
    u32 fv[4] = { 0 }, pv[4] = { 0 }, mask = 0x3F, flags = 0, os = 0x40004, type = 1, sub = 0;
    u16 lang = g_lang;
    for (;;) {
        t = skip_options(t);
        if (word(t, "FILEVERSION") || word(t, "PRODUCTVERSION")) {
            u32 *v = word(t, "FILEVERSION") ? fv : pv;
            t = t->next;
            for (int i = 0; i < 4 && starts_number(t); i++) { v[i] = number(&t, t); t = comma(t); }
        } else if (word(t, "FILEFLAGSMASK")) mask = number(&t, t->next);
        else if (word(t, "FILEFLAGS")) flags = number(&t, t->next);
        else if (word(t, "FILEOS")) os = number(&t, t->next);
        else if (word(t, "FILETYPE")) type = number(&t, t->next);
        else if (word(t, "FILESUBTYPE")) sub = number(&t, t->next);
        else if (word(t, "LANGUAGE") || word(t, "CHARACTERISTICS") || word(t, "VERSION")) t = common_statements(t, &lang);
        else break;
    }
    Buf b = { 0 };
    static const char root[] = "VS_VERSION_INFO";
    WStr key = wide_of_utf8(root, (int)sizeof(root) - 1);
    ver_key(&b, 0, key, 0);
    put32(&b, 0xFEEF04BD);
    put32(&b, 0x00010000);
    put32(&b, fv[0] << 16 | fv[1]);
    put32(&b, fv[2] << 16 | fv[3]);
    put32(&b, pv[0] << 16 | pv[1]);
    put32(&b, pv[2] << 16 | pv[3]);
    put32(&b, mask);
    put32(&b, flags);
    put32(&b, os);
    put32(&b, type);
    put32(&b, sub);
    put32(&b, 0);
    put32(&b, 0);
    set16(&b, 2, 52);       // the value is VS_FIXEDFILEINFO
    t = expect_begin(t);
    while (!is_end(t)) {
        align4(&b);
        if (word(t, "BLOCK")) t = ver_block(t->next, &b);
        else if (word(t, "VALUE")) t = ver_value(t->next, &b);
        else cc_error_tok(t, "в VERSIONINFO ожидалось BLOCK или VALUE");
    }
    set16(&b, 0, (u32)b.n);
    add(num_id(RT_VERSION), name, lang, b.p, b.n);
    return t->next;
}

// ---- MENU and MENUEX ----

static u32 menu_flag(Token *t)
{
    static const char *names[] = { "GRAYED", "INACTIVE", "CHECKED", "MENUBARBREAK", "MENUBREAK", "HELP", 0 };
    static const u32 bits[] = { 0x1, 0x2, 0x8, 0x20, 0x40, 0x4000 };
    for (int i = 0; names[i]; i++)
        if (word(t, names[i])) return bits[i];
    return 0;
}

static Token *menu_items(Token *t, Buf *b)
{
    t = expect_begin(t);
    int last = -1;          // where the flags of the item before are: it gets MF_END at the end
    while (!is_end(t)) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END у меню");
        if (word(t, "MENUITEM") && word(t->next, "SEPARATOR")) {
            last = b->n;
            put16(b, 0);
            put16(b, 0);
            put16(b, 0);
            t = t->next->next;
        } else if (word(t, "MENUITEM") || word(t, "POPUP")) {
            int popup = word(t, "POPUP");
            WStr text = string(&t, t->next);
            u32 id = 0, flags = popup ? 0x10 : 0;
            if (!popup) { t = comma(t); id = number(&t, t); }
            for (;;) {
                t = comma(t);
                u32 f = menu_flag(t);
                if (!f) break;
                flags |= f;
                t = t->next;
            }
            last = b->n;
            put16(b, flags);
            if (!popup) put16(b, id);
            put_wstr(b, text);
            if (popup) t = menu_items(t, b);
        } else
            cc_error_tok(t, "в меню ожидалось MENUITEM или POPUP");
    }
    if (last >= 0) set16(b, last, get16(b->p + last) | 0x80);
    return t->next;
}

static Token *menuex_items(Token *t, Buf *b)
{
    t = expect_begin(t);
    int last = -1;
    while (!is_end(t)) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END у меню");
        int popup = word(t, "POPUP");
        if (!popup && !word(t, "MENUITEM")) cc_error_tok(t, "в меню ожидалось MENUITEM или POPUP");
        t = t->next;
        WStr text = { 0 };
        if (t->kind == TK_STR) text = string(&t, t);
        else text = wide_of_utf8("", 0);
        u32 v[4] = { 0 };       // id, type, state, help id
        for (int i = 0; i < 4 && cc_equal(t, ","); i++) {
            t = t->next;
            if (starts_number(t)) v[i] = number(&t, t);
        }
        if (!popup && !text.n && !v[1]) v[1] = 0x800;       // MENUITEM with no text: MFT_SEPARATOR
        put32(b, v[1]);
        put32(b, v[2]);
        put32(b, v[0]);
        last = b->n;
        put16(b, popup ? 1 : 0);
        put_wstr(b, text);
        align4(b);
        if (popup) {
            put32(b, v[3]);
            t = menuex_items(t, b);
        }
    }
    if (last >= 0) set16(b, last, get16(b->p + last) | 0x80);
    return t->next;
}

static Token *menu(CcResId name, Token *t, int ex)
{
    u16 lang = g_lang;
    t = common_statements(skip_options(t), &lang);
    Buf b = { 0 };
    if (ex) {
        put16(&b, 1);
        put16(&b, 4);
        put32(&b, 0);
        t = menuex_items(t, &b);
    } else {
        put16(&b, 0);
        put16(&b, 0);
        t = menu_items(t, &b);
    }
    add(num_id(RT_MENU), name, lang, b.p, b.n);
    return t;
}

// ---- ACCELERATORS ----

static Token *accelerators(CcResId name, Token *t)
{
    u16 lang = g_lang;
    t = common_statements(skip_options(t), &lang);
    t = expect_begin(t);
    Buf b = { 0 };
    while (!is_end(t)) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END у ACCELERATORS");
        Token *ev = t;
        u32 key = 0;
        int ctrl_char = 0, is_char = 0;
        if (t->kind == TK_STR) {
            WStr s = string(&t, t);
            if (s.n == 2 && s.s[0] == '^') { key = s.s[1]; ctrl_char = 1; }
            else if (s.n == 1) { key = s.s[0]; is_char = 1; }
            else cc_error_tok(ev, "клавиша — один знак или ^знак");
        } else
            key = number(&t, t);
        t = comma(t);
        u32 id = number(&t, t);
        u32 flags = 0;
        while (cc_equal(t, ",")) {
            t = t->next;
            if (word(t, "VIRTKEY")) flags |= 1;
            else if (word(t, "ASCII")) flags &= ~1u;
            else if (word(t, "NOINVERT")) flags |= 2;
            else if (word(t, "SHIFT")) flags |= 4;
            else if (word(t, "CONTROL")) flags |= 8;
            else if (word(t, "ALT")) flags |= 0x10;
            else cc_error_tok(t, "неизвестный признак клавиши");
            t = t->next;
        }
        if (ctrl_char) {
            if (key >= 'a' && key <= 'z') key -= 32;
            if (flags & 1) flags |= 8; else key = key & 0x1F;
        } else if (is_char && (flags & 1) && key >= 'a' && key <= 'z')
            key -= 32;
        put16(&b, flags);
        put16(&b, key);
        put16(&b, id);
        put16(&b, 0);
    }
    if (b.n) set16(&b, b.n - 8, get16(b.p + b.n - 8) | 0x80);
    add(num_id(RT_ACCELERATOR), name, lang, b.p, b.n);
    return t->next;
}

// ---- DIALOG (DLGTEMPLATE) and DIALOGEX (DLGTEMPLATEEX) ----

#define WS_POPUP 0x80000000u
#define WS_CHILD 0x40000000u
#define WS_VISIBLE 0x10000000u
#define WS_CAPTION 0x00C00000u
#define WS_BORDER 0x00800000u
#define WS_SYSMENU 0x00080000u
#define WS_GROUP 0x00020000u
#define WS_TABSTOP 0x00010000u
#define DS_SETFONT 0x40u

// a window class: a string or a word; the six classes of Windows are numbers (atoms)
static void put_class(Buf *b, WStr cls)
{
    static const char *names[] = { "BUTTON", "EDIT", "STATIC", "LISTBOX", "SCROLLBAR", "COMBOBOX", 0 };
    WStr u = upper(cls);
    for (int i = 0; names[i]; i++) {
        int n = cc_strlen(names[i]), same = n == u.n;
        for (int k = 0; same && k < n; k++) same = u.s[k] == (u16)names[i][k];
        if (same) { put16(b, 0xFFFF); put16(b, 0x80 + (u32)i); return; }
    }
    put_wstr(b, cls);
}

static WStr word_text(Token **rest, Token *t)
{
    if (t->kind == TK_STR) return string(rest, t);
    if (t->kind != TK_IDENT && t->kind != TK_KEYWORD) cc_error_tok(t, "ожидалось имя");
    *rest = t->next;
    return wide_of_utf8(t->loc, t->len);
}

// rest: style [, exstyle] after the position (or the other way for CONTROL)
static u32 style_value(Token **rest, Token *t, u32 base)
{
    u32 off = 0;
    u32 v = (u32)expr(rest, t, &off);
    return (base | v) & ~off;
}

typedef struct { const char *kw; int cls; u32 style; int text; } CtlKind;     // cls: atom - 0x80

static const CtlKind g_ctl[] = {
    { "LTEXT", 2, 0x0 | WS_GROUP, 1 }, { "RTEXT", 2, 0x2 | WS_GROUP, 1 }, { "CTEXT", 2, 0x1 | WS_GROUP, 1 },
    { "PUSHBUTTON", 0, 0x0 | WS_TABSTOP, 1 }, { "DEFPUSHBUTTON", 0, 0x1 | WS_TABSTOP, 1 },
    { "CHECKBOX", 0, 0x2 | WS_TABSTOP, 1 }, { "AUTOCHECKBOX", 0, 0x3 | WS_TABSTOP, 1 },
    { "RADIOBUTTON", 0, 0x4, 1 }, { "AUTORADIOBUTTON", 0, 0x9, 1 },
    { "STATE3", 0, 0x5 | WS_TABSTOP, 1 }, { "AUTO3STATE", 0, 0x6 | WS_TABSTOP, 1 },
    { "GROUPBOX", 0, 0x7, 1 }, { "PUSHBOX", 0, 0xA | WS_TABSTOP, 1 },
    { "EDITTEXT", 1, WS_BORDER | WS_TABSTOP, 0 }, { "LISTBOX", 3, 0x1 | WS_BORDER, 0 },
    { "COMBOBOX", 5, 0x1 | WS_TABSTOP, 0 }, { "SCROLLBAR", 4, 0x0, 0 },
    { 0, 0, 0, 0 }
};

static Token *dialog_control(Token *t, Buf *b, int dlgex)
{
    u32 style = 0, ex = 0, id = 0, help = 0;
    i64 x, y, cx = 0, cy = 0;
    WStr cls = { 0 };
    int atom = -1;
    Buf text = { 0 };       // the title as sz_Or_Ord

    if (word(t, "CONTROL")) {
        t = t->next;
        if (starts_number(t)) { put16(&text, 0xFFFF); put16(&text, number(&t, t)); }
        else put_wstr(&text, string(&t, t));
        t = comma(t);
        id = number(&t, t);
        t = comma(t);
        cls = word_text(&t, t);
        t = comma(t);
        style = style_value(&t, t, WS_CHILD | WS_VISIBLE);
        t = comma(t);
        x = expr(&t, t, 0); t = comma(t);
        y = expr(&t, t, 0); t = comma(t);
        cx = expr(&t, t, 0); t = comma(t);
        cy = expr(&t, t, 0);
        if (cc_equal(t, ",")) ex = (u32)expr(&t, t->next, 0);
        if (cc_equal(t, ",")) help = (u32)expr(&t, t->next, 0);
    } else if (word(t, "ICON")) {
        t = t->next;
        if (starts_number(t)) { put16(&text, 0xFFFF); put16(&text, number(&t, t)); }
        else put_wstr(&text, word_text(&t, t));
        t = comma(t);
        id = number(&t, t); t = comma(t);
        x = expr(&t, t, 0); t = comma(t);
        y = expr(&t, t, 0);
        style = WS_CHILD | WS_VISIBLE | 0x3;     // SS_ICON
        if (cc_equal(t, ",")) { cx = expr(&t, t->next, 0); t = comma(t); cy = expr(&t, t, 0); }
        if (cc_equal(t, ",")) style = style_value(&t, t->next, style);
        if (cc_equal(t, ",")) ex = (u32)expr(&t, t->next, 0);
        atom = 2;
    } else {
        const CtlKind *k = 0;
        for (int i = 0; g_ctl[i].kw; i++)
            if (word(t, g_ctl[i].kw)) k = &g_ctl[i];
        if (!k) cc_error_tok(t, "неизвестный элемент окна (CONTROL, LTEXT, PUSHBUTTON, EDITTEXT…)");
        t = t->next;
        if (k->text) {
            if (starts_number(t)) { put16(&text, 0xFFFF); put16(&text, number(&t, t)); }
            else put_wstr(&text, string(&t, t));
            t = comma(t);
        } else
            put16(&text, 0);
        id = number(&t, t); t = comma(t);
        x = expr(&t, t, 0); t = comma(t);
        y = expr(&t, t, 0); t = comma(t);
        cx = expr(&t, t, 0); t = comma(t);
        cy = expr(&t, t, 0);
        style = WS_CHILD | WS_VISIBLE | k->style;
        if (cc_equal(t, ",")) style = style_value(&t, t->next, style);
        if (cc_equal(t, ",")) ex = (u32)expr(&t, t->next, 0);
        if (cc_equal(t, ",")) help = (u32)expr(&t, t->next, 0);
        atom = k->cls;
    }

    align4(b);
    if (dlgex) {                // DLGITEMTEMPLATEEX
        put32(b, help);
        put32(b, ex);
        put32(b, style);
    } else {                    // DLGITEMTEMPLATE
        put32(b, style);
        put32(b, ex);
    }
    put16(b, (u32)x);
    put16(b, (u32)y);
    put16(b, (u32)cx);
    put16(b, (u32)cy);
    if (dlgex) put32(b, id); else put16(b, id);
    if (atom >= 0) { put16(b, 0xFFFF); put16(b, 0x80 + (u32)atom); }
    else put_class(b, cls);
    if (!text.n) put16(&text, 0);
    put_bytes(b, text.p, text.n);
    put16(b, 0);            // no creation data
    return t;
}

static Token *dialog(CcResId name, Token *t, int ex)
{
    t = skip_options(t);
    i64 x = expr(&t, t, 0); t = comma(t);
    i64 y = expr(&t, t, 0); t = comma(t);
    i64 cx = expr(&t, t, 0); t = comma(t);
    i64 cy = expr(&t, t, 0);
    u32 help = 0;
    if (ex && cc_equal(t, ",")) help = (u32)expr(&t, t->next, 0);

    u16 lang = g_lang;
    u32 style = WS_POPUP | WS_BORDER | WS_SYSMENU, exstyle = 0;
    int has_caption = 0, has_font = 0;
    WStr caption = { 0 }, face = { 0 };
    u32 point = 0, weight = 0, italic = 0, charset = 1;
    Buf menu_name = { 0 }, cls = { 0 };
    for (;;) {
        if (word(t, "STYLE")) style = style_value(&t, t->next, 0);
        else if (word(t, "EXSTYLE")) exstyle = (u32)expr(&t, t->next, 0);
        else if (word(t, "CAPTION")) { caption = string(&t, t->next); has_caption = 1; }
        else if (word(t, "FONT")) {
            point = number(&t, t->next);
            t = comma(t);
            face = string(&t, t);
            if (cc_equal(t, ",")) { weight = number(&t, t->next); }
            if (cc_equal(t, ",")) { italic = number(&t, t->next); }
            if (cc_equal(t, ",")) { charset = number(&t, t->next); }
            has_font = 1;
        } else if (word(t, "MENU")) {
            CcResId m = res_id(&t, t->next);
            if (m.str) { WStr w = { (u16 *)m.str, m.len }; put_wstr(&menu_name, w); }
            else { put16(&menu_name, 0xFFFF); put16(&menu_name, m.num); }
        } else if (word(t, "CLASS")) {
            if (starts_number(t->next)) { u32 n = number(&t, t->next); put16(&cls, 0xFFFF); put16(&cls, n); }
            else put_class(&cls, word_text(&t, t->next));
        } else if (word(t, "LANGUAGE") || word(t, "CHARACTERISTICS") || word(t, "VERSION"))
            t = common_statements(t, &lang);
        else
            break;
    }
    if (has_caption) style |= WS_CAPTION;
    if (has_font) style |= DS_SETFONT;

    Buf b = { 0 };
    if (ex) {                   // DLGTEMPLATEEX
        put16(&b, 1);
        put16(&b, 0xFFFF);
        put32(&b, help);
        put32(&b, exstyle);
        put32(&b, style);
    } else {                    // DLGTEMPLATE
        put32(&b, style);
        put32(&b, exstyle);
    }
    int count_at = b.n;
    put16(&b, 0);
    put16(&b, (u32)x);
    put16(&b, (u32)y);
    put16(&b, (u32)cx);
    put16(&b, (u32)cy);
    if (menu_name.n) put_bytes(&b, menu_name.p, menu_name.n); else put16(&b, 0);
    if (cls.n) put_bytes(&b, cls.p, cls.n); else put16(&b, 0);
    if (has_caption) put_wstr(&b, caption); else put16(&b, 0);
    if (style & DS_SETFONT) {
        put16(&b, point);
        if (ex) {
            put16(&b, weight);
            put8(&b, italic);
            put8(&b, charset);
        }
        put_wstr(&b, face);
    }
    t = expect_begin(t);
    int count = 0;
    while (!is_end(t)) {
        if (t->kind == TK_EOF) cc_error_tok(t, "нет END у диалога");
        t = dialog_control(t, &b, ex);
        count++;
    }
    set16(&b, count_at, (u32)count);
    add(num_id(RT_DIALOG), name, lang, b.p, b.n);
    return t->next;
}

// ---- the script ----

typedef struct { const char *kw; u32 type; } FileType;
static const FileType g_file_types[] = {
    { "BITMAP", RT_BITMAP }, { "RCDATA", RT_RCDATA }, { "HTML", RT_HTML }, { "FONT", RT_FONT },
    { "MESSAGETABLE", RT_MESSAGETABLE }, { "ANICURSOR", RT_ANICURSOR }, { "ANIICON", RT_ANIICON },
    { "MANIFEST", RT_MANIFEST }, { "RT_MANIFEST", RT_MANIFEST }, { "RT_RCDATA", RT_RCDATA }, { 0, 0 }
};

static void statement_list(Token *t)
{
    while (t->kind != TK_EOF) {
        if (word(t, "LANGUAGE")) {
            u16 lang = 0;
            t = common_statements(t, &lang);
            g_lang = lang;
            continue;
        }
        if (word(t, "STRINGTABLE")) { t = stringtable(t->next); continue; }
        if (cc_equal(t, ";")) { t = t->next; continue; }

        CcResId name = res_id(&t, t);
        Token *kw = t;
        if (kw->kind == TK_EOF) cc_error_tok(kw, "после имени ресурса ожидался его вид (ICON, DIALOGEX…)");
        if (word(kw, "ICON") || word(kw, "CURSOR")) {
            Token *f = skip_options(kw->next);
            icon_or_cursor(name, f, word(kw, "CURSOR"));
            t = f->next;
        } else if (word(kw, "VERSIONINFO")) t = versioninfo(name, kw->next);
        else if (word(kw, "MENU")) t = menu(name, kw->next, 0);
        else if (word(kw, "MENUEX")) t = menu(name, kw->next, 1);
        else if (word(kw, "ACCELERATORS")) t = accelerators(name, kw->next);
        else if (word(kw, "DIALOG")) t = dialog(name, kw->next, 0);
        else if (word(kw, "DIALOGEX")) t = dialog(name, kw->next, 1);
        else {
            // a file type of Windows, a number, or a name of one's own
            CcResId type = { 0 };
            int found = 0;
            for (int i = 0; g_file_types[i].kw; i++)
                if (word(kw, g_file_types[i].kw)) { type = num_id(g_file_types[i].type); found = 1; t = kw->next; break; }
            if (!found) type = res_id(&t, kw);
            u16 lang = g_lang;
            t = common_statements(skip_options(t), &lang);
            if (t->kind == TK_STR) {
                int size;
                const u8 *d = file_data(t, &size);
                if (type.num == RT_BITMAP && !type.str) {          // without its BITMAPFILEHEADER
                    if (size < 14 || d[0] != 'B' || d[1] != 'M') cc_error_tok(t, "это не файл .bmp");
                    d += 14;
                    size -= 14;
                }
                add(type, name, lang, d, size);
                t = t->next;
            } else if (is_begin(t)) {
                Buf b = { 0 };
                t = raw_block(t, &b);
                add(type, name, lang, b.p, b.n);
            } else
                cc_error_tok(t, "ожидалось имя файла в кавычках или BEGIN");
        }
    }
}

void cc_compile_rc(const char *name, const char *text)
{
    g_rc_name = name;
    g_lang = 0;
    Token *t = cc_tokenize(name, cc_join("#define RC_INVOKED 1\n#line 1\n", "", text));
    t = cc_preprocess(t);
    statement_list(script_only(t));
}

// ---- a .res of rc.exe: headers and data, each aligned to 4 ----

static CcResId res_file_id(const u8 **p, const u8 *end)
{
    if (*p + 2 > end) return num_id(0);
    if (get16(*p) == 0xFFFF) {
        CcResId id = num_id(get16(*p + 2));
        *p += 4;
        return id;
    }
    const u8 *s = *p;
    int n = 0;
    while (s + 2 * n + 2 <= end && get16(s + 2 * n)) n++;
    u16 *w = cc_alloc(sizeof(u16) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) w[i] = (u16)get16(s + 2 * i);
    *p += 2 * n + 2;
    CcResId id = { 0 };
    id.str = w;
    id.len = n;
    return id;
}

void cc_read_res(const char *name)
{
    int size;
    const u8 *f = cc_read_binary(name, &size);
    if (!f) cc_error_at(name, 0, "нет файла %s", name);
    const u8 *p = f, *end = f + size;
    while (p + 32 <= end) {
        u32 data_size = get32(p), header_size = get32(p + 4);
        if (header_size < 24 || p + header_size + data_size > end)
            cc_error_at(name, 0, "это не файл ресурсов .res (или он обрезан)");
        const u8 *q = p + 8;
        CcResId type = res_file_id(&q, end);
        CcResId id = res_file_id(&q, end);
        while ((q - f) & 3) q++;
        u16 lang = (u16)get16(q + 6);           // DataVersion, MemoryFlags, LanguageId
        if (!(type.num == 0 && !type.str && data_size == 0)) {       // the first, empty one
            if ((type.num == RT_ICON || type.num == RT_CURSOR) && !type.str && !id.str && id.num >= g_next_image)
                g_next_image = id.num + 1;
            add(type, id, lang, p + header_size, (int)data_size);
        }
        p += header_size + data_size;
        while ((p - f) & 3 && p < end) p++;
    }
}

int cc_resources(CcResource **out)
{
    emit_strings();
    g_nstr = 0;
    *out = g_res;
    return g_nres;
}

void cc_resources_reset(void)
{
    g_res = 0;
    g_nres = g_capres = 0;
    g_next_image = 1;
    g_str = 0;
    g_nstr = g_capstr = 0;
    g_lang = 0;
}
