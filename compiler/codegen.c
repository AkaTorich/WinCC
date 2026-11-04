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

// Address of an lvalue -> rax.
static void gen_addr(Node *node)
{
    switch (node->kind) {
    case ND_VAR:
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
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
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
        println("  cmp eax, 0");
    else
        println("  cmp rax, 0");
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
    for (int i = total; i < 4; i++) {
        println("  push 0");
        g_depth++;
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

    static const char *gpr[] = { "rcx", "rdx", "r8", "r9" };
    for (int i = 0; i < 4; i++) {
        println("  mov %s, qword ptr [rsp+%d]", gpr[i], i * 8);
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
    case ND_MEMBER:
        gen_addr(node);
        load(node->ty);
        if (node->kind == ND_MEMBER && node->member->is_bitfield)
            bitfield_extract(node->member);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->ty);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_ASSIGN:
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
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_expr(node->rhs);
        return;
    case ND_CAST:
        gen_expr(node->lhs);
        cast(node->lhs->ty, node->ty);
        return;
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
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        println("  je .L.else.%d", c);
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
        gen_expr(node->lhs);
        cmp_zero(node->lhs->ty);
        println("  je .L.false.%d", c);
        gen_expr(node->rhs);
        cmp_zero(node->rhs->ty);
        println("  je .L.false.%d", c);
        println("  mov eax, 1");
        println("  jmp .L.end.%d", c);
        println(".L.false.%d:", c);
        println("  xor eax, eax");
        println(".L.end.%d:", c);
        return;
    }
    case ND_LOGOR: {
        int c = count();
        gen_expr(node->lhs);
        cmp_zero(node->lhs->ty);
        println("  jne .L.true.%d", c);
        gen_expr(node->rhs);
        cmp_zero(node->rhs->ty);
        println("  jne .L.true.%d", c);
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

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("rcx");         // rax = lhs, rcx = rhs

    int wide = node->lhs->ty->kind == TY_LONG || node->lhs->ty->base;
    const char *ax = wide ? "rax" : "eax";
    const char *cx = wide ? "rcx" : "ecx";
    const char *dx = wide ? "rdx" : "edx";

    switch (node->kind) {
    case ND_ADD: println("  add %s, %s", ax, cx); return;
    case ND_SUB: println("  sub %s, %s", ax, cx); return;
    case ND_MUL: println("  imul %s, %s", ax, cx); return;
    case ND_DIV:
    case ND_MOD:
        if (node->ty->is_unsigned) {
            println("  xor edx, edx");
            println("  div %s", cx);
        } else {
            println(wide ? "  cqo" : "  cdq");
            println("  idiv %s", cx);
        }
        if (node->kind == ND_MOD)
            println("  mov %s, %s", ax, dx);
        return;
    case ND_BITAND: println("  and %s, %s", ax, cx); return;
    case ND_BITOR: println("  or %s, %s", ax, cx); return;
    case ND_BITXOR: println("  xor %s, %s", ax, cx); return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        println("  cmp %s, %s", ax, cx);
        if (node->kind == ND_EQ) println("  sete al");
        else if (node->kind == ND_NE) println("  setne al");
        else if (node->kind == ND_LT) println(node->lhs->ty->is_unsigned ? "  setb al" : "  setl al");
        else println(node->lhs->ty->is_unsigned ? "  setbe al" : "  setle al");
        println("  movzx eax, al");
        return;
    case ND_SHL: println("  shl %s, cl", ax); return;
    case ND_SHR:
        println(node->lhs->ty->is_unsigned ? "  shr %s, cl" : "  sar %s, cl", ax);
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
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        println("  je .L.else.%d", c);
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
        if (node->var)
            println("  mov qword ptr [rbp%+d], rsp", node->var->offset);
        println(".L.begin.%d:", c);
        dbg_loc(node->tok);             // each round comes back to the loop's line
        if (node->cond) {
            gen_expr(node->cond);
            cmp_zero(node->cond->ty);
            println("  je %s", node->brk_label);
        }
        gen_stmt(node->then);
        println("%s:", node->cont_label);
        if (node->var)
            println("  mov rsp, qword ptr [rbp%+d]", node->var->offset);
        if (node->inc) {
            dbg_loc(node->tok);
            gen_expr(node->inc);
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
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        println("  jne .L.begin.%d", c);
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
        gen_expr(node->lhs);
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

        // parameters live in the caller-provided home slots at rbp+16+8*i
        int slot = is_big_struct(fn->ty->return_ty) ? 1 : 0;
        for (Obj *p = fn->params; p; p = p->next) {
            p->offset = 16 + 8 * slot++;
        }
        int nslots = slot;

        int bottom = 0;
        for (Obj *var = fn->locals; var; var = var->next) {
            if (var->offset > 0)
                continue;       // parameter
            if (var == fn->va_area) {
                var->offset = 16 + 8 * nslots;
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
        if (cc_target_win && !fn->is_naked)
            println(".seh_proc 0");                 // unwind info: push rbp; mov rbp, rsp (asm.c, .pdata)
        g_nfunclets = 0;
        g_any_seh |= fn->has_seh;
        println(".dbgfunc %s", fn->tok ? cc_strndup(fn->tok->loc, fn->tok->len) : fn->name);   // the C name (a static's label may be "name.2")
        dbg_loc(fn->body->lbrace ? fn->body->lbrace : fn->tok);
        if (fn->is_naked) {
            // __declspec(naked): only the asm of the body; it returns by itself (ret)
            g_depth = 0;
            gen_stmt(fn->body);
            println(".dbgend");
            continue;
        }
        println("  push rbp");
        println("  mov rbp, rsp");
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
            println("  mov qword ptr [rbp+16], rcx");
            g_ret_ptr_offset = 16;
            slot = 1;
        }
        Obj *p = fn->params;
        for (; slot < 4; slot++) {
            Type *ty = p ? p->ty : 0;
            if (!p && !fn->ty->is_variadic)
                break;
            if (ty && is_flonum(ty))
                println("  movq qword ptr [rbp+%d], xmm%d", 16 + 8 * slot, slot);
            else
                println("  mov qword ptr [rbp+%d], %s", 16 + 8 * slot, gpr[slot]);
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

        gen_stmt(fn->body);
        if (g_depth != 0)
            cc_error_tok(fn->tok, "внутренняя ошибка: стек не сошёлся (%d)", g_depth);

        // falling off the end of main returns 0
        if (cc_streq(fn->name, "main"))
            println("  xor eax, eax");
        dbg_loc(fn->body->rbrace);      // every return passes the closing brace
        println(".L.return.%s:", fn->name);
        println("  mov rsp, rbp");
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
    if (cc_file_asm) {
        println(".text");
        out_raw(cc_file_asm, cc_strlen(cc_file_asm));
        out_raw("\n", 1);
    }
    return g_out.buf ? g_out.buf : "";
}
