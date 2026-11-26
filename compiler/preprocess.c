// Preprocessor: #include, #define (object and function-like, # and ##,
// __VA_ARGS__), #undef, #if/#ifdef/#ifndef/#elif/#else/#endif, defined(),
// #error, #pragma once, __FILE__, __LINE__.
#include "cc.h"

typedef struct MacroParam MacroParam;
struct MacroParam {
    MacroParam *next;
    char *name;
};

typedef struct MacroArg MacroArg;
struct MacroArg {
    MacroArg *next;
    char *name;
    Token *tok;
};

typedef struct Macro Macro;
struct Macro {
    Macro *next;
    char *name;
    int is_objlike;
    MacroParam *params;
    int is_variadic;
    Token *body;
    int deleted;
    int builtin;                // 1 = __FILE__, 2 = __LINE__
};

typedef struct CondIncl CondIncl;
struct CondIncl {
    CondIncl *next;
    enum { IN_THEN, IN_ELIF, IN_ELSE } ctx;
    Token *tok;
    int included;
};

// Hide sets stop a macro from expanding inside its own expansion.
typedef struct Hideset Hideset;
struct Hideset {
    Hideset *next;
    char *name;
};

// macros by name: a hash table (windows.h alone defines thousands); in a chain the newest comes first
#define MACRO_BUCKETS 4096
static Macro *g_macros[MACRO_BUCKETS];

// does s begin with the (lower-case) word w, any case?
static int prefix_nocase(const char *s, const char *w, int n)
{
    for (int i = 0; i < n; i++) {
        char a = s[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != w[i]) return 0;
    }
    return 1;
}

static unsigned macro_hash(const char *s, int len)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < len; i++)
        h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h & (MACRO_BUCKETS - 1);
}
static CondIncl *g_cond;
static const char *g_once[64];
static int g_once_count;
static int g_include_depth;
static int g_pack_now, g_pack_stack[32], g_pack_depth;      // #pragma pack

static Hideset *get_hs(Token *t) { return (Hideset *)t->hideset; }
static void set_hs(Token *t, Hideset *hs) { t->hideset = hs; }

static Hideset *new_hs(char *name)
{
    Hideset *h = cc_alloc(sizeof(Hideset));
    h->name = name;
    return h;
}

static Hideset *hs_union(Hideset *a, Hideset *b)
{
    Hideset head = { 0 };
    Hideset *cur = &head;
    for (; a; a = a->next)
        cur = cur->next = new_hs(a->name);
    cur->next = b;
    return head.next;
}

static int hs_contains(Hideset *hs, const char *s, int len)
{
    for (; hs; hs = hs->next)
        if (cc_strlen(hs->name) == len && cc_strneq(hs->name, s, len))
            return 1;
    return 0;
}

static Hideset *hs_intersection(Hideset *a, Hideset *b)
{
    Hideset head = { 0 };
    Hideset *cur = &head;
    for (; a; a = a->next)
        if (hs_contains(b, a->name, cc_strlen(a->name)))
            cur = cur->next = new_hs(a->name);
    return head.next;
}

static Token *copy_token(Token *tok)
{
    Token *t = cc_alloc(sizeof(Token));
    *t = *tok;
    t->next = 0;
    set_hs(t, get_hs(tok));
    return t;
}

static Token *new_eof(Token *tok)
{
    Token *t = copy_token(tok);
    t->kind = TK_EOF;
    t->len = 0;
    return t;
}

static Token *add_hideset(Token *tok, Hideset *hs)
{
    Token head = { 0 };
    Token *cur = &head;
    for (; tok; tok = tok->next) {
        Token *t = copy_token(tok);
        set_hs(t, hs_union(get_hs(t), hs));
        cur = cur->next = t;
    }
    return head.next;
}

static Token *append(Token *a, Token *b)
{
    if (a->kind == TK_EOF)
        return b;
    Token head = { 0 };
    Token *cur = &head;
    for (; a->kind != TK_EOF; a = a->next)
        cur = cur->next = copy_token(a);
    cur->next = b;
    return head.next;
}

static int is_hash(Token *tok)
{
    return tok->at_bol && cc_equal(tok, "#");
}

static Token *skip_line(Token *tok)
{
    if (tok->at_bol)
        return tok;
    // extra tokens after a directive are ignored
    while (!tok->at_bol)
        tok = tok->next;
    return tok;
}

static Token *copy_line(Token **rest, Token *tok)
{
    Token head = { 0 };
    Token *cur = &head;
    for (; !tok->at_bol; tok = tok->next)
        cur = cur->next = copy_token(tok);
    cur->next = new_eof(tok);
    *rest = tok;
    return head.next;
}

static char *tok_name(Token *t)
{
    return cc_strndup(t->loc, t->len);
}

static Macro *find_macro(Token *tok)
{
    if (tok->kind != TK_IDENT)
        return 0;
    for (Macro *m = g_macros[macro_hash(tok->loc, tok->len)]; m; m = m->next)
        if (cc_strlen(m->name) == tok->len && cc_strneq(m->name, tok->loc, tok->len))
            return m->deleted ? 0 : m;
    return 0;
}

static Macro *add_macro(char *name, int is_objlike, Token *body)
{
    Macro *m = cc_alloc(sizeof(Macro));
    unsigned h = macro_hash(name, cc_strlen(name));
    m->next = g_macros[h];
    m->name = name;
    m->is_objlike = is_objlike;
    m->body = body;
    g_macros[h] = m;
    return m;
}

// ---- #if expression ----

static Token *preprocess2(Token *tok);

static char *beside(const char *from, const char *name);
static char *join_tokens(Token *tok, Token *end);

// __has_include("x.h") / (<x.h>): is there such a file, where #include would look for it
static int has_include(Token *at, const char *name, int quoted)
{
    if (quoted && at->file && cc_read_file(beside(at->file, name)))
        return 1;
    if (cc_builtin_header(name))
        return 1;
    cc_include_angle = !quoted;
    int found = cc_read_file(name) != 0;
    cc_include_angle = 0;
    return found;
}

static Token *read_const_expr(Token **rest, Token *tok)
{
    tok = copy_line(rest, tok);
    Token head = { 0 };
    Token *cur = &head;
    while (tok->kind != TK_EOF) {
        if (cc_equal(tok, "__has_include") || cc_equal(tok, "__has_include_next")) {
            Token *start = tok;
            tok = cc_skip(tok->next, "(");
            char *name;
            int quoted = tok->kind == TK_STR;
            if (quoted) {
                name = cc_strndup(tok->str, tok->str_len - 1);
                tok = tok->next;
            } else if (cc_equal(tok, "<")) {
                Token *first = tok->next, *t = first;
                while (!cc_equal(t, ">") && t->kind != TK_EOF)
                    t = t->next;
                name = join_tokens(first, t);
                tok = t->kind == TK_EOF ? t : t->next;
            } else {
                cc_error_tok(start, "в __has_include нужно \"файл.h\" или <файл.h>");
            }
            tok = cc_skip(tok, ")");
            Token *num = copy_token(start);
            num->kind = TK_NUM;
            num->val = has_include(start, name, quoted);
            cur = cur->next = num;
            continue;
        }
        if (cc_equal(tok, "defined")) {
            Token *start = tok;
            int paren = cc_consume(&tok, tok->next, "(");
            if (tok->kind != TK_IDENT)
                cc_error_tok(start, "после defined нужно имя макроса");
            Macro *m = find_macro(tok);
            tok = tok->next;
            if (paren)
                tok = cc_skip(tok, ")");
            Token *num = copy_token(start);
            num->kind = TK_NUM;
            num->val = m ? 1 : 0;
            cur = cur->next = num;
            continue;
        }
        cur = cur->next = tok;
        tok = tok->next;
    }
    cur->next = tok;
    return head.next;
}

// Tiny evaluator for #if: integers, identifiers = 0, full operator set.
static i64 ce_cond(Token **rest, Token *tok);

static i64 ce_primary(Token **rest, Token *tok)
{
    if (cc_equal(tok, "(")) {
        i64 v = ce_cond(&tok, tok->next);
        *rest = cc_skip(tok, ")");
        return v;
    }
    if (tok->kind == TK_NUM) {
        *rest = tok->next;
        return tok->val;
    }
    if (tok->kind == TK_IDENT || tok->kind == TK_KEYWORD) {
        *rest = tok->next;
        return 0;
    }
    cc_error_tok(tok, "неверное выражение в #if");
}

static i64 ce_unary(Token **rest, Token *tok)
{
    if (cc_equal(tok, "+")) return ce_unary(rest, tok->next);
    if (cc_equal(tok, "-")) return -ce_unary(rest, tok->next);
    if (cc_equal(tok, "!")) return !ce_unary(rest, tok->next);
    if (cc_equal(tok, "~")) return ~ce_unary(rest, tok->next);
    return ce_primary(rest, tok);
}

static int ce_prec(Token *t)
{
    static const struct { const char *op; int prec; } ops[] = {
        { "*", 10 }, { "/", 10 }, { "%", 10 }, { "+", 9 }, { "-", 9 }, { "<<", 8 }, { ">>", 8 },
        { "<", 7 }, { "<=", 7 }, { ">", 7 }, { ">=", 7 }, { "==", 6 }, { "!=", 6 },
        { "&", 5 }, { "^", 4 }, { "|", 3 }, { "&&", 2 }, { "||", 1 },
    };
    if (t->kind != TK_PUNCT)
        return 0;
    for (int i = 0; i < (int)(sizeof(ops) / sizeof(ops[0])); i++)
        if (cc_equal(t, ops[i].op))
            return ops[i].prec;
    return 0;
}

static i64 ce_binary(Token **rest, Token *tok, int min_prec)
{
    i64 lhs = ce_unary(&tok, tok);
    for (;;) {
        int prec = ce_prec(tok);
        if (!prec || prec < min_prec)
            break;
        Token *op = tok;
        i64 rhs = ce_binary(&tok, tok->next, prec + 1);
        if (cc_equal(op, "*")) lhs *= rhs;
        else if (cc_equal(op, "/")) { if (!rhs) cc_error_tok(op, "деление на ноль в #if"); lhs /= rhs; }
        else if (cc_equal(op, "%")) { if (!rhs) cc_error_tok(op, "деление на ноль в #if"); lhs %= rhs; }
        else if (cc_equal(op, "+")) lhs += rhs;
        else if (cc_equal(op, "-")) lhs -= rhs;
        else if (cc_equal(op, "<<")) lhs <<= rhs;
        else if (cc_equal(op, ">>")) lhs >>= rhs;
        else if (cc_equal(op, "<")) lhs = lhs < rhs;
        else if (cc_equal(op, "<=")) lhs = lhs <= rhs;
        else if (cc_equal(op, ">")) lhs = lhs > rhs;
        else if (cc_equal(op, ">=")) lhs = lhs >= rhs;
        else if (cc_equal(op, "==")) lhs = lhs == rhs;
        else if (cc_equal(op, "!=")) lhs = lhs != rhs;
        else if (cc_equal(op, "&")) lhs &= rhs;
        else if (cc_equal(op, "^")) lhs ^= rhs;
        else if (cc_equal(op, "|")) lhs |= rhs;
        else if (cc_equal(op, "&&")) lhs = lhs && rhs;
        else if (cc_equal(op, "||")) lhs = lhs || rhs;
    }
    *rest = tok;
    return lhs;
}

static i64 ce_cond(Token **rest, Token *tok)
{
    i64 c = ce_binary(&tok, tok, 1);
    if (!cc_equal(tok, "?")) {
        *rest = tok;
        return c;
    }
    i64 a = ce_cond(&tok, tok->next);
    tok = cc_skip(tok, ":");
    i64 b = ce_cond(&tok, tok);
    *rest = tok;
    return c ? a : b;
}

static i64 eval_const_expr(Token **rest, Token *tok)
{
    Token *start = tok;
    Token *expr = read_const_expr(rest, tok->next);
    expr = preprocess2(expr);
    if (expr->kind == TK_EOF)
        cc_error_tok(start, "пустое выражение в #if");
    Token *end;
    i64 v = ce_cond(&end, expr);
    if (end->kind != TK_EOF)
        cc_error_tok(end, "лишнее в выражении #if");
    return v;
}

static void push_cond(Token *tok, int included)
{
    CondIncl *c = cc_alloc(sizeof(CondIncl));
    c->next = g_cond;
    c->ctx = IN_THEN;
    c->tok = tok;
    c->included = included;
    g_cond = c;
}

// Skip until the matching #elif / #else / #endif.
static Token *skip_cond_incl(Token *tok)
{
    while (tok->kind != TK_EOF) {
        if (is_hash(tok) && (cc_equal(tok->next, "if") || cc_equal(tok->next, "ifdef") ||
                             cc_equal(tok->next, "ifndef"))) {
            int depth = 1;
            tok = tok->next->next;
            while (tok->kind != TK_EOF && depth) {
                if (is_hash(tok)) {
                    Token *d = tok->next;
                    if (cc_equal(d, "if") || cc_equal(d, "ifdef") || cc_equal(d, "ifndef"))
                        depth++;
                    else if (cc_equal(d, "endif"))
                        depth--;
                }
                tok = tok->next;
            }
            continue;
        }
        if (is_hash(tok) && (cc_equal(tok->next, "elif") || cc_equal(tok->next, "else") ||
                             cc_equal(tok->next, "endif")))
            break;
        tok = tok->next;
    }
    return tok;
}

// ---- macro definition and expansion ----

static MacroParam *read_macro_params(Token **rest, Token *tok, int *variadic)
{
    MacroParam head = { 0 };
    MacroParam *cur = &head;
    while (!cc_equal(tok, ")")) {
        if (cur != &head)
            tok = cc_skip(tok, ",");
        if (cc_equal(tok, "...")) {
            *variadic = 1;
            *rest = cc_skip(tok->next, ")");
            return head.next;
        }
        if (tok->kind != TK_IDENT)
            cc_error_tok(tok, "ожидалось имя параметра");
        MacroParam *p = cc_alloc(sizeof(MacroParam));
        p->name = tok_name(tok);
        cur = cur->next = p;
        tok = tok->next;
    }
    *rest = tok->next;
    return head.next;
}

static void read_macro_definition(Token **rest, Token *tok)
{
    if (tok->kind != TK_IDENT)
        cc_error_tok(tok, "после #define нужно имя");
    char *name = tok_name(tok);
    tok = tok->next;
    if (!tok->has_space && cc_equal(tok, "(")) {
        int variadic = 0;
        MacroParam *params = read_macro_params(&tok, tok->next, &variadic);
        Macro *m = add_macro(name, 0, copy_line(rest, tok));
        m->params = params;
        m->is_variadic = variadic;
    } else {
        add_macro(name, 1, copy_line(rest, tok));
    }
}

static MacroArg *read_macro_arg_one(Token **rest, Token *tok, int read_rest)
{
    Token head = { 0 };
    Token *cur = &head;
    int level = 0;
    for (;;) {
        if (level == 0 && cc_equal(tok, ")"))
            break;
        if (level == 0 && !read_rest && cc_equal(tok, ","))
            break;
        if (tok->kind == TK_EOF)
            cc_error_tok(tok, "аргументы макроса не закрыты");
        if (cc_equal(tok, "("))
            level++;
        else if (cc_equal(tok, ")"))
            level--;
        cur = cur->next = copy_token(tok);
        tok = tok->next;
    }
    cur->next = new_eof(tok);
    MacroArg *arg = cc_alloc(sizeof(MacroArg));
    arg->tok = head.next;
    *rest = tok;
    return arg;
}

static MacroArg *read_macro_args(Token **rest, Token *tok, MacroParam *params, int variadic)
{
    Token *start = tok;
    tok = tok->next->next;          // name (
    MacroArg head = { 0 };
    MacroArg *cur = &head;
    MacroParam *pp = params;
    for (; pp; pp = pp->next) {
        if (cur != &head) {
            if (cc_equal(tok, ")"))
                cc_error_tok(start, "мало аргументов у макроса %s", cc_strndup(start->loc, start->len));
            tok = cc_skip(tok, ",");
        }
        cur = cur->next = read_macro_arg_one(&tok, tok, 0);
        cur->name = pp->name;
    }
    if (variadic) {
        MacroArg *arg;
        if (cc_equal(tok, ")")) {
            arg = cc_alloc(sizeof(MacroArg));
            arg->tok = new_eof(tok);
        } else {
            if (pp != params || params)
                tok = cc_skip(tok, ",");
            arg = read_macro_arg_one(&tok, tok, 1);
        }
        arg->name = "__VA_ARGS__";
        cur = cur->next = arg;
    } else if (pp) {
        cc_error_tok(start, "мало аргументов у макроса %s", cc_strndup(start->loc, start->len));
    }
    if (!cc_equal(tok, ")"))
        cc_error_tok(start, "много аргументов у макроса %s", cc_strndup(start->loc, start->len));
    *rest = tok;
    return head.next;
}

static MacroArg *find_arg(MacroArg *args, Token *tok)
{
    for (MacroArg *a = args; a; a = a->next)
        if (tok->len == cc_strlen(a->name) && cc_strneq(tok->loc, a->name, tok->len))
            return a;
    return 0;
}

static char *join_tokens(Token *tok, Token *end)
{
    int len = 1;
    for (Token *t = tok; t != end && t->kind != TK_EOF; t = t->next)
        len += t->len + 1;
    char *buf = cc_alloc((size_t)len);
    int pos = 0;
    for (Token *t = tok; t != end && t->kind != TK_EOF; t = t->next) {
        if (t != tok && t->has_space)
            buf[pos++] = ' ';
        for (int i = 0; i < t->len; i++)
            buf[pos++] = t->loc[i];
    }
    buf[pos] = 0;
    return buf;
}

static Token *stringize(Token *hash, Token *arg)
{
    char *s = join_tokens(arg, 0);
    int n = cc_strlen(s), len = 3;
    for (int i = 0; i < n; i++)
        len += (s[i] == '\\' || s[i] == '"') ? 2 : 1;
    char *buf = cc_alloc((size_t)len);
    int pos = 0;
    buf[pos++] = '"';
    for (int i = 0; i < n; i++) {
        if (s[i] == '\\' || s[i] == '"')
            buf[pos++] = '\\';
        buf[pos++] = s[i];
    }
    buf[pos++] = '"';
    Token *t = cc_tokenize(hash->file, buf);
    t->line = hash->line;
    return t;
}

static Token *paste(Token *lhs, Token *rhs)
{
    char *buf = cc_format("%s%s", cc_strndup(lhs->loc, lhs->len), cc_strndup(rhs->loc, rhs->len));
    Token *t = cc_tokenize(lhs->file, buf);
    if (t->next->kind != TK_EOF)
        cc_error_tok(lhs, "склейка ## дала не один знак: %s", buf);
    t->line = lhs->line;
    return t;
}

static MacroArg *va_args_of(MacroArg *args)
{
    for (MacroArg *a = args; a; a = a->next)
        if (cc_streq(a->name, "__VA_ARGS__"))
            return a;
    return 0;
}

static Token *subst(Token *tok, MacroArg *args)
{
    Token head = { 0 };
    Token *cur = &head;
    while (tok->kind != TK_EOF) {
        // __VA_OPT__(x): x when the variable arguments are not empty, else nothing (C23)
        if (cc_equal(tok, "__VA_OPT__") && cc_equal(tok->next, "(")) {
            MacroArg *va = va_args_of(args);
            Token inner = { 0 }, *in = &inner, *t = tok->next->next;
            for (int depth = 1;; t = t->next) {
                if (t->kind == TK_EOF)
                    cc_error_tok(tok, "не закрыта скобка __VA_OPT__(");
                if (cc_equal(t, "(")) depth++;
                if (cc_equal(t, ")") && --depth == 0) break;
                in = in->next = copy_token(t);
            }
            in->next = new_eof(t);
            if (va && va->tok->kind != TK_EOF && inner.next->kind != TK_EOF)
                for (Token *k = subst(inner.next, args); k->kind != TK_EOF; k = k->next)
                    cur = cur->next = copy_token(k);
            tok = t->next;
            continue;
        }
        // ", ## __VA_ARGS__" (GNU): no variable arguments -- the comma goes too
        if (cc_equal(tok, "##") && cur != &head && cc_equal(cur, ",")) {
            MacroArg *va = find_arg(args, tok->next);
            if (va && cc_streq(va->name, "__VA_ARGS__")) {
                if (va->tok->kind == TK_EOF) {
                    Token *p = &head;
                    while (p->next != cur)
                        p = p->next;
                    cur = p;
                    cur->next = 0;
                } else {
                    for (Token *k = preprocess2(va->tok); k->kind != TK_EOF; k = k->next)
                        cur = cur->next = copy_token(k);
                }
                tok = tok->next->next;
                continue;
            }
        }
        if (cc_equal(tok, "#")) {
            MacroArg *arg = find_arg(args, tok->next);
            if (!arg)
                cc_error_tok(tok->next, "после # нужен параметр макроса");
            cur = cur->next = stringize(tok, arg->tok);
            tok = tok->next->next;
            continue;
        }
        if (cc_equal(tok, "##")) {
            if (cur == &head)
                cc_error_tok(tok, "## в начале макроса");
            if (tok->next->kind == TK_EOF)
                cc_error_tok(tok, "## в конце макроса");
            MacroArg *arg = find_arg(args, tok->next);
            if (arg) {
                if (arg->tok->kind != TK_EOF) {
                    *cur = *paste(cur, arg->tok);
                    for (Token *t = arg->tok->next; t->kind != TK_EOF; t = t->next)
                        cur = cur->next = copy_token(t);
                }
                tok = tok->next->next;
                continue;
            }
            *cur = *paste(cur, tok->next);
            tok = tok->next->next;
            continue;
        }
        MacroArg *arg = find_arg(args, tok);
        if (arg && cc_equal(tok->next, "##")) {
            Token *rhs = tok->next->next;
            if (arg->tok->kind == TK_EOF) {
                MacroArg *arg2 = find_arg(args, rhs);
                if (arg2) {
                    for (Token *t = arg2->tok; t->kind != TK_EOF; t = t->next)
                        cur = cur->next = copy_token(t);
                } else {
                    cur = cur->next = copy_token(rhs);
                }
                tok = rhs->next;
                continue;
            }
            for (Token *t = arg->tok; t->kind != TK_EOF; t = t->next)
                cur = cur->next = copy_token(t);
            tok = tok->next;
            continue;
        }
        if (arg) {
            Token *t = preprocess2(arg->tok);
            t->at_bol = tok->at_bol;
            t->has_space = tok->has_space;
            for (; t->kind != TK_EOF; t = t->next)
                cur = cur->next = copy_token(t);
            tok = tok->next;
            continue;
        }
        cur = cur->next = copy_token(tok);
        tok = tok->next;
    }
    cur->next = tok;
    return head.next;
}

static int expand_macro(Token **rest, Token *tok)
{
    if (hs_contains(get_hs(tok), tok->loc, tok->len))
        return 0;
    Macro *m = find_macro(tok);
    if (!m)
        return 0;

    // __FILE__ and __LINE__ are the place of use: inside a macro's body they
    // already carry the file and line of the macro's call (see below)
    if (m->builtin == 6)
        return 0;                   // __has_include outside #if: as it is
    if (m->builtin) {
        Token *t = copy_token(tok);
        if (m->builtin == 1) {
            const char *f = tok->file ? tok->file : "?";
            int n = cc_strlen(f), pos = 0;
            char *s = cc_alloc((size_t)n * 2 + 3);
            s[pos++] = '"';
            for (int i = 0; i < n; i++) {
                if (f[i] == '\\' || f[i] == '"')
                    s[pos++] = '\\';        // "dir\file.c" stays a path, not escapes
                s[pos++] = f[i];
            }
            s[pos++] = '"';
            Token *st = cc_tokenize(tok->file, s);
            st->line = tok->line;
            st->at_bol = tok->at_bol;
            st->has_space = tok->has_space;
            st->next = tok->next;
            *rest = st;
            return 1;
        }
        // __LINE__, __COUNTER__, __DATE__, __TIME__: real tokens, text and all, so that "#x" of
        // S(__LINE__) gives "12", not "__LINE__"
        static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        static int counter;
        int now[6];
        const char *text;
        if (m->builtin == 2)
            text = cc_format("%d", tok->line);
        else if (m->builtin == 3)
            text = cc_format("%d", counter++);
        else {
            cc_now(now);
            if (m->builtin == 4)
                text = cc_format("\"%s %2d %d\"", months[(now[1] + 11) % 12], now[2], now[0]);
            else
                text = cc_format("\"%02d:%02d:%02d\"", now[3], now[4], now[5]);
        }
        Token *nt = cc_tokenize(tok->file, text);
        nt->line = tok->line;
        nt->at_bol = tok->at_bol;
        nt->has_space = tok->has_space;
        nt->next = tok->next;
        *rest = nt;
        (void)t;
        return 1;
    }

    if (m->is_objlike) {
        Hideset *hs = hs_union(get_hs(tok), new_hs(m->name));
        Token *body = add_hideset(m->body, hs);
        for (Token *t = body; t->kind != TK_EOF; t = t->next) {
            t->file = tok->file;    // a header's macro belongs to the place of its call
            t->line = tok->line;
        }
        if (body->kind != TK_EOF) {
            body->at_bol = tok->at_bol;
            body->has_space = tok->has_space;
        }
        *rest = append(body, tok->next);
        return 1;
    }

    if (!cc_equal(tok->next, "("))
        return 0;           // function-like macro name used without ()
    Token *macro_tok = tok;
    MacroArg *args = read_macro_args(&tok, tok, m->params, m->is_variadic);
    Token *rparen = tok;
    Hideset *hs = hs_intersection(get_hs(macro_tok), get_hs(rparen));
    hs = hs_union(hs, new_hs(m->name));
    Token *body = subst(m->body, args);
    body = add_hideset(body, hs);
    for (Token *t = body; t->kind != TK_EOF; t = t->next) {
        t->file = macro_tok->file;
        t->line = macro_tok->line;
    }
    if (body->kind != TK_EOF) {
        body->at_bol = macro_tok->at_bol;
        body->has_space = macro_tok->has_space;
    }
    *rest = append(body, tok->next);
    return 1;
}

// The folder of `from` joined with `name`, with "." and ".." resolved:
// "Hermetic/Ankh/src/ankh.c" + "../../HieroArc/src/hieroarc.h"
// -> "Hermetic/HieroArc/src/hieroarc.h".
static char *beside(const char *from, const char *name)
{
    int dl = 0, nl = cc_strlen(name);
    for (int i = 0; from && from[i]; i++)
        if (from[i] == '/' || from[i] == '\\')
            dl = i + 1;
    char *raw = cc_alloc((size_t)(dl + nl + 1));
    for (int i = 0; i < dl; i++) raw[i] = from[i] == '\\' ? '/' : from[i];
    for (int i = 0; i < nl; i++) raw[dl + i] = name[i] == '\\' ? '/' : name[i];
    raw[dl + nl] = 0;
    char *out = cc_alloc((size_t)(dl + nl + 1));
    int o = 0;
    const char *p = raw;
    if (*p == '/') out[o++] = *p++;              // an absolute path stays one
    while (*p) {
        const char *s = p;
        while (*p && *p != '/') p++;
        int sl = (int)(p - s);
        if (*p == '/') p++;
        if (!sl || (sl == 1 && s[0] == '.')) continue;
        if (sl == 2 && s[0] == '.' && s[1] == '.') {
            int e = o;                           // the last folder written goes, unless it is ".." itself
            if (e && out[e - 1] == '/') e--;
            int b = e;
            while (b > 0 && out[b - 1] != '/') b--;
            if (e > b && !(e - b == 2 && out[b] == '.' && out[b + 1] == '.')) { o = b; continue; }
        }
        for (int i = 0; i < sl; i++) out[o++] = s[i];
        if (*p || p[-1] == '/') out[o++] = '/';
    }
    if (o && out[o - 1] == '/' && raw[dl + nl - 1] != '/') o--;
    out[o] = 0;
    return out;
}

// #include "x.h" is looked for beside the file that includes it first, then
// among the system's headers, then from the current folder; <x.h> among the
// system's headers first.
int cc_include_angle;

static Token *include_file(Token *tok, const char *name, Token *filename_tok, int quoted)
{
    const char *src = 0;
    if (quoted && filename_tok->file) {
        char *near_name = beside(filename_tok->file, name);
        for (int i = 0; i < g_once_count; i++)
            if (cc_streq(g_once[i], near_name))
                return tok;
        src = cc_read_file(near_name);
        if (src) name = near_name;
    }
    for (int i = 0; i < g_once_count; i++)
        if (cc_streq(g_once[i], name))
            return tok;
    if (++g_include_depth > 32)
        cc_error_tok(filename_tok, "слишком глубокие #include");
    if (!src)
        src = cc_builtin_header(name);
    if (!src) {
        cc_include_angle = !quoted;
        src = cc_read_file(name);
        cc_include_angle = 0;
    }
    if (!src)
        cc_error_tok(filename_tok, "нет файла %s", name);
    Token *t = cc_tokenize(cc_strndup(name, cc_strlen(name)), src);
    g_include_depth--;
    return append(t, tok);
}

// #pragma push_macro("X") / pop_macro("X"): a macro's definition put aside and brought back
static Macro *g_pushed[64];
static const char *g_pushed_name[64];
static int g_pushed_count;

// The words of a #pragma (c: its first word, at: where it is), from #pragma, _Pragma("...")
// or __pragma(...); what the parser must see goes to *cur.
static void pragma(Token *at, Token *c, Token **cur)
{
    if (cc_equal(c, "once")) {
        if (g_once_count < 64)
            g_once[g_once_count++] = at->file;
        return;
    }
    // #pragma comment(lib, "user32"): a DLL (or a library folder lib\name) to take functions from
    if (cc_equal(c, "comment") && cc_equal(c->next, "(") && cc_equal(c->next->next, "lib") &&
        cc_equal(c->next->next->next, ",") && c->next->next->next->next->kind == TK_STR) {
        const char *lib = c->next->next->next->next->str;
        int known = 0;
        for (int i = 0; i < cc_pragma_lib_count; i++)
            if (cc_streq(cc_pragma_libs[i], lib))
                known = 1;
        if (!known && cc_pragma_lib_count < 64)
            cc_pragma_libs[cc_pragma_lib_count++] = lib;
        return;
    }
    // #pragma comment(linker, "/SUBSYSTEM:WINDOWS"): a windowed program even with main()
    if (cc_equal(c, "comment") && cc_equal(c->next, "(") && cc_equal(c->next->next, "linker") &&
        cc_equal(c->next->next->next, ",") && c->next->next->next->next->kind == TK_STR) {
        const char *s = c->next->next->next->next->str;
        for (; *s; s++) {
            if (prefix_nocase(s, "subsystem:windows", 17)) cc_pragma_subsystem = 2;
            if (prefix_nocase(s, "subsystem:console", 17)) cc_pragma_subsystem = 3;
            if (s[0] == '/' && prefix_nocase(s + 1, "manifest", 8)) cc_pragma_manifest = 1;   // WinCC's manifest
        }
        return;
    }
    // #pragma pack(n) / pack(push[, n]) / pack(pop) / pack(): the parser gets "__wcc_pack(n)"
    if (cc_equal(c, "pack") && cc_equal(c->next, "(")) {
        Token *a = c->next->next;
        if (cc_equal(a, "push")) {
            if (g_pack_depth < 32) g_pack_stack[g_pack_depth++] = g_pack_now;
            if (cc_equal(a->next, ",") && a->next->next->kind == TK_NUM)
                g_pack_now = (int)a->next->next->val;
        } else if (cc_equal(a, "pop")) {
            if (g_pack_depth > 0) g_pack_now = g_pack_stack[--g_pack_depth];
        } else if (a->kind == TK_NUM) {
            g_pack_now = (int)a->val;
        } else {
            g_pack_now = 0;
        }
        Token *t = cc_tokenize(at->file, cc_format("__wcc_pack(%d)", g_pack_now));
        for (; t->kind != TK_EOF; t = t->next) {
            t->line = at->line;
            *cur = (*cur)->next = t;
        }
        return;
    }
    // #pragma message("text"): said while building, as MSVC does
    if (cc_equal(c, "message")) {
        Token *s = c->next;
        if (cc_equal(s, "(")) s = s->next;
        if (s->kind == TK_STR && cc_warn_hook)
            cc_warn_hook(s->str);
        return;
    }
    if ((cc_equal(c, "push_macro") || cc_equal(c, "pop_macro")) && cc_equal(c->next, "(") &&
        c->next->next->kind == TK_STR) {
        const char *name = c->next->next->str;
        Token *nt = cc_tokenize(at->file, name);
        if (cc_equal(c, "push_macro")) {
            if (g_pushed_count < 64) {
                g_pushed_name[g_pushed_count] = name;
                g_pushed[g_pushed_count++] = find_macro(nt);
            }
        } else {
            for (int i = g_pushed_count - 1; i >= 0; i--)
                if (cc_streq(g_pushed_name[i], name)) {
                    Macro *was = g_pushed[i], *m = add_macro(cc_strndup(name, cc_strlen(name)), 1, 0);
                    if (was) {
                        Macro *next = m->next;  // its place in the table stays; the rest is the old macro
                        *m = *was;
                        m->next = next;
                    } else {
                        m->deleted = 1;
                    }
                    for (int k = i; k + 1 < g_pushed_count; k++) {
                        g_pushed[k] = g_pushed[k + 1];
                        g_pushed_name[k] = g_pushed_name[k + 1];
                    }
                    g_pushed_count--;
                    break;
                }
        }
        return;
    }
    // what Microsoft's and other compilers' headers say to their compilers: nothing to do here
    static const char *quiet[] = {
        "warning", "region", "endregion", "intrinsic", "function", "optimize", "data_seg", "bss_seg",
        "code_seg", "const_seg", "init_seg", "section", "STDC", "GCC", "clang", "inline_depth",
        "inline_recursion", "detect_mismatch", "fenv_access", "float_control", "fp_contract", "managed",
        "unmanaged", "auto_inline", "check_stack", "runtime_checks", "strict_gs_check", "deprecated",
        "include_alias", "loop", "vtordisp", "conform", "component", "alloc_text", "execution_character_set",
        "setlocale", "system_header", "diagnostic", "unroll", "omp", "code_page", 0,
    };
    for (int i = 0; quiet[i]; i++)
        if (cc_equal(c, quiet[i]))
            return;
    if (c->kind == TK_IDENT && !c->at_bol)
        cc_warning_tok(c, "неизвестная #pragma %s — пропущена", cc_strndup(c->loc, c->len));
}

// _Pragma("pack(1)") and __pragma(pack(1)): a #pragma written inside a line or a macro
static int pragma_operator(Token **rest, Token *tok, Token **cur)
{
    if (cc_equal(tok, "_Pragma") && cc_equal(tok->next, "(") && tok->next->next->kind == TK_STR &&
        cc_equal(tok->next->next->next, ")")) {
        Token *t = cc_tokenize(tok->file, tok->next->next->str);
        for (Token *k = t; k->kind != TK_EOF; k = k->next) {
            k->line = tok->line;
            k->at_bol = 0;
        }
        pragma(tok, t, cur);
        *rest = tok->next->next->next->next;
        return 1;
    }
    if (cc_equal(tok, "__pragma") && cc_equal(tok->next, "(")) {
        Token head = { 0 }, *c = &head, *t = tok->next->next;
        for (int depth = 1;; t = t->next) {
            if (t->kind == TK_EOF)
                cc_error_tok(tok, "не закрыта скобка __pragma(");
            if (cc_equal(t, "(")) depth++;
            if (cc_equal(t, ")") && --depth == 0) break;
            c = c->next = copy_token(t);
            c->at_bol = 0;
        }
        c->next = new_eof(t);
        pragma(tok, head.next, cur);
        *rest = t->next;
        return 1;
    }
    return 0;
}

static Token *preprocess2(Token *tok)
{
    Token head = { 0 };
    Token *cur = &head;

    while (tok->kind != TK_EOF) {
        if (expand_macro(&tok, tok))
            continue;
        if (pragma_operator(&tok, tok, &cur))
            continue;
        if (!is_hash(tok)) {
            cur = cur->next = tok;
            tok = tok->next;
            continue;
        }

        Token *start = tok;
        tok = tok->next;

        if (tok->at_bol)            // lone '#'
            continue;

        if (cc_equal(tok, "include")) {
            Token *ft = tok->next;
            char *name;
            int quoted = ft->kind == TK_STR;
            if (ft->kind == TK_STR) {
                name = cc_strndup(ft->str, ft->str_len - 1);
                tok = skip_line(ft->next);
            } else if (cc_equal(ft, "<")) {
                Token *t = ft->next;
                Token *first = t;
                while (!cc_equal(t, ">")) {
                    if (t->at_bol || t->kind == TK_EOF)
                        cc_error_tok(ft, "ожидалось '>'");
                    t = t->next;
                }
                name = join_tokens(first, t);
                tok = skip_line(t->next);
            } else if (ft->kind == TK_IDENT) {
                // #include CONFIG_H: the macro gives "file.h" or <file.h>
                Token *line = preprocess2(copy_line(&tok, ft));
                if (line->kind == TK_STR) {
                    name = cc_strndup(line->str, line->str_len - 1);
                    quoted = 1;
                } else if (cc_equal(line, "<")) {
                    Token *t = line->next;
                    while (!cc_equal(t, ">") && t->kind != TK_EOF)
                        t = t->next;
                    if (t->kind == TK_EOF)
                        cc_error_tok(ft, "макрос в #include дал <… без '>'");
                    name = join_tokens(line->next, t);
                } else {
                    cc_error_tok(ft, "макрос в #include должен дать \"файл.h\" или <файл.h>");
                }
            } else {
                cc_error_tok(ft, "после #include нужно имя файла");
            }
            tok = include_file(tok, name, ft, quoted);
            continue;
        }
        if (cc_equal(tok, "define")) {
            read_macro_definition(&tok, tok->next);
            continue;
        }
        if (cc_equal(tok, "undef")) {
            tok = tok->next;
            if (tok->kind != TK_IDENT)
                cc_error_tok(tok, "после #undef нужно имя");
            Macro *m = add_macro(tok_name(tok), 1, 0);
            m->deleted = 1;
            tok = skip_line(tok->next);
            continue;
        }
        if (cc_equal(tok, "if")) {
            i64 v = eval_const_expr(&tok, tok);
            push_cond(start, v != 0);
            if (!v)
                tok = skip_cond_incl(tok);
            continue;
        }
        if (cc_equal(tok, "ifdef") || cc_equal(tok, "ifndef")) {
            int want = cc_equal(tok, "ifdef");
            int defined = find_macro(tok->next) != 0;
            push_cond(tok, defined == want);
            tok = skip_line(tok->next->next);
            if (defined != want)
                tok = skip_cond_incl(tok);
            continue;
        }
        if (cc_equal(tok, "elif")) {
            if (!g_cond || g_cond->ctx == IN_ELSE)
                cc_error_tok(start, "#elif без #if");
            g_cond->ctx = IN_ELIF;
            if (!g_cond->included) {
                if (eval_const_expr(&tok, tok))
                    g_cond->included = 1;
                else
                    tok = skip_cond_incl(tok);
            } else {
                copy_line(&tok, tok->next);
                tok = skip_cond_incl(tok);
            }
            continue;
        }
        if (cc_equal(tok, "else")) {
            if (!g_cond || g_cond->ctx == IN_ELSE)
                cc_error_tok(start, "#else без #if");
            g_cond->ctx = IN_ELSE;
            tok = skip_line(tok->next);
            if (g_cond->included)
                tok = skip_cond_incl(tok);
            continue;
        }
        if (cc_equal(tok, "endif")) {
            if (!g_cond)
                cc_error_tok(start, "#endif без #if");
            g_cond = g_cond->next;
            tok = skip_line(tok->next);
            continue;
        }
        if (cc_equal(tok, "pragma")) {
            pragma(tok, tok->next, &cur);
            do {
                tok = tok->next;
            } while (!tok->at_bol);
            continue;
        }
        if (cc_equal(tok, "error") || cc_equal(tok, "warning")) {
            Token *end = tok->next;                 // the text: to the end of the line, not of the file
            while (!end->at_bol && end->kind != TK_EOF)
                end = end->next;
            const char *text = join_tokens(tok->next, end);
            if (cc_equal(tok, "error"))
                cc_error_tok(tok, "#error %s", text);
            cc_warning_tok(tok, "%s", text);
            tok = end;
            continue;
        }
        if (cc_equal(tok, "line")) {
            // #line N ["file"]: the next line is line N (of that file); the lines after it follow on
            Token *n = tok->next;
            if (n->at_bol || n->kind != TK_NUM)
                cc_error_tok(tok, "после #line нужен номер строки");
            const char *name = !n->next->at_bol && n->next->kind == TK_STR ? n->next->str : 0;
            int delta = (int)n->val - (tok->line + 1);
            const char *file = tok->file;
            do {
                tok = tok->next;
            } while (!tok->at_bol);
            for (Token *t = tok; t->kind != TK_EOF; t = t->next)
                if (t->file == file) {
                    t->line += delta;
                    if (name)
                        t->file = name;
                }
            continue;
        }
        cc_error_tok(tok, "неизвестная директива");
    }
    cur->next = tok;
    return head.next;
}

void cc_define_macro(const char *name, const char *body)
{
    Token *t = cc_tokenize("<builtin>", body);
    add_macro(cc_strndup(name, cc_strlen(name)), 1, t);
}

static void init_macros(void)
{
    for (int i = 0; i < MACRO_BUCKETS; i++)
        g_macros[i] = 0;
    g_pack_now = g_pack_depth = 0;
    g_cond = 0;
    g_once_count = 0;
    g_include_depth = 0;
    if (cc_target_win) {
        cc_define_macro("_WIN32", "1");
        cc_define_macro("_WIN64", "1");
        cc_define_macro("_M_X64", "100");
        cc_define_macro("_M_AMD64", "100");
        cc_define_macro("__WINCC__", "1");
        // Microsoft C words: one calling convention in x64, so these say nothing
        static const char *empty[] = { "__stdcall", "__cdecl", "__fastcall", "__thiscall", "__vectorcall",
                                       "__w64", "__ptr64", "__ptr32", "__unaligned", "__restrict", 0 };
        for (int i = 0; empty[i]; i++)
            cc_define_macro(empty[i], "");
        cc_define_macro("__inline", "inline");
        cc_define_macro("__forceinline", "inline");
        cc_define_macro("__int8", "char");
        cc_define_macro("__int16", "short");
        cc_define_macro("__int32", "int");
        cc_define_macro("__int64", "long long");
    } else {
        cc_define_macro("__DYNASTYOS__", "1");
    }
    for (int i = 0; i < cc_user_define_count; i++)
        cc_define_macro(cc_user_define_names[i], cc_user_define_values[i]);
    cc_define_macro("__STDC__", "1");
    cc_define_macro("__STDC_VERSION__", "201112L");
    cc_define_macro("__STDC_NO_COMPLEX__", "1");     // complex.h is Microsoft's: _Dcomplex and functions, no _Complex
    cc_define_macro("__x86_64__", "1");
    cc_define_macro("NULL", "((void*)0)");
    cc_define_macro("true", "1");
    cc_define_macro("false", "0");
    cc_define_macro("bool", "_Bool");
    cc_define_macro("va_list", "__builtin_va_list");
    add_macro("__FILE__", 1, 0)->builtin = 1;
    add_macro("__LINE__", 1, 0)->builtin = 2;
    add_macro("__COUNTER__", 1, 0)->builtin = 3;
    add_macro("__DATE__", 1, 0)->builtin = 4;
    add_macro("__TIME__", 1, 0)->builtin = 5;
    add_macro("__has_include", 1, 0)->builtin = 6;      // only for #if; "defined(__has_include)" is 1
    add_macro("__has_include_next", 1, 0)->builtin = 6;
}

// One translation unit: it starts with only the predefined macros, and its
// own #pragma once and include guards (cc.c calls this per source).
Token *cc_preprocess(Token *tok)
{
    init_macros();
    tok = preprocess2(tok);
    if (g_cond)
        cc_error_tok(g_cond->tok, "#if не закрыт #endif");
    cc_convert_keywords(tok);
    // adjacent string literals are joined in the parser
    return tok;
}
