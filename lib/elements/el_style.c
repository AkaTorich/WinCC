// Оформление строкой (как CSS): el_style для одного элемента и таблица стилей
// el_stylesheet для тегов/классов/имён. Свои настройки элемента сильнее таблицы.
#include "el_internal.h"
#include <stdio.h>

#define MAXTK 8

// обрезать пробелы по краям на месте; вернуть начало
static char *trim(char *s) {
    char *e;
    while (*s && (unsigned char)*s <= ' ') s++;
    e = s + strlen(s);
    while (e > s && (unsigned char)e[-1] <= ' ') e--;
    *e = 0;
    return s;
}

// разбить строку (портится) на слова через пробел/таб
static int tokens(char *s, char **tk) {
    int n = 0;
    while (*s && n < MAXTK) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        tk[n++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
        if (*s) *s++ = 0;
    }
    return n;
}

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_color(const char *s, u32 *out) {
    size_t n, i;
    u32 v = 0;
    if (*s == '#') s++;
    n = strlen(s);
    for (i = 0; i < n; i++) if (hexv(s[i]) < 0) return 0;
    if (n == 3) {
        int r = hexv(s[0]), gg = hexv(s[1]), b = hexv(s[2]);
        *out = (u32)(((r * 17) << 16) | ((gg * 17) << 8) | (b * 17));
        return 1;
    }
    if (n == 6) { for (i = 0; i < 6; i++) v = (v << 4) | (u32)hexv(s[i]); *out = v; return 1; }
    return 0;
}

static void four(char **tk, int n, int *t, int *r, int *b, int *l) {
    if (n == 1) { int v = atoi(tk[0]); *l = *t = *r = *b = v; }
    else if (n == 2) { int v = atoi(tk[0]), hh = atoi(tk[1]); *t = *b = v; *l = *r = hh; }
    else if (n == 3) { *t = atoi(tk[0]); *l = *r = atoi(tk[1]); *b = atoi(tk[2]); }
    else if (n >= 4) { *t = atoi(tk[0]); *r = atoi(tk[1]); *b = atoi(tk[2]); *l = atoi(tk[3]); }
}

static void apply_decl(Element *e, const char *key, char *val) {
    char *tk[MAXTK], *vcopy;
    int n;
    u32 c;
    if (!strcmp(key, "font-family")) {
        // имя целиком (может быть из нескольких слов, в кавычках или без)
        size_t L = strlen(val);
        if (L >= 2 && (val[0] == '"' || val[0] == '\'') && val[L - 1] == val[0]) { val[L - 1] = 0; val++; }
        e->mono = *val ? font_face_by_name(val) : 0;
        return;
    }
    vcopy = sdup(val);
    n = vcopy ? tokens(vcopy, tk) : 0;
    if (!strcmp(key, "background")) { if (n && parse_color(tk[0], &c)) { e->bg = c; e->has_bg = 1; e->grad = 0; } }
    else if (!strcmp(key, "color")) { if (n && parse_color(tk[0], &c)) { e->fg = c; e->has_fg = 1; } }
    else if (!strcmp(key, "border")) { if (n && parse_color(tk[0], &c)) { e->bd = c; e->has_bd = 1; if (e->bw < 0) e->bw = 1; } }
    else if (!strcmp(key, "border-width")) { if (n) e->bw = atoi(tk[0]); }
    else if (!strcmp(key, "radius")) e->radius = n ? atoi(tk[0]) : 0;
    else if (!strcmp(key, "width")) e->w = n ? atoi(tk[0]) : -1;
    else if (!strcmp(key, "height")) e->h = n ? atoi(tk[0]) : -1;
    else if (!strcmp(key, "padding")) four(tk, n, &e->pt, &e->pr, &e->pb, &e->pl);
    else if (!strcmp(key, "margin")) four(tk, n, &e->mt, &e->mr, &e->mb, &e->ml);
    else if (!strcmp(key, "gradient") && n >= 2) {
        u32 a, b;
        if (parse_color(tk[0], &a) && parse_color(tk[1], &b)) {
            e->bg = a; e->bg2 = b; e->has_bg = 1; e->grad = 1;
            if (n >= 3) { if (!strcmp(tk[2], "horizontal")) e->grad = 2; else if (!strcmp(tk[2], "diagonal")) e->grad = 3; else e->grad = 1; }
        }
    }
    else if (!strcmp(key, "background-alpha")) e->bg_a = n ? atoi(tk[0]) : 0;
    else if (!strcmp(key, "shadow")) {
        if (n && !strcmp(tk[0], "none")) e->sh_a = 0;
        else if (n >= 1 && parse_color(tk[0], &c)) {
            e->sh_col = c;
            e->sh_blur = n > 1 ? atoi(tk[1]) : 12;
            e->sh_dx = n > 2 ? atoi(tk[2]) : 0;
            e->sh_dy = n > 3 ? atoi(tk[3]) : 4;
            e->sh_a = n > 4 ? atoi(tk[4]) : 120;
        }
    }
    else if (!strcmp(key, "hover-background")) { if (n && parse_color(tk[0], &c)) { e->hbg = c; e->has_hbg = 1; } }
    else if (!strcmp(key, "press-background")) { if (n && parse_color(tk[0], &c)) { e->pbg = c; e->has_pbg = 1; } }
    else if (!strcmp(key, "hover-color")) { if (n && parse_color(tk[0], &c)) { e->hfg = c; e->has_hfg = 1; } }
    else if (!strcmp(key, "hover-border")) { if (n && parse_color(tk[0], &c)) { e->hbd = c; e->has_hbd = 1; } }
    else if (!strcmp(key, "font-size")) e->fsize = n ? atoi(tk[0]) : 0;
    else if (!strcmp(key, "font-weight")) e->bold = (n && (!strcmp(tk[0], "bold") || atoi(tk[0]) >= 600)) ? 1 : 0;
    else if (!strcmp(key, "text-align")) e->talign = n ? (!strcmp(tk[0], "center") ? 1 : (!strcmp(tk[0], "right") ? 2 : 0)) : 0;
    free(vcopy);
}

void apply_style_string(int h, const char *css) {
    char *s, *p;
    if (!valid(h) || !css) return;
    s = sdup(css);
    p = s;
    while (p && *p) {
        char *semi = strchr(p, ';'), *col;
        char *next = semi ? semi + 1 : NULL;
        if (semi) *semi = 0;
        col = strchr(p, ':');
        if (col) {
            *col = 0;
            apply_decl(E(h), trim(p), trim(col + 1));
        }
        p = next;
    }
    free(s);
    request_redraw();
}

void el_style(el e, const char *css) {
    if (!valid(e) || !css) return;
    sset(&E(e)->inl, css);
    apply_style_string(e, css);
}

// ----------------------------------------------------------- таблица -------

void el_stylesheet(const char *css) {
    char *s, *p;
    if (!css) return;
    s = sdup(css);
    p = s;
    while (p && *p) {
        char *open = strchr(p, '{'), *close, *sel, *body, *q;
        if (!open) break;
        close = strchr(open, '}');
        if (!close) break;
        *open = 0; *close = 0;
        sel = trim(p);
        body = trim(open + 1);
        p = close + 1;
        // несколько селекторов через запятую
        q = sel;
        while (q && *q) {
            char *comma = strchr(q, ','), *one, *st;
            int state = 0;
            Rule r;
            if (comma) *comma = 0;
            one = trim(q);
            q = comma ? comma + 1 : NULL;
            if (!*one) continue;
            if ((st = strstr(one, ":hover")) != NULL) { state = 1; *st = 0; }
            else if ((st = strstr(one, ":pressed")) != NULL) { state = 2; *st = 0; }
            r.sel = sdup(trim(one));
            r.state = state;
            r.css = sdup(body);
            VPUSH(g.rules, r);
        }
    }
    free(s);
    g.rules_gen++;
    restyle_all();
}

int el_stylesheet_file(const char *path) {
    WCHAR *wp;
    FILE *f;
    long n;
    char *buf;
    if (!path) return 0;
    wp = wdup(path);
    f = _wfopen(wp, L"rb");
    free(wp);
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return 0; }
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    n = (long)fread(buf, 1, (size_t)n, f);
    buf[n] = 0;
    fclose(f);
    el_stylesheet(buf);
    free(buf);
    return 1;
}

static const char *kind_tag(int k) {
    switch (k) {
    case K_WINDOW: return "window"; case K_STACK: return "stack"; case K_DOCK: return "dock"; case K_WRAP: return "wrap";
    case K_GRID: return "grid"; case K_SCROLL: return "scroll"; case K_CANVAS: return "canvas"; case K_BORDER: return "border";
    case K_GROUP: return "group"; case K_TABS: return "tabs"; case K_TAB: return "tab"; case K_TEXT: return "text";
    case K_BUTTON: return "button"; case K_CHECK: return "check"; case K_RADIO: return "radio"; case K_EDIT: return "edit";
    case K_MEMO: return "memo"; case K_PASSWORD: return "password"; case K_COMBO: return "combo"; case K_LIST: return "list";
    case K_SLIDER: return "slider"; case K_PROGRESS: return "progress"; case K_IMAGE: return "image"; case K_NUMBER: return "number";
    case K_TOOLBAR: return "toolbar"; case K_TOOL: return "tool"; case K_PAINT: return "paint"; case K_DIALOG: return "dialog";
    default: return "";
    }
}

// есть ли слово cls (длины n) в списке классов через пробел
static int has_class(const Element *e, const char *cls, size_t n) {
    const char *s = e->cls;
    if (!s || !n) return 0;
    while (*s) {
        const char *w;
        while (*s == ' ') s++;
        w = s;
        while (*s && *s != ' ') s++;
        if ((size_t)(s - w) == n && !strncmp(w, cls, n)) return 1;
    }
    return 0;
}

static int sel_match(const Element *e, const char *sel) {
    // tag  .class  #name  tag.class
    const char *tag = NULL, *cls = NULL, *id = NULL;
    size_t tn = 0, cn = 0, in = 0;
    const char *p = sel;
    while (*p) {
        const char *start;
        char kind = 0;
        if (*p == '.' || *p == '#') kind = *p++;
        start = p;
        while (*p && *p != '.' && *p != '#') p++;
        if (kind == '.') { cls = start; cn = (size_t)(p - start); }
        else if (kind == '#') { id = start; in = (size_t)(p - start); }
        else { tag = start; tn = (size_t)(p - start); }
    }
    if (tag && tn && !(tn == 1 && *tag == '*')) {
        const char *kt = kind_tag(e->kind);
        if (strlen(kt) != tn || strncmp(kt, tag, tn)) return 0;
    }
    if (cls && cn && !has_class(e, cls, cn)) return 0;
    if (id && in) {
        const char *nm = S(e->name);
        if (strlen(nm) != in || strncmp(nm, id, in)) return 0;
    }
    return 1;
}

void style_apply_sheet(int root) {
    int i, j;
    (void)root;
    if (!g.rules.n) return;
    for (i = 1; i < g.el.n; i++) {
        if (!E(i)->alive) continue;
        if (E(i)->styled_gen == g.rules_gen) continue;
        for (j = 0; j < g.rules.n; j++) {
            const Rule *r = &g.rules.a[j];
            if (r->state != 0) continue;              // :hover/:pressed — пока не применяем из таблицы
            if (sel_match(E(i), S(r->sel))) apply_style_string(i, r->css);
        }
        if (E(i)->inl) apply_style_string(i, E(i)->inl);  // свой стиль поверх
        E(i)->styled_gen = g.rules_gen;
    }
}

void restyle_all(void) {
    int i;
    for (i = 1; i < g.el.n; i++) if (E(i)->alive) E(i)->styled_gen = -1;
    request_redraw();
}
