// Code generator: AST -> x86-64 assembly text (Intel syntax), which asm.c
// then turns into machine code. Stack-machine style: every expression leaves
// its value in rax (integers, pointers) or xmm0 (float, double).
// Calling convention: Microsoft x64, the same as the kernel, so programs
// call kernel functions directly and the other way round.
#include "cc.h"

typedef struct {
    char *buf;
    int len, cap;
} TextBuf;

static TextBuf g_out;
static int g_depth;             // 8-byte pushes since the prologue
static int g_label_count;
static Obj *g_fn;
static int g_ret_ptr_offset;    // rbp offset of the hidden struct-return pointer, 0 if none
static Node *g_funclets[256];   // __try of this function: filters and __finally blocks, written after it
static int g_nfunclets;
static int g_any_seh;           // some function has __try: the handler's thunk is written once

static void out_raw(const char *s, int n)
{
    if (g_out.len + n + 1 > g_out.cap) {
        int cap = g_out.cap ? g_out.cap * 2 : 1 << 16;
        while (cap < g_out.len + n + 1)
            cap *= 2;
        char *nb = cc_alloc((size_t)cap);
        for (int i = 0; i < g_out.len; i++)
            nb[i] = g_out.buf[i];
        g_out.buf = nb;
        g_out.cap = cap;
    }
    for (int i = 0; i < n; i++)
        g_out.buf[g_out.len++] = s[i];
    g_out.buf[g_out.len] = 0;
}

int cc_vformat(char *buf, int cap, const char *fmt, va_list ap);

static int fmt(char *buf, int cap, const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    int n = cc_vformat(buf, cap, f, ap);
    va_end(ap);
    return n < cap ? n : cap - 1;
}

static void println(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = cc_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof(buf))
        n = (int)sizeof(buf) - 1;
    out_raw(buf, n);
    out_raw("\n", 1);
}

static int count(void)
{
    return g_label_count++;
}

// ---- debug info: the line table (asm.c writes it into a PE as .dydbg) ----
// .dbgfile N name   the program's file number N (as the compiler was given it)
// .loc N LINE       the code from here on is that line of file N
// .dbgfunc name / .dbgend   where a C function's code begins and ends
// A .loc goes before every statement, a loop's condition and step, a
// function's "{" (its first byte) and "}" (the epilogue). Lines of the
// system's own headers (and <builtin>) are not the program's: none there.
// Frames: every C function begins with "push rbp; mov rbp, rsp" and ends with
// "mov rsp, rbp; pop rbp; ret" (emit_text), so [rbp] is the caller's rbp and
// [rbp+8] the return address -- the debugger (debug.c) walks the stack so.
static const char *g_dbg_files[256];
static int g_dbg_nfiles;
static const char *g_dbg_last;          // the last file looked up, and its number
static int g_dbg_last_n;

static int dbg_file(const char *name)
{
    if (!name || name[0] == '<')
        return -1;
    if (name == g_dbg_last)
        return g_dbg_last_n;
    int n = -1;
    for (int i = 0; i < g_dbg_nfiles && n < 0; i++)
        if (cc_streq(g_dbg_files[i], name))
            n = i;
    if (n < 0) {
        if (cc_builtin_header(name) || g_dbg_nfiles == 256)
            return -1;
        n = g_dbg_nfiles++;
        g_dbg_files[n] = name;
        char head[32];
        int k = fmt(head, sizeof(head), ".dbgfile %d ", n);
        out_raw(head, k);
        out_raw(name, cc_strlen(name));
        out_raw("\n", 1);
    }
    g_dbg_last = name;
    g_dbg_last_n = n;
    return n;
}

static void dbg_loc(Token *tok)
{
    if (!tok || !g_fn)
        return;
    int f = dbg_file(tok->file);
    if (f >= 0)
        println(".loc %d %d", f, tok->line);
}

static void push(void)
{
    println("  push rax");
    g_depth++;
}

static void pop(const char *reg)
{
    println("  pop %s", reg);
    g_depth--;
}

static void pushf(void)
{
    println("  sub rsp, 8");
    println("  movsd qword ptr [rsp], xmm0");
    g_depth++;
}

static void popf(int reg)
{
    println("  movsd xmm%d, qword ptr [rsp]", reg);
    println("  add rsp, 8");
    g_depth--;
}

static int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static int is_small_struct(Type *ty)
{
    return (ty->kind == TY_STRUCT || ty->kind == TY_UNION) &&
           (ty->size == 1 || ty->size == 2 || ty->size == 4 || ty->size == 8);
}

static int is_big_struct(Type *ty)
{
    return (ty->kind == TY_STRUCT || ty->kind == TY_UNION) && !is_small_struct(ty);
}

static void gen_expr(Node *node);
static void gen_stmt(Node *node);
static void cmp_zero(Type *ty);
static const char *ptr_of(int size);
typedef struct { Node *ptr, *index; int scale; i64 disp; } ElemAddr;    // [base+index*scale+disp]
static int elem_addr(Node *lv, ElemAddr *e);
static char *elem_operand(ElemAddr *e, int leaf_index);
static void gen_void(Node *n);
static void drop_casts_one(Node *n);

// Address of an lvalue -> rax.
static void gen_addr(Node *node)
{
    switch (node->kind) {
    case ND_VAR:
        if (node->var->reg)
            cc_error_tok(node->tok, "внутренняя ошибка: адрес переменной в регистре");
        if (node->var->is_local && node->var->indirect)     // a VLA, an over-aligned local: its slot holds the address
            println("  mov rax, qword ptr [rbp%+d]", node->var->offset);
        else if (node->var->is_local)
            println("  lea rax, [rbp%+d]", node->var->offset);
        else if (node->var->is_tls) {
            // this thread's copy: TEB.ThreadLocalStoragePointer (gs:[0x58]) [_tls_index] + the offset in .tls
            println("  db 101, 72, 139, 4, 37, 88, 0, 0, 0");      // mov rax, gs:[0x58]
            println("  mov ecx, dword ptr [rip+__wcc_tls_index]");
            println("  mov rax, qword ptr [rax+rcx*8]");
            println("  lea rax, [rax+@%s]", node->var->name);
        } else
            println("  lea rax, [rip+@%s]", node->var->name);   // @: a C name, even "dx" or "word"
        return;
    case ND_VLA_PTR:
        println("  lea rax, [rbp%+d]", node->var->offset);
        return;
    case ND_DEREF: {
        ElemAddr e;
        if (elem_addr(node, &e) && (e.index || e.disp)) {
            println("  lea rax, %s", elem_operand(&e, 0));
            return;
        }
        gen_expr(node->lhs);
        return;
    }
    case ND_COMMA:
        gen_void(node->lhs);
        gen_addr(node->rhs);
        return;
    case ND_MEMBER:
        gen_addr(node->lhs);
        if (node->member->offset)
            println("  add rax, %d", node->member->offset);
        return;
    case ND_FUNCALL:
        if (node->ret_buffer) {
            gen_expr(node);
            return;
        }
        break;
    case ND_ASSIGN:
    case ND_COND:
    case ND_STMT_EXPR:
        if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION) {
            gen_expr(node);
            return;
        }
        break;
    default:
        break;
    }
    cc_error_tok(node->tok, "это не адрес (не lvalue)");
}

// Value at address rax -> rax / xmm0.
static void load(Type *ty)
{
    switch (ty->kind) {
    case TY_ARRAY:
    case TY_VLA:
    case TY_STRUCT:
    case TY_UNION:
    case TY_FUNC:
        return;             // the address is the value
    case TY_FLOAT:
        println("  movss xmm0, dword ptr [rax]");
        return;
    case TY_DOUBLE:
        println("  movsd xmm0, qword ptr [rax]");
        return;
    default:
        break;
    }
    const char *ext = ty->is_unsigned ? "movzx" : "movsx";
    if (ty->size == 1)
        println("  %s eax, byte ptr [rax]", ext);
    else if (ty->size == 2)
        println("  %s eax, word ptr [rax]", ext);
    else if (ty->size == 4)
        println(ty->is_unsigned ? "  mov eax, dword ptr [rax]" : "  movsxd rax, dword ptr [rax]");
    else
        println("  mov rax, qword ptr [rax]");
}

// A bit field out of its unit in rax: its bits moved to the top, then back down, with the
// sign of a signed field (a _Bool is never signed).
static void bitfield_extract(Member *m)
{
    int up = 64 - m->bit_width - m->bit_offset, down = 64 - m->bit_width;
    if (up)
        println("  shl rax, %d", up);
    if (down)
        println(m->ty->is_unsigned || m->ty->kind == TY_BOOL ? "  shr rax, %d" : "  sar rax, %d", down);
}

// Store rax / xmm0 to the address on top of the stack.
static void store(Type *ty)
{
    pop("rcx");
    switch (ty->kind) {
    case TY_STRUCT:
    case TY_UNION:
        println("  mov r8, rax");
        println("  push rsi");
        println("  push rdi");
        println("  mov rsi, rax");
        println("  mov rdi, rcx");
        println("  mov rcx, %d", ty->size);
        println("  rep movsb");
        println("  pop rdi");
        println("  pop rsi");
        println("  mov rax, r8");
        return;
    case TY_FLOAT:
        println("  movss dword ptr [rcx], xmm0");
        return;
    case TY_DOUBLE:
        println("  movsd qword ptr [rcx], xmm0");
        return;
    default:
        break;
    }
    if (ty->size == 1)
        println("  mov byte ptr [rcx], al");
    else if (ty->size == 2)
        println("  mov word ptr [rcx], ax");
    else if (ty->size == 4)
        println("  mov dword ptr [rcx], eax");
    else
        println("  mov qword ptr [rcx], rax");
}

// rax after an operation of that size: the value as its type has it
static void extend(Type *ty)
{
    if (ty->size == 1)
        println(ty->is_unsigned || ty->kind == TY_BOOL ? "  movzx eax, al" : "  movsx eax, al");
    else if (ty->size == 2)
        println(ty->is_unsigned ? "  movzx eax, ax" : "  movsx eax, ax");
    else if (ty->size == 4 && !ty->is_unsigned)
        println("  movsxd rax, eax");
    else if (ty->size == 4)
        println("  mov eax, eax");
}

static const char *reg_of(int size, int which)     // which: 0 rax, 1 rdx
{
    static const char *a[] = { "al", "ax", "eax", "rax" }, *d[] = { "dl", "dx", "edx", "rdx" };
    int i = size == 1 ? 0 : size == 2 ? 1 : size == 4 ? 2 : 3;
    return which ? d[i] : a[i];
}

static const char *ptr_of(int size)
{
    return size == 1 ? "byte" : size == 2 ? "word" : size == 4 ? "dword" : "qword";
}

// asm text; @x of a local becomes its place, [rbp-...]
static void emit_asm(Node *node)
{
    const char *s = node->asm_str;
    for (const char *p = s; *p;) {
        AsmRef *r = 0;
        if (*p == '@')
            for (r = node->asm_refs; r && r->name != p + 1; r = r->next)
                ;
        if (r) {
            char buf[32];
            int n = fmt(buf, sizeof(buf), "[rbp%+d]", r->var->offset);
            out_raw(buf, n);
            p += 1 + r->len;
            continue;
        }
        const char *q = p + 1;
        while (*q && *q != '@')
            q++;
        out_raw(p, (int)(q - p));
        p = q;
    }
    out_raw("\n", 1);
}

// a __finally block as a funclet: rcx 0 (the normal way out), rdx our frame
static void call_funclet(const char *label)
{
    int room = 32 + (g_depth % 2) * 8;          // rsp 16-aligned at the call
    println("  sub rsp, %d", room);
    println("  xor ecx, ecx");
    println("  mov rdx, rbp");
    println("  call %s", label);
    println("  add rsp, %d", room);
}

static void cmp_zero(Type *ty)
{
    if (ty->kind == TY_FLOAT) {
        println("  xorps xmm1, xmm1");
        println("  ucomiss xmm0, xmm1");
        return;
    }
    if (ty->kind == TY_DOUBLE) {
        println("  xorpd xmm1, xmm1");
        println("  ucomisd xmm0, xmm1");
        return;
    }
    if (is_integer(ty) && ty->size <= 4)
        println("  test eax, eax");
    else
        println("  test rax, rax");
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64 };

static int get_type_id(Type *ty)
{
    switch (ty->kind) {
    case TY_CHAR: return ty->is_unsigned ? U8 : I8;
    case TY_SHORT: return ty->is_unsigned ? U16 : I16;
    case TY_INT: return ty->is_unsigned ? U32 : I32;
    case TY_LONG: return ty->is_unsigned ? U64 : I64;
    case TY_FLOAT: return F32;
    case TY_DOUBLE: return F64;
    default: return U64;
    }
}

// Casts between the ten scalar kinds; "" = nothing to do.
static const char *g_cast_table[10][10] = {
    // to: i8, i16, i32, i64, u8, u16, u32, u64, f32, f64
    /* i8  */ { "", "", "", "movsxd rax, eax", "movzx eax, al", "movzx eax, ax", "", "movsxd rax, eax",
                "cvtsi2ss xmm0, eax", "cvtsi2sd xmm0, eax" },
    /* i16 */ { "movsx eax, al", "", "", "movsxd rax, eax", "movzx eax, al", "movzx eax, ax", "",
                "movsxd rax, eax", "cvtsi2ss xmm0, eax", "cvtsi2sd xmm0, eax" },
    /* i32 */ { "movsx eax, al", "movsx eax, ax", "", "movsxd rax, eax", "movzx eax, al", "movzx eax, ax", "",
                "movsxd rax, eax", "cvtsi2ss xmm0, eax", "cvtsi2sd xmm0, eax" },
    /* i64 */ { "movsx eax, al", "movsx eax, ax", "", "", "movzx eax, al", "movzx eax, ax", "mov eax, eax",
                "", "cvtsi2ss xmm0, rax", "cvtsi2sd xmm0, rax" },
    /* u8  */ { "movsx eax, al", "", "", "movsxd rax, eax", "", "", "", "movsxd rax, eax",
                "cvtsi2ss xmm0, eax", "cvtsi2sd xmm0, eax" },
    /* u16 */ { "movsx eax, al", "movsx eax, ax", "", "movsxd rax, eax", "movzx eax, al", "", "", "movsxd rax, eax",
                "cvtsi2ss xmm0, eax", "cvtsi2sd xmm0, eax" },
    /* u32 */ { "movsx eax, al", "movsx eax, ax", "", "mov eax, eax", "movzx eax, al", "movzx eax, ax", "",
                "mov eax, eax", "mov eax, eax\n  cvtsi2ss xmm0, rax", "mov eax, eax\n  cvtsi2sd xmm0, rax" },
    /* u64 */ { "movsx eax, al", "movsx eax, ax", "", "", "movzx eax, al", "movzx eax, ax", "mov eax, eax", "",
                "U64F32", "U64F64" },
    /* f32 */ { "cvttss2si eax, xmm0\n  movsx eax, al", "cvttss2si eax, xmm0\n  movsx eax, ax",
                "cvttss2si eax, xmm0", "cvttss2si rax, xmm0", "cvttss2si eax, xmm0\n  movzx eax, al",
                "cvttss2si eax, xmm0\n  movzx eax, ax", "cvttss2si rax, xmm0\n  mov eax, eax",
                "F32U64", "", "cvtss2sd xmm0, xmm0" },
    /* f64 */ { "cvttsd2si eax, xmm0\n  movsx eax, al", "cvttsd2si eax, xmm0\n  movsx eax, ax",
                "cvttsd2si eax, xmm0", "cvttsd2si rax, xmm0", "cvttsd2si eax, xmm0\n  movzx eax, al",
                "cvttsd2si eax, xmm0\n  movzx eax, ax", "cvttsd2si rax, xmm0\n  mov eax, eax",
                "F64U64", "cvtsd2ss xmm0, xmm0", "" },
};

// float/double -> unsigned 64 bits: values from 2^63 up are converted less 2^63
// and get the top bit back (cvttsd2si alone gives 0x8000000000000000 for them)
static void float_to_u64(int is_double)
{
    int c = count();
    println("  mov rcx, %s", is_double ? "4890909195324358656" : "1593835520");   // 2^63: 0x43E0000000000000 / 0x5F000000
    println("  movq xmm1, rcx");
    println("  %s xmm0, xmm1", is_double ? "comisd" : "comiss");
    println("  jae .L.f2u.big.%d", c);
    println("  %s rax, xmm0", is_double ? "cvttsd2si" : "cvttss2si");
    println("  jmp .L.f2u.end.%d", c);
    println(".L.f2u.big.%d:", c);
    println("  %s xmm0, xmm1", is_double ? "subsd" : "subss");
    println("  %s rax, xmm0", is_double ? "cvttsd2si" : "cvttss2si");
    println("  mov rcx, 1");
    println("  shl rcx, 63");
    println("  xor rax, rcx");
    println(".L.f2u.end.%d:", c);
}

static void u64_to_float(int is_double)
{
    // halve with rounding bit when the top bit is set, convert, double
    int c = count();
    const char *cvt = is_double ? "cvtsi2sd" : "cvtsi2ss";
    const char *add = is_double ? "addsd" : "addss";
    println("  test rax, rax");
    println("  js .L.u2f.neg.%d", c);
    println("  %s xmm0, rax", cvt);
    println("  jmp .L.u2f.end.%d", c);
    println(".L.u2f.neg.%d:", c);
    println("  mov rcx, rax");
    println("  shr rcx, 1");
    println("  and eax, 1");
    println("  or rcx, rax");
    println("  %s xmm0, rcx", cvt);
    println("  %s xmm0, xmm0", add);
    println(".L.u2f.end.%d:", c);
}

static void cast(Type *from, Type *to)
{
    if (to->kind == TY_VOID)
        return;
    if (to->kind == TY_BOOL) {
        cmp_zero(from);
        println("  setne al");
        println("  movzx eax, al");
        return;
    }
    int t1 = get_type_id(from), t2 = get_type_id(to);
    const char *s = g_cast_table[t1][t2];
    if (cc_streq(s, "U64F32"))
        u64_to_float(0);
    else if (cc_streq(s, "U64F64"))
        u64_to_float(1);
    else if (cc_streq(s, "F32U64"))
        float_to_u64(0);
    else if (cc_streq(s, "F64U64"))
        float_to_u64(1);
    else if (*s)
        println("  %s", s);
}

// ---- shorter code: registers for locals, places named directly, constants counted here, operands
// without the stack ----

// The registers of locals: callee-saved, so a call keeps them (Obj.reg: 1..7)
static const char *g_reg64[8] = { 0, "rbx", "rsi", "rdi", "r12", "r13", "r14", "r15" };
static const char *g_reg32[8] = { 0, "ebx", "esi", "edi", "r12d", "r13d", "r14d", "r15d" };
static const char *g_reg16[8] = { 0, "bx", "si", "di", "r12w", "r13w", "r14w", "r15w" };
static const char *g_reg8[8] = { 0, "bl", "sil", "dil", "r12b", "r13b", "r14b", "r15b" };
static const int g_reg_num[8] = { 0, 3, 6, 7, 12, 13, 14, 15 };   // their numbers in the unwind codes

// A place the code names directly: a register, [rbp-N] for a local, [rip+@name+N] for a global of
// this unit (a field of either too). Not: a VLA or over-aligned local, a thread's variable, a bit field,
// a global of another unit (it may be a DLL's).
typedef struct { int ok, rip, off, reg; const char *name; } Place;

static Place place_of(Node *node)
{
    Place p = { 0 };
    if (node->kind == ND_VAR) {
        Obj *v = node->var;
        if (v->is_local) {
            if (v->indirect)
                return p;
            p.ok = 1;
            p.reg = v->reg;
            p.off = v->offset;
            return p;
        }
        if (v->is_tls || !v->is_definition || v->is_function)
            return p;
        p.ok = p.rip = 1;
        p.name = v->name;
        return p;
    }
    if (node->kind == ND_MEMBER && !node->member->is_bitfield) {
        p = place_of(node->lhs);
        if (p.reg)
            p.ok = 0;
        p.off += node->member->offset;
        return p;
    }
    return p;
}

static char *place_text(Place p)
{
    if (!p.rip)
        return cc_format("[rbp%+d]", p.off);
    return p.off ? cc_format("[rip+@%s%+d]", p.name, p.off) : cc_format("[rip+@%s]", p.name);
}

static int is_int_or_ptr(Type *ty)
{
    return ty && (is_integer(ty) || ty->kind == TY_PTR);
}

// a value as its type keeps it: cut to the size, with the sign of a signed type
static i64 fit_type(i64 v, Type *ty)
{
    if (ty->kind == TY_BOOL)
        return v != 0;
    switch (ty->size) {
    case 1: return ty->is_unsigned ? (i64)(u8)v : (i64)(signed char)v;
    case 2: return ty->is_unsigned ? (i64)(unsigned short)v : (i64)(short)v;
    case 4: return ty->is_unsigned ? (i64)(u32)v : (i64)(int)v;
    default: return v;
    }
}

// An integer expression the compiler counts itself (numbers, casts, + - * & | ^ ~ << >>): 1 and *v
static int const_int(Node *n, i64 *v)
{
    if (!is_int_or_ptr(n->ty))
        return 0;
    i64 a, b;
    switch (n->kind) {
    case ND_NUM:
        a = n->val;
        break;
    case ND_CAST:
        if (!const_int(n->lhs, &a))
            return 0;
        break;
    case ND_NEG:
    case ND_BITNOT:
        if (!const_int(n->lhs, &a))
            return 0;
        a = n->kind == ND_NEG ? -a : ~a;
        break;
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_BITAND:
    case ND_BITOR:
    case ND_BITXOR:
    case ND_SHL:
    case ND_SHR:
        if (n->ty->base || !const_int(n->lhs, &a) || !const_int(n->rhs, &b))
            return 0;
        switch (n->kind) {
        case ND_ADD: a = (i64)((u64)a + (u64)b); break;
        case ND_SUB: a = (i64)((u64)a - (u64)b); break;
        case ND_MUL: a = (i64)((u64)a * (u64)b); break;
        case ND_BITAND: a &= b; break;
        case ND_BITOR: a |= b; break;
        case ND_BITXOR: a ^= b; break;
        case ND_SHL:
            if (b < 0 || b > 63) return 0;
            a = (i64)((u64)a << b);
            break;
        default:
            if (b < 0 || b > 63) return 0;
            a = n->lhs->ty->is_unsigned ? (i64)((u64)a >> b) : a >> b;
            break;
        }
        break;
    default:
        return 0;
    }
    *v = fit_type(a, n->ty);
    return 1;
}

static void emit_int(i64 v)
{
    if (v == 0)
        println("  xor eax, eax");
    else if ((u64)v <= 0xFFFFFFFFull)
        println("  mov eax, %u", (u32)v);
    else
        println("  mov rax, %lld", v);
}

// rax / xmm0 = the value at a place (an array, a struct: its address). A register holds its local
// as the store below left it: 8 bytes whole, 4 as they are, 1 and 2 widened to 4 as the type wants.
static void load_mem(Type *ty, const char *addr);
static void store_mem(Type *ty, const char *addr);

static void load_at(Type *ty, Place p)
{
    if (p.reg) {
        if (ty->size == 8)
            println("  mov rax, %s", g_reg64[p.reg]);
        else if (ty->size == 4 && !ty->is_unsigned)
            println("  movsxd rax, %s", g_reg32[p.reg]);
        else
            println("  mov eax, %s", g_reg32[p.reg]);
        return;
    }
    load_mem(ty, place_text(p));
}

// rax / xmm0 = the value at an address the instruction names ("[rbp-8]", "[rbx+rax*4]")
static void load_mem(Type *ty, const char *addr)
{
    switch (ty->kind) {
    case TY_ARRAY:
    case TY_VLA:
    case TY_STRUCT:
    case TY_UNION:
    case TY_FUNC:
        println("  lea rax, %s", addr);
        return;
    case TY_FLOAT:
        println("  movss xmm0, dword ptr %s", addr);
        return;
    case TY_DOUBLE:
        println("  movsd xmm0, qword ptr %s", addr);
        return;
    default:
        break;
    }
    const char *ext = ty->is_unsigned ? "movzx" : "movsx";
    if (ty->size == 1)
        println("  %s eax, byte ptr %s", ext, addr);
    else if (ty->size == 2)
        println("  %s eax, word ptr %s", ext, addr);
    else if (ty->size == 4)
        println(ty->is_unsigned ? "  mov eax, dword ptr %s" : "  movsxd rax, dword ptr %s", addr);
    else
        println("  mov rax, qword ptr %s", addr);
}

// rax / xmm0 to a place
static void store_at(Type *ty, Place p)
{
    if (p.reg) {
        const char *ext = ty->is_unsigned || ty->kind == TY_BOOL ? "movzx" : "movsx";
        if (ty->size == 8)
            println("  mov %s, rax", g_reg64[p.reg]);
        else if (ty->size == 4)
            println("  mov %s, eax", g_reg32[p.reg]);
        else if (ty->size == 2)
            println("  %s %s, ax", ext, g_reg32[p.reg]);
        else
            println("  %s %s, al", ext, g_reg32[p.reg]);
        return;
    }
    store_mem(ty, place_text(p));
}

static void store_mem(Type *ty, const char *addr)
{
    if (ty->kind == TY_FLOAT)
        println("  movss dword ptr %s, xmm0", addr);
    else if (ty->kind == TY_DOUBLE)
        println("  movsd qword ptr %s, xmm0", addr);
    else if (ty->size == 1)
        println("  mov byte ptr %s, al", addr);
    else if (ty->size == 2)
        println("  mov word ptr %s, ax", addr);
    else if (ty->size == 4)
        println("  mov dword ptr %s, eax", addr);
    else
        println("  mov qword ptr %s, rax", addr);
}

// A value read without rax and without the stack: a number, an integer variable (widened as a cast does)
static int is_leaf(Node *n)
{
    i64 v;
    if (const_int(n, &v))
        return 1;
    if (n->kind == ND_CAST && is_int_or_ptr(n->ty) && is_int_or_ptr(n->lhs->ty) && n->ty->kind != TY_BOOL &&
        n->ty->size >= n->lhs->ty->size)
        n = n->lhs;
    return (n->kind == ND_VAR || n->kind == ND_MEMBER) && is_int_or_ptr(n->ty) && place_of(n).ok;
}

// The operand of a leaf for "op rax, X": the number itself when imm (it fits 32 bits), a register the
// local lives in, else rcx / ecx loaded here (always rcx when in_rcx: a shift wants cl)
static const char *leaf_operand(Node *n, int wide, int imm, int in_rcx)
{
    i64 v;
    if (const_int(n, &v)) {
        if (imm && v >= -2147483647 - 1 && v <= 2147483647)
            return cc_format("%lld", v);
        if (wide)
            println("  mov rcx, %lld", v);
        else
            println("  mov ecx, %u", (u32)v);
        return wide ? "rcx" : "ecx";
    }
    if (n->kind == ND_CAST)
        n = n->lhs;
    Type *ty = n->ty;
    Place p = place_of(n);
    if (p.reg) {
        if (wide && ty->size <= 4 && !ty->is_unsigned) {      // a signed one widened to 64 bits
            println("  movsxd rcx, %s", g_reg32[p.reg]);
            return "rcx";
        }
        if (in_rcx || (wide && ty->size < 8)) {
            println(ty->size == 8 ? "  mov rcx, %s" : "  mov ecx, %s", ty->size == 8 ? g_reg64[p.reg] : g_reg32[p.reg]);
            return wide ? "rcx" : "ecx";
        }
        return wide ? g_reg64[p.reg] : g_reg32[p.reg];
    }
    const char *addr = place_text(p);
    const char *ext = ty->is_unsigned ? "movzx" : "movsx";
    if (ty->size == 1)
        println("  %s ecx, byte ptr %s", ext, addr);
    else if (ty->size == 2)
        println("  %s ecx, word ptr %s", ext, addr);
    else if (ty->size == 4)
        println(ty->is_unsigned ? "  mov ecx, dword ptr %s" : "  movsxd rcx, dword ptr %s", addr);
    else
        println("  mov rcx, qword ptr %s", addr);
    return wide ? "rcx" : "ecx";
}

// A local in a register read as it is: its name when the operation has its width (else 0)
static const char *reg_leaf(Node *n, int wide)
{
    if (n->kind != ND_VAR || !is_int_or_ptr(n->ty))
        return 0;
    Place p = place_of(n);
    if (!p.reg)
        return 0;
    if (wide)
        return n->ty->size == 8 ? g_reg64[p.reg] : 0;
    return n->ty->size <= 4 ? g_reg32[p.reg] : 0;
}

static int commutes(int kind)
{
    return kind == ND_ADD || kind == ND_MUL || kind == ND_BITAND || kind == ND_BITOR || kind == ND_BITXOR ||
           kind == ND_EQ || kind == ND_NE || kind == ND_LT || kind == ND_LE;
}

// The two sides of an integer operation: the left to rax, the right as an operand (a number, a register).
// *swapped: the sides changed places (only where the operation allows it; a comparison turns round).
static const char *int_operands(Node *node, int wide, int imm, int in_rcx, int *swapped)
{
    *swapped = 0;
    if (is_leaf(node->rhs)) {
        gen_expr(node->lhs);
        return leaf_operand(node->rhs, wide, imm, in_rcx);
    }
    if (commutes(node->kind) && is_leaf(node->lhs)) {
        gen_expr(node->rhs);
        *swapped = 1;
        return leaf_operand(node->lhs, wide, imm, in_rcx);
    }
    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("rcx");
    return wide ? "rcx" : "ecx";
}

static int is_number(const char *op) { return (*op >= '0' && *op <= '9') || *op == '-'; }

static i64 parse_i64(const char *s)
{
    int neg = *s == '-';
    if (neg) s++;
    u64 v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (u64)(*s++ - '0');
    return neg ? -(i64)v : (i64)v;
}

// the condition of jcc / setcc for == != < <= (turned round when swapped, inverted when negate)
static const char *cond_code(int kind, int is_unsigned, int swapped, int negate)
{
    int k = kind == ND_EQ ? 0 : kind == ND_NE ? 1 : kind == ND_LT ? 2 : 3;     // e ne lt le gt ge
    if (swapped && k >= 2)
        k += 2;
    if (negate) {
        static const int neg[6] = { 1, 0, 5, 4, 3, 2 };
        k = neg[k];
    }
    static const char *s[6] = { "e", "ne", "l", "le", "g", "ge" }, *u[6] = { "e", "ne", "b", "be", "a", "ae" };
    return is_unsigned ? u[k] : s[k];
}

static int is_compare(Node *n)
{
    return (n->kind == ND_EQ || n->kind == ND_NE || n->kind == ND_LT || n->kind == ND_LE) &&
           !is_flonum(n->lhs->ty);
}

// Jump to label when the condition is true (when = 1) or false (when = 0): a comparison goes straight
// to the jump, && and || become jumps too.
static void gen_branch(Node *c, const char *label, int when)
{
    if (c->kind == ND_NOT) {
        gen_branch(c->lhs, label, !when);
        return;
    }
    if (c->kind == ND_LOGAND || c->kind == ND_LOGOR) {
        int is_and = c->kind == ND_LOGAND;
        if (when != is_and) {       // && to false, || to true: either side decides
            gen_branch(c->lhs, label, when);
            gen_branch(c->rhs, label, when);
        } else {                    // && to true, || to false: both sides must agree
            char *skip = cc_format(".L.skip.%d", count());
            gen_branch(c->lhs, skip, !when);
            gen_branch(c->rhs, label, when);
            println("%s:", skip);
        }
        return;
    }
    if (is_compare(c)) {
        int wide = c->lhs->ty->kind == TY_LONG || c->lhs->ty->base;
        int swapped = 0;
        i64 kv;
        const char *left = reg_leaf(c->lhs, wide), *op;
        if (!left && const_int(c->lhs, &kv) && reg_leaf(c->rhs, wide) && kv >= -2147483647 && kv <= 2147483647) {
            println("  cmp %s, %lld", reg_leaf(c->rhs, wide), kv);       // 0 <= j: j >= 0
            println("  j%s %s", cond_code(c->kind, c->lhs->ty->is_unsigned, 1, !when), label);
            return;
        }
        if (left && is_leaf(c->rhs))
            op = leaf_operand(c->rhs, wide, 1, 0);
        else if (left) {
            gen_expr(c->rhs);
            op = wide ? "rax" : "eax";
        } else {
            left = wide ? "rax" : "eax";
            op = int_operands(c, wide, 1, 0, &swapped);
        }
        println("  cmp %s, %s", left, op);
        println("  j%s %s", cond_code(c->kind, c->lhs->ty->is_unsigned, swapped, !when), label);
        return;
    }
    i64 v;
    if (const_int(c, &v)) {         // while (1), if (0)
        if ((v != 0) == when)
            println("  jmp %s", label);
        return;
    }
    Place p;
    if ((c->kind == ND_VAR || c->kind == ND_MEMBER) && is_int_or_ptr(c->ty) && (p = place_of(c)).reg) {
        println(c->ty->size == 8 ? "  test %s, %s" : "  test %s, %s", c->ty->size == 8 ? g_reg64[p.reg] : g_reg32[p.reg],
                c->ty->size == 8 ? g_reg64[p.reg] : g_reg32[p.reg]);
        println(when ? "  jne %s" : "  je %s", label);
        return;
    }
    gen_expr(c);
    cmp_zero(c->ty);
    println(when ? "  jne %s" : "  je %s", label);
}


// ---- an element through a pointer as one memory operand: [base+index*scale+disp] ----
// *(p + i*s), *(p + k), p->x, a[i].y: p a pointer in a register or in memory, an array of the frame or
// a global one; s 1, 2, 4 or 8. The index goes to rax (any expression) or, with leaf_index, to rcx
// (only a variable or a number: rax holds the value to store then). rdx holds a base that is not in a
// register already.
// (to) n: a cast node made here, as the parser makes them
static Node *new_cast_node(Node *n, Type *to)
{
    Node *c = cc_alloc(sizeof(Node));
    c->kind = ND_CAST;
    c->tok = n->tok;
    c->lhs = n;
    c->ty = to;
    return c;
}

static int elem_addr(Node *lv, ElemAddr *e)
{
    e->ptr = e->index = 0;
    e->scale = 1;
    e->disp = 0;
    while (lv->kind == ND_MEMBER) {
        if (lv->member->is_bitfield)
            return 0;
        e->disp += lv->member->offset;
        lv = lv->lhs;
    }
    if (lv->kind != ND_DEREF)
        return 0;
    Node *a = lv->lhs;
    i64 k;
    if (a->kind == ND_ADD && a->ty->base && a->lhs->ty->base && is_int_or_ptr(a->rhs->ty)) {
        Node *i = a->rhs;
        if (i->kind == ND_CAST && i->ty->size == 8 && is_integer(i->lhs->ty))     // the index made a pointer's size
            i = i->lhs;
        if (!is_integer(i->ty))
            return 0;
        if (const_int(i, &k)) {
            e->disp += k;
        } else if (i->kind == ND_MUL && const_int(i->rhs, &k) && (k == 1 || k == 2 || k == 4 || k == 8) &&
                   i->lhs->ty->size == 8) {
            e->index = i->lhs;
            e->scale = (int)k;
        } else if (i->ty->size == 8) {
            e->index = i;
        } else
            return 0;
        a = a->lhs;
    }
    // (long)(j + k): the k goes to the displacement (a signed int does not overflow)
    while (e->index && e->index->kind == ND_CAST && e->index->ty->size == 8) {
        Node *in = e->index->lhs;
        if (!((in->kind == ND_ADD || in->kind == ND_SUB) && is_integer(in->ty) && !in->ty->is_unsigned &&
              in->ty->size == 4 && const_int(in->rhs, &k) && k > -65536 && k < 65536))
            break;
        Node *c = new_cast_node(in->lhs, e->index->ty);
        e->disp += (in->kind == ND_ADD ? k : -k) * e->scale;
        e->index = c;
    }
    if (e->disp < -0x40000000 || e->disp > 0x40000000)
        return 0;
    // the base: a pointer variable, or an array variable (its address)
    if (a->kind != ND_VAR || !a->var->is_local && (a->var->is_tls || !a->var->is_definition))
        return 0;
    if (a->ty->kind == TY_PTR && place_of(a).ok)
        return e->ptr = a, 1;
    if (a->ty->kind == TY_ARRAY && place_of(a).ok && !a->var->reg)
        return e->ptr = a, 1;
    return 0;
}

// The code that makes the operand ready, and its text
static char *elem_operand(ElemAddr *e, int leaf_index)
{
    const char *index = 0;
    if (e->index) {
        if (leaf_index || is_leaf(e->index))
            index = leaf_operand(e->index, 1, 0, 0);       // its register, or rcx
        else {
            gen_expr(e->index);
            index = "rax";
        }
    }
    Place p = place_of(e->ptr);
    const char *base;
    i64 disp = e->disp;
    if (e->ptr->ty->kind == TY_ARRAY && !p.rip) {        // an array of the frame: rbp and its offset
        base = "rbp";
        disp += p.off;
    } else if (p.reg) {
        base = g_reg64[p.reg];
    } else if (e->ptr->ty->kind == TY_ARRAY) {
        println("  lea rdx, %s", place_text(p));
        base = "rdx";
    } else {
        println("  mov rdx, qword ptr %s", place_text(p));
        base = "rdx";
    }
    if (!index)
        return disp ? cc_format("[%s%+lld]", base, disp) : cc_format("[%s]", base);
    if (disp)
        return cc_format("[%s+%s*%d%+lld]", base, index, e->scale, disp);
    return cc_format("[%s+%s*%d]", base, index, e->scale);
}

static int elem_leaf_index(ElemAddr *e)
{
    return !e->index || is_leaf(e->index);
}

static int is_scalar(Type *ty)
{
    return ty->kind != TY_STRUCT && ty->kind != TY_UNION && ty->kind != TY_ARRAY && ty->kind != TY_VLA &&
           ty->kind != TY_FUNC;
}

// ---- an expression whose value is not used: assignments go straight to their place ----

static int same_place(Node *a, Node *b)
{
    if (a->kind != b->kind)
        return 0;
    if (a->kind == ND_VAR)
        return a->var == b->var;
    if (a->kind == ND_MEMBER)
        return a->member == b->member && same_place(a->lhs, b->lhs);
    return 0;
}

// x = number, x = x op leaf (op: + - & | ^ << >>), with x a variable: one instruction. 1 = done.
static int assign_direct(Node *n)
{
    Node *lv = n->lhs, *r = n->rhs;
    Type *ty = lv->ty;
    if (!is_int_or_ptr(ty))
        return 0;
    while (r->kind == ND_CAST && is_int_or_ptr(r->ty) && is_int_or_ptr(r->lhs->ty) && r->ty->kind != TY_BOOL &&
           r->ty->size == r->lhs->ty->size)
        r = r->lhs;                     // the same bits
    Place p = place_of(lv);
    if (!p.ok)
        return 0;
    const char *dst = p.reg ? (ty->size == 8 ? g_reg64[p.reg] : g_reg32[p.reg])
                            : cc_format("%s ptr %s", ptr_of(ty->size), place_text(p));
    i64 v;
    if (const_int(r, &v)) {
        if (p.reg) {
            if (v == 0)
                println("  xor %s, %s", g_reg32[p.reg], g_reg32[p.reg]);
            else if (ty->size == 8)
                println("  mov %s, %lld", dst, v);
            else
                println("  mov %s, %d", dst, (int)v);
            return 1;
        }
        if (ty->size == 8 && (v < -2147483647 - 1 || v > 2147483647))
            return 0;
        println("  mov %s, %lld", dst, ty->size == 1 ? (i64)(u8)v : ty->size == 2 ? (i64)(unsigned short)v : v);
        return 1;
    }
    int k = r->kind;
    if (p.reg && ty->size >= 4) {
        int wide = ty->size == 8;
        const char *src = reg_leaf(r, wide);
        if (src) {                                              // x = y
            println("  mov %s, %s", dst, src);
            return 1;
        }
        if ((k == ND_ADD || k == ND_SUB) && r->ty->size == ty->size && const_int(r->rhs, &v) &&
            v > -0x40000000 && v < 0x40000000 && (src = reg_leaf(r->lhs, wide)) && !same_place(r->lhs, lv)) {
            Place q = place_of(r->lhs);                         // x = y + k: one lea
            println("  lea %s, [%s%+lld]", dst, g_reg64[q.reg], k == ND_ADD ? v : -v);
            return 1;
        }
    }
    if (!(k == ND_ADD || k == ND_SUB || k == ND_BITAND || k == ND_BITOR || k == ND_BITXOR || k == ND_SHL || k == ND_SHR))
        return 0;
    if (r->ty->size != ty->size || !same_place(r->lhs, lv))
        return 0;
    if (p.reg && ty->size < 4)          // a register keeps a char widened: x + 1 must not run past it
        return 0;
    if (!is_leaf(r->rhs)) {             // x = x op (expression): the expression, then one instruction
        if (k == ND_SHL || k == ND_SHR)
            return 0;
        static const char *ins[] = { "add", "sub", "and", "or", "xor" };
        int i = k == ND_ADD ? 0 : k == ND_SUB ? 1 : k == ND_BITAND ? 2 : k == ND_BITOR ? 3 : 4;
        gen_expr(r->rhs);
        println("  %s %s, %s", ins[i], dst, reg_of(ty->size, 0));
        return 1;
    }
    if ((k == ND_SHL || k == ND_SHR) && !const_int(r->rhs, &v))
        return 0;
    int wide = ty->size == 8;
    const char *op = leaf_operand(r->rhs, wide, 1, 0);
    if (!is_number(op) && !p.reg && ty->size < 4)
        op = ty->size == 1 ? "cl" : "cx";           // a variable of a char is in rcx then
    static const char *names[] = { "add", "sub", "and", "or", "xor" };
    const char *ins = k == ND_ADD ? names[0] : k == ND_SUB ? names[1] : k == ND_BITAND ? names[2] :
                      k == ND_BITOR ? names[3] : k == ND_BITXOR ? names[4] : k == ND_SHL ? "shl" :
                      r->lhs->ty->is_unsigned ? "shr" : "sar";
    if (k == ND_SHL || k == ND_SHR)
        op = cc_format("%d", (int)(v & 63));
    println("  %s %s, %s", ins, dst, op);
    return 1;
}

static void gen_void(Node *n)
{
    switch (n->kind) {
    case ND_CAST:
        gen_void(n->lhs);           // a conversion does nothing more
        return;
    case ND_COMMA:
        gen_void(n->lhs);
        gen_void(n->rhs);
        return;
    case ND_ADD:
    case ND_SUB: {
        i64 v;
        if (is_int_or_ptr(n->ty) && const_int(n->rhs, &v)) {      // "(i = i + 1) - 1" of i++
            gen_void(n->lhs);
            return;
        }
        break;
    }
    case ND_ASSIGN:
        if (assign_direct(n))
            return;
        break;
    default:
        break;
    }
    gen_expr(n);
}

static int log2_exact(i64 v)
{
    if (v <= 0 || (v & (v - 1)))
        return -1;
    int k = 0;
    while ((1ll << k) != v)
        k++;
    return k;
}

// ---- casts that change nothing (int to int, a pointer to a pointer) go away before the code ----
// The parser converts both sides of an operation to their common type even when it is theirs already.

static int same_bits(Type *a, Type *b)
{
    if (!is_int_or_ptr(a) || !is_int_or_ptr(b) || a->kind == TY_BOOL || b->kind == TY_BOOL)
        return 0;
    if ((a->kind == TY_PTR) != (b->kind == TY_PTR))
        return 0;
    return a->size == b->size && a->is_unsigned == b->is_unsigned;
}

static Node *drop_cast(Node *n)
{
    while (n && n->kind == ND_CAST && same_bits(n->ty, n->lhs->ty))
        n = n->lhs;
    return n;
}

static void drop_casts(Node *n)
{
    for (; n; n = n->next) {
        n->lhs = drop_cast(n->lhs);
        n->rhs = drop_cast(n->rhs);
        n->cond = drop_cast(n->cond);
        drop_casts_one(n->lhs);
        drop_casts_one(n->rhs);
        drop_casts_one(n->cond);
        drop_casts_one(n->then);
        drop_casts_one(n->els);
        drop_casts_one(n->init);
        drop_casts_one(n->inc);
        drop_casts(n->body);
        drop_casts(n->args);
    }
}

static void drop_casts_one(Node *n)
{
    if (!n)
        return;
    Node *next = n->next;
    n->next = 0;
    drop_casts(n);
    n->next = next;
}

// ---- which locals live in registers ----
// A local goes into a register when it is an integer or a pointer whose address is never taken; the
// ones used most (a use in a loop counts 8 times, in a loop in a loop 64...) get the 7 registers.
// Not in a function with __try (its funclets read the frame), asm() (it may name a local or a register),
// setjmp (a longjmp would bring back old register values), variable arguments, or naked.

typedef struct { Obj *var; long long weight; int bad; } RegUse;
static RegUse g_ru[512];
static int g_nru;
static int g_ra_off;            // something in the function forbids registers

static RegUse *reg_use(Obj *v)
{
    for (int i = 0; i < g_nru; i++)
        if (g_ru[i].var == v)
            return &g_ru[i];
    if (g_nru == 512)
        return 0;
    RegUse *u = &g_ru[g_nru++];
    u->var = v;
    u->weight = 0;
    u->bad = 0;
    return u;
}

static void ra_walk(Node *n, long long w);

static void ra_list(Node *n, long long w)
{
    for (; n; n = n->next)
        ra_walk(n, w);
}

// the local whose address an lvalue is (a.b.c -> a), or 0
static Obj *addr_owner(Node *n)
{
    while (n && n->kind == ND_MEMBER)
        n = n->lhs;
    return n && n->kind == ND_VAR && n->var->is_local ? n->var : 0;
}

static void ra_walk(Node *n, long long w)
{
    if (!n || g_ra_off)
        return;
    switch (n->kind) {
    case ND_ASM:
    case ND_TRY:
        g_ra_off = 1;
        return;
    case ND_VAR:
        if (n->var->is_local) {
            RegUse *u = reg_use(n->var);
            if (u) u->weight += w; else g_ra_off = 1;
        }
        return;
    case ND_ADDR: {
        Obj *v = addr_owner(n->lhs);
        if (v) {
            RegUse *u = reg_use(v);
            if (u) u->bad = 1; else g_ra_off = 1;
        }
        break;
    }
    case ND_MEMZERO:
    case ND_VLA_PTR:
        if (n->var) {
            RegUse *u = reg_use(n->var);
            if (u) u->bad = 1; else g_ra_off = 1;
        }
        break;
    case ND_FUNCALL:
        if (n->lhs && n->lhs->kind == ND_VAR) {
            const char *f = n->lhs->var->name;
            for (const char *q = f; *q; q++)
                if (cc_strneq(q, "setjmp", 6) || cc_strneq(q, "sigsetjmp", 9))
                    g_ra_off = 1;
        }
        if (n->ret_buffer) {
            RegUse *u = reg_use(n->ret_buffer);
            if (u) u->bad = 1;
        }
        break;
    case ND_FOR:
    case ND_DO:
        ra_walk(n->init, w);
        w = w < (1ll << 40) ? w * 8 : w;
        break;
    default:
        break;
    }
    ra_walk(n->lhs, w);
    ra_walk(n->rhs, w);
    ra_walk(n->cond, w);
    ra_walk(n->then, w);
    ra_walk(n->els, w);
    if (n->kind != ND_FOR && n->kind != ND_DO)
        ra_walk(n->init, w);
    ra_walk(n->inc, w);
    ra_list(n->body, w);
    ra_list(n->args, w);
}

static int reg_candidate(Obj *fn, Obj *v)
{
    Type *ty = v->ty;
    if (v->indirect || v->is_tls || v == fn->va_area || !is_int_or_ptr(ty) || ty->is_atomic)
        return 0;
    return ty->size == 1 || ty->size == 2 || ty->size == 4 || ty->size == 8;
}

static void alloc_regs(Obj *fn)
{
    fn->nsave = fn->save_pad = 0;
    for (Obj *v = fn->locals; v; v = v->next)
        v->reg = 0;
    if (fn->is_naked || fn->has_seh || fn->ty->is_variadic || fn->is_variadic)
        return;
    g_nru = 0;
    g_ra_off = 0;
    ra_walk(fn->body, 1);
    if (g_ra_off)
        return;
    for (int r = 1; r <= 7; r++) {
        RegUse *best = 0;
        for (int i = 0; i < g_nru; i++) {
            RegUse *u = &g_ru[i];
            if (u->bad || u->var->reg || !u->weight || !reg_candidate(fn, u->var))
                continue;
            int listed = 0;
            for (Obj *v = fn->locals; v && !listed; v = v->next)
                listed = v == u->var;
            if (!listed)
                continue;
            if (!best || u->weight > best->weight)
                best = u;
        }
        if (!best || best->weight < 6)       // pushing and popping the register costs more
            break;
        best->var->reg = r;
        fn->save[fn->nsave++] = r;
    }
    fn->save_pad = fn->nsave & 1;       // rbp stays 16-aligned
}

// ---- function calls (Microsoft x64) ----
// Every argument occupies one 8-byte slot. Slots are pushed right to left,
// so they end up exactly where the ABI wants them: [rsp] = arg 0 (also the
// callee's home area for the first four). The first four are then also
// loaded into rcx/rdx/r8/r9 and xmm0..3.

static void push_arg(Node *arg)
{
    gen_expr(arg);
    Type *ty = arg->ty;
    if (ty->kind == TY_FLOAT) {
        println("  movd eax, xmm0");
        push();
        return;
    }
    if (ty->kind == TY_DOUBLE) {
        println("  movq rax, xmm0");
        push();
        return;
    }
    if (is_small_struct(ty)) {
        if (ty->size == 1) println("  movzx eax, byte ptr [rax]");
        else if (ty->size == 2) println("  movzx eax, word ptr [rax]");
        else if (ty->size == 4) println("  mov eax, dword ptr [rax]");
        else println("  mov rax, qword ptr [rax]");
    }
    // big structs travel by address; the callee makes its own copy
    push();
}

static void gen_funcall(Node *node)
{
    int nargs = 0;
    for (Node *a = node->args; a; a = a->next)
        nargs++;
    int hidden = node->ret_buffer && is_big_struct(node->ty);
    int total = nargs + hidden;
    int slots = total < 4 ? 4 : total;

    int pad = (g_depth + slots) % 2;
    if (pad) {
        println("  sub rsp, 8");
        g_depth++;
    }
    if (total < 4) {                    // home slots of the callee with nothing in them
        println("  sub rsp, %d", (4 - total) * 8);
        g_depth += 4 - total;
    }

    // right to left
    Node *args[256];
    int n = 0;
    for (Node *a = node->args; a && n < 256; a = a->next)
        args[n++] = a;
    for (int i = n - 1; i >= 0; i--)
        push_arg(args[i]);
    if (hidden) {
        println("  lea rax, [rbp%+d]", node->ret_buffer->offset);
        push();
    }

    // callee address
    Node *fn = node->lhs;
    int direct = fn->kind == ND_VAR && fn->var->ty->kind == TY_FUNC;
    if (!direct) {
        gen_expr(fn);
        println("  mov r10, rax");
    }

    // the first four into registers: integers into rcx..r9, floats into xmm0..3 and (for a variadic
    // callee, which reads them from there) into rcx..r9 as well
    static const char *gpr[] = { "rcx", "rdx", "r8", "r9" };
    for (int i = 0; i < 4 && i < total; i++) {
        Type *aty = hidden ? (i ? args[i - 1]->ty : 0) : args[i]->ty;
        println("  mov %s, qword ptr [rsp+%d]", gpr[i], i * 8);
        if (aty && is_flonum(aty))
            println("  movq xmm%d, qword ptr [rsp+%d]", i, i * 8);
    }

    // variadic callees on MS x64 need nothing else: floats are in both registers
    if (direct)
        println("  call @%s", fn->var->name);
    else
        println("  call r10");

    println("  add rsp, %d", (slots + pad) * 8);
    g_depth -= slots + pad;
    Type *ty = node->ty;
    if (ty->kind == TY_BOOL) {
        println("  movzx eax, al");
    } else if (is_integer(ty) && ty->size < 4) {
        if (ty->size == 1)
            println(ty->is_unsigned ? "  movzx eax, al" : "  movsx eax, al");
        else
            println(ty->is_unsigned ? "  movzx eax, ax" : "  movsx eax, ax");
    }
    if (node->ret_buffer && is_small_struct(ty)) {
        println("  lea rcx, [rbp%+d]", node->ret_buffer->offset);
        if (ty->size == 1) println("  mov byte ptr [rcx], al");
        else if (ty->size == 2) println("  mov word ptr [rcx], ax");
        else if (ty->size == 4) println("  mov dword ptr [rcx], eax");
        else println("  mov qword ptr [rcx], rax");
        println("  mov rax, rcx");
    }
}

// ---- expressions ----

static void gen_expr(Node *node)
{
    i64 k;
    if (node->kind != ND_NUM && const_int(node, &k)) {
        emit_int(k);
        return;
    }
    switch (node->kind) {
    case ND_NULL_EXPR:
        return;
    case ND_NUM:
        if (node->ty->kind == TY_FLOAT) {
            float f = (float)node->fval;
            u32 bits = *(u32 *)&f;
            println("  mov eax, %u", bits);
            println("  movd xmm0, eax");
            return;
        }
        if (node->ty->kind == TY_DOUBLE) {
            double d = node->fval;
            u64 bits = *(u64 *)&d;
            println("  mov rax, %llu", bits);
            println("  movq xmm0, rax");
            return;
        }
        if (node->val == 0)
            println("  xor eax, eax");
        else if ((u64)node->val <= 0xFFFFFFFFull)
            println("  mov eax, %u", (u32)node->val);
        else
            println("  mov rax, %lld", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        if (node->ty->kind == TY_FLOAT) {
            println("  mov eax, 0x80000000");
            println("  movd xmm1, eax");
            println("  xorps xmm0, xmm1");
            return;
        }
        if (node->ty->kind == TY_DOUBLE) {
            println("  mov rax, 0x8000000000000000");
            println("  movq xmm1, rax");
            println("  xorpd xmm0, xmm1");
            return;
        }
        println("  neg rax");
        return;
    case ND_VAR:
    case ND_MEMBER: {
        Place p = place_of(node);
        if (p.ok) {
            load_at(node->ty, p);
            return;
        }
        ElemAddr e;
        if (node->kind == ND_MEMBER && elem_addr(node, &e)) {
            load_mem(node->ty, elem_operand(&e, 0));
            return;
        }
        gen_addr(node);
        load(node->ty);
        if (node->kind == ND_MEMBER && node->member->is_bitfield)
            bitfield_extract(node->member);
        return;
    }
    case ND_DEREF: {
        ElemAddr e;
        if (elem_addr(node, &e)) {
            load_mem(node->ty, elem_operand(&e, 0));
            return;
        }
        gen_expr(node->lhs);
        load(node->ty);
        return;
    }
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_ASSIGN: {
        Place p = place_of(node->lhs);
        Type *lty = node->lhs->ty;
        if (p.ok && lty->kind != TY_STRUCT && lty->kind != TY_UNION && lty->kind != TY_ARRAY) {
            gen_expr(node->rhs);
            store_at(lty, p);
            return;
        }
        ElemAddr e;
        if (is_scalar(lty) && elem_addr(node->lhs, &e) && elem_leaf_index(&e)) {
            gen_expr(node->rhs);
            store_mem(lty, elem_operand(&e, 1));
            return;
        }
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
            // the new bits into the unit, the other bits as they were
            Member *m = node->lhs->member;
            u64 mask = m->bit_width == 64 ? ~0ull : (1ull << m->bit_width) - 1;
            println("  mov r8, rax");
            println("  mov r9, %lld", (long long)mask);
            println("  and r8, r9");
            if (m->bit_offset)
                println("  shl r8, %d", m->bit_offset);
            println("  mov rax, qword ptr [rsp]");
            load(m->ty);
            println("  mov r9, %lld", (long long)~(mask << m->bit_offset));
            println("  and rax, r9");
            println("  or rax, r8");
            store(m->ty);
            bitfield_extract(m);                    // the value of "a.x = v" is what a.x now holds
            return;
        }
        store(node->ty);
        return;
    }
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next) {
            if (!n->next && n->kind == ND_EXPR_STMT)
                gen_expr(n->lhs);           // the last one is the value of ({ ... })
            else
                gen_stmt(n);
        }
        return;
    case ND_COMMA:
        gen_void(node->lhs);
        gen_expr(node->rhs);
        return;
    case ND_CAST: {
        gen_expr(node->lhs);
        Type *f = node->lhs->ty, *t = node->ty;
        int loaded = node->lhs->kind == ND_VAR || node->lhs->kind == ND_MEMBER || node->lhs->kind == ND_DEREF;
        if (loaded && is_integer(f) && f->kind != TY_BOOL && f->size == 4 && is_int_or_ptr(t) && t->size == 8 &&
            !(node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield))
            return;                     // a 4-byte load is widened already (movsxd / mov eax)
        cast(f, t);
        return;
    }
    case ND_MEMZERO:
        println("  push rdi");
        println(node->var->indirect ? "  mov rdi, qword ptr [rbp%+d]" : "  lea rdi, [rbp%+d]", node->var->offset);
        println("  mov rcx, %d", node->var->ty->size);
        println("  xor eax, eax");
        println("  rep stosb");
        println("  pop rdi");
        return;
    case ND_COND: {
        int c = count();
        gen_branch(node->cond, cc_format(".L.else.%d", c), 0);
        gen_expr(node->then);
        println("  jmp .L.end.%d", c);
        println(".L.else.%d:", c);
        gen_expr(node->els);
        println(".L.end.%d:", c);
        return;
    }
    case ND_NOT:
        gen_expr(node->lhs);
        cmp_zero(node->lhs->ty);
        println("  sete al");
        println("  movzx eax, al");
        return;
    case ND_BITNOT:
        gen_expr(node->lhs);
        println("  not rax");
        return;
    case ND_LOGAND: {
        int c = count();
        char *f = cc_format(".L.false.%d", c);
        gen_branch(node->lhs, f, 0);
        gen_branch(node->rhs, f, 0);
        println("  mov eax, 1");
        println("  jmp .L.end.%d", c);
        println(".L.false.%d:", c);
        println("  xor eax, eax");
        println(".L.end.%d:", c);
        return;
    }
    case ND_LOGOR: {
        int c = count();
        char *t = cc_format(".L.true.%d", c);
        gen_branch(node->lhs, t, 1);
        gen_branch(node->rhs, t, 1);
        println("  xor eax, eax");
        println("  jmp .L.end.%d", c);
        println(".L.true.%d:", c);
        println("  mov eax, 1");
        println(".L.end.%d:", c);
        return;
    }
    case ND_FUNCALL:
        gen_funcall(node);
        return;
    case ND_ASM:
        emit_asm(node);
        return;
    case ND_ALLOCA: {
        // rsp goes down by the size; what the expression has pushed so far (g_depth slots) moves
        // down with it, the memory is above them. Each page on the way is touched first: Windows
        // grows the stack only through its guard page.
        int c = count();
        int align = node->val < 16 ? 16 : (int)node->val;
        gen_expr(node->lhs);
        println("  add rax, %d", 15 + (align > 16 ? align - 16 : 0));
        println("  and rax, -16");
        println("  mov r10, rsp");
        println("  mov r11, rsp");
        println("  sub r11, rax");
        println("  mov rdx, rsp");
        println(".L.alloca.probe.%d:", c);
        println("  sub rdx, 4096");
        println("  cmp rdx, r11");
        println("  jb .L.alloca.done.%d", c);
        println("  mov cl, byte ptr [rdx]");
        println("  jmp .L.alloca.probe.%d", c);
        println(".L.alloca.done.%d:", c);
        println("  mov cl, byte ptr [r11]");
        println("  mov rsp, r11");
        for (int i = 0; i < g_depth; i++) {
            println("  mov rcx, qword ptr [r10+%d]", i * 8);
            println("  mov qword ptr [rsp+%d], rcx", i * 8);
        }
        println("  lea rax, [rsp+%d]", g_depth * 8);
        if (align > 16) {
            println("  add rax, %d", align - 1);
            println("  and rax, %d", -align);
        }
        return;
    }
    case ND_CAS: {
        // lock cmpxchg: 1 if *p was the expected and is the new now; else 0, the expected gets *p
        int c = count();
        int sz = node->lhs->ty->base->size;
        gen_expr(node->lhs);
        push();
        gen_expr(node->then);
        push();
        gen_expr(node->rhs);
        println("  mov r8, rax");
        pop("rdx");
        pop("rcx");
        println("  mov %s, %s ptr [r8]", reg_of(sz, 0), ptr_of(sz));
        println("  lock cmpxchg %s ptr [rcx], %s", ptr_of(sz), reg_of(sz, 1));
        println("  je .L.cas.ok.%d", c);
        println("  mov %s ptr [r8], %s", ptr_of(sz), reg_of(sz, 0));
        println("  xor eax, eax");
        println("  jmp .L.cas.end.%d", c);
        println(".L.cas.ok.%d:", c);
        println("  mov eax, 1");
        println(".L.cas.end.%d:", c);
        return;
    }
    case ND_EXCH:
    case ND_FETCH_ADD: {
        int sz = node->ty->size;
        gen_expr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("rcx");
        if (node->kind == ND_EXCH)
            println("  xchg %s ptr [rcx], %s", ptr_of(sz), reg_of(sz, 0));
        else
            println("  lock xadd %s ptr [rcx], %s", ptr_of(sz), reg_of(sz, 0));
        extend(node->ty);
        return;
    }
    default:
        break;
    }

    if (is_flonum(node->lhs->ty)) {
        gen_expr(node->rhs);
        pushf();
        gen_expr(node->lhs);
        popf(1);        // xmm0 = lhs, xmm1 = rhs
        const char *sz = node->lhs->ty->kind == TY_FLOAT ? "ss" : "sd";
        switch (node->kind) {
        case ND_ADD: println("  add%s xmm0, xmm1", sz); return;
        case ND_SUB: println("  sub%s xmm0, xmm1", sz); return;
        case ND_MUL: println("  mul%s xmm0, xmm1", sz); return;
        case ND_DIV: println("  div%s xmm0, xmm1", sz); return;
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
            println("  ucomi%s xmm1, xmm0", sz);
            if (node->kind == ND_EQ) {
                println("  sete al");
                println("  setnp dl");
                println("  and al, dl");
            } else if (node->kind == ND_NE) {
                println("  setne al");
                println("  setp dl");
                println("  or al, dl");
            } else if (node->kind == ND_LT) {
                println("  seta al");
            } else {
                println("  setae al");
            }
            println("  and al, 1");
            println("  movzx eax, al");
            return;
        default:
            cc_error_tok(node->tok, "неверная операция над числами с точкой");
        }
    }

    int wide = node->lhs->ty->kind == TY_LONG || node->lhs->ty->base;
    const char *ax = wide ? "rax" : "eax";
    const char *dx = wide ? "rdx" : "edx";
    int swapped;
    // unsigned division by a power of two: a shift, a mask
    i64 dv;
    if ((node->kind == ND_DIV || node->kind == ND_MOD) && node->ty->is_unsigned && const_int(node->rhs, &dv) &&
        log2_exact(dv) >= 0 && dv <= 0x7FFFFFFF) {
        gen_expr(node->lhs);
        if (node->kind == ND_DIV) {
            if (dv > 1)
                println("  shr %s, %d", ax, log2_exact(dv));
        } else
            println("  and %s, %lld", ax, dv - 1);
        return;
    }
    // rax = lhs, op = rhs: a number (when the instruction takes one), a register; a shift wants cl
    int imm = node->kind != ND_DIV && node->kind != ND_MOD;
    int shift = node->kind == ND_SHL || node->kind == ND_SHR;
    const char *op = int_operands(node, wide, imm, shift, &swapped);
    int num = is_number(op);
    long long v = num ? parse_i64(op) : 0;

    switch (node->kind) {
    case ND_ADD: println("  add %s, %s", ax, op); return;
    case ND_SUB: println("  sub %s, %s", ax, op); return;
    case ND_MUL:
        if (num && v == 1)
            return;
        if (num && log2_exact(v) > 0)
            println("  shl %s, %d", ax, log2_exact(v));
        else if (num)
            println("  imul %s, %s, %s", ax, ax, op);
        else
            println("  imul %s, %s", ax, op);
        return;
    case ND_DIV:
    case ND_MOD:
        if (node->ty->is_unsigned) {
            println("  xor edx, edx");
            println("  div %s", op);
        } else {
            println(wide ? "  cqo" : "  cdq");
            println("  idiv %s", op);
        }
        if (node->kind == ND_MOD)
            println("  mov %s, %s", ax, dx);
        return;
    case ND_BITAND: println("  and %s, %s", ax, op); return;
    case ND_BITOR: println("  or %s, %s", ax, op); return;
    case ND_BITXOR: println("  xor %s, %s", ax, op); return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        println("  cmp %s, %s", ax, op);
        println("  set%s al", cond_code(node->kind, node->lhs->ty->is_unsigned, swapped, 0));
        println("  movzx eax, al");
        return;
    case ND_SHL:
        println("  shl %s, %s", ax, num ? cc_format("%d", (int)(v & 63)) : "cl");
        return;
    case ND_SHR:
        println(node->lhs->ty->is_unsigned ? "  shr %s, %s" : "  sar %s, %s", ax, num ? cc_format("%d", (int)(v & 63)) : "cl");
        return;
    default:
        break;
    }
    cc_error_tok(node->tok, "неверное выражение");
}

// ---- statements ----

static void gen_stmt(Node *node)
{
    if (node->kind != ND_BLOCK)
        dbg_loc(node->tok);
    switch (node->kind) {
    case ND_IF: {
        int c = count();
        gen_branch(node->cond, cc_format(".L.else.%d", c), 0);
        gen_stmt(node->then);
        println("  jmp .L.end.%d", c);
        println(".L.else.%d:", c);
        if (node->els)
            gen_stmt(node->els);
        println(".L.end.%d:", c);
        return;
    }
    case ND_FOR: {
        int c = count();
        if (node->init)
            gen_stmt(node->init);
        if (node->cond && !node->var) {
            // the condition at the bottom: one jump a round (into it first)
            println("  jmp .L.cond.%d", c);
            println(".L.body.%d:", c);
            gen_stmt(node->then);
            println("%s:", node->cont_label);
            if (node->inc) {
                dbg_loc(node->tok);
                gen_void(node->inc);
            }
            println(".L.cond.%d:", c);
            dbg_loc(node->tok);
            gen_branch(node->cond, cc_format(".L.body.%d", c), 1);
            println("%s:", node->brk_label);
            return;
        }
        if (node->var)
            println("  mov qword ptr [rbp%+d], rsp", node->var->offset);
        println(".L.begin.%d:", c);
        dbg_loc(node->tok);             // each round comes back to the loop's line
        if (node->cond)
            gen_branch(node->cond, node->brk_label, 0);
        gen_stmt(node->then);
        println("%s:", node->cont_label);
        if (node->var)
            println("  mov rsp, qword ptr [rbp%+d]", node->var->offset);
        if (node->inc) {
            dbg_loc(node->tok);
            gen_void(node->inc);
        }
        println("  jmp .L.begin.%d", c);
        println("%s:", node->brk_label);
        if (node->var)
            println("  mov rsp, qword ptr [rbp%+d]", node->var->offset);
        return;
    }
    case ND_DO: {
        int c = count();
        if (node->var)
            println("  mov qword ptr [rbp%+d], rsp", node->var->offset);
        println(".L.begin.%d:", c);
        gen_stmt(node->then);
        println("%s:", node->cont_label);
        if (node->var)
            println("  mov rsp, qword ptr [rbp%+d]", node->var->offset);
        dbg_loc(node->cond->tok);       // the line of "while (...)"
        gen_branch(node->cond, cc_format(".L.begin.%d", c), 1);
        println("%s:", node->brk_label);
        if (node->var)
            println("  mov rsp, qword ptr [rbp%+d]", node->var->offset);
        return;
    }
    case ND_SWITCH: {
        gen_expr(node->cond);
        const char *ax = node->cond->ty->size == 8 ? "rax" : "eax";
        const char *cx = node->cond->ty->size == 8 ? "rcx" : "ecx";
        for (Node *n = node->case_next; n; n = n->case_next) {
            if (n->begin == n->end) {
                println("  mov rcx, %lld", n->begin);
                println("  cmp %s, %s", ax, cx);
                println("  je %s", n->label);
                continue;
            }
            // range: (x - begin) <= end - begin, unsigned
            println("  mov rdx, rax");
            println("  mov rcx, %lld", n->begin);
            println("  sub rdx, rcx");
            println("  mov rcx, %lld", n->end - n->begin);
            println(node->cond->ty->size == 8 ? "  cmp rdx, rcx" : "  cmp edx, ecx");
            println("  jbe %s", n->label);
        }
        if (node->default_case)
            println("  jmp %s", node->default_case->label);
        println("  jmp %s", node->brk_label);
        gen_stmt(node->then);
        println("%s:", node->brk_label);
        return;
    }
    case ND_CASE:
        println("%s:", node->label);
        gen_stmt(node->lhs);
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_GOTO:
        for (Node *f = node->els; f; f = f->next)        // __finally blocks it leaves
            call_funclet(f->label);
        println("  jmp %s", node->unique_label);
        return;
    case ND_TRY: {
        println("  mov qword ptr [rbp%+d], rsp", node->var->offset);
        println("%s:", node->label);
        gen_stmt(node->then);
        println("%s:", node->unique_label);
        if (node->val == 0) {
            // __except: the system comes here after the unwind, eax the exception's code
            int c = count();
            println("  jmp .L.try.end.%d", c);
            println("%s:", node->cont_label);
            println("  mov rsp, qword ptr [rbp%+d]", node->var->offset);
            println("  mov dword ptr [rbp%+d], eax", g_fn->seh_code->offset);
            gen_stmt(node->els);
            println(".L.try.end.%d:", c);
            println(".seh_scope %s, %s, %s, %s", node->label, node->unique_label, node->brk_label, node->cont_label);
        } else {
            println(".seh_scope %s, %s, %s, 0", node->label, node->unique_label, node->brk_label);
            call_funclet(node->brk_label);      // the normal way out runs the __finally block too
        }
        if (g_nfunclets == 256)
            cc_error_tok(node->tok, "в одной функции больше 256 __try");
        g_funclets[g_nfunclets++] = node;
        return;
    }
    case ND_LABEL:
        println("%s:", node->unique_label);
        gen_stmt(node->lhs);
        return;
    case ND_RETURN:
        if (node->lhs) {
            gen_expr(node->lhs);
            Type *ty = node->lhs->ty;
            if (is_small_struct(ty)) {
                if (ty->size == 1) println("  movzx eax, byte ptr [rax]");
                else if (ty->size == 2) println("  movzx eax, word ptr [rax]");
                else if (ty->size == 4) println("  mov eax, dword ptr [rax]");
                else println("  mov rax, qword ptr [rax]");
            } else if (is_big_struct(ty)) {
                println("  push rsi");
                println("  push rdi");
                println("  mov rsi, rax");
                println("  mov rdi, qword ptr [rbp%+d]", g_ret_ptr_offset);
                println("  mov rcx, %d", ty->size);
                println("  rep movsb");
                println("  pop rdi");
                println("  pop rsi");
                println("  mov rax, qword ptr [rbp%+d]", g_ret_ptr_offset);
            }
        }
        if (node->els) {                // __finally blocks on the way out; the value waits
            println("  mov qword ptr [rbp%+d], rax", node->var->offset);
            println("  movq qword ptr [rbp%+d], xmm0", node->var->offset + 8);
            for (Node *f = node->els; f; f = f->next)
                call_funclet(f->label);
            println("  mov rax, qword ptr [rbp%+d]", node->var->offset);
            println("  movq xmm0, qword ptr [rbp%+d]", node->var->offset + 8);
        }
        println("  jmp .L.return.%s", g_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_void(node->lhs);
        return;
    case ND_ASM:
        emit_asm(node);
        return;
    default:
        break;
    }
    cc_error_tok(node->tok, "неверный оператор");
}

// ---- frame layout ----

static void assign_lvar_offsets(Obj *prog)
{
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (!fn->is_function || !fn->body)
            continue;

        drop_casts(fn->body);
        // locals in registers; their saved values sit between rbp and the caller's rbp
        // (wcc -O0: none -- every variable stays in memory, as a debugger likes it)
        if (!cc_no_regalloc)
            alloc_regs(fn);
        int base = 16 + 8 * (fn->nsave + fn->save_pad);
        // parameters live in the caller-provided home slots at rbp+base+8*i
        int slot = is_big_struct(fn->ty->return_ty) ? 1 : 0;
        for (Obj *p = fn->params; p; p = p->next) {
            p->offset = base + 8 * slot++;
        }
        int nslots = slot;

        int bottom = 0;
        for (Obj *var = fn->locals; var; var = var->next) {
            if (var->offset > 0)
                continue;       // parameter
            if (var == fn->va_area) {
                var->offset = base + 8 * nslots;
                continue;
            }
            // rbp is 16-aligned: a local aligned more is indirect (parse.c): its slot holds the address
            int align = var->indirect ? 8 : var->align < 1 ? 1 : var->align;
            if (var->ty->kind == TY_ARRAY && var->ty->size >= 16 && align < 16)
                align = 16;
            if (align > 16)
                align = 16;
            bottom += var->indirect ? 8 : var->ty->size > 0 ? var->ty->size : 0;
            bottom = align_to(bottom, align);
            var->offset = -bottom;
        }
        // big struct parameters get a local copy
        for (Obj *p = fn->params; p; p = p->next) {
            if (is_big_struct(p->ty)) {
                bottom += p->ty->size;
                bottom = align_to(bottom, p->ty->align > 8 ? p->ty->align : 8);
                // remember the home slot in `align`, the copy in `offset`
                p->align = p->offset;
                p->offset = -bottom;
            }
        }
        fn->stack_size = align_to(bottom, 16);
    }
}

// WinCC: a function or variable of the library (runtime\) -- it goes into a
// program only if the program reaches it (strip.c, by the ".gc" mark before it)
static int in_library(Obj *o)
{
    return cc_first_lib_unit && o->tok && o->tok->unit >= cc_first_lib_unit;
}

// ---- debug info for the WinCC debugger (wcc -g): frames, variables, types ----
// .dbgframe NSAVE PAD REG...       the registers pushed after rbp (in order) and the 8-byte pad
// .dbgvar LOC LINE TYPE NAME       a parameter (LINE 0) or local: LOC s-16 (at rbp-16), S-16 (rbp-16 holds
//                                  its address: a VLA, an over-aligned one), rrbx (in a register)
// .dbgglobal LABEL FILE LINE TYPE NAME   a global variable (FILE: its .dbgfile number)
// .dbgstruct ID SIZE s|u / .dbgfield OFF BITOFF BITS TYPE NAME   (all at the end of the text)
// TYPE: c C (char, unsigned char) b (bool) i2 i4 i8 u2 u4 u8 f4 f8 e4 (enum) v (void)
//       F (a function) *T (pointer) [N]T (array) sID (struct or union, by its .dbgstruct)
// Every function gets them, the library's too (a .lib of the programmer's own is a library as well);
// the debugger tells the program's files from WinCC's runtime\ and lib\ by their folders.
int cc_debug_info, cc_no_regalloc;
static Type **g_dbg_structs;
static int g_dbg_nstructs, g_dbg_capstructs;

static int dbg_struct_id(Type *ty)
{
    while (ty->origin)
        ty = ty->origin;
    for (int i = 0; i < g_dbg_nstructs; i++)
        if (g_dbg_structs[i] == ty)
            return i;
    if (g_dbg_nstructs == g_dbg_capstructs) {
        int cap = g_dbg_capstructs ? g_dbg_capstructs * 2 : 64;
        Type **nb = cc_alloc(sizeof(Type *) * (size_t)cap);
        for (int i = 0; i < g_dbg_nstructs; i++)
            nb[i] = g_dbg_structs[i];
        g_dbg_structs = nb;
        g_dbg_capstructs = cap;
    }
    g_dbg_structs[g_dbg_nstructs] = ty;
    return g_dbg_nstructs++;
}

static int dbg_type(char *b, int n, int cap, Type *ty)
{
    if (!ty || n > cap - 24) {
        if (n < cap - 1)
            b[n++] = '?';
        b[n] = 0;
        return n;
    }
    switch (ty->kind) {
    case TY_VOID: b[n++] = 'v'; break;
    case TY_BOOL: b[n++] = 'b'; break;
    case TY_CHAR: b[n++] = ty->is_unsigned ? 'C' : 'c'; break;
    case TY_SHORT: case TY_INT: case TY_LONG:
        n += fmt(b + n, cap - n, "%s%d", ty->is_unsigned ? "u" : "i", ty->size);
        break;
    case TY_ENUM: n += fmt(b + n, cap - n, "e%d", ty->size); break;
    case TY_FLOAT: case TY_DOUBLE: n += fmt(b + n, cap - n, "f%d", ty->size); break;
    case TY_PTR: case TY_VLA:
        b[n++] = '*';
        n = dbg_type(b, n, cap, ty->base);
        break;
    case TY_ARRAY:
        n += fmt(b + n, cap - n, "[%d]", ty->array_len);
        n = dbg_type(b, n, cap, ty->base);
        break;
    case TY_STRUCT: case TY_UNION: n += fmt(b + n, cap - n, "s%d", dbg_struct_id(ty)); break;
    case TY_FUNC: b[n++] = 'F'; break;
    default: b[n++] = '?'; break;
    }
    b[n] = 0;
    return n;
}

// a C name the program wrote (not the compiler's own __va_area__, compound literals...)
static int dbg_name_ok(const char *s)
{
    if (!s || !s[0] || (s[0] == '_' && s[1] == '_') || (s[0] >= '0' && s[0] <= '9'))
        return 0;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' || (u8)*s >= 0x80))
            return 0;
    return 1;
}

static void dbg_function(Obj *fn)
{
    char list[64];
    int n = 0;
    list[0] = 0;
    for (int i = 0; i < fn->nsave; i++)
        n += fmt(list + n, (int)sizeof(list) - n, " %s", g_reg64[fn->save[i]]);
    println(".dbgframe %d %d%s", fn->nsave, fn->save_pad, list);
    // the parameters (their own list), then the locals
    for (int pass = 0; pass < 2; pass++)
    for (Obj *v = pass ? fn->locals : fn->params; v; v = v->next) {
        if ((pass && !v->tok) || v == fn->va_area || !dbg_name_ok(v->name))      // a parameter has no token
            continue;
        if (pass) {
            int param = 0;
            for (Obj *q = fn->params; q && !param; q = q->next)
                param = q == v;
            if (param)
                continue;
        }
        char ty[256];
        dbg_type(ty, 0, (int)sizeof(ty), v->ty);
        char loc[24];
        if (v->reg)
            fmt(loc, (int)sizeof(loc), "r%s", g_reg64[v->reg]);
        else
            fmt(loc, (int)sizeof(loc), "%s%d", v->indirect ? "S" : "s", v->offset);
        println(".dbgvar %s %d %s %s", loc, pass ? v->tok->line : 0, ty, v->name);     // line 0: a parameter
    }
}

static void dbg_structs(void)
{
    if (!g_dbg_nstructs)
        return;
    println(".text");
    for (int i = 0; i < g_dbg_nstructs; i++) {      // the list grows while it goes: members name more
        Type *ty = g_dbg_structs[i];
        println(".dbgstruct %d %d %s", i, ty->size, ty->kind == TY_UNION ? "u" : "s");
        for (Member *m = ty->members; m; m = m->next) {
            char t[256];
            dbg_type(t, 0, (int)sizeof(t), m->ty);
            println(".dbgfield %d %d %d %s %s", m->offset, m->is_bitfield ? m->bit_offset : 0,
                    m->is_bitfield ? m->bit_width : 0, t, m->name ? cc_strndup(m->name->loc, m->name->len) : "-");
        }
    }
}

static void emit_data(Obj *prog)
{
    println(".data");
    const char *section = ".data";
    int any_tls = 0;
    for (Obj *var = prog; var; var = var->next) {
        if (var->is_function || !var->is_definition)
            continue;
        // Windows: zeros take no room in the file (.bss), one copy per thread (.tls)
        const char *want = var->is_tls ? ".tls" : cc_target_win && !var->init_data ? ".bss" : ".data";
        any_tls |= var->is_tls;
        if (want != section) {
            println("%s", want);
            section = want;
        }
        int align = var->align < 1 ? 1 : var->align;      // _Alignas(4096) as asked; big arrays at least 16
        if (var->ty->kind == TY_ARRAY && var->ty->size >= 16 && align < 16)
            align = 16;
        if (in_library(var) || (cc_first_lib_unit && var->name[0] == '.'))    // string literals too
            println(".gc");
        println(".align %d", align);
        println("%s:", var->name);
        if (cc_debug_info && var->tok && var->name[0] != '.' && !var->is_tls) {
            int file = dbg_file(var->tok->file);
            if (file >= 0) {
                char ty[256];
                dbg_type(ty, 0, (int)sizeof(ty), var->ty);
                println(".dbgglobal %s %d %d %s %s", var->name, file, var->tok->line, ty, cc_strndup(var->tok->loc, var->tok->len));
            }
        }
        int size = var->ty->size > 0 ? var->ty->size : 0;
        if (!var->init_data) {
            if (size)
                println(".zero %d", size);
            continue;
        }
        Reloc *rel = var->rel;
        int pos = 0;
        while (pos < size) {
            if (rel && rel->offset == pos) {
                if (rel->label) println("  dq @%s%+lld", rel->label, rel->addend);
                else println("  dq 0%+lld", rel->addend);
                rel = rel->next;
                pos += 8;
                continue;
            }
            // runs of plain bytes, 16 per line
            char line[128];
            int n = 0, k = 0;
            n += fmt(line, sizeof(line), "  db ");
            for (; pos < size && k < 16 && !(rel && rel->offset == pos); k++, pos++) {
                char num[8];
                int m = fmt(num, sizeof(num), k ? ", %d" : "%d", (int)(unsigned char)var->init_data[pos]);
                for (int i = 0; i < m; i++)
                    line[n++] = num[i];
            }
            line[n] = 0;
            println("%s", line);
        }
    }
    if (any_tls) {                      // the loader writes the program's TLS slot number here
        println(".data");
        println(".align 8");
        println("__wcc_tls_index:");
        println(".zero 8");
    }
}

// A filter or a __finally block of __try, as a function the system calls: rdx is the frame
// of its function (rbp there), rcx the exception's pointers (a filter) or 1 when it is an
// unwind (a __finally block). rbx keeps its own rsp; the unwind info says so (.seh_proc 1).
static void emit_funclet(Node *node)
{
    println(".seh_proc 1");
    println("%s:", node->brk_label);
    println("  push rbp");
    println("  push rbx");
    println("  sub rsp, 8");
    println("  mov rbx, rsp");
    println("  mov rbp, rdx");
    g_depth = 0;
    if (node->val == 0) {
        println("  mov qword ptr [rbp%+d], rcx", g_fn->seh_xptr->offset);
        gen_expr(node->cond);
    } else {
        println("  mov dword ptr [rbp%+d], ecx", g_fn->seh_abn->offset);
        gen_stmt(node->els);
    }
    if (g_depth != 0)
        cc_error_tok(node->tok, "внутренняя ошибка: стек не сошёлся в __try (%d)", g_depth);
    println("  mov rsp, rbx");
    println("  add rsp, 8");
    println("  pop rbx");
    println("  pop rbp");
    println("  ret");
    println(".seh_endproc");
}

static void emit_text(Obj *prog)
{
    println(".text");
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (!fn->is_function || !fn->is_definition || !fn->body)
            continue;
        g_fn = fn;
        if (in_library(fn))
            println(".gc");
        if (fn->is_static || in_library(fn))
            println(".local %s", fn->name);         // not in a PE's export table (nor the library's)
        if (fn->is_export)
            println(".export %s", fn->name);        // __declspec(dllexport)
        println("%s:", fn->name);
        if (cc_target_win && !fn->is_naked) {
            if (!fn->nsave)
                println(".seh_proc 0");             // unwind info: push rbp; mov rbp, rsp (asm.c, .pdata)
            else {                                  // push rbp; push the registers; [sub rsp, 8]; mov rbp, rsp
                char list[64];
                int n = 0;
                for (int i = 0; i < fn->nsave; i++)
                    n += fmt(list + n, (int)sizeof(list) - n, ", %d", g_reg_num[fn->save[i]]);
                println(".seh_proc 0, %d%s", fn->save_pad, list);
            }
        }
        g_nfunclets = 0;
        g_any_seh |= fn->has_seh;
        println(".dbgfunc %s", fn->tok ? cc_strndup(fn->tok->loc, fn->tok->len) : fn->name);   // the C name (a static's label may be "name.2")
        Token *open = fn->body->lbrace ? fn->body->lbrace : fn->tok;
        if (cc_debug_info)
            dbg_function(fn);
        // wcc -g: "{" is where the frame is made and the parameters are in place (below), so a
        // debugger stopped on it sees them; otherwise it is the function's first byte
        if (!cc_debug_info || fn->is_naked)
            dbg_loc(open);
        if (fn->is_naked) {
            // __declspec(naked): only the asm of the body; it returns by itself (ret)
            g_depth = 0;
            gen_stmt(fn->body);
            println(".dbgend");
            continue;
        }
        println("  push rbp");
        for (int i = 0; i < fn->nsave; i++)
            println("  push %s", g_reg64[fn->save[i]]);
        if (fn->save_pad)
            println("  sub rsp, 8");
        println("  mov rbp, rsp");
        int base = 16 + 8 * (fn->nsave + fn->save_pad);
        // a frame bigger than a page is touched page by page from the top first: a system that grows
        // the stack by a guard page (Windows under cctest) must not be jumped over
        for (int off = 4096; off < fn->stack_size; off += 4096)
            println("  mov qword ptr [rbp-%d], rax", off);
        if (fn->stack_size)
            println("  sub rsp, %d", fn->stack_size);
        g_depth = 0;

        // spill register arguments into the home area
        static const char *gpr[] = { "rcx", "rdx", "r8", "r9" };
        int slot = 0;
        g_ret_ptr_offset = 0;
        if (is_big_struct(fn->ty->return_ty)) {
            println("  mov qword ptr [rbp+%d], rcx", base);
            g_ret_ptr_offset = base;
            slot = 1;
        }
        Obj *p = fn->params;
        for (; slot < 4; slot++) {
            Type *ty = p ? p->ty : 0;
            if (!p && !fn->ty->is_variadic)
                break;
            if (p && p->reg) {          // a parameter that lives in a register: straight there
                static const char *g32[] = { "ecx", "edx", "r8d", "r9d" }, *g16[] = { "cx", "dx", "r8w", "r9w" },
                                  *g8[] = { "cl", "dl", "r8b", "r9b" };
                const char *ext = ty->is_unsigned || ty->kind == TY_BOOL ? "movzx" : "movsx";
                if (ty->size == 8) println("  mov %s, %s", g_reg64[p->reg], gpr[slot]);
                else if (ty->size == 4) println("  mov %s, %s", g_reg32[p->reg], g32[slot]);
                else if (ty->size == 2) println("  %s %s, %s", ext, g_reg32[p->reg], g16[slot]);
                else println("  %s %s, %s", ext, g_reg32[p->reg], g8[slot]);
            } else if (ty && is_flonum(ty))
                println("  movq qword ptr [rbp+%d], xmm%d", base + 8 * slot, slot);
            else
                println("  mov qword ptr [rbp+%d], %s", base + 8 * slot, gpr[slot]);
            if (p)
                p = p->next;
        }
        // copy big struct parameters out of the caller's memory
        for (Obj *q = fn->params; q; q = q->next) {
            if (is_big_struct(q->ty)) {
                println("  push rsi");
                println("  push rdi");
                println("  mov rsi, qword ptr [rbp+%d]", q->align);
                println("  lea rdi, [rbp%+d]", q->offset);
                println("  mov rcx, %d", q->ty->size);
                println("  rep movsb");
                println("  pop rdi");
                println("  pop rsi");
            }
        }

        // parameters that live in registers: from their home slots
        for (Obj *q = fn->params; q; q = q->next)
            if (q->reg && q->offset >= base + 32) {       // the fifth and further: from the stack
                Place mem = { 1, 0, q->offset, 0, 0 };
                load_at(q->ty, mem);
                Place r = { 1, 0, 0, q->reg, 0 };
                store_at(q->ty, r);
            }

        if (cc_debug_info)
            dbg_loc(open);
        gen_stmt(fn->body);
        if (g_depth != 0)
            cc_error_tok(fn->tok, "внутренняя ошибка: стек не сошёлся (%d)", g_depth);

        // falling off the end of main returns 0
        if (cc_streq(fn->name, "main"))
            println("  xor eax, eax");
        dbg_loc(fn->body->rbrace);      // every return passes the closing brace
        println(".L.return.%s:", fn->name);
        println("  mov rsp, rbp");
        if (fn->save_pad)
            println("  add rsp, 8");
        for (int i = fn->nsave - 1; i >= 0; i--)
            println("  pop %s", g_reg64[fn->save[i]]);
        println("  pop rbp");
        println("  ret");
        if (cc_target_win)
            println(".seh_endproc");
        println(".dbgend");
        for (int i = 0; i < g_nfunclets; i++)
            emit_funclet(g_funclets[i]);
    }
    if (g_any_seh) {
        // the handler of __try is the system's (ntdll); the unwind info names code inside the program
        println("__wcc_seh_handler:");
        println("  jmp __C_specific_handler");
    }
    g_fn = 0;
}

char *cc_codegen_text(Obj *prog)
{
    g_out.buf = 0;
    g_out.len = g_out.cap = 0;
    g_label_count = 0;
    g_dbg_nfiles = 0;
    g_dbg_last = 0;
    g_fn = 0;
    g_any_seh = 0;
    g_dbg_nstructs = 0;
    // the parser lists globals newest first; emit in source order
    Obj *rev = 0;
    for (Obj *o = prog; o;) {
        Obj *next = o->next;
        o->next = rev;
        rev = o;
        o = next;
    }
    assign_lvar_offsets(rev);
    emit_data(rev);
    emit_text(rev);
    if (cc_debug_info)
        dbg_structs();
    if (cc_file_asm) {
        println(".text");
        out_raw(cc_file_asm, cc_strlen(cc_file_asm));
        out_raw("\n", 1);
    }
    return g_out.buf ? g_out.buf : "";
}
