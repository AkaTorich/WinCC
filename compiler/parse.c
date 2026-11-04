// Recursive-descent parser: tokens -> typed AST.
// Follows the C99 grammar closely; notable gaps: bit-fields, VLAs.
#include "cc.h"

typedef struct VarScope {
    Obj *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
} VarScope;

typedef struct ScopeEntry ScopeEntry;
struct ScopeEntry {
    ScopeEntry *next;
    char *name;
    int len;
    VarScope *vs;
};

typedef struct TagEntry TagEntry;
struct TagEntry {
    TagEntry *next;
    char *name;
    int len;
    Type *ty;
};

typedef struct Scope Scope;
struct Scope {
    Scope *next;
    ScopeEntry *vars;
    TagEntry *tags;
};

typedef struct {
    int is_typedef, is_static, is_extern, is_inline;
    int is_export;              // __declspec(dllexport)
    int is_naked;               // __declspec(naked)
    int is_tls;                 // _Thread_local, thread_local, __declspec(thread)
    int is_selectany;           // __declspec(selectany)
    int align;
} VarAttr;

typedef struct Initializer Initializer;
struct Initializer {
    Initializer *next;
    Type *ty;
    Token *tok;
    int is_flexible;
    Node *expr;
    Node *copy;                          // union: the whole value from an expression (u = other)
    Initializer **children;
};

typedef struct InitDesg InitDesg;
struct InitDesg {
    InitDesg *next;
    int idx;
    Member *member;
    Obj *var;
};

static Obj *g_locals;
static Obj *g_globals;
static Scope *g_scope;
static Obj *g_current_fn;
static Node *g_gotos, *g_labels;
static char *g_brk_label, *g_cont_label;
static Node *g_current_switch;
static int g_unique_id;

static Scope g_global_scope;

static int g_param_depth;               // inside a parameter list: an array size may name a parameter
static int g_vla_count;                 // VLAs declared so far: a loop with one inside frees them each round
// __try: where __leave goes; the __finally funclets around this point (outermost first) and how
// many of them were there where the loop or switch that break/continue leave began
static char *g_leave_label;
static char *g_fin[64];
static int g_nfin, g_brk_fin, g_cont_fin;
static int g_seh_ctx;                   // 1 in an __except filter, 2 in its block, 3 in a __finally block
static Obj *g_seh_xptr, *g_seh_code, *g_seh_abn;   // the function's EXCEPTION_POINTERS*, code, abnormal flag

// ---- sources: each is its own translation unit ----
// A unit starts with a fresh file scope (typedefs, tags, enums, statics);
// functions and variables with external linkage are shared by name.
static int g_unit;                      // the source being parsed now (1, 2...; 0: before any)
static int g_pack;                      // #pragma pack now: the most a member is aligned to (0 = natural)

// "__wcc_pack(n)": what the preprocessor makes of #pragma pack. Taken where a
// declaration or statement may start; 1 if one was taken.
static int take_pack(Token **rest, Token *tok)
{
    if (!cc_equal(tok, "__wcc_pack"))
        return 0;
    tok = cc_skip(tok->next, "(");
    g_pack = (int)tok->val;
    *rest = cc_skip(tok->next, ")");
    return 1;
}

// The source a token is of (1, 2...); its headers' tokens are its too.
static int unit_of(Token *tok)
{
    return tok ? tok->unit : 0;
}

// Another source's static is not seen here.
static int hidden(Obj *var)
{
    return var && var->unit && var->unit != g_unit;
}

// Statics of different sources with one name get names of their own in the
// assembly ("name.2"); a name every source sees keeps its own.
static void link_name(Obj *var)
{
    for (Obj *g = g_globals; g; g = g->next) {
        if (g == var || !cc_streq(g->name, var->name))
            continue;
        if (g->unit && g->unit != var->unit)
            g->name = cc_format("%s.%d", g->name, g->unit);
        else if (var->unit && var->unit != g->unit) {
            var->name = cc_format("%s.%d", var->name, var->unit);
            return;
        }
    }
}

// ---- helpers ----

static Node *new_node(NodeKind kind, Token *tok)
{
    Node *n = cc_alloc(sizeof(Node));
    n->kind = kind;
    n->tok = tok;
    return n;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok)
{
    Node *n = new_node(kind, tok);
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok)
{
    Node *n = new_node(kind, tok);
    n->lhs = expr;
    return n;
}

static Node *new_num(i64 val, Token *tok)
{
    Node *n = new_node(ND_NUM, tok);
    n->val = val;
    return n;
}

static Node *new_long(i64 val, Token *tok)
{
    Node *n = new_num(val, tok);
    n->ty = ty_long;
    return n;
}

static Node *new_ulong(i64 val, Token *tok)
{
    Node *n = new_num(val, tok);
    n->ty = ty_ulong;
    return n;
}

static Node *new_var_node(Obj *var, Token *tok)
{
    Node *n = new_node(ND_VAR, tok);
    n->var = var;
    return n;
}

Node *new_cast(Node *expr, Type *ty)
{
    add_type(expr);
    Node *n = new_node(ND_CAST, expr->tok);
    n->lhs = expr;
    n->ty = copy_type(ty);
    return n;
}

static char *new_unique_name(void)
{
    return cc_format(".L..%d", g_unique_id++);
}

static char *get_ident(Token *tok)
{
    if (tok->kind != TK_IDENT)
        cc_error_tok(tok, "ожидалось имя");
    return cc_strndup(tok->loc, tok->len);
}

static int get_number(Token *tok)
{
    if (tok->kind != TK_NUM)
        cc_error_tok(tok, "ожидалось число");
    return (int)tok->val;
}

// ---- scopes ----

static void enter_scope(void)
{
    Scope *sc = cc_alloc(sizeof(Scope));
    sc->next = g_scope;
    g_scope = sc;
}

static void leave_scope(void)
{
    g_scope = g_scope->next;
}

static VarScope *find_var(Token *tok)
{
    for (Scope *sc = g_scope; sc; sc = sc->next)
        for (ScopeEntry *e = sc->vars; e; e = e->next)
            if (e->len == tok->len && cc_strneq(e->name, tok->loc, tok->len) && !hidden(e->vs->var))
                return e->vs;
    return 0;
}

static Type *find_tag(Token *tok)
{
    for (Scope *sc = g_scope; sc; sc = sc->next)
        for (TagEntry *e = sc->tags; e; e = e->next)
            if (e->len == tok->len && cc_strneq(e->name, tok->loc, tok->len))
                return e->ty;
    return 0;
}

static VarScope *push_scope(char *name)
{
    VarScope *vs = cc_alloc(sizeof(VarScope));
    ScopeEntry *e = cc_alloc(sizeof(ScopeEntry));
    e->name = name;
    e->len = cc_strlen(name);
    e->vs = vs;
    e->next = g_scope->vars;
    g_scope->vars = e;
    return vs;
}

static void push_tag_scope(Token *tok, Type *ty)
{
    TagEntry *e = cc_alloc(sizeof(TagEntry));
    e->name = cc_strndup(tok->loc, tok->len);
    e->len = tok->len;
    e->ty = ty;
    e->next = g_scope->tags;
    g_scope->tags = e;
}

static Obj *new_var(char *name, Type *ty)
{
    Obj *var = cc_alloc(sizeof(Obj));
    var->name = name;
    var->ty = ty;
    var->align = ty->align;
    push_scope(name)->var = var;
    return var;
}

static Obj *new_lvar(char *name, Type *ty)
{
    Obj *var = new_var(name, ty);
    var->is_local = 1;
    var->next = g_locals;
    g_locals = var;
    return var;
}

static Obj *new_gvar(char *name, Type *ty)
{
    Obj *var = new_var(name, ty);
    var->next = g_globals;
    var->is_static = 1;
    var->is_definition = 1;
    g_globals = var;
    return var;
}

static Obj *new_anon_gvar(Type *ty)
{
    return new_gvar(new_unique_name(), ty);
}

static Obj *new_string_literal(char *p, Type *ty)
{
    Obj *var = new_anon_gvar(ty);
    var->init_data = p;
    return var;
}

// the element type of a string literal kind
static Type *str_elem_type(int kind)
{
    if (kind == STR_WIDE)
        return cc_target_win ? ty_ushort : ty_int;      // wchar_t: UTF-16 in Windows
    return kind == STR_U16 ? ty_ushort : kind == STR_U32 ? ty_uint : ty_char;
}

// Adjacent string literals become one token. Plain parts take the kind of the
// others (re-read from the source as wide text); two different wide kinds, or
// u8 with a wide one, cannot be joined.
static Token *join_strings(Token **rest, Token *tok)
{
    int kind = STR_PLAIN, count = 0;
    Token *t = tok;
    for (; t->kind == TK_STR; t = t->next, count++) {
        int k = t->str_kind;
        if (k == STR_PLAIN || k == kind)
            continue;
        if (kind != STR_PLAIN)
            cc_error_tok(t, "строки разных видов (u8, u, U, L) не соединяются");
        kind = k;
    }
    *rest = t;
    if (count == 1)
        return tok;
    int size = cc_str_elem_size(kind), total = 1;
    Token **parts = cc_alloc(sizeof(Token *) * (size_t)count);
    t = tok;
    for (int i = 0; i < count; i++, t = t->next) {
        parts[i] = t;
        if (t->str_kind != kind) {          // a plain part, read again as this kind
            parts[i] = cc_alloc(sizeof(Token));
            *parts[i] = *t;
            cc_widen_string(parts[i], kind);
        }
        total += parts[i]->str_len - 1;
    }
    Token *r = cc_alloc(sizeof(Token));
    *r = *tok;
    r->str = cc_alloc((size_t)total * (size_t)size);
    r->str_len = total;
    r->str_kind = kind;
    int pos = 0;
    for (int i = 0; i < count; i++) {
        int bytes = (parts[i]->str_len - 1) * size;
        for (int b = 0; b < bytes; b++)
            r->str[pos++] = parts[i]->str[b];
    }
    for (int b = 0; b < size; b++)
        r->str[pos++] = 0;
    return r;
}

// element i of a decoded literal, with the sign its type has
static i64 str_elem(Token *s, int i)
{
    const u8 *p = (const u8 *)s->str;
    switch (cc_str_elem_size(s->str_kind)) {
    case 2: return (i64)(p[2 * i] | p[2 * i + 1] << 8);
    case 4: {
        u32 v = (u32)p[4 * i] | (u32)p[4 * i + 1] << 8 | (u32)p[4 * i + 2] << 16 | (u32)p[4 * i + 3] << 24;
        return s->str_kind == STR_WIDE ? (i64)(int)v : (i64)v;
    }
    default: return (i64)(char)p[i];
    }
}

// ---- forward declarations ----

static int is_typename(Token *tok);
static Type *declspec(Token **rest, Token *tok, VarAttr *attr);
static Type *typename_(Token **rest, Token *tok);
static Type *enum_specifier(Token **rest, Token *tok);
static Type *type_suffix(Token **rest, Token *tok, Type *ty);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr);
static void initializer2(Token **rest, Token *tok, Initializer *init);
static Initializer *initializer(Token **rest, Token *tok, Type *ty, Type **new_ty);
static Node *lvar_initializer(Token **rest, Token *tok, Obj *var);
static void gvar_initializer(Token **rest, Token *tok, Obj *var);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static i64 eval(Node *node);
static i64 eval2(Node *node, char ***label);
static i64 eval_rval(Node *node, char ***label);
static int is_const_expr(Node *node);
static Node *assign(Token **rest, Token *tok);
static Node *logor(Token **rest, Token *tok);
static double eval_double(Node *node);
static Node *conditional(Token **rest, Token *tok);
static Node *logand(Token **rest, Token *tok);
static Node *bitor(Token **rest, Token *tok);
static Node *bitxor(Token **rest, Token *tok);
static Node *bitand(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *shift(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *new_add(Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(Node *lhs, Node *rhs, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *cast(Token **rest, Token *tok);
static Member *get_struct_member(Type *ty, Token *tok);
static Type *struct_decl(Token **rest, Token *tok);
static Type *union_decl(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *funcall(Token **rest, Token *tok, Node *node);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);
static Token *parse_typedef(Token *tok, Type *basety);
static int is_function(Token *tok);
static Token *function(Token *tok, Type *basety, VarAttr *attr);
static Token *global_variable(Token *tok, Type *basety, VarAttr *attr);

static int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static Type *find_typedef(Token *tok)
{
    if (tok->kind == TK_IDENT) {
        VarScope *vs = find_var(tok);
        if (vs)
            return vs->type_def;
    }
    return 0;
}

static void push_tag_scope(Token *tok, Type *ty);

// declspec = ("void" | "_Bool" | "char" | "short" | "int" | "long" | "float" | "double"
//             | "typedef" | "static" | "extern" | "inline" | "signed" | "unsigned"
//             | struct-decl | union-decl | typedef-name | enum-specifier
//             | "const" | "volatile" | "auto" | "register" | "restrict" | "_Noreturn"
//             | "_Alignas" ("(" typename | const-expr ")"))+
static Type *declspec(Token **rest, Token *tok, VarAttr *attr)
{
    enum {
        VOID = 1 << 0, BOOL = 1 << 2, CHAR = 1 << 4, SHORT = 1 << 6, INT = 1 << 8,
        LONG = 1 << 10, FLOAT = 1 << 12, DOUBLE = 1 << 14, OTHER = 1 << 16,
        SIGNED = 1 << 17, UNSIGNED = 1 << 18,
    };
    Type *ty = ty_int;
    int counter = 0, atomic = 0;

    while (is_typename(tok)) {
        if (cc_equal(tok, "typedef") || cc_equal(tok, "static") || cc_equal(tok, "extern") ||
            cc_equal(tok, "inline")) {
            if (!attr)
                cc_error_tok(tok, "здесь класс памяти не допускается");
            if (cc_equal(tok, "typedef")) attr->is_typedef = 1;
            else if (cc_equal(tok, "static")) attr->is_static = 1;
            else if (cc_equal(tok, "extern")) attr->is_extern = 1;
            else attr->is_inline = 1;
            if (attr->is_typedef && attr->is_static + attr->is_extern + attr->is_inline > 0)
                cc_error_tok(tok, "typedef нельзя сочетать со static, extern, inline");
            tok = tok->next;
            continue;
        }
        if (cc_equal(tok, "_Thread_local") || cc_equal(tok, "thread_local") || cc_equal(tok, "__thread")) {
            if (!attr)
                cc_error_tok(tok, "здесь _Thread_local не допускается");
            if (!cc_target_win)
                cc_error_tok(tok, "_Thread_local есть только в программах для Windows");
            attr->is_tls = 1;
            tok = tok->next;
            continue;
        }
        if (cc_equal(tok, "_Atomic")) {
            if (cc_equal(tok->next, "(")) {         // _Atomic(int): a type
                if (counter)
                    break;
                tok = cc_skip(tok->next, "(");
                ty = typename_(&tok, tok);
                tok = cc_skip(tok, ")");
                counter += OTHER;
            } else {
                tok = tok->next;                    // _Atomic int: a qualifier
            }
            atomic = 1;
            continue;
        }
        // typeof(expression or type): the type it has; the expression is not computed (C23, GNU)
        if (cc_equal(tok, "typeof") || cc_equal(tok, "__typeof__") || cc_equal(tok, "__typeof") ||
            cc_equal(tok, "typeof_unqual") || cc_equal(tok, "__typeof_unqual__") || cc_equal(tok, "__typeof_unqual")) {
            if (counter)
                break;
            tok = cc_skip(tok->next, "(");
            if (is_typename(tok)) {
                ty = typename_(&tok, tok);
            } else {
                Node *e = expr(&tok, tok);
                add_type(e);
                ty = e->ty;
            }
            tok = cc_skip(tok, ")");
            counter += OTHER;
            continue;
        }
        if (cc_equal(tok, "const") || cc_equal(tok, "volatile") || cc_equal(tok, "auto") ||
            cc_equal(tok, "register") || cc_equal(tok, "restrict") || cc_equal(tok, "__restrict") ||
            cc_equal(tok, "_Noreturn")) {
            tok = tok->next;
            continue;
        }
        // __declspec(...) as in Microsoft C: dllexport puts the function into the
        // export table of a .dll/.exe, align(n) is _Alignas(n), naked leaves a function
        // without its frame; thread is refused (it would be one variable for all threads);
        // anything else (dllimport, noreturn, selectany...) is skipped
        if (cc_equal(tok, "__declspec")) {
            tok = cc_skip(tok->next, "(");
            if (attr && cc_equal(tok, "dllexport"))
                attr->is_export = 1;
            if (attr && cc_equal(tok, "naked"))
                attr->is_naked = 1;
            if (cc_equal(tok, "thread")) {
                if (!attr)
                    cc_error_tok(tok, "здесь __declspec(thread) не допускается");
                if (!cc_target_win)
                    cc_error_tok(tok, "__declspec(thread) есть только в программах для Windows");
                attr->is_tls = 1;
            }
            if (attr && cc_equal(tok, "selectany"))
                attr->is_selectany = 1;
            if (cc_equal(tok, "align") && cc_equal(tok->next, "(")) {
                if (!attr)
                    cc_error_tok(tok, "__declspec(align) здесь не допускается");
                Token *n = tok->next->next;
                attr->align = (int)eval(conditional(&n, n));
            }
            for (int depth = 1; depth > 0; tok = tok->next) {
                if (tok->kind == TK_EOF)
                    cc_error_tok(tok, "не закрыта скобка __declspec(");
                if (cc_equal(tok, "(")) depth++;
                else if (cc_equal(tok, ")")) depth--;
            }
            continue;
        }
        if (cc_equal(tok, "_Alignas")) {
            if (!attr)
                cc_error_tok(tok, "_Alignas здесь не допускается");
            tok = cc_skip(tok->next, "(");
            if (is_typename(tok))
                attr->align = typename_(&tok, tok)->align;
            else
                attr->align = (int)eval(conditional(&tok, tok));
            tok = cc_skip(tok, ")");
            continue;
        }

        Type *ty2 = find_typedef(tok);
        if (cc_equal(tok, "struct") || cc_equal(tok, "union") || cc_equal(tok, "enum") || ty2) {
            if (counter)
                break;
            if (cc_equal(tok, "struct"))
                ty = struct_decl(&tok, tok->next);
            else if (cc_equal(tok, "union"))
                ty = union_decl(&tok, tok->next);
            else if (cc_equal(tok, "enum"))
                ty = enum_specifier(&tok, tok->next);
            else {
                ty = ty2;
                tok = tok->next;
            }
            counter += OTHER;
            continue;
        }

        if (cc_equal(tok, "void")) counter += VOID;
        else if (cc_equal(tok, "_Bool")) counter += BOOL;
        else if (cc_equal(tok, "char")) counter += CHAR;
        else if (cc_equal(tok, "short")) counter += SHORT;
        else if (cc_equal(tok, "int")) counter += INT;
        else if (cc_equal(tok, "long")) counter += LONG;
        else if (cc_equal(tok, "float")) counter += FLOAT;
        else if (cc_equal(tok, "double")) counter += DOUBLE;
        else if (cc_equal(tok, "signed")) counter |= SIGNED;
        else if (cc_equal(tok, "unsigned")) counter |= UNSIGNED;
        else cc_error_tok(tok, "внутренняя ошибка");

        switch (counter) {
        case VOID: ty = ty_void; break;
        case BOOL: ty = ty_bool; break;
        case CHAR: case SIGNED + CHAR: ty = ty_char; break;
        case UNSIGNED + CHAR: ty = ty_uchar; break;
        case SHORT: case SHORT + INT: case SIGNED + SHORT: case SIGNED + SHORT + INT: ty = ty_short; break;
        case UNSIGNED + SHORT: case UNSIGNED + SHORT + INT: ty = ty_ushort; break;
        case INT: case SIGNED: case SIGNED + INT: ty = ty_int; break;
        case UNSIGNED: case UNSIGNED + INT: ty = ty_uint; break;
        // Windows: a lone long is 4 bytes (as int), long long is 8
        case LONG: case LONG + INT: case SIGNED + LONG: case SIGNED + LONG + INT:
            ty = cc_target_win ? ty_int : ty_long; break;
        case UNSIGNED + LONG: case UNSIGNED + LONG + INT:
            ty = cc_target_win ? ty_uint : ty_ulong; break;
        case LONG + LONG: case LONG + LONG + INT: case SIGNED + LONG + LONG:
        case SIGNED + LONG + LONG + INT: ty = ty_long; break;
        case UNSIGNED + LONG + LONG:
        case UNSIGNED + LONG + LONG + INT: ty = ty_ulong; break;
        case FLOAT: ty = ty_float; break;
        case DOUBLE: case LONG + DOUBLE: ty = ty_double; break;
        default: cc_error_tok(tok, "неверный тип");
        }
        tok = tok->next;
    }
    if (atomic) {
        ty = copy_type(ty);
        ty->is_atomic = 1;
    }
    *rest = tok;
    return ty;
}

// func-params = ("void" | param ("," param)* ("," "...")?)? ")"
static Type *func_params(Token **rest, Token *tok, Type *ty)
{
    if (cc_equal(tok, "void") && cc_equal(tok->next, ")")) {
        *rest = tok->next->next;
        return func_type(ty);
    }
    Type head = { 0 };
    Type *cur = &head;
    int is_variadic = 0;
    g_param_depth++;
    while (!cc_equal(tok, ")")) {
        if (cur != &head)
            tok = cc_skip(tok, ",");
        if (cc_equal(tok, "...")) {
            is_variadic = 1;
            tok = tok->next;
            cc_skip(tok, ")");
            break;
        }
        Type *ty2 = declspec(&tok, tok, 0);
        ty2 = declarator(&tok, tok, ty2);
        Token *name = ty2->name;
        if (ty2->kind == TY_ARRAY || ty2->kind == TY_VLA) {     // int a[n][m]: a pointer to the rows
            ty2 = pointer_to(ty2->base);
            ty2->name = name;
        } else if (ty2->kind == TY_FUNC) {
            ty2 = pointer_to(ty2);
            ty2->name = name;
        }
        cur = cur->next = copy_type(ty2);
    }
    g_param_depth--;
    if (cur == &head)
        is_variadic = 1;        // "f()" accepts anything
    ty = func_type(ty);
    ty->params = head.next;
    ty->is_variadic = is_variadic;
    *rest = tok->next;
    return ty;
}

// array-dimensions = ("static" | "restrict")* const-expr? "]" type-suffix
// A parameter's array size that names something not known yet (an earlier parameter):
// its tokens are read again in the function's body. 1 if every name in it is a constant here.
static int size_is_known(Token *tok)
{
    for (int depth = 0; tok->kind != TK_EOF; tok = tok->next) {
        if (cc_equal(tok, "[") || cc_equal(tok, "("))
            depth++;
        else if (cc_equal(tok, ")"))
            depth--;
        else if (cc_equal(tok, "]") && depth-- == 0)
            return 1;
        else if (tok->kind == TK_IDENT && !is_typename(tok) && !cc_equal(tok, "sizeof") &&
                 !cc_equal(tok, "_Alignof")) {
            VarScope *vs = find_var(tok);
            if (!vs || !vs->enum_ty)
                return 0;
        }
    }
    return 1;
}

static Type *array_dimensions(Token **rest, Token *tok, Type *ty)
{
    while (cc_equal(tok, "static") || cc_equal(tok, "restrict") || cc_equal(tok, "__restrict") ||
           cc_equal(tok, "const") || cc_equal(tok, "volatile"))
        tok = tok->next;
    if (cc_equal(tok, "]")) {
        ty = type_suffix(rest, tok->next, ty);
        return array_of(ty, -1);
    }
    if (cc_equal(tok, "*") && cc_equal(tok->next, "]")) {      // int a[*]: a VLA of a prototype
        ty = type_suffix(rest, tok->next->next, ty);
        return vla_of(ty, 0, 0);
    }
    if (g_param_depth > 0 && !size_is_known(tok)) {
        Token *start = tok;
        for (int depth = 0; !(cc_equal(tok, "]") && depth == 0); tok = tok->next) {
            if (tok->kind == TK_EOF)
                cc_error_tok(start, "не закрыта скобка [");
            if (cc_equal(tok, "[") || cc_equal(tok, "(")) depth++;
            if (cc_equal(tok, "]") || cc_equal(tok, ")")) depth--;
        }
        ty = type_suffix(rest, tok->next, ty);
        return vla_of(ty, 0, start);
    }
    Node *e = conditional(&tok, tok);
    tok = cc_skip(tok, "]");
    ty = type_suffix(rest, tok, ty);
    if (ty->kind == TY_VLA || !is_const_expr(e)) {
        if (!g_current_fn && g_param_depth == 0)
            cc_error_tok(e->tok, "массив переменной длины бывает только внутри функции");
        return vla_of(ty, e, 0);
    }
    i64 n = eval(e);
    if (n < 0)
        cc_error_tok(e->tok, "размер массива меньше нуля (%lld)", n);
    return array_of(ty, (int)n);
}

static Type *type_suffix(Token **rest, Token *tok, Type *ty)
{
    if (cc_equal(tok, "("))
        return func_params(rest, tok->next, ty);
    if (cc_equal(tok, "["))
        return array_dimensions(rest, tok->next, ty);
    *rest = tok;
    return ty;
}

static Type *pointers(Token **rest, Token *tok, Type *ty)
{
    while (cc_consume(&tok, tok, "*")) {
        ty = pointer_to(ty);
        while (cc_equal(tok, "const") || cc_equal(tok, "volatile") || cc_equal(tok, "restrict") ||
               cc_equal(tok, "__restrict") || cc_equal(tok, "_Atomic") || cc_equal(tok, "__ptr64") ||
               cc_equal(tok, "__unaligned"))
            tok = tok->next;
    }
    *rest = tok;
    return ty;
}

// declarator = pointers ("(" ident ")" | "(" declarator ")" | ident) type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty)
{
    ty = pointers(&tok, tok, ty);
    if (cc_equal(tok, "(") && !is_typename(tok->next) && !cc_equal(tok->next, ")")) {
        Token *start = tok;
        Type dummy = { 0 };
        declarator(&tok, start->next, &dummy);
        tok = cc_skip(tok, ")");
        ty = type_suffix(rest, tok, ty);
        return declarator(&tok, start->next, ty);
    }
    Token *name = 0;
    Token *name_pos = tok;
    if (tok->kind == TK_IDENT) {
        name = tok;
        tok = tok->next;
    }
    ty = type_suffix(rest, tok, ty);
    ty->name = name;
    (void)name_pos;
    return ty;
}

// abstract-declarator = pointers ("(" abstract-declarator ")")? type-suffix
static Type *abstract_declarator(Token **rest, Token *tok, Type *ty)
{
    ty = pointers(&tok, tok, ty);
    if (cc_equal(tok, "(") && !is_typename(tok->next) && !cc_equal(tok->next, ")")) {
        Token *start = tok;
        Type dummy = { 0 };
        abstract_declarator(&tok, start->next, &dummy);
        tok = cc_skip(tok, ")");
        ty = type_suffix(rest, tok, ty);
        return abstract_declarator(&tok, start->next, ty);
    }
    return type_suffix(rest, tok, ty);
}

static Type *typename_(Token **rest, Token *tok)
{
    Type *ty = declspec(&tok, tok, 0);
    return abstract_declarator(rest, tok, ty);
}

static int is_end(Token *tok)
{
    return cc_equal(tok, "}") || (cc_equal(tok, ",") && cc_equal(tok->next, "}"));
}

static int consume_end(Token **rest, Token *tok)
{
    if (cc_equal(tok, "}")) {
        *rest = tok->next;
        return 1;
    }
    if (cc_equal(tok, ",") && cc_equal(tok->next, "}")) {
        *rest = tok->next->next;
        return 1;
    }
    return 0;
}

// enum-specifier = ident? "{" enum-list? "}"
static Type *enum_specifier(Token **rest, Token *tok)
{
    Type *ty = enum_type();
    Token *tag = 0;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }
    if (tag && !cc_equal(tok, "{")) {
        Type *t = find_tag(tag);
        if (!t)
            cc_error_tok(tag, "неизвестный enum");
        if (t->kind != TY_ENUM)
            cc_error_tok(tag, "это не enum");
        *rest = tok;
        return t;
    }
    tok = cc_skip(tok, "{");
    int i = 0, val = 0;
    while (!consume_end(rest, tok)) {
        if (i++ > 0)
            tok = cc_skip(tok, ",");
        char *name = get_ident(tok);
        tok = tok->next;
        if (cc_equal(tok, "="))
            val = (int)eval(conditional(&tok, tok->next));
        VarScope *vs = push_scope(name);
        vs->enum_ty = ty;
        vs->enum_val = val++;
    }
    if (tag)
        push_tag_scope(tag, ty);
    return ty;
}

// declaration = declspec (declarator ("=" initializer)? ("," declarator ("=" initializer)?)*)? ";"
// _Static_assert(condition, "text"); static_assert too, and without the text (C23)
static int is_static_assert(Token *tok)
{
    return cc_equal(tok, "_Static_assert") || cc_equal(tok, "static_assert");
}

static Token *static_assertion(Token *tok)
{
    Token *start = tok;
    tok = cc_skip(tok->next, "(");
    i64 ok = eval(conditional(&tok, tok));
    const char *text = 0;
    if (cc_consume(&tok, tok, ",")) {
        if (tok->kind != TK_STR)
            cc_error_tok(tok, "после запятой в _Static_assert нужна строка");
        text = tok->str;
        while (tok->kind == TK_STR)
            tok = tok->next;
    }
    tok = cc_skip(tok, ")");
    tok = cc_skip(tok, ";");
    if (!ok) {
        if (text)
            cc_error_tok(start, "условие _Static_assert ложно: %s", text);
        cc_error_tok(start, "условие _Static_assert ложно");
    }
    return tok;
}

// Between declarators of one declaration: "," -- or the declaration should have ended with ";"
// ("int a = 1 int b" is a ";" forgotten, not a "," missing)
static Token *next_declarator(Token *tok)
{
    if (!cc_equal(tok, ","))
        cc_error_tok(tok, "ожидалось ';'");
    return tok->next;
}

// A VLA somewhere in the type: int a[n], int (*p)[n], int a[3][n]
static int has_vla(Type *ty)
{
    for (; ty; ty = (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_VLA) ? ty->base : 0)
        if (ty->kind == TY_VLA)
            return 1;
    return 0;
}

// The sizes of the VLAs in a type, inner first, into hidden locals: n * the size of an element
static Node *compute_vla_size(Type *ty, Token *tok)
{
    Node *node = new_node(ND_NULL_EXPR, tok);
    if (ty->base && (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_VLA))
        node = new_binary(ND_COMMA, node, compute_vla_size(ty->base, tok), tok);
    if (ty->kind != TY_VLA)
        return node;
    if (!ty->vla_len && ty->vla_tok) {          // a parameter's size: read now, the parameters are seen
        Token *t = ty->vla_tok;
        ty->vla_len = conditional(&t, t);
    }
    if (!ty->vla_len)
        cc_error_tok(tok, "размер массива [*] здесь неизвестен");
    Node *base_sz = ty->base->kind == TY_VLA ? new_var_node(ty->base->vla_size, tok) : new_ulong(ty->base->size, tok);
    ty->vla_size = new_lvar("", ty_ulong);
    Node *e = new_binary(ND_ASSIGN, new_var_node(ty->vla_size, tok),
                         new_binary(ND_MUL, new_cast(ty->vla_len, ty_ulong), base_sz, tok), tok);
    return new_binary(ND_COMMA, node, e, tok);
}

// var's slot = stack memory of size bytes, aligned to align (a VLA, an over-aligned local)
static Node *indirect_alloc(Obj *var, Node *size, int align, Token *tok)
{
    var->indirect = 1;
    g_current_fn->has_alloca = 1;
    Node *a = new_unary(ND_ALLOCA, size, tok);
    a->val = align < 16 ? 16 : align;
    Node *slot = new_node(ND_VLA_PTR, tok);
    slot->var = var;
    return new_unary(ND_EXPR_STMT, new_binary(ND_ASSIGN, slot, a, tok), tok);
}

static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr)
{
    Node head = { 0 };
    Node *cur = &head;
    int i = 0;
    while (!cc_equal(tok, ";")) {
        if (i++ > 0)
            tok = next_declarator(tok);
        Type *ty = declarator(&tok, tok, basety);
        if (ty->kind == TY_VOID)
            cc_error_tok(tok, "переменная типа void");
        if (!ty->name)
            cc_error_tok(tok, "нет имени переменной");

        if (attr && attr->is_tls && !attr->is_extern)
            attr->is_static = 1;            // thread_local inside a function is static (one per thread)
        if (has_vla(ty)) {
            if (attr && (attr->is_static || attr->is_extern))
                cc_error_tok(ty->name, "массив переменной длины не бывает static или extern");
            cur = cur->next = new_unary(ND_EXPR_STMT, compute_vla_size(ty, ty->name), ty->name);
        }
        if (ty->kind == TY_VLA) {
            if (cc_equal(tok, "="))
                cc_error_tok(tok, "массив переменной длины нельзя инициализировать");
            Obj *var = new_lvar(get_ident(ty->name), ty);
            var->tok = ty->name;
            g_vla_count++;
            cur = cur->next = indirect_alloc(var, new_var_node(ty->vla_size, ty->name),
                                             attr && attr->align > 16 ? attr->align : 16, ty->name);
            continue;
        }

        if (attr && attr->is_static) {
            // static local: a global with a unique name
            Obj *var = new_anon_gvar(ty);
            push_scope(get_ident(ty->name))->var = var;
            var->is_tls = attr->is_tls;
            if (attr->align)
                var->align = attr->align;
            if (cc_equal(tok, "="))
                gvar_initializer(&tok, tok->next, var);
            continue;
        }
        if (attr && attr->is_extern) {
            Obj *var = new_gvar(get_ident(ty->name), ty);
            var->is_definition = 0;
            continue;
        }
        Obj *var = new_lvar(get_ident(ty->name), ty);
        var->tok = ty->name;
        if (attr && attr->align)
            var->align = attr->align;
        if (var->align > 16 && ty->size > 0)    // more than rbp keeps: its memory is found at run time
            cur = cur->next = indirect_alloc(var, new_ulong(ty->size, ty->name), var->align, ty->name);
        if (cc_equal(tok, "=")) {
            Node *e = lvar_initializer(&tok, tok->next, var);
            cur = cur->next = new_unary(ND_EXPR_STMT, e, ty->name);   // the variable's line (the line table)
        }
        if (var->ty->size < 0)
            cc_error_tok(ty->name, "у переменной неполный тип");
        if (var->ty->kind == TY_VOID)
            cc_error_tok(ty->name, "переменная типа void");
    }
    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

// ---- initializers ----

static Token *skip_excess_element(Token *tok)
{
    if (cc_equal(tok, "{")) {
        tok = skip_excess_element(tok->next);
        return cc_skip(tok, "}");
    }
    assign(&tok, tok);
    return tok;
}

static Initializer *new_initializer(Type *ty, int is_flexible)
{
    Initializer *init = cc_alloc(sizeof(Initializer));
    init->ty = ty;
    if (ty->kind == TY_ARRAY) {
        if (is_flexible && ty->size < 0) {
            init->is_flexible = 1;
            return init;
        }
        init->children = cc_alloc(sizeof(Initializer *) * (size_t)(ty->array_len > 0 ? ty->array_len : 1));
        for (int i = 0; i < ty->array_len; i++)
            init->children[i] = new_initializer(ty->base, 0);
        return init;
    }
    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
        int len = 0;
        for (Member *m = ty->members; m; m = m->next)
            len++;
        init->children = cc_alloc(sizeof(Initializer *) * (size_t)(len ? len : 1));
        for (Member *m = ty->members; m; m = m->next) {
            if (is_flexible && ty->is_flexible && !m->next) {
                Initializer *child = cc_alloc(sizeof(Initializer));
                child->ty = m->ty;
                child->is_flexible = 1;
                int idx = 0;
                for (Member *k = ty->members; k != m; k = k->next)
                    idx++;
                init->children[idx] = child;
            } else {
                int idx = 0;
                for (Member *k = ty->members; k != m; k = k->next)
                    idx++;
                init->children[idx] = new_initializer(m->ty, 0);
            }
        }
        return init;
    }
    return init;
}

static int member_index(Type *ty, Member *mem)
{
    int i = 0;
    for (Member *m = ty->members; m != mem; m = m->next)
        i++;
    return i;
}

static void string_initializer(Token **rest, Token *tok, Initializer *init)
{
    Token *s = join_strings(rest, tok);
    if (init->ty->base->size != cc_str_elem_size(s->str_kind))
        cc_error_tok(tok, "массив такого типа нельзя задать этой строкой (подойдёт %s)",
                     init->ty->base->size == 4 ? "L\"\" или U\"\"" : init->ty->base->size == 2 ? "u\"\"" : "обычная строка или u8\"\"");
    if (init->is_flexible)
        *init = *new_initializer(array_of(init->ty->base, s->str_len), 0);
    int len = init->ty->array_len < s->str_len ? init->ty->array_len : s->str_len;
    for (int i = 0; i < len; i++)
        init->children[i]->expr = new_num(s->str_kind == STR_PLAIN || s->str_kind == STR_U8
                                          ? (unsigned char)s->str[i] : str_elem(s, i), tok);
}

static void array_designator(Token **rest, Token *tok, Type *ty, int *begin, int *end)
{
    *begin = (int)eval(conditional(&tok, tok->next));
    if (*begin >= ty->array_len && ty->array_len >= 0)
        cc_error_tok(tok, "индекс за пределами массива");
    *end = *begin;
    *rest = cc_skip(tok, "]");
}

static Member *struct_designator(Token **rest, Token *tok, Type *ty)
{
    Token *start = tok;
    tok = cc_skip(tok, ".");
    if (tok->kind != TK_IDENT)
        cc_error_tok(tok, "ожидалось имя поля");
    Member *m = get_struct_member(ty, tok);
    if (!m)
        cc_error_tok(start, "нет такого поля");
    // a field of an unnamed struct or union inside: that member, and the same ".name" goes on in it
    *rest = m->name ? tok->next : start;
    return m;
}

static void struct_initializer1(Token **rest, Token *tok, Initializer *init);
static void array_initializer1(Token **rest, Token *tok, Initializer *init);

static int count_array_init_elements(Token *tok, Type *ty)
{
    Initializer *dummy = new_initializer(ty->base, 0);
    int i = 0, max = 0;
    while (!consume_end(&tok, tok)) {
        if (i > 0)
            tok = cc_skip(tok, ",");
        if (cc_equal(tok, "[")) {
            i = (int)eval(conditional(&tok, tok->next));
            tok = cc_skip(tok, "]");
            if (cc_equal(tok, "="))
                tok = tok->next;
        }
        initializer2(&tok, tok, dummy);
        i++;
        if (i > max)
            max = i;
    }
    return max;
}

static void designation(Token **rest, Token *tok, Initializer *init)
{
    if (cc_equal(tok, "[")) {
        if (init->ty->kind != TY_ARRAY)
            cc_error_tok(tok, "это не массив");
        int begin, end;
        array_designator(&tok, tok, init->ty, &begin, &end);
        if (cc_equal(tok, "[") || cc_equal(tok, "."))
            designation(&tok, tok, init->children[begin]);
        else {
            if (cc_equal(tok, "="))
                tok = tok->next;
            initializer2(&tok, tok, init->children[begin]);
        }
        *rest = tok;
        return;
    }
    if (cc_equal(tok, ".") && (init->ty->kind == TY_STRUCT || init->ty->kind == TY_UNION)) {
        Member *m = struct_designator(&tok, tok, init->ty);
        Initializer *child = init->children[member_index(init->ty, m)];
        if (init->ty->kind == TY_UNION)
            init->expr = (Node *)m;             // the member it is, as union_initializer marks it
        if (cc_equal(tok, "[") || cc_equal(tok, "."))
            designation(&tok, tok, child);
        else {
            if (cc_equal(tok, "="))
                tok = tok->next;
            initializer2(&tok, tok, child);
        }
        *rest = tok;
        return;
    }
    if (cc_equal(tok, "="))
        tok = tok->next;
    initializer2(rest, tok, init);
}

static void array_initializer1(Token **rest, Token *tok, Initializer *init)
{
    tok = cc_skip(tok, "{");
    if (init->is_flexible) {
        int len = count_array_init_elements(tok, init->ty);
        *init = *new_initializer(array_of(init->ty->base, len), 0);
    }
    int first = 1;
    for (int i = 0; !consume_end(rest, tok); i++) {
        if (!first)
            tok = cc_skip(tok, ",");
        first = 0;
        if (cc_equal(tok, "[")) {
            int begin, end;
            array_designator(&tok, tok, init->ty, &begin, &end);
            if (cc_equal(tok, "[") || cc_equal(tok, "."))
                designation(&tok, tok, init->children[begin]);
            else {
                if (cc_equal(tok, "="))
                    tok = tok->next;
                initializer2(&tok, tok, init->children[begin]);
            }
            i = begin;
            continue;
        }
        if (i < init->ty->array_len)
            initializer2(&tok, tok, init->children[i]);
        else
            tok = skip_excess_element(tok);
    }
}

static void array_initializer2(Token **rest, Token *tok, Initializer *init, int i)
{
    if (init->is_flexible) {
        int len = count_array_init_elements(tok, init->ty);
        *init = *new_initializer(array_of(init->ty->base, len), 0);
    }
    for (; i < init->ty->array_len && !is_end(tok); i++) {
        Token *start = tok;
        if (i > 0)
            tok = cc_skip(tok, ",");
        if (cc_equal(tok, "[") || cc_equal(tok, ".")) {
            *rest = start;
            return;
        }
        initializer2(&tok, tok, init->children[i]);
    }
    *rest = tok;
}

// The next member an initializer list fills: unnamed bit fields are passed over.
static Member *init_member(Member *m)
{
    while (m && m->is_bitfield && !m->name)
        m = m->next;
    return m;
}

static void struct_initializer1(Token **rest, Token *tok, Initializer *init)
{
    tok = cc_skip(tok, "{");
    Member *mem = init_member(init->ty->members);
    int first = 1;
    while (!consume_end(rest, tok)) {
        if (!first)
            tok = cc_skip(tok, ",");
        first = 0;
        if (cc_equal(tok, ".")) {
            mem = struct_designator(&tok, tok, init->ty);
            Initializer *child = init->children[member_index(init->ty, mem)];
            if (cc_equal(tok, "[") || cc_equal(tok, "."))
                designation(&tok, tok, child);
            else {
                if (cc_equal(tok, "="))
                    tok = tok->next;
                initializer2(&tok, tok, child);
            }
            mem = init_member(mem->next);
            continue;
        }
        if (mem) {
            initializer2(&tok, tok, init->children[member_index(init->ty, mem)]);
            mem = init_member(mem->next);
        } else {
            tok = skip_excess_element(tok);
        }
    }
}

static void struct_initializer2(Token **rest, Token *tok, Initializer *init, Member *mem)
{
    int first = 1;
    for (mem = init_member(mem); mem && !is_end(tok); mem = init_member(mem->next)) {
        Token *start = tok;
        if (!first)
            tok = cc_skip(tok, ",");
        first = 0;
        if (cc_equal(tok, "[") || cc_equal(tok, ".")) {
            *rest = start;
            return;
        }
        initializer2(&tok, tok, init->children[member_index(init->ty, mem)]);
    }
    *rest = tok;
}

static void union_initializer(Token **rest, Token *tok, Initializer *init)
{
    if (cc_equal(tok, "{") && cc_equal(tok->next, ".")) {
        Member *m = struct_designator(&tok, tok->next, init->ty);
        init->children[member_index(init->ty, m)]->ty = m->ty;
        Initializer *child = init->children[member_index(init->ty, m)];
        if (cc_equal(tok, ".") || cc_equal(tok, "[")) {
            designation(&tok, tok, child);
        } else {
            if (cc_equal(tok, "="))
                tok = tok->next;
            initializer2(&tok, tok, child);
        }
        // remember which member was initialised
        init->expr = (Node *)m;
        *rest = cc_skip(tok, "}");
        return;
    }
    Member *first = init_member(init->ty->members);
    init->expr = (Node *)first;
    Initializer *child = init->children[first ? member_index(init->ty, first) : 0];
    if (cc_equal(tok, "{")) {
        initializer2(&tok, tok->next, child);
        cc_consume(&tok, tok, ",");
        *rest = cc_skip(tok, "}");
    } else {
        initializer2(rest, tok, child);
    }
}

static void initializer2(Token **rest, Token *tok, Initializer *init)
{
    if (init->ty->kind == TY_ARRAY && tok->kind == TK_STR) {
        string_initializer(rest, tok, init);
        return;
    }
    if (init->ty->kind == TY_ARRAY) {
        if (cc_equal(tok, "{"))
            array_initializer1(rest, tok, init);
        else
            array_initializer2(rest, tok, init, 0);
        return;
    }
    if (init->ty->kind == TY_STRUCT) {
        if (cc_equal(tok, "{")) {
            struct_initializer1(rest, tok, init);
            return;
        }
        Node *e = assign(rest, tok);
        add_type(e);
        if (e->ty->kind == TY_STRUCT) {
            init->expr = e;
            return;
        }
        struct_initializer2(rest, tok, init, init->ty->members);
        return;
    }
    if (init->ty->kind == TY_UNION) {
        if (!cc_equal(tok, "{")) {               // union u = other_union;
            Node *e = assign(rest, tok);
            add_type(e);
            if (e->ty->kind == TY_UNION) {
                init->copy = e;
                return;
            }
        }
        union_initializer(rest, tok, init);
        return;
    }
    if (cc_equal(tok, "{")) {
        initializer2(&tok, tok->next, init);   // int x = {3};
        *rest = cc_skip(tok, "}");
        return;
    }
    init->expr = assign(rest, tok);
}

static Type *copy_struct_type(Type *ty)
{
    ty = copy_type(ty);
    Member head = { 0 };
    Member *cur = &head;
    for (Member *m = ty->members; m; m = m->next) {
        Member *m2 = cc_alloc(sizeof(Member));
        *m2 = *m;
        cur = cur->next = m2;
    }
    ty->members = head.next;
    return ty;
}

static Initializer *initializer(Token **rest, Token *tok, Type *ty, Type **new_ty)
{
    Initializer *init = new_initializer(ty, 1);
    initializer2(rest, tok, init);
    if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) && ty->is_flexible) {
        ty = copy_struct_type(ty);
        Member *m = ty->members;
        int idx = 0;
        while (m->next) {
            m = m->next;
            idx++;
        }
        m->ty = init->children[idx]->ty;
        ty->size += m->ty->size;
        *new_ty = ty;
        return init;
    }
    *new_ty = init->ty;
    return init;
}

static Node *init_desg_expr(InitDesg *desg, Token *tok)
{
    if (desg->var)
        return new_var_node(desg->var, tok);
    if (desg->member) {
        Node *n = new_unary(ND_MEMBER, init_desg_expr(desg->next, tok), tok);
        n->member = desg->member;
        return n;
    }
    Node *lhs = init_desg_expr(desg->next, tok);
    Node *rhs = new_num(desg->idx, tok);
    return new_unary(ND_DEREF, new_add(lhs, rhs, tok), tok);
}

static Node *create_lvar_init(Initializer *init, Type *ty, InitDesg *desg, Token *tok)
{
    if (ty->kind == TY_ARRAY) {
        Node *node = new_node(ND_NULL_EXPR, tok);
        for (int i = 0; i < ty->array_len; i++) {
            InitDesg desg2 = { desg, i };
            Node *rhs = create_lvar_init(init->children[i], ty->base, &desg2, tok);
            node = new_binary(ND_COMMA, node, rhs, tok);
        }
        return node;
    }
    if (ty->kind == TY_STRUCT && !init->expr) {
        Node *node = new_node(ND_NULL_EXPR, tok);
        int idx = 0;
        for (Member *m = ty->members; m; m = m->next, idx++) {
            InitDesg desg2 = { desg, 0, m };
            Node *rhs = create_lvar_init(init->children[idx], m->ty, &desg2, tok);
            node = new_binary(ND_COMMA, node, rhs, tok);
        }
        return node;
    }
    if (ty->kind == TY_UNION && init->copy)
        return new_binary(ND_ASSIGN, init_desg_expr(desg, tok), init->copy, tok);
    if (ty->kind == TY_UNION) {
        Member *m = init->expr ? (Member *)init->expr : ty->members;
        if (!m)
            return new_node(ND_NULL_EXPR, tok);
        InitDesg desg2 = { desg, 0, m };
        return create_lvar_init(init->children[member_index(ty, m)], m->ty, &desg2, tok);
    }
    if (!init->expr)
        return new_node(ND_NULL_EXPR, tok);
    Node *lhs = init_desg_expr(desg, tok);
    return new_binary(ND_ASSIGN, lhs, init->expr, tok);
}

// A local initializer zeroes the whole variable first, then assigns
// the given elements. A scalar with a value is assigned whole: no zeroing
// (it was a "rep stosb" before every "int x = ...").
static Node *lvar_initializer(Token **rest, Token *tok, Obj *var)
{
    Initializer *init = initializer(rest, tok, var->ty, &var->ty);
    InitDesg desg = { 0, 0, 0, var };
    Node *lhs;
    if (var->ty->kind != TY_ARRAY && var->ty->kind != TY_STRUCT && var->ty->kind != TY_UNION && init->expr)
        lhs = new_node(ND_NULL_EXPR, tok);
    else {
        lhs = new_node(ND_MEMZERO, tok);
        lhs->var = var;
    }
    Node *rhs = create_lvar_init(init, var->ty, &desg, tok);
    return new_binary(ND_COMMA, lhs, rhs, tok);
}

static void write_buf(char *buf, u64 val, int sz)
{
    for (int i = 0; i < sz; i++)
        buf[i] = (char)(val >> (8 * i));
}

// A bit field's value into the bits of its unit, the other bits kept.
static void write_bitfield(Initializer *init, Member *m, char *buf, int offset)
{
    if (!init->expr || !m->bit_width)
        return;
    u64 val = (u64)eval(init->expr);
    if (m->ty->kind == TY_BOOL)
        val = val != 0;
    char *p = buf + offset + m->offset;
    u64 unit = 0;
    for (int i = 0; i < m->ty->size; i++)
        unit |= (u64)(u8)p[i] << (8 * i);
    u64 mask = m->bit_width == 64 ? ~0ull : (1ull << m->bit_width) - 1;
    unit = (unit & ~(mask << m->bit_offset)) | ((val & mask) << m->bit_offset);
    write_buf(p, unit, m->ty->size);
}

static Reloc *write_gvar_data(Reloc *cur, Initializer *init, Type *ty, char *buf, int offset)
{
    if (ty->kind == TY_ARRAY) {
        int sz = ty->base->size;
        for (int i = 0; i < ty->array_len; i++)
            cur = write_gvar_data(cur, init->children[i], ty->base, buf, offset + sz * i);
        return cur;
    }
    if (ty->kind == TY_STRUCT) {
        int idx = 0;
        for (Member *m = ty->members; m; m = m->next, idx++) {
            if (m->is_bitfield)
                write_bitfield(init->children[idx], m, buf, offset);
            else
                cur = write_gvar_data(cur, init->children[idx], m->ty, buf, offset + m->offset);
        }
        return cur;
    }
    if (ty->kind == TY_UNION) {
        Member *m = init->expr ? (Member *)init->expr : ty->members;
        if (!m)
            return cur;
        if (m->is_bitfield) {
            write_bitfield(init->children[member_index(ty, m)], m, buf, offset);
            return cur;
        }
        return write_gvar_data(cur, init->children[member_index(ty, m)], m->ty, buf, offset + m->offset);
    }
    if (!init->expr)
        return cur;
    if (ty->kind == TY_FLOAT) {
        float f = (float)eval_double(init->expr);
        u32 bits;
        char *pf = (char *)&f;
        char *pb = (char *)&bits;
        for (int i = 0; i < 4; i++) pb[i] = pf[i];
        write_buf(buf + offset, bits, 4);
        return cur;
    }
    if (ty->kind == TY_DOUBLE) {
        double d = eval_double(init->expr);
        u64 bits;
        char *pf = (char *)&d;
        char *pb = (char *)&bits;
        for (int i = 0; i < 8; i++) pb[i] = pf[i];
        write_buf(buf + offset, bits, 8);
        return cur;
    }
    char **label = 0;
    i64 val = eval2(init->expr, &label);
    if (!label) {
        write_buf(buf + offset, (u64)val, ty->size);
        return cur;
    }
    Reloc *rel = cc_alloc(sizeof(Reloc));
    rel->offset = offset;
    rel->label = *label;
    rel->addend = val;
    cur->next = rel;
    return cur->next;
}

static void gvar_initializer(Token **rest, Token *tok, Obj *var)
{
    Initializer *init = initializer(rest, tok, var->ty, &var->ty);
    Reloc head = { 0 };
    char *buf = cc_alloc((size_t)(var->ty->size > 0 ? var->ty->size : 1));
    write_gvar_data(&head, init, var->ty, buf, 0);
    var->init_data = buf;
    var->rel = head.next;
}

// ---- statements ----

static int is_typename(Token *tok)
{
    static const char *kw[] = {
        "void", "_Bool", "char", "short", "int", "long", "struct", "union", "typedef", "enum",
        "static", "extern", "_Alignas", "signed", "unsigned", "const", "volatile", "auto",
        "register", "restrict", "__restrict", "_Noreturn", "float", "double", "inline",
        "__declspec", "_Thread_local", "thread_local", "__thread", "_Atomic", "typeof", "__typeof__",
        "__typeof", "typeof_unqual", "__typeof_unqual__", "__typeof_unqual",
    };
    for (int i = 0; i < (int)(sizeof(kw) / sizeof(kw[0])); i++)
        if (cc_equal(tok, kw[i]))
            return 1;
    return find_typedef(tok) != 0;
}

// asm-stmt = "asm" ("volatile")? "(" string-literal ")" ";"
static Node *asm_stmt(Token **rest, Token *tok)
{
    Node *node = new_node(ND_ASM, tok);
    tok = tok->next;
    while (cc_equal(tok, "volatile") || cc_equal(tok, "inline"))
        tok = tok->next;
    tok = cc_skip(tok, "(");
    if (tok->kind != TK_STR)
        cc_error_tok(tok, "в asm нужна строка с командами");
    // adjacent literals join into one program
    Token *s = join_strings(&tok, tok);
    if (cc_str_elem_size(s->str_kind) != 1)
        cc_error_tok(s, "в asm нужна обычная строка");
    node->asm_str = s->str;
    // @x of a local variable or parameter: its place in the frame, [rbp-...] (codegen.c)
    for (const char *p = s->str; g_current_fn && *p; p++) {
        if (*p != '@' || !(p[1] == '_' || (p[1] >= 'a' && p[1] <= 'z') || (p[1] >= 'A' && p[1] <= 'Z')))
            continue;
        int n = 1;
        while (p[n] == '_' || (p[n] >= 'a' && p[n] <= 'z') || (p[n] >= 'A' && p[n] <= 'Z') || (p[n] >= '0' && p[n] <= '9'))
            n++;
        Token name = *s;
        name.loc = p + 1;
        name.len = n - 1;
        VarScope *vs = find_var(&name);
        if (vs && vs->var && vs->var->is_local) {
            AsmRef *r = cc_alloc(sizeof(AsmRef));
            r->name = p + 1;
            r->len = n - 1;
            r->var = vs->var;
            r->next = node->asm_refs;
            node->asm_refs = r;
        }
        p += n - 1;
    }
    tok = cc_skip(tok, ")");
    *rest = cc_skip(tok, ";");
    return node;
}

// The __finally funclets a jump out of here passes, innermost first (each a node with its label)
static Node *finally_calls(int from)
{
    Node head = { 0 }, *cur = &head;
    for (int i = g_nfin - 1; i >= from; i--) {
        cur = cur->next = new_node(ND_NULL_EXPR, 0);
        cur->label = g_fin[i];
    }
    return head.next;
}

static void seh_vars(void)
{
    if (!g_seh_xptr) {
        g_seh_xptr = new_lvar("", pointer_to(ty_void));
        g_seh_code = new_lvar("", ty_uint);
        g_seh_abn = new_lvar("", ty_int);
        g_current_fn->seh_xptr = g_seh_xptr;
        g_current_fn->seh_code = g_seh_code;
        g_current_fn->seh_abn = g_seh_abn;
    }
}

// __try { } __except (filter) { }   __try { } __finally { }   (Microsoft C, structured exceptions)
static Node *try_stmt(Token **rest, Token *tok)
{
    if (!cc_target_win)
        cc_error_tok(tok, "__try есть только в программах для Windows");
    if (g_seh_ctx == 1 || g_seh_ctx == 3)
        cc_error_tok(tok, "__try внутри __finally и внутри условия __except не поддерживается");
    Node *node = new_node(ND_TRY, tok);
    g_current_fn->has_seh = 1;
    seh_vars();
    node->var = new_lvar("", ty_long);          // rsp at the __try: back to it in the __except block
    node->label = new_unique_name();            // the guarded code: from here...
    node->unique_label = new_unique_name();     // ...to here (also where __leave goes)
    node->brk_label = new_unique_name();        // the funclet: the filter or the __finally block
    node->cont_label = new_unique_name();       // __except: its block
    Token *body = tok->next;
    if (!cc_equal(body, "{"))
        cc_error_tok(body, "после __try нужен блок { }");
    Token *t = body->next;
    for (int depth = 1; depth > 0; t = t->next) {
        if (t->kind == TK_EOF)
            cc_error_tok(body, "блок __try не закрыт");
        if (cc_equal(t, "{")) depth++;
        if (cc_equal(t, "}")) depth--;
    }
    int fin = cc_equal(t, "__finally") || cc_equal(t, "_finally");
    if (fin) {
        if (g_nfin == 64)
            cc_error_tok(tok, "слишком много вложенных __try");
        g_fin[g_nfin++] = node->brk_label;
    }
    char *leave = g_leave_label;
    g_leave_label = node->unique_label;
    node->then = compound_stmt(&tok, body->next);
    g_leave_label = leave;
    if (fin)
        g_nfin--;
    int ctx = g_seh_ctx;
    if (cc_equal(tok, "__except") || cc_equal(tok, "_except")) {
        tok = cc_skip(tok->next, "(");
        g_seh_ctx = 1;
        node->cond = expr(&tok, tok);
        add_type(node->cond);
        if (!is_integer(node->cond->ty))
            cc_error_tok(node->cond->tok, "условие __except — целое число: EXCEPTION_EXECUTE_HANDLER и подобные");
        node->cond = new_cast(node->cond, ty_int);
        tok = cc_skip(tok, ")");
        if (!cc_equal(tok, "{"))
            cc_error_tok(tok, "после __except (...) нужен блок { }");
        g_seh_ctx = 2;
        node->els = compound_stmt(&tok, tok->next);
    } else if (fin) {
        tok = tok->next;
        if (!cc_equal(tok, "{"))
            cc_error_tok(tok, "после __finally нужен блок { }");
        char *brk = g_brk_label, *cont = g_cont_label;
        g_brk_label = g_cont_label = g_leave_label = 0;     // the block is a funclet: no jumps out of it
        g_seh_ctx = 3;
        node->val = 1;
        node->els = compound_stmt(&tok, tok->next);
        g_brk_label = brk;
        g_cont_label = cont;
        g_leave_label = leave;
    } else {
        cc_error_tok(tok, "после __try { } нужен __except (...) { } или __finally { }");
    }
    g_seh_ctx = ctx;
    *rest = tok;
    return node;
}

static Node *stmt(Token **rest, Token *tok)
{
    if (cc_equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok);
        if (g_seh_ctx == 3)
            cc_error_tok(tok, "return внутри __finally не поддерживается");
        if (g_nfin) {
            node->els = finally_calls(0);
            node->var = new_lvar("", array_of(ty_char, 16));   // the value, kept while they run
        }
        if (cc_consume(rest, tok->next, ";"))
            return node;
        Node *e = expr(&tok, tok->next);
        *rest = cc_skip(tok, ";");
        add_type(e);
        Type *ty = g_current_fn->ty->return_ty;
        if (ty->kind != TY_STRUCT && ty->kind != TY_UNION)
            e = new_cast(e, ty);
        node->lhs = e;
        return node;
    }
    if (cc_equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok);
        tok = cc_skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = cc_skip(tok, ")");
        node->then = stmt(&tok, tok);
        if (cc_equal(tok, "else"))
            node->els = stmt(&tok, tok->next);
        *rest = tok;
        return node;
    }
    if (cc_equal(tok, "switch")) {
        Node *node = new_node(ND_SWITCH, tok);
        tok = cc_skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = cc_skip(tok, ")");
        Node *sw = g_current_switch;
        g_current_switch = node;
        char *brk = g_brk_label;
        int brk_fin = g_brk_fin;
        g_brk_label = node->brk_label = new_unique_name();
        g_brk_fin = g_nfin;
        node->then = stmt(rest, tok);
        g_current_switch = sw;
        g_brk_label = brk;
        g_brk_fin = brk_fin;
        return node;
    }
    if (cc_equal(tok, "case")) {
        if (!g_current_switch)
            cc_error_tok(tok, "case вне switch");
        Node *node = new_node(ND_CASE, tok);
        i64 begin = eval(conditional(&tok, tok->next));
        i64 end = begin;
        if (cc_equal(tok, "...")) {
            end = eval(conditional(&tok, tok->next));
            if (end < begin)
                cc_error_tok(tok, "пустой диапазон case");
        }
        for (Node *c = g_current_switch->case_next; c; c = c->case_next)
            if (begin <= c->end && c->begin <= end)
                cc_error_tok(node->tok, "case %lld уже есть в этом switch", begin > c->begin ? begin : c->begin);
        tok = cc_skip(tok, ":");
        node->label = new_unique_name();
        node->lhs = stmt(rest, tok);
        node->begin = begin;
        node->end = end;
        node->case_next = g_current_switch->case_next;
        g_current_switch->case_next = node;
        return node;
    }
    if (cc_equal(tok, "default")) {
        if (!g_current_switch)
            cc_error_tok(tok, "default вне switch");
        if (g_current_switch->default_case)
            cc_error_tok(tok, "default уже есть в этом switch");
        Node *node = new_node(ND_CASE, tok);
        tok = cc_skip(tok->next, ":");
        node->label = new_unique_name();
        node->lhs = stmt(rest, tok);
        g_current_switch->default_case = node;
        return node;
    }
    if (cc_equal(tok, "for")) {
        Node *node = new_node(ND_FOR, tok);
        tok = cc_skip(tok->next, "(");
        enter_scope();
        char *brk = g_brk_label, *cont = g_cont_label;
        int brk_fin = g_brk_fin, cont_fin = g_cont_fin;
        g_brk_label = node->brk_label = new_unique_name();
        g_cont_label = node->cont_label = new_unique_name();
        g_brk_fin = g_cont_fin = g_nfin;
        if (is_typename(tok)) {
            VarAttr attr = { 0 };
            Type *basety = declspec(&tok, tok, &attr);
            node->init = declaration(&tok, tok, basety, &attr);
        } else {
            node->init = expr_stmt(&tok, tok);
        }
        if (!cc_equal(tok, ";"))
            node->cond = expr(&tok, tok);
        tok = cc_skip(tok, ";");
        if (!cc_equal(tok, ")"))
            node->inc = expr(&tok, tok);
        tok = cc_skip(tok, ")");
        int vlas = g_vla_count;
        node->then = stmt(rest, tok);
        if (g_vla_count != vlas)
            node->var = new_lvar("", ty_long);      // rsp before the body: each round starts from it again
        leave_scope();
        g_brk_label = brk;
        g_cont_label = cont;
        g_brk_fin = brk_fin;
        g_cont_fin = cont_fin;
        return node;
    }
    if (cc_equal(tok, "while")) {
        Node *node = new_node(ND_FOR, tok);
        tok = cc_skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = cc_skip(tok, ")");
        char *brk = g_brk_label, *cont = g_cont_label;
        int brk_fin = g_brk_fin, cont_fin = g_cont_fin;
        g_brk_label = node->brk_label = new_unique_name();
        g_cont_label = node->cont_label = new_unique_name();
        g_brk_fin = g_cont_fin = g_nfin;
        int vlas = g_vla_count;
        node->then = stmt(rest, tok);
        if (g_vla_count != vlas)
            node->var = new_lvar("", ty_long);
        g_brk_label = brk;
        g_cont_label = cont;
        g_brk_fin = brk_fin;
        g_cont_fin = cont_fin;
        return node;
    }
    if (cc_equal(tok, "do")) {
        Node *node = new_node(ND_DO, tok);
        char *brk = g_brk_label, *cont = g_cont_label;
        int brk_fin = g_brk_fin, cont_fin = g_cont_fin;
        g_brk_label = node->brk_label = new_unique_name();
        g_cont_label = node->cont_label = new_unique_name();
        g_brk_fin = g_cont_fin = g_nfin;
        int vlas = g_vla_count;
        node->then = stmt(&tok, tok->next);
        if (g_vla_count != vlas)
            node->var = new_lvar("", ty_long);
        g_brk_label = brk;
        g_cont_label = cont;
        g_brk_fin = brk_fin;
        g_cont_fin = cont_fin;
        tok = cc_skip(tok, "while");
        tok = cc_skip(tok, "(");
        node->cond = expr(&tok, tok);
        tok = cc_skip(tok, ")");
        *rest = cc_skip(tok, ";");
        return node;
    }
    if (cc_equal(tok, "asm") || cc_equal(tok, "__asm__"))
        return asm_stmt(rest, tok);
    if (cc_equal(tok, "__try") || cc_equal(tok, "_try"))
        return try_stmt(rest, tok);
    if (cc_equal(tok, "__leave") || cc_equal(tok, "_leave")) {
        if (!g_leave_label)
            cc_error_tok(tok, "__leave вне блока __try");
        Node *node = new_node(ND_GOTO, tok);
        node->unique_label = g_leave_label;
        *rest = cc_skip(tok->next, ";");
        return node;
    }
    if (cc_equal(tok, "goto")) {
        Node *node = new_node(ND_GOTO, tok);
        node->label = get_ident(tok->next);
        node->goto_next = g_gotos;
        g_gotos = node;
        *rest = cc_skip(tok->next->next, ";");
        return node;
    }
    if (cc_equal(tok, "break")) {
        if (!g_brk_label)
            cc_error_tok(tok, "break вне цикла или switch");
        Node *node = new_node(ND_GOTO, tok);
        node->unique_label = g_brk_label;
        if (g_nfin > g_brk_fin)
            node->els = finally_calls(g_brk_fin);
        *rest = cc_skip(tok->next, ";");
        return node;
    }
    if (cc_equal(tok, "continue")) {
        if (!g_cont_label)
            cc_error_tok(tok, "continue вне цикла");
        Node *node = new_node(ND_GOTO, tok);
        node->unique_label = g_cont_label;
        if (g_nfin > g_cont_fin)
            node->els = finally_calls(g_cont_fin);
        *rest = cc_skip(tok->next, ";");
        return node;
    }
    if (tok->kind == TK_IDENT && cc_equal(tok->next, ":")) {
        Node *node = new_node(ND_LABEL, tok);
        node->label = cc_strndup(tok->loc, tok->len);
        node->unique_label = new_unique_name();
        node->lhs = stmt(rest, tok->next->next);
        node->goto_next = g_labels;
        g_labels = node;
        return node;
    }
    if (cc_equal(tok, "{"))
        return compound_stmt(rest, tok->next);
    return expr_stmt(rest, tok);
}

static Node *compound_stmt(Token **rest, Token *tok)
{
    Node *node = new_node(ND_BLOCK, tok);
    Node head = { 0 };
    Node *cur = &head;
    Token *first = tok;
    enter_scope();
    while (!cc_equal(tok, "}")) {
        // all sources are one stream of tokens: a block left open runs into the next file
        if (tok->kind == TK_EOF || tok->unit != first->unit)
            cc_error_tok(first, "не хватает '}': блок, который начинается здесь, не закрыт до конца файла");
        if (take_pack(&tok, tok))
            continue;
        if (is_static_assert(tok)) {
            tok = static_assertion(tok);
            continue;
        }
        if (is_typename(tok) && !cc_equal(tok->next, ":")) {
            VarAttr attr = { 0 };
            Type *basety = declspec(&tok, tok, &attr);
            if (attr.is_typedef) {
                tok = parse_typedef(tok, basety);
                continue;
            }
            if (is_function(tok)) {
                tok = function(tok, basety, &attr);
                continue;
            }
            cur = cur->next = declaration(&tok, tok, basety, &attr);
        } else {
            cur = cur->next = stmt(&tok, tok);
        }
        add_type(cur);
    }
    leave_scope();
    node->body = head.next;
    node->rbrace = tok;
    *rest = tok->next;
    return node;
}

static Node *expr_stmt(Token **rest, Token *tok)
{
    if (cc_equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }
    Node *node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(&tok, tok);
    *rest = cc_skip(tok, ";");
    return node;
}

static Node *expr(Token **rest, Token *tok)
{
    Node *node = assign(&tok, tok);
    if (cc_equal(tok, ","))
        return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);
    *rest = tok;
    return node;
}

// ---- constant evaluation ----

static i64 eval(Node *node)
{
    return eval2(node, 0);
}

static i64 eval2(Node *node, char ***label)
{
    add_type(node);
    if (is_flonum(node->ty))
        return (i64)eval_double(node);

    switch (node->kind) {
    case ND_ADD: return eval2(node->lhs, label) + eval(node->rhs);
    case ND_SUB: return eval2(node->lhs, label) - eval(node->rhs);
    case ND_MUL: return eval(node->lhs) * eval(node->rhs);
    case ND_DIV: {
        i64 r = eval(node->rhs);
        if (!r)
            cc_error_tok(node->tok, "деление на ноль");
        if (node->ty->is_unsigned)
            return (i64)((u64)eval(node->lhs) / (u64)r);
        return eval(node->lhs) / r;
    }
    case ND_NEG: return -eval(node->lhs);
    case ND_MOD: {
        i64 r = eval(node->rhs);
        if (!r)
            cc_error_tok(node->tok, "деление на ноль");
        if (node->ty->is_unsigned)
            return (i64)((u64)eval(node->lhs) % (u64)r);
        return eval(node->lhs) % r;
    }
    case ND_BITAND: return eval(node->lhs) & eval(node->rhs);
    case ND_BITOR: return eval(node->lhs) | eval(node->rhs);
    case ND_BITXOR: return eval(node->lhs) ^ eval(node->rhs);
    case ND_SHL: return eval(node->lhs) << eval(node->rhs);
    case ND_SHR:
        if (node->ty->is_unsigned && node->ty->size == 8)
            return (i64)((u64)eval(node->lhs) >> eval(node->rhs));
        return eval(node->lhs) >> eval(node->rhs);
    case ND_EQ: return eval(node->lhs) == eval(node->rhs);
    case ND_NE: return eval(node->lhs) != eval(node->rhs);
    case ND_LT:
        if (node->lhs->ty->is_unsigned)
            return (u64)eval(node->lhs) < (u64)eval(node->rhs);
        return eval(node->lhs) < eval(node->rhs);
    case ND_LE:
        if (node->lhs->ty->is_unsigned)
            return (u64)eval(node->lhs) <= (u64)eval(node->rhs);
        return eval(node->lhs) <= eval(node->rhs);
    case ND_COND: return eval(node->cond) ? eval2(node->then, label) : eval2(node->els, label);
    case ND_COMMA: return eval2(node->rhs, label);
    case ND_NOT: return !eval(node->lhs);
    case ND_BITNOT: return ~eval(node->lhs);
    case ND_LOGAND: return eval(node->lhs) && eval(node->rhs);
    case ND_LOGOR: return eval(node->lhs) || eval(node->rhs);
    case ND_CAST: {
        i64 val = eval2(node->lhs, label);
        if (is_integer(node->ty)) {
            switch (node->ty->size) {
            case 1: return node->ty->is_unsigned ? (u8)val : (signed char)val;
            case 2: return node->ty->is_unsigned ? (unsigned short)val : (short)val;
            case 4: return node->ty->is_unsigned ? (u32)val : (int)val;
            }
        }
        return val;
    }
    case ND_ADDR: return eval_rval(node->lhs, label);
    case ND_MEMBER:
        if (!label)
            cc_error_tok(node->tok, "не константа");
        if (node->ty->kind != TY_ARRAY)
            cc_error_tok(node->tok, "неверный инициализатор");
        return eval_rval(node->lhs, label) + node->member->offset;
    case ND_VAR:
        if (!label)
            cc_error_tok(node->tok, "не константа");
        if (node->var->ty->kind != TY_ARRAY && node->var->ty->kind != TY_FUNC)
            cc_error_tok(node->tok, "неверный инициализатор");
        *label = &node->var->name;
        return 0;
    case ND_NUM: return node->val;
    default:
        break;
    }
    cc_error_tok(node->tok, "не константа");
}

static i64 eval_rval(Node *node, char ***label)
{
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_local)
            cc_error_tok(node->tok, "не константа");
        *label = &node->var->name;
        return 0;
    case ND_DEREF:
        return eval2(node->lhs, label);
    case ND_MEMBER:
        return eval_rval(node->lhs, label) + node->member->offset;
    default:
        break;
    }
    cc_error_tok(node->tok, "неверный инициализатор");
}

static int is_const_expr(Node *node)
{
    add_type(node);
    switch (node->kind) {
    case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_BITAND: case ND_BITOR:
    case ND_BITXOR: case ND_SHL: case ND_SHR: case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
    case ND_LOGAND: case ND_LOGOR: case ND_MOD:
        return is_const_expr(node->lhs) && is_const_expr(node->rhs);
    case ND_COND:
        if (!is_const_expr(node->cond))
            return 0;
        return is_const_expr(eval(node->cond) ? node->then : node->els);
    case ND_COMMA:
        return is_const_expr(node->rhs);
    case ND_NEG: case ND_NOT: case ND_BITNOT: case ND_CAST:
        return is_const_expr(node->lhs);
    case ND_NUM:
        return 1;
    default:
        return 0;
    }
}

static double eval_double(Node *node)
{
    add_type(node);
    if (is_integer(node->ty)) {
        if (node->ty->is_unsigned)
            return (double)(u64)eval(node);
        return (double)eval(node);
    }
    switch (node->kind) {
    case ND_ADD: return eval_double(node->lhs) + eval_double(node->rhs);
    case ND_SUB: return eval_double(node->lhs) - eval_double(node->rhs);
    case ND_MUL: return eval_double(node->lhs) * eval_double(node->rhs);
    case ND_DIV: return eval_double(node->lhs) / eval_double(node->rhs);
    case ND_NEG: return -eval_double(node->lhs);
    case ND_COND: return eval_double(node->cond) ? eval_double(node->then) : eval_double(node->els);
    case ND_COMMA: return eval_double(node->rhs);
    case ND_CAST:
        if (is_flonum(node->lhs->ty))
            return eval_double(node->lhs);
        return (double)eval(node->lhs);
    case ND_NUM: return node->fval;
    default:
        break;
    }
    cc_error_tok(node->tok, "не константа");
}

// ---- expressions ----

// Converts "A op= B" into "tmp = &A, *tmp = *tmp op B".
static Node *to_assign(Node *binary)
{
    add_type(binary->lhs);
    add_type(binary->rhs);
    Token *tok = binary->tok;
    // a bit field has no address: A.x op= C becomes tmp = &A, tmp->x = tmp->x op C
    if (binary->lhs->kind == ND_MEMBER && binary->lhs->member->is_bitfield) {
        Node *owner = binary->lhs->lhs;
        Obj *var = new_lvar("", pointer_to(owner->ty));
        Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok), new_unary(ND_ADDR, owner, tok), tok);
        Node *field1 = new_unary(ND_MEMBER, new_unary(ND_DEREF, new_var_node(var, tok), tok), tok);
        Node *field2 = new_unary(ND_MEMBER, new_unary(ND_DEREF, new_var_node(var, tok), tok), tok);
        field1->member = field2->member = binary->lhs->member;
        Node *expr2 = new_binary(ND_ASSIGN, field1, new_binary(binary->kind, field2, binary->rhs, tok), tok);
        return new_binary(ND_COMMA, expr1, expr2, tok);
    }
    if (binary->lhs->ty->is_atomic) {
        // _Atomic: ({ p = &A; v = B; old = *p; do new = old op v; while (!cas(p, &old, new)); new; })
        Type *ty = binary->lhs->ty;
        Obj *p = new_lvar("", pointer_to(ty));
        Obj *v = new_lvar("", binary->rhs->ty);
        Obj *old = new_lvar("", ty);
        Obj *nw = new_lvar("", ty);
        Node *e = new_binary(ND_ASSIGN, new_var_node(p, tok), new_unary(ND_ADDR, binary->lhs, tok), tok);
        e = new_binary(ND_COMMA, e, new_binary(ND_ASSIGN, new_var_node(v, tok), binary->rhs, tok), tok);
        e = new_binary(ND_COMMA, e, new_binary(ND_ASSIGN, new_var_node(old, tok),
                                               new_unary(ND_DEREF, new_var_node(p, tok), tok), tok), tok);
        Node *loop = new_node(ND_DO, tok);
        loop->brk_label = new_unique_name();
        loop->cont_label = new_unique_name();
        loop->then = new_unary(ND_EXPR_STMT, new_binary(ND_ASSIGN, new_var_node(nw, tok),
                                   new_binary(binary->kind, new_var_node(old, tok), new_var_node(v, tok), tok), tok), tok);
        Node *cas = new_node(ND_CAS, tok);
        cas->lhs = new_var_node(p, tok);
        cas->rhs = new_unary(ND_ADDR, new_var_node(old, tok), tok);
        cas->then = new_var_node(nw, tok);
        loop->cond = new_unary(ND_NOT, cas, tok);
        Node *se = new_node(ND_STMT_EXPR, tok);
        se->body = new_unary(ND_EXPR_STMT, e, tok);
        se->body->next = loop;
        loop->next = new_unary(ND_EXPR_STMT, new_var_node(nw, tok), tok);
        return se;
    }
    Obj *var = new_lvar("", pointer_to(binary->lhs->ty));
    Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok), new_unary(ND_ADDR, binary->lhs, tok), tok);
    Node *expr2 = new_binary(ND_ASSIGN, new_unary(ND_DEREF, new_var_node(var, tok), tok),
                             new_binary(binary->kind, new_unary(ND_DEREF, new_var_node(var, tok), tok),
                                        binary->rhs, tok),
                             tok);
    return new_binary(ND_COMMA, expr1, expr2, tok);
}

static Node *assign(Token **rest, Token *tok)
{
    Node *node = conditional(&tok, tok);
    if (cc_equal(tok, "="))
        return new_binary(ND_ASSIGN, node, assign(rest, tok->next), tok);
    if (cc_equal(tok, "+="))
        return to_assign(new_add(node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "-="))
        return to_assign(new_sub(node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "*="))
        return to_assign(new_binary(ND_MUL, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "/="))
        return to_assign(new_binary(ND_DIV, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "%="))
        return to_assign(new_binary(ND_MOD, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "&="))
        return to_assign(new_binary(ND_BITAND, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "|="))
        return to_assign(new_binary(ND_BITOR, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "^="))
        return to_assign(new_binary(ND_BITXOR, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, "<<="))
        return to_assign(new_binary(ND_SHL, node, assign(rest, tok->next), tok));
    if (cc_equal(tok, ">>="))
        return to_assign(new_binary(ND_SHR, node, assign(rest, tok->next), tok));
    *rest = tok;
    return node;
}

static Node *conditional(Token **rest, Token *tok)
{
    Node *cond = logor(&tok, tok);
    if (!cc_equal(tok, "?")) {
        *rest = tok;
        return cond;
    }
    Node *node = new_node(ND_COND, tok);
    node->cond = cond;
    node->then = expr(&tok, tok->next);
    tok = cc_skip(tok, ":");
    node->els = conditional(rest, tok);
    return node;
}

static Node *logor(Token **rest, Token *tok)
{
    Node *node = logand(&tok, tok);
    while (cc_equal(tok, "||")) {
        Token *start = tok;
        node = new_binary(ND_LOGOR, node, logand(&tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

static Node *logand(Token **rest, Token *tok)
{
    Node *node = bitor(&tok, tok);
    while (cc_equal(tok, "&&")) {
        Token *start = tok;
        node = new_binary(ND_LOGAND, node, bitor(&tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

static Node *bitor(Token **rest, Token *tok)
{
    Node *node = bitxor(&tok, tok);
    while (cc_equal(tok, "|")) {
        Token *start = tok;
        node = new_binary(ND_BITOR, node, bitxor(&tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

static Node *bitxor(Token **rest, Token *tok)
{
    Node *node = bitand(&tok, tok);
    while (cc_equal(tok, "^")) {
        Token *start = tok;
        node = new_binary(ND_BITXOR, node, bitand(&tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

static Node *bitand(Token **rest, Token *tok)
{
    Node *node = equality(&tok, tok);
    while (cc_equal(tok, "&")) {
        Token *start = tok;
        node = new_binary(ND_BITAND, node, equality(&tok, tok->next), start);
    }
    *rest = tok;
    return node;
}

static Node *equality(Token **rest, Token *tok)
{
    Node *node = relational(&tok, tok);
    for (;;) {
        Token *start = tok;
        if (cc_equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, "!=")) {
            node = new_binary(ND_NE, node, relational(&tok, tok->next), start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *relational(Token **rest, Token *tok)
{
    Node *node = shift(&tok, tok);
    for (;;) {
        Token *start = tok;
        if (cc_equal(tok, "<")) {
            node = new_binary(ND_LT, node, shift(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, "<=")) {
            node = new_binary(ND_LE, node, shift(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, ">")) {
            node = new_binary(ND_LT, shift(&tok, tok->next), node, start);
            continue;
        }
        if (cc_equal(tok, ">=")) {
            node = new_binary(ND_LE, shift(&tok, tok->next), node, start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *shift(Token **rest, Token *tok)
{
    Node *node = add(&tok, tok);
    for (;;) {
        Token *start = tok;
        if (cc_equal(tok, "<<")) {
            node = new_binary(ND_SHL, node, add(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, ">>")) {
            node = new_binary(ND_SHR, node, add(&tok, tok->next), start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

// The size of what a pointer points to: a number, or a VLA's hidden size
static Node *elem_size(Type *base, Token *tok)
{
    if (base->kind == TY_VLA) {
        if (!base->vla_size)
            cc_error_tok(tok, "размер массива здесь неизвестен");
        return new_cast(new_var_node(base->vla_size, tok), ty_long);
    }
    return new_long(base->size, tok);
}

// Pointer arithmetic scales the integer by the element size.
static Node *new_add(Node *lhs, Node *rhs, Token *tok)
{
    add_type(lhs);
    add_type(rhs);
    if (is_numeric(lhs->ty) && is_numeric(rhs->ty))
        return new_binary(ND_ADD, lhs, rhs, tok);
    if (lhs->ty->base && rhs->ty->base)
        cc_error_tok(tok, "нельзя складывать два указателя");
    if (!lhs->ty->base && rhs->ty->base) {
        Node *t = lhs;
        lhs = rhs;
        rhs = t;
    }
    if (!lhs->ty->base)
        cc_error_tok(tok, "неверные операнды +");
    rhs = new_binary(ND_MUL, new_cast(rhs, ty_long), elem_size(lhs->ty->base, tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok)
{
    add_type(lhs);
    add_type(rhs);
    if (is_numeric(lhs->ty) && is_numeric(rhs->ty))
        return new_binary(ND_SUB, lhs, rhs, tok);
    if (lhs->ty->base && is_integer(rhs->ty)) {
        rhs = new_binary(ND_MUL, new_cast(rhs, ty_long), elem_size(lhs->ty->base, tok), tok);
        add_type(rhs);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = lhs->ty;
        return node;
    }
    if (lhs->ty->base && rhs->ty->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = ty_long;
        if (lhs->ty->base->kind == TY_VLA)
            return new_binary(ND_DIV, node, elem_size(lhs->ty->base, tok), tok);
        int sz = lhs->ty->base->size > 0 ? lhs->ty->base->size : 1;
        return new_binary(ND_DIV, node, new_num(sz, tok), tok);
    }
    cc_error_tok(tok, "неверные операнды -");
}

static Node *add(Token **rest, Token *tok)
{
    Node *node = mul(&tok, tok);
    for (;;) {
        Token *start = tok;
        if (cc_equal(tok, "+")) {
            node = new_add(node, mul(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, "-")) {
            node = new_sub(node, mul(&tok, tok->next), start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *mul(Token **rest, Token *tok)
{
    Node *node = cast(&tok, tok);
    for (;;) {
        Token *start = tok;
        if (cc_equal(tok, "*")) {
            node = new_binary(ND_MUL, node, cast(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, "/")) {
            node = new_binary(ND_DIV, node, cast(&tok, tok->next), start);
            continue;
        }
        if (cc_equal(tok, "%")) {
            node = new_binary(ND_MOD, node, cast(&tok, tok->next), start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *compound_literal(Token **rest, Token *tok, Type *ty, Token *start)
{
    if (g_scope->next == 0 || !g_current_fn) {
        Obj *var = new_anon_gvar(ty);
        gvar_initializer(rest, tok, var);
        return new_var_node(var, start);
    }
    Obj *var = new_lvar("", ty);
    Node *lhs = lvar_initializer(rest, tok, var);
    Node *rhs = new_var_node(var, tok);
    return new_binary(ND_COMMA, lhs, rhs, start);
}

static Node *cast(Token **rest, Token *tok)
{
    if (cc_equal(tok, "(") && is_typename(tok->next)) {
        Token *start = tok;
        Type *ty = typename_(&tok, tok->next);
        tok = cc_skip(tok, ")");
        if (cc_equal(tok, "{"))
            return compound_literal(rest, tok, ty, start);
        Node *node = new_cast(cast(rest, tok), ty);
        node->tok = start;
        return node;
    }
    return unary(rest, tok);
}

static Node *unary(Token **rest, Token *tok)
{
    if (cc_equal(tok, "+"))
        return cast(rest, tok->next);
    if (cc_equal(tok, "-"))
        return new_unary(ND_NEG, cast(rest, tok->next), tok);
    if (cc_equal(tok, "&")) {
        Node *lhs = cast(rest, tok->next);
        add_type(lhs);
        if (lhs->kind == ND_MEMBER && lhs->member->is_bitfield)
            cc_error_tok(tok, "у битового поля нет адреса");
        return new_unary(ND_ADDR, lhs, tok);
    }
    if (cc_equal(tok, "*")) {
        Node *node = cast(rest, tok->next);
        add_type(node);
        if (node->ty->kind == TY_FUNC)
            return node;
        return new_unary(ND_DEREF, node, tok);
    }
    if (cc_equal(tok, "!"))
        return new_unary(ND_NOT, cast(rest, tok->next), tok);
    if (cc_equal(tok, "~"))
        return new_unary(ND_BITNOT, cast(rest, tok->next), tok);
    if (cc_equal(tok, "++"))
        return to_assign(new_add(unary(rest, tok->next), new_num(1, tok), tok));
    if (cc_equal(tok, "--"))
        return to_assign(new_sub(unary(rest, tok->next), new_num(1, tok), tok));
    return postfix(rest, tok);
}

static Type *struct_union_decl(Token **rest, Token *tok, TypeKind kind);

static void struct_members(Token **rest, Token *tok, Type *ty)
{
    Member head = { 0 };
    Member *cur = &head;
    while (!cc_equal(tok, "}")) {
        if (is_static_assert(tok)) {
            tok = static_assertion(tok);
            continue;
        }
        VarAttr attr = { 0 };
        Type *basety = declspec(&tok, tok, &attr);
        int first = 1;
        // anonymous struct/union member
        if ((basety->kind == TY_STRUCT || basety->kind == TY_UNION) && cc_consume(&tok, tok, ";")) {
            Member *m = cc_alloc(sizeof(Member));
            m->ty = basety;
            cur = cur->next = m;
            continue;
        }
        while (!cc_consume(&tok, tok, ";")) {
            if (!first)
                tok = next_declarator(tok);
            first = 0;
            Member *m = cc_alloc(sizeof(Member));
            m->ty = declarator(&tok, tok, basety);
            m->name = m->ty->name;
            if (m->ty->kind == TY_VLA)
                cc_error_tok(m->name ? m->name : tok, "поле структуры не бывает массивом переменной длины");
            if (attr.align > m->ty->align) {        // _Alignas(16) int x: the member's own type, aligned so
                m->ty = copy_type(m->ty);
                m->ty->align = attr.align;
            }
            if (cc_equal(tok, ":")) {           // a bit field: "int a : 3", "int : 0"
                Token *at = tok;
                if (!is_integer(m->ty))
                    cc_error_tok(at, "битовое поле бывает только целого типа");
                i64 width = eval(conditional(&tok, tok->next));
                if (width < 0 || width > m->ty->size * 8)
                    cc_error_tok(at, "ширина битового поля — от 0 до %d", m->ty->size * 8);
                if (width == 0 && m->name)
                    cc_error_tok(at, "у битового поля шириной 0 не бывает имени");
                m->is_bitfield = 1;
                m->bit_width = (int)width;
            }
            cur = cur->next = m;
        }
    }
    // flexible array member
    if (cur != &head && cur->ty->kind == TY_ARRAY && cur->ty->array_len < 0) {
        cur->ty = array_of(cur->ty->base, 0);
        ty->is_flexible = 1;
    }
    *rest = tok->next;
    ty->members = head.next;
}

static Type *struct_union_decl(Token **rest, Token *tok, TypeKind kind)
{
    Token *tag = 0;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }
    if (tag && !cc_equal(tok, "{")) {
        *rest = tok;
        Type *ty2 = find_tag(tag);
        if (ty2)
            return ty2;
        Type *ty = struct_type();
        ty->kind = kind;
        ty->size = -1;
        push_tag_scope(tag, ty);
        return ty;
    }
    tok = cc_skip(tok, "{");
    Type *ty = struct_type();
    ty->kind = kind;
    ty->pack = g_pack;
    struct_members(rest, tok, ty);

    if (tag) {
        // complete an earlier forward declaration in this scope
        for (TagEntry *e = g_scope->tags; e; e = e->next) {
            if (e->len == tag->len && cc_strneq(e->name, tag->loc, tag->len)) {
                Type *old = e->ty;
                ty->origin = 0;
                *old = *ty;
                return old;
            }
        }
        push_tag_scope(tag, ty);
    }
    return ty;
}

// A member's alignment: its own, or less under #pragma pack(n)
static int member_align(Type *st, Member *m)
{
    return st->pack > 0 && st->pack < m->ty->align ? st->pack : m->ty->align;
}

static Type *struct_decl(Token **rest, Token *tok)
{
    Type *ty = struct_union_decl(rest, tok, TY_STRUCT);
    if (ty->size < 0 && !ty->members)
        return ty;
    if (ty->size >= 0 && ty->members == 0 && ty->size == 0 && ty->align == 1) {
        // empty struct is allowed
    }
    // Bit fields as MSVC lays them: a field takes bits of a unit the size of its type; the
    // next field shares the unit if its type is of the same size and it fits, else a new
    // unit begins. ": 0" after a bit field closes the unit (the next one aligned to its
    // type); anywhere else it does nothing.
    int offset = 0;
    int unit = 0, unit_offset = 0, used = 0;       // the open unit: its size in bytes (0 = none)
    ty->align = 1;
    for (Member *m = ty->members; m; m = m->next) {
        int a = member_align(ty, m);
        if (m->is_bitfield) {
            int sz = m->ty->size;
            if (m->bit_width == 0) {
                if (unit) {
                    offset = align_to(unit_offset + unit, a);
                    unit = 0;
                    if (ty->align < a)
                        ty->align = a;
                }
                m->offset = offset;
                continue;
            }
            if (unit != sz || used + m->bit_width > sz * 8) {
                if (unit)
                    offset = unit_offset + unit;
                unit_offset = offset = align_to(offset, a);
                unit = sz;
                used = 0;
            }
            m->offset = unit_offset;
            m->bit_offset = used;
            used += m->bit_width;
            if (ty->align < a)
                ty->align = a;
            continue;
        }
        if (unit) {
            offset = unit_offset + unit;
            unit = 0;
        }
        offset = align_to(offset, a);
        m->offset = offset;
        offset += m->ty->size;
        if (ty->align < a)
            ty->align = a;
    }
    if (unit)
        offset = unit_offset + unit;
    ty->size = align_to(offset, ty->align);
    return ty;
}

static Type *union_decl(Token **rest, Token *tok)
{
    Type *ty = struct_union_decl(rest, tok, TY_UNION);
    if (ty->size < 0 && !ty->members)
        return ty;
    ty->align = 1;
    ty->size = 0;
    for (Member *m = ty->members; m; m = m->next) {
        if (m->is_bitfield && m->bit_width == 0)
            continue;
        if (ty->align < member_align(ty, m) && !m->is_bitfield)     // MSVC: bit fields do not align a union
            ty->align = member_align(ty, m);
        if (ty->size < m->ty->size)
            ty->size = m->ty->size;
    }
    ty->size = align_to(ty->size, ty->align);
    return ty;
}

static Member *get_struct_member(Type *ty, Token *tok)
{
    for (Member *m = ty->members; m; m = m->next) {
        if (!m->name && (m->ty->kind == TY_STRUCT || m->ty->kind == TY_UNION)) {
            if (get_struct_member(m->ty, tok))
                return m;
            continue;
        }
        if (m->name && m->name->len == tok->len && cc_strneq(m->name->loc, tok->loc, tok->len))
            return m;
    }
    return 0;
}

// Anonymous members are reached through a chain of ND_MEMBER nodes.
static Node *struct_ref(Node *node, Token *tok)
{
    add_type(node);
    if (node->ty->kind != TY_STRUCT && node->ty->kind != TY_UNION)
        cc_error_tok(node->tok, "это не структура и не объединение");
    Type *ty = node->ty;
    for (;;) {
        Member *m = get_struct_member(ty, tok);
        if (!m)
            cc_error_tok(tok, "нет такого поля");
        node = new_unary(ND_MEMBER, node, tok);
        node->member = m;
        if (m->name)
            break;
        ty = m->ty;
    }
    return node;
}

// x++ becomes (typeof x)((x += 1) - 1); a bit field cuts the new value (7 + 1 in 3 bits
// is 0), so there: p = &A, old = p->x, p->x = old + 1, old
static Node *new_inc_dec(Node *node, Token *tok, int addend)
{
    add_type(node);
    if (node->kind == ND_MEMBER && node->member->is_bitfield) {
        Node *owner = node->lhs;
        Obj *p = new_lvar("", pointer_to(owner->ty));
        Obj *old = new_lvar("", node->ty);
        Node *field1 = new_unary(ND_MEMBER, new_unary(ND_DEREF, new_var_node(p, tok), tok), tok);
        Node *field2 = new_unary(ND_MEMBER, new_unary(ND_DEREF, new_var_node(p, tok), tok), tok);
        field1->member = field2->member = node->member;
        Node *e = new_binary(ND_ASSIGN, new_var_node(p, tok), new_unary(ND_ADDR, owner, tok), tok);
        e = new_binary(ND_COMMA, e, new_binary(ND_ASSIGN, new_var_node(old, tok), field1, tok), tok);
        e = new_binary(ND_COMMA, e,
                       new_binary(ND_ASSIGN, field2, new_add(new_var_node(old, tok), new_num(addend, tok), tok), tok),
                       tok);
        return new_binary(ND_COMMA, e, new_var_node(old, tok), tok);
    }
    return new_cast(new_add(to_assign(new_add(node, new_num(addend, tok), tok)), new_num(-addend, tok), tok),
                    node->ty);
}

static Node *postfix(Token **rest, Token *tok)
{
    if (cc_equal(tok, "(") && is_typename(tok->next)) {
        Token *start = tok;
        Type *ty = typename_(&tok, tok->next);
        tok = cc_skip(tok, ")");
        return compound_literal(rest, tok, ty, start);
    }
    Node *node = primary(&tok, tok);
    for (;;) {
        if (cc_equal(tok, "(")) {
            node = funcall(&tok, tok->next, node);
            continue;
        }
        if (cc_equal(tok, "[")) {
            Token *start = tok;
            Node *idx = expr(&tok, tok->next);
            tok = cc_skip(tok, "]");
            node = new_unary(ND_DEREF, new_add(node, idx, start), start);
            continue;
        }
        if (cc_equal(tok, ".")) {
            node = struct_ref(node, tok->next);
            tok = tok->next->next;
            continue;
        }
        if (cc_equal(tok, "->")) {
            node = new_unary(ND_DEREF, node, tok);
            node = struct_ref(node, tok->next);
            tok = tok->next->next;
            continue;
        }
        if (cc_equal(tok, "++")) {
            node = new_inc_dec(node, tok, 1);
            tok = tok->next;
            continue;
        }
        if (cc_equal(tok, "--")) {
            node = new_inc_dec(node, tok, -1);
            tok = tok->next;
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *funcall(Token **rest, Token *tok, Node *fn)
{
    add_type(fn);
    if (fn->ty->kind != TY_FUNC && (fn->ty->kind != TY_PTR || fn->ty->base->kind != TY_FUNC))
        cc_error_tok(fn->tok, "это не функция");
    Type *ty = fn->ty->kind == TY_FUNC ? fn->ty : fn->ty->base;
    Type *param_ty = ty->params;
    Node head = { 0 };
    Node *cur = &head;
    while (!cc_equal(tok, ")")) {
        if (cur != &head)
            tok = cc_skip(tok, ",");
        Node *arg = assign(&tok, tok);
        add_type(arg);
        if (!param_ty && !ty->is_variadic)
            cc_error_tok(tok, "слишком много аргументов");
        if (param_ty) {
            if (param_ty->kind != TY_STRUCT && param_ty->kind != TY_UNION)
                arg = new_cast(arg, param_ty);
            param_ty = param_ty->next;
        } else if (arg->ty->kind == TY_FLOAT) {
            arg = new_cast(arg, ty_double);   // default argument promotion
        } else if (arg->ty->kind == TY_ARRAY || arg->ty->kind == TY_VLA) {
            arg = new_cast(arg, pointer_to(arg->ty->base));
        } else if (is_integer(arg->ty) && arg->ty->size < 4) {
            arg = new_cast(arg, ty_int);
        }
        cur = cur->next = arg;
    }
    if (param_ty)
        cc_error_tok(tok, "мало аргументов");
    *rest = cc_skip(tok, ")");
    Node *node = new_unary(ND_FUNCALL, fn, tok);
    node->func_ty = ty;
    node->ty = ty->return_ty;
    node->args = head.next;
    if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION)
        node->ret_buffer = new_lvar("", node->ty);
    return node;
}

static Node *primary(Token **rest, Token *tok)
{
    Token *start = tok;
    if (cc_equal(tok, "(") && cc_equal(tok->next, "{")) {
        // GNU statement expression
        Node *node = new_node(ND_STMT_EXPR, tok);
        node->body = compound_stmt(&tok, tok->next->next)->body;
        *rest = cc_skip(tok, ")");
        return node;
    }
    if (cc_equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = cc_skip(tok, ")");
        return node;
    }
    if (cc_equal(tok, "sizeof") && cc_equal(tok->next, "(") && is_typename(tok->next->next)) {
        Type *ty = typename_(&tok, tok->next->next);
        *rest = cc_skip(tok, ")");
        if (ty->kind == TY_VLA) {
            if (ty->vla_size)
                return new_var_node(ty->vla_size, start);
            Node *size = compute_vla_size(ty, start);       // first: it makes ty->vla_size
            return new_binary(ND_COMMA, size, new_var_node(ty->vla_size, start), start);
        }
        if (ty->size < 0)
            cc_error_tok(tok, "sizeof от неполного типа");
        return new_ulong(ty->size, start);
    }
    if (cc_equal(tok, "sizeof")) {
        Node *node = unary(rest, tok->next);
        add_type(node);
        if (node->ty->kind == TY_VLA) {
            if (!node->ty->vla_size)
                cc_error_tok(tok, "размер массива здесь неизвестен");
            return new_var_node(node->ty->vla_size, tok);
        }
        if (node->ty->size < 0)
            cc_error_tok(tok, "sizeof от неполного типа");
        return new_ulong(node->ty->size, tok);
    }
    if (cc_equal(tok, "_Alignof") && cc_equal(tok->next, "(") && is_typename(tok->next->next)) {
        Type *ty = typename_(&tok, tok->next->next);
        *rest = cc_skip(tok, ")");
        return new_ulong(ty->align, tok);
    }
    if (cc_equal(tok, "_Alignof")) {
        Node *node = unary(rest, tok->next);
        add_type(node);
        return new_ulong(node->ty->align, tok);
    }
    if (cc_equal(tok, "_Generic")) {
        // _Generic(x, int: a, double: b, default: c): the branch of x's type; x is not computed
        tok = cc_skip(tok->next, "(");
        Node *ctl = assign(&tok, tok);
        add_type(ctl);
        Type *t = ctl->ty;
        if (t->kind == TY_ARRAY || t->kind == TY_VLA)
            t = pointer_to(t->base);
        else if (t->kind == TY_FUNC)
            t = pointer_to(t);
        Node *chosen = 0, *dflt = 0;
        while (cc_consume(&tok, tok, ",")) {
            if (cc_equal(tok, "default")) {
                tok = cc_skip(tok->next, ":");
                Node *e = assign(&tok, tok);
                if (dflt)
                    cc_error_tok(tok, "в _Generic два default");
                dflt = e;
                continue;
            }
            Type *at = typename_(&tok, tok);
            tok = cc_skip(tok, ":");
            Node *e = assign(&tok, tok);
            if (!chosen && is_compatible(at, t))
                chosen = e;
        }
        *rest = cc_skip(tok, ")");
        if (!chosen)
            chosen = dflt;
        if (!chosen)
            cc_error_tok(start, "_Generic: для этого типа нет ветки и нет default");
        return chosen;
    }
    if ((cc_equal(tok, "_alloca") || cc_equal(tok, "__builtin_alloca") ||
         (cc_equal(tok, "alloca") && !find_var(tok))) && cc_equal(tok->next, "(")) {
        // memory in the function's own stack: freed when the function returns
        if (!g_current_fn)
            cc_error_tok(tok, "_alloca бывает только внутри функции");
        g_current_fn->has_alloca = 1;
        Node *node = new_node(ND_ALLOCA, tok);
        tok = tok->next->next;
        node->lhs = assign(&tok, tok);
        node->val = 16;
        *rest = cc_skip(tok, ")");
        return node;
    }
    if ((cc_equal(tok, "__builtin_atomic_cas") || cc_equal(tok, "__builtin_atomic_exchange") ||
         cc_equal(tok, "__builtin_atomic_fetch_add")) && cc_equal(tok->next, "(")) {
        // cas(p, &expected, new): 1 and *p = new if *p was expected, else 0 and expected = *p;
        // exchange(p, v): *p = v, the old value; fetch_add(p, v): *p += v, the old value
        NodeKind k = cc_equal(tok, "__builtin_atomic_cas") ? ND_CAS :
                     cc_equal(tok, "__builtin_atomic_exchange") ? ND_EXCH : ND_FETCH_ADD;
        Node *node = new_node(k, tok);
        tok = tok->next->next;
        node->lhs = assign(&tok, tok);
        add_type(node->lhs);
        if (!node->lhs->ty->base || node->lhs->ty->base->size > 8 ||
            (node->lhs->ty->base->kind != TY_PTR && !is_integer(node->lhs->ty->base)))
            cc_error_tok(node->lhs->tok, "здесь нужен указатель на целое число или указатель");
        tok = cc_skip(tok, ",");
        node->rhs = assign(&tok, tok);
        if (k == ND_CAS) {
            tok = cc_skip(tok, ",");
            node->then = new_cast(assign(&tok, tok), node->lhs->ty->base);
        }
        if (k == ND_FETCH_ADD && node->lhs->ty->base->kind == TY_PTR)   // a pointer moves by its elements
            node->rhs = new_binary(ND_MUL, new_cast(node->rhs, ty_long),
                                   elem_size(node->lhs->ty->base->base, tok), tok);
        *rest = cc_skip(tok, ")");
        return node;
    }
    if (tok->kind == TK_IDENT && cc_equal(tok->next, "(") && cc_equal(tok->next->next, ")") &&
        (cc_equal(tok, "GetExceptionCode") || cc_equal(tok, "_exception_code") ||
         cc_equal(tok, "GetExceptionInformation") || cc_equal(tok, "_exception_info") ||
         cc_equal(tok, "AbnormalTermination") || cc_equal(tok, "_abnormal_termination")) && !find_var(tok)) {
        *rest = tok->next->next->next;
        int info = cc_equal(tok, "GetExceptionInformation") || cc_equal(tok, "_exception_info");
        int abn = cc_equal(tok, "AbnormalTermination") || cc_equal(tok, "_abnormal_termination");
        if (abn) {
            if (g_seh_ctx != 3)
                cc_error_tok(tok, "AbnormalTermination() бывает только в блоке __finally");
            return new_var_node(g_seh_abn, tok);
        }
        if (info) {
            if (g_seh_ctx != 1)
                cc_error_tok(tok, "GetExceptionInformation() бывает только в условии __except (...)");
            return new_var_node(g_seh_xptr, tok);
        }
        if (g_seh_ctx == 1) {       // the filter: ExceptionPointers->ExceptionRecord->ExceptionCode
            Node *rec = new_unary(ND_DEREF, new_cast(new_var_node(g_seh_xptr, tok), pointer_to(pointer_to(ty_uint))), tok);
            return new_unary(ND_DEREF, rec, tok);
        }
        if (g_seh_ctx == 2)
            return new_var_node(g_seh_code, tok);
        cc_error_tok(tok, "GetExceptionCode() бывает только в __except: в условии или в блоке");
    }
    if (cc_equal(tok, "__builtin_va_start")) {
        // __builtin_va_start(ap, last): ap = address of the first variadic slot
        Node *node = new_node(ND_ASSIGN, tok);
        tok = cc_skip(tok->next, "(");
        Node *ap = assign(&tok, tok);
        tok = cc_skip(tok, ",");
        assign(&tok, tok);
        *rest = cc_skip(tok, ")");
        if (!g_current_fn || !g_current_fn->va_area)
            cc_error_tok(start, "va_start вне функции с ...");
        node->lhs = ap;
        node->rhs = new_cast(new_unary(ND_ADDR, new_var_node(g_current_fn->va_area, tok), tok),
                             pointer_to(ty_char));
        return node;
    }
    if (cc_equal(tok, "__builtin_va_arg")) {
        // *(T *)((ap += 8) - 8), every slot is 8 bytes in our ABI. A struct
        // or union of another size than 1, 2, 4, 8 travels by address
        // (Microsoft x64): the slot holds a pointer, *(T *)*(char **)slot.
        tok = cc_skip(tok->next, "(");
        Node *ap = assign(&tok, tok);
        tok = cc_skip(tok, ",");
        Type *ty = typename_(&tok, tok);
        *rest = cc_skip(tok, ")");
        Node *inc = to_assign(new_binary(ND_ADD, ap, new_num(8, start), start));
        Node *slot = new_sub(new_cast(inc, pointer_to(ty_char)), new_num(8, start), start);
        if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) &&
            ty->size != 1 && ty->size != 2 && ty->size != 4 && ty->size != 8)
            slot = new_unary(ND_DEREF, new_cast(slot, pointer_to(pointer_to(ty_char))), start);
        return new_unary(ND_DEREF, new_cast(slot, pointer_to(ty)), start);
    }
    if (cc_equal(tok, "__builtin_va_end")) {
        tok = cc_skip(tok->next, "(");
        assign(&tok, tok);
        *rest = cc_skip(tok, ")");
        return new_node(ND_NULL_EXPR, start);
    }
    if (tok->kind == TK_IDENT && g_current_fn && (cc_equal(tok, "__func__") || cc_equal(tok, "__FUNCTION__"))) {
        // the name of the function it is in: static const char __func__[] = "name"
        Token *n = g_current_fn->tok;
        char *name = n ? cc_strndup(n->loc, n->len) : g_current_fn->name;
        Obj *var = new_string_literal(name, array_of(ty_char, cc_strlen(name) + 1));
        *rest = tok->next;
        return new_var_node(var, tok);
    }
    if (tok->kind == TK_IDENT) {
        VarScope *vs = find_var(tok);
        *rest = tok->next;
        if (vs) {
            if (vs->var)
                return new_var_node(vs->var, tok);
            if (vs->enum_ty)
                return new_num(vs->enum_val, tok);
        }
        if (cc_equal(tok->next, "("))
            cc_error_tok(tok, "функция %s не объявлена (нет #include с ней?)", get_ident(tok));
        cc_error_tok(tok, "неизвестное имя %s", get_ident(tok));
    }
    if (tok->kind == TK_STR) {
        // adjacent literals: "ab" "cd" -> "abcd"; L"..." is an array of wchar_t
        Token *s = join_strings(rest, tok);
        Obj *var = new_string_literal(s->str, array_of(str_elem_type(s->str_kind), s->str_len));
        return new_var_node(var, tok);
    }
    if (tok->kind == TK_NUM) {
        if (tok->too_wide)
            cc_error_tok(tok, "символ не помещается в wchar_t (16 бит в Windows): возьмите U''");
        Node *node;
        if (tok->num_ty && is_flonum(tok->num_ty)) {
            node = new_node(ND_NUM, tok);
            node->fval = tok->fval;
            node->ty = tok->num_ty;
        } else {
            node = new_num(tok->val, tok);
            if (tok->is_long)
                node->ty = tok->is_unsigned ? ty_ulong : ty_long;
            else if (tok->is_unsigned)
                node->ty = ty_uint;
            else
                node->ty = ty_int;
        }
        *rest = tok->next;
        return node;
    }
    cc_error_tok(tok, "ожидалось выражение");
}

static Token *parse_typedef(Token *tok, Type *basety)
{
    int first = 1;
    while (!cc_consume(&tok, tok, ";")) {
        if (!first)
            tok = next_declarator(tok);
        first = 0;
        Type *ty = declarator(&tok, tok, basety);
        if (!ty->name)
            cc_error_tok(tok, "у typedef нет имени");
        push_scope(get_ident(ty->name))->type_def = ty;
    }
    return tok;
}

static void create_param_lvars(Type *param)
{
    if (param) {
        create_param_lvars(param->next);
        if (!param->name)
            cc_error_tok(0, "у параметра нет имени");
        new_lvar(get_ident(param->name), param);
    }
}

static void resolve_goto_labels(void)
{
    for (Node *x = g_gotos; x; x = x->goto_next) {
        for (Node *y = g_labels; y; y = y->goto_next) {
            if (cc_streq(x->label, y->label)) {
                x->unique_label = y->unique_label;
                break;
            }
        }
        if (!x->unique_label)
            cc_error_tok(x->tok->next, "нет такой метки");
    }
    g_gotos = g_labels = 0;
}

static Obj *find_func(char *name)
{
    Scope *sc = g_scope;
    while (sc->next)
        sc = sc->next;
    for (ScopeEntry *e = sc->vars; e; e = e->next)
        if (cc_streq(e->name, name) && e->vs->var && e->vs->var->is_function && !hidden(e->vs->var))
            return e->vs->var;
    return 0;
}

// A variable of this source's file scope with that name (not a function).
static Obj *find_global_var(char *name)
{
    Scope *sc = g_scope;
    while (sc->next)
        sc = sc->next;
    for (ScopeEntry *e = sc->vars; e; e = e->next)
        if (cc_streq(e->name, name) && e->vs->var && !e->vs->var->is_function && !hidden(e->vs->var))
            return e->vs->var;
    return 0;
}

// A function with external linkage declared by an earlier source.
static Obj *find_extern_func(char *name)
{
    for (Obj *g = g_globals; g; g = g->next)
        if (g->is_function && !g->unit && cc_streq(g->name, name))
            return g;
    return 0;
}

static int is_function(Token *tok)
{
    if (cc_equal(tok, ";"))
        return 0;
    Type dummy = { 0 };
    Type *ty = declarator(&tok, tok, &dummy);
    return ty->kind == TY_FUNC;
}

// int f(a, b) int a; char *b; { ... }: the old (K&R) way, the types of the names after ")"
static Token *knr_params(Token *tok, Type *fty)
{
    while (!cc_equal(tok, "{")) {
        if (tok->kind == TK_EOF || !is_typename(tok))
            cc_error_tok(tok, "ожидалось объявление параметра или '{'");
        Type *bt = declspec(&tok, tok, 0);
        for (int first = 1; !cc_consume(&tok, tok, ";"); first = 0) {
            if (!first)
                tok = cc_skip(tok, ",");
            Type *pt = declarator(&tok, tok, bt);
            if (!pt->name)
                cc_error_tok(tok, "ожидалось имя параметра");
            Type **pp = &fty->params;
            while (*pp && !((*pp)->name && (*pp)->name->len == pt->name->len &&
                            cc_strneq((*pp)->name->loc, pt->name->loc, pt->name->len)))
                pp = &(*pp)->next;
            if (!*pp)
                cc_error_tok(pt->name, "%s нет в списке параметров", get_ident(pt->name));
            if (pt->kind == TY_ARRAY || pt->kind == TY_VLA)
                pt = pointer_to(pt->base);
            else if (pt->kind == TY_FUNC)
                pt = pointer_to(pt);
            else if (pt->kind == TY_FLOAT)
                pt = ty_double;                 // without a prototype the caller sends a double
            Type *np = copy_type(pt);
            np->name = (*pp)->name;
            np->next = (*pp)->next;
            *pp = np;
        }
    }
    return tok;
}

static Token *function(Token *tok, Type *basety, VarAttr *attr)
{
    Type *ty = declarator(&tok, tok, basety);
    if (!ty->name)
        cc_error_tok(tok, "у функции нет имени");
    if (ty->kind == TY_FUNC && !cc_equal(tok, "{") && !cc_equal(tok, ";") && !cc_equal(tok, ",") && is_typename(tok))
        tok = knr_params(tok, ty);
    char *name = get_ident(ty->name);

    int is_static = attr->is_static || (attr->is_inline && !attr->is_extern);
    if (find_global_var(name))
        cc_error_tok(ty->name, "имя %s уже занято переменной", name);
    Obj *fn = find_func(name);
    if (!fn && !is_static && (fn = find_extern_func(name))) {
        // declared by another source: the same function, now seen here too
        Scope *saved = g_scope;
        while (g_scope->next)
            g_scope = g_scope->next;
        push_scope(name)->var = fn;
        g_scope = saved;
    }
    // a prototype here keeps the type of a body in another source
    int keep_type = fn && fn->is_definition && !cc_equal(tok, "{") && fn->tok && fn->tok->unit != g_unit;
    if (fn) {
        if (!fn->is_function)
            cc_error_tok(tok, "имя уже занято переменной");
        if (fn->is_definition && cc_equal(tok, "{"))
            cc_error_tok(tok, "функция %s уже определена", name);
        fn->is_definition = fn->is_definition || cc_equal(tok, "{");
    } else {
        // functions always live in the global scope
        Scope *saved = g_scope;
        Scope *sc = g_scope;
        while (sc->next)
            sc = sc->next;
        g_scope = sc;
        fn = new_gvar(name, ty);
        g_scope = saved;
        fn->is_function = 1;
        fn->is_definition = cc_equal(tok, "{");
        fn->is_static = is_static;
        fn->unit = fn->is_static ? unit_of(ty->name) : 0;
        link_name(fn);
    }
    if (!keep_type) {
        fn->ty = ty;
        fn->tok = ty->name;
    }
    if (attr->is_export)
        fn->is_export = 1;
    if (attr->is_naked)
        fn->is_naked = 1;

    if (cc_consume(&tok, tok, ";"))
        return tok;

    Obj *saved_locals = g_locals;
    Obj *saved_fn = g_current_fn;
    g_current_fn = fn;
    g_locals = 0;
    g_seh_xptr = g_seh_code = g_seh_abn = 0;
    g_nfin = g_brk_fin = g_cont_fin = 0;
    g_seh_ctx = 0;
    enter_scope();
    create_param_lvars(ty->params);

    // struct results are returned through a hidden pointer
    fn->params = g_locals;
    if (ty->is_variadic)
        fn->va_area = new_lvar("__va_area__", array_of(ty_char, 0));

    // int f(int n, int a[n][n]): the sizes from the parameters, first thing in the body
    // (their hidden locals come after the parameters' list)
    Node *vla_init = 0, **vp = &vla_init;
    for (Type *pt = ty->params; pt; pt = pt->next)
        if (has_vla(pt)) {
            *vp = new_unary(ND_EXPR_STMT, compute_vla_size(pt, pt->name ? pt->name : tok), tok);
            add_type(*vp);
            vp = &(*vp)->next;
        }

    Token *lbrace = tok;
    tok = cc_skip(tok, "{");
    fn->body = compound_stmt(&tok, tok);
    fn->body->lbrace = lbrace;
    if (vla_init) {
        *vp = fn->body->body;
        fn->body->body = vla_init;
    }
    fn->locals = g_locals;
    leave_scope();
    resolve_goto_labels();
    g_locals = saved_locals;
    g_current_fn = saved_fn;
    return tok;
}

static Token *global_variable(Token *tok, Type *basety, VarAttr *attr)
{
    int first = 1;
    while (!cc_consume(&tok, tok, ";")) {
        if (!first)
            tok = next_declarator(tok);
        first = 0;
        Type *ty = declarator(&tok, tok, basety);
        if (!ty->name)
            cc_error_tok(tok, "у переменной нет имени");
        if (find_func(get_ident(ty->name)))
            cc_error_tok(ty->name, "имя %s уже занято функцией", get_ident(ty->name));
        Obj *var = new_gvar(get_ident(ty->name), ty);
        var->tok = ty->name;
        var->is_definition = !attr->is_extern;
        var->is_static = attr->is_static;
        var->unit = attr->is_static ? unit_of(ty->name) : 0;
        var->is_tls = attr->is_tls;
        var->is_selectany = attr->is_selectany;
        link_name(var);
        if (ty->kind == TY_VLA)
            cc_error_tok(ty->name, "массив переменной длины бывает только внутри функции");
        if (attr->align)
            var->align = attr->align;
        if (cc_equal(tok, "="))
            gvar_initializer(&tok, tok->next, var);
    }
    return tok;
}

// One file-scope variable declared several times -- "static int x; static int x;",
// "int y; int y = 5;" (tentative definitions, C11 6.9.2) -- is one object: the
// one with the initializer stays (its type, when the other's array was left
// unsized), the rest go; two initializers are an error.
static void merge_tentative(void)
{
    for (Obj *v = g_globals; v; v = v->next) {
        if (v->is_function || !v->is_definition)
            continue;
        for (Obj **pp = &v->next; *pp;) {
            Obj *w = *pp;
            if (w->is_function || !w->is_definition || !cc_streq(w->name, v->name)) {
                pp = &w->next;
                continue;
            }
            if (v->is_selectany || w->is_selectany) {      // __declspec(selectany): the first one stays
                v->is_tls |= w->is_tls;
                *pp = w->next;
                continue;
            }
            if (w->is_tls != v->is_tls && w->tok)
                cc_error_tok(w->tok, "%s объявлена и потоковой, и обычной", w->name);
            if (w->init_data && v->init_data) {
                if (w->tok) cc_error_tok(w->tok, "переменная %s задана дважды", w->name);
                cc_error_at("", 0, "переменная %s задана дважды", w->name);
            }
            if (w->init_data) {
                v->init_data = w->init_data;
                v->rel = w->rel;
                v->ty = w->ty;
            } else if (!v->init_data && w->ty->size > v->ty->size)
                v->ty = w->ty;
            if (w->align > v->align)
                v->align = w->align;
            *pp = w->next;
        }
    }
}

// A new source begins: an empty file scope with the built-in typedefs.
static void begin_file_scope(void)
{
    g_global_scope.next = 0;
    g_global_scope.vars = 0;
    g_global_scope.tags = 0;
    g_scope = &g_global_scope;

    // DyOS built-in typedefs
    push_scope("size_t")->type_def = ty_ulong;
    push_scope("ptrdiff_t")->type_def = ty_long;
    push_scope("intptr_t")->type_def = ty_long;
    push_scope("uintptr_t")->type_def = ty_ulong;
    push_scope("__builtin_va_list")->type_def = pointer_to(ty_char);
    push_scope("int8_t")->type_def = ty_char;
    push_scope("uint8_t")->type_def = ty_uchar;
    push_scope("int16_t")->type_def = ty_short;
    push_scope("uint16_t")->type_def = ty_ushort;
    push_scope("int32_t")->type_def = ty_int;
    push_scope("uint32_t")->type_def = ty_uint;
    push_scope("int64_t")->type_def = ty_long;
    push_scope("uint64_t")->type_def = ty_ulong;
}

Obj *cc_parse(Token *tok)
{
    g_globals = 0;
    g_locals = 0;
    g_current_fn = 0;
    g_gotos = g_labels = 0;
    g_brk_label = g_cont_label = 0;
    g_current_switch = 0;
    g_unique_id = 0;
    g_unit = -1;
    cc_file_asm = 0;
    begin_file_scope();
    while (tok->kind != TK_EOF) {
        if (unit_of(tok) != g_unit) {       // the next source begins: its own file scope
            g_unit = unit_of(tok);
            g_pack = 0;
            begin_file_scope();
        }
        if (take_pack(&tok, tok))
            continue;
        if (cc_equal(tok, "asm") || cc_equal(tok, "__asm__")) {    // file-scope asm("..."): text as is
            Node *a = asm_stmt(&tok, tok);
            // an error in it names the C file and line (asm.c, .srcfile)
            char *text = cc_join(cc_format(".srcfile %d %s", a->tok->line, a->tok->file ? a->tok->file : "?"),
                                 "\n", a->asm_str);
            cc_file_asm = cc_file_asm ? cc_join(cc_file_asm, "\n", text) : text;
            continue;
        }
        if (cc_equal(tok, "}"))
            cc_error_tok(tok, "лишняя '}': ей нет пары '{'");
        if (is_static_assert(tok)) {
            tok = static_assertion(tok);
            continue;
        }
        VarAttr attr = { 0 };
        Type *basety = declspec(&tok, tok, &attr);
        if (attr.is_typedef) {
            tok = parse_typedef(tok, basety);
            continue;
        }
        if (is_function(tok)) {
            tok = function(tok, basety, &attr);
            continue;
        }
        tok = global_variable(tok, basety, &attr);
    }
    merge_tentative();
    return g_globals;
}
