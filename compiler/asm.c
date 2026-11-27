// x86-64 assembler, Intel syntax. Assembles the compiler output together
// with inline asm and .asm files straight into executable memory.
//
// Three passes: collect label names, measure every instruction (sizes depend
// only on the text, never on addresses), then emit with final addresses.
// Jumps and calls always use 32-bit offsets. Names the program does not
// define are looked up among the host symbols (kernel functions) and reached
// through a table of 8-byte slots placed after the data.
#include "cc.h"
#include "peformat.h"

// .bss and .tls only in a Windows program; elsewhere .bss is .data
enum { SEC_TEXT, SEC_DATA, SEC_BSS, SEC_TLS, SEC_COUNT };
enum { OP_NONE, OP_REG, OP_XMM, OP_IMM, OP_MEM };
#define REG_RIP 16

typedef struct {
    int kind;
    int size;           // bytes; 0 = not given
    int reg;            // register number 0..15
    int high8;          // ah ch dh bh
    int rex8;           // spl bpl sil dil need a REX prefix
    int base, index, scale;
    i64 disp;
    const char *sym;    // symbol in a displacement or immediate
    int sym_len;
    i64 imm;
    int seg;            // gs:[...] 0x65, fs:[...] 0x64: the prefix of a segment, or 0
} Operand;

typedef struct Label {
    struct Label *next;
    const char *name;
    int len;
    int section;
    int offset;
    int defined;
    int local;          // .local: a static function, never exported
    int exported;       // .export: __declspec(dllexport)
    struct AliasDef *alias;     // made by this .alias (another name of a label), or 0
} Label;

typedef struct Import {
    struct Import *next;
    const char *name;
    int len;
    void *addr;
    int slot;           // index in the slot table
    const char *dll;    // Windows: the DLL that exports it
} Import;

// .alias name, target: name is one more label of target -- unless the program
// defines name itself, or an earlier .alias took it (the library's printf).
typedef struct AliasDef {
    struct AliasDef *next;
    const char *name, *target;
    int nlen, tlen;
} AliasDef;

// .seh_proc / .seh_scope / .seh_endproc of the code generator: the .pdata table of a Windows program
typedef struct SehScope {
    struct SehScope *next;
    const char *name[4];        // the guarded code's start and end, the filter or __finally funclet, the target
    int len[4];
} SehScope;

typedef struct SehProc {
    int start, end;             // in .text
    int kind;                   // 0 a function (push rbp; mov rbp, rsp), 1 a funclet (push rbp; push rbx; sub rsp, 8; mov rbx, rsp)
    int nregs, pad;             // a function: push rbp; push regs[0..nregs); sub rsp, 8 when pad; mov rbp, rsp
    u8 regs[8];
    SehScope *scopes, *tail;
    int nscopes;
} SehProc;

struct Asm {
    int pass;           // 0 labels, 1 sizes, 2 emit
    int section;
    int pos[SEC_COUNT]; // current offset per section
    int size[SEC_COUNT];
    u8 *base[SEC_COUNT];        // final memory per section (pass 2); a label of .tls means its offset there
    u8 *slots;          // import slots (pass 2)
    Label *labels[256];
    Import *imports;
    int import_count;
    const CcSymbol *externs;
    int line;
    const char *line_text;
    // the C line this code is of (.dbgfile / .loc of the code generator): an error names it;
    // 0 outside C functions (.asm files, data)
    const char *c_files[256];
    int c_file_len[256];
    int c_file, c_line;
    int disp_pos;       // offset of a RIP-relative displacement awaiting a fix
    const char *disp_sym;
    int disp_sym_len;
    i64 disp_add;
    int disp_import;    // the RIP displacement targets an import slot
    // PE (.dll) mode
    int pe;             // 1 = building a relocatable PE image, not fixed memory
    int any_export;     // some label is .export: only those (and main) are exported
    u8 *image_buf;     // start of the single PE image buffer (RVA 0 base)
    u64 image_base;     // the image's preferred address
    const CcWinTarget *win;     // a Windows .exe/.dll (WinCC), or 0
    AliasDef *aliases, *alias_tail;
    struct BaseReloc *base_relocs;
    int base_reloc_count;
    int pass1_abs;     // absolute addresses seen in pass 1: base relocations will be there
    // debug info (PE mode, recorded in pass 1): the .dydbg section
    struct DbgLine *dlines;
    int ndl, dlcap;
    struct DbgFunc *dfuncs;
    int ndf, dfcap;
    const char *dfile[256];
    int dfile_len[256];
    int ndfile;
    struct DbgExtra *dextra;
    int ndx, dxcap;
    // the functions for .pdata (pass 1)
    SehProc *procs;
    int nproc, proccap;
    // numeric labels "1:" (and "1b", "1f" to them): how many of each number came so far in the pass
    int numlab[1000];
    // .srcfile: the lines from here are of that file (an .asm file, asm() of C outside functions)
    const char *src_name;
    int src_len, src_base;
};

typedef struct DbgLine { int off, line, file; } DbgLine;                 // text offset -> file, line
typedef struct DbgFunc { int start, end; const char *name; int len; } DbgFunc;
// wcc -g: .dbgframe/.dbgvar (of function func), .dbgglobal, .dbgstruct, .dbgfield -- the rest of the line as it is
typedef struct DbgExtra { char kind; int func; const char *s; int len; } DbgExtra;

typedef struct BaseReloc {
    struct BaseReloc *next;
    int section;
    int offset;
} BaseReloc;

// ---- errors ----

__declspec(noreturn) static void asm_error(Asm *as, const char *fmt, ...)
{
    char msg[256], shown[80];
    va_list ap;
    va_start(ap, fmt);
    cc_vformat(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    int n = 0;
    const char *p = as->line_text;
    while (p && (*p == ' ' || *p == '\t'))      // the line as it reads, without its indent
        p++;
    while (p && *p && *p != '\n' && n < 70) {
        if (*p != '\r')
            shown[n++] = *p;
        p++;
    }
    shown[n] = 0;
    if (as->c_line > 0 && as->c_files[as->c_file])     // in a C function: its file and line
        cc_error_at(cc_strndup(as->c_files[as->c_file], as->c_file_len[as->c_file]), as->c_line,
                    "ассемблер: %s [%s]", msg, shown);
    if (as->src_name)                                   // an .asm file: its own line
        cc_error_at(cc_strndup(as->src_name, as->src_len), as->line + as->src_base, "ассемблер: %s [%s]", msg, shown);
    cc_error_at(0, 0, "ассемблер, строка %d текста (wcc -S сохранит его в .s): %s [%s]", as->line, msg, shown);
}

// ---- small text helpers ----

static int lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static int is_id1(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || c == '$' || c == '@' || c >= 0x80;
}

static int is_id2(int c)
{
    return is_id1(c) || (c >= '0' && c <= '9');
}

static int word_eq(const char *s, int len, const char *w)
{
    int n = cc_strlen(w);
    if (n != len)
        return 0;
    for (int i = 0; i < n; i++)
        if (lower((unsigned char)s[i]) != w[i])
            return 0;
    return 1;
}

static const char *skip_sp(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;
    return p;
}

// ---- labels ----

static u32 hash_name(const char *s, int len)
{
    u32 h = 2166136261u;
    for (int i = 0; i < len; i++)
        h = (h ^ (u8)s[i]) * 16777619u;
    return h;
}

static Label *find_label(Asm *as, const char *name, int len)
{
    for (Label *l = as->labels[hash_name(name, len) & 255]; l; l = l->next)
        if (l->len == len && cc_strneq(l->name, name, len))
            return l;
    return 0;
}

static Label *add_label(Asm *as, const char *name, int len)
{
    Label *l = find_label(as, name, len);
    if (l)
        return l;
    l = cc_alloc(sizeof(Label));
    l->name = name;
    l->len = len;
    u32 h = hash_name(name, len) & 255;
    l->next = as->labels[h];
    as->labels[h] = l;
    return l;
}

static Import *find_import(Asm *as, const char *name, int len)
{
    for (Import *im = as->imports; im; im = im->next)
        if (im->len == len && cc_strneq(im->name, name, len))
            return im;
    for (const CcSymbol *s = as->externs; s && s->name; s++) {
        if (cc_strlen(s->name) == len && cc_strneq(s->name, name, len)) {
            Import *im = cc_alloc(sizeof(Import));
            im->name = name;
            im->len = len;
            im->addr = s->addr;
            im->slot = as->import_count++;
            im->next = as->imports;
            as->imports = im;
            return im;
        }
    }
    // In a DLL any external name becomes an import; the loader resolves it.
    if (as->pe) {
        Import *im = cc_alloc(sizeof(Import));
        im->name = name;
        im->len = len;
        im->addr = 0;
        im->slot = as->import_count++;
        im->next = as->imports;
        as->imports = im;
        return im;
    }
    return 0;
}

static void record_base_reloc(Asm *as)
{
    BaseReloc *r = cc_alloc(sizeof(BaseReloc));
    r->section = as->section;
    r->offset = as->pos[as->section];
    r->next = as->base_relocs;
    as->base_relocs = r;
    as->base_reloc_count++;
}

static int is_local_symbol(Asm *as, const char *name, int len)
{
    Label *l = find_label(as, name, len);
    return l && l->defined;
}

// Absolute address of a symbol (pass 2), or 0 in earlier passes.
static u64 symbol_address(Asm *as, const char *name, int len)
{
    if (as->pass < 2)
        return 0;
    Label *l = find_label(as, name, len);
    if (l && l->defined)
        return l->section == SEC_TLS ? (u64)l->offset : (u64)(as->base[l->section] + l->offset);
    Import *im = find_import(as, name, len);
    if (im)
        return (u64)im->addr;
    asm_error(as, "неизвестное имя %s", cc_strndup(name, len));
}

// ---- output ----

static void emit8(Asm *as, int b)
{
    if (as->pass == 2) {
        if (as->section != SEC_BSS)
            as->base[as->section][as->pos[as->section]] = (u8)b;
        else if (b)
            asm_error(as, "в .bss бывают только нули");
    }
    as->pos[as->section]++;
}

static void emit16(Asm *as, u64 v)
{
    emit8(as, (int)(v & 0xFF));
    emit8(as, (int)((v >> 8) & 0xFF));
}

static void emit32(Asm *as, u64 v)
{
    for (int i = 0; i < 4; i++)
        emit8(as, (int)((v >> (8 * i)) & 0xFF));
}

static void emit64(Asm *as, u64 v)
{
    for (int i = 0; i < 8; i++)
        emit8(as, (int)((v >> (8 * i)) & 0xFF));
}

static void patch32(Asm *as, int at, u64 v)
{
    if (as->pass != 2)
        return;
    for (int i = 0; i < 4; i++)
        as->base[SEC_TEXT][at + i] = (u8)(v >> (8 * i));
}

static u64 current_address(Asm *as)
{
    return (u64)(as->base[as->section] + as->pos[as->section]);
}

// ---- operand parsing ----

static const char *g_reg64[] = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" };
static const char *g_reg32[] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                                 "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" };
static const char *g_reg16[] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
                                 "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" };
static const char *g_reg8[] = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
                                "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" };
static const char *g_reg8h[] = { "ah", "ch", "dh", "bh" };

// Returns 1 and fills `op` if the word is a register name.
static int parse_reg(const char *s, int len, Operand *op)
{
    for (int i = 0; i < 16; i++) {
        if (word_eq(s, len, g_reg64[i])) { op->kind = OP_REG; op->reg = i; op->size = 8; return 1; }
        if (word_eq(s, len, g_reg32[i])) { op->kind = OP_REG; op->reg = i; op->size = 4; return 1; }
        if (word_eq(s, len, g_reg16[i])) { op->kind = OP_REG; op->reg = i; op->size = 2; return 1; }
        if (word_eq(s, len, g_reg8[i])) {
            op->kind = OP_REG; op->reg = i; op->size = 1; op->rex8 = i >= 4 && i < 8; return 1;
        }
    }
    for (int i = 0; i < 4; i++)
        if (word_eq(s, len, g_reg8h[i])) {
            op->kind = OP_REG; op->reg = i + 4; op->size = 1; op->high8 = 1; return 1;
        }
    if (len >= 4 && len <= 5 && (lower(s[0]) == 'x' || lower(s[0]) == 'y') && lower(s[1]) == 'm' && lower(s[2]) == 'm') {
        int n = 0;
        for (int i = 3; i < len; i++) {
            if (s[i] < '0' || s[i] > '9')
                return 0;
            n = n * 10 + (s[i] - '0');
        }
        if (n > 15)
            return 0;
        op->kind = OP_XMM;
        op->reg = n;
        op->size = lower(s[0]) == 'y' ? 32 : 16;    // ymm: AVX, 256 bits
        return 1;
    }
    return 0;
}

static int parse_number(Asm *as, const char **pp, i64 *out)
{
    const char *p = *pp;
    u64 v = 0;
    if (*p == '\'') {
        p++;
        v = (u8)*p++;
        if (*p != '\'')
            asm_error(as, "символ не закрыт");
        *pp = p + 1;
        *out = (i64)v;
        return 1;
    }
    if (!(*p >= '0' && *p <= '9'))
        return 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        for (;; p++) {
            int c = lower((unsigned char)*p);
            if (c >= '0' && c <= '9') v = v * 16 + (u64)(c - '0');
            else if (c >= 'a' && c <= 'f') v = v * 16 + (u64)(c - 'a' + 10);
            else break;
        }
    } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B') && (p[2] == '0' || p[2] == '1')) {
        p += 2;
        for (; *p == '0' || *p == '1'; p++)
            v = v * 2 + (u64)(*p - '0');
    } else {
        // decimal, or hex with an 'h' suffix
        const char *q = p;
        int hex = 0;
        while (is_id2((unsigned char)*q))
            q++;
        if (q > p && lower(q[-1]) == 'h')
            hex = 1;
        if (hex) {
            for (; p < q - 1; p++) {
                int c = lower((unsigned char)*p);
                v = v * 16 + (u64)(c <= '9' ? c - '0' : c - 'a' + 10);
            }
            p = q;
        } else {
            for (; *p >= '0' && *p <= '9'; p++)
                v = v * 10 + (u64)(*p - '0');
        }
    }
    if (is_id2((unsigned char)*p))
        asm_error(as, "неверное число");
    *pp = p;
    *out = (i64)v;
    return 1;
}

// expression inside [ ] or an immediate: terms joined by + and -
static void parse_terms(Asm *as, const char **pp, Operand *op, int in_mem)
{
    const char *p = *pp;
    int sign = 1, first = 1;
    for (;;) {
        p = skip_sp(p);
        if (*p == '+' || *p == '-') {
            sign = *p == '-' ? -1 : 1;
            p++;
            p = skip_sp(p);
        } else if (!first) {
            break;
        }
        first = 0;
        i64 num;
        if (*p >= '0' && *p <= '9') {           // 1b, 2f: the numeric label before or after this line
            const char *q = p;
            while (*q >= '0' && *q <= '9')
                q++;
            int dir = lower((unsigned char)*q);
            if ((dir == 'b' || dir == 'f') && !is_id2((unsigned char)q[1]) && q - p <= 3) {
                int n = 0;
                for (const char *d = p; d < q; d++)
                    n = n * 10 + (*d - '0');
                int k = as->numlab[n] - (dir == 'b');
                if (k < 0)
                    asm_error(as, "перед этой строкой нет метки %d:", n);
                if (op->sym)
                    asm_error(as, "два имени в одном выражении");
                if (sign < 0)
                    asm_error(as, "имя нельзя вычитать");
                op->sym = cc_format(".Lnum.%d.%d", n, k);
                op->sym_len = cc_strlen(op->sym);
                p = q + 1;
                continue;
            }
        }
        if (parse_number(as, &p, &num)) {
            p = skip_sp(p);
            if (in_mem && *p == '*') {          // 4*rcx
                p = skip_sp(p + 1);
                const char *s = p;
                while (is_id2((unsigned char)*p))
                    p++;
                Operand r = { 0 };
                if (!parse_reg(s, (int)(p - s), &r) || r.kind != OP_REG || r.size != 8)
                    asm_error(as, "ожидался регистр индекса");
                op->index = r.reg;
                op->scale = (int)num;
                continue;
            }
            op->disp += sign * num;
            op->imm += sign * num;
            continue;
        }
        if (is_id1((unsigned char)*p)) {
            const char *s = p;
            while (is_id2((unsigned char)*p))
                p++;
            int len = (int)(p - s);
            int named = s[0] == '@' && len > 1;    // a C name: "@dx" is the variable dx, not the register
            if (named) { s++; len--; }
            Operand r = { 0 };
            if (!named && in_mem && parse_reg(s, len, &r)) {
                if (r.kind != OP_REG || (r.size != 8 && !(r.size == 4)))
                    asm_error(as, "в адресе нужен 64-битный регистр");
                p = skip_sp(p);
                if (*p == '*') {
                    p = skip_sp(p + 1);
                    i64 sc;
                    if (!parse_number(as, &p, &sc))
                        asm_error(as, "ожидался масштаб");
                    op->index = r.reg;
                    op->scale = (int)sc;
                } else if (op->base < 0) {
                    op->base = r.reg;
                } else if (op->index < 0) {
                    op->index = r.reg;
                    op->scale = 1;
                } else {
                    asm_error(as, "слишком много регистров в адресе");
                }
                continue;
            }
            if (!named && in_mem && word_eq(s, len, "rip")) {
                op->base = REG_RIP;
                continue;
            }
            if (op->sym)
                asm_error(as, "два имени в одном выражении");
            if (sign < 0)
                asm_error(as, "имя нельзя вычитать");
            op->sym = s;
            op->sym_len = len;
            continue;
        }
        asm_error(as, "непонятный операнд");
    }
    *pp = p;
}

static const char *parse_operand(Asm *as, const char *p, Operand *op)
{
    op->base = op->index = -1;
    op->scale = 1;
    p = skip_sp(p);

    if (*p == '@') {                        // a C name (see parse_terms): a symbol, whatever it is called
        op->kind = OP_IMM;
        parse_terms(as, &p, op, 0);
        return skip_sp(p);
    }

    // size keywords -- only before "ptr" or "[": otherwise "word" may be a name
    static const struct { const char *w; int size; } sizes[] = {
        { "byte", 1 }, { "word", 2 }, { "dword", 4 }, { "qword", 8 }, { "xmmword", 16 }, { "oword", 16 }, { "ymmword", 32 },
        { "tbyte", 10 }, { "tword", 10 },     // x87: 80-bit
    };
    for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
        int n = cc_strlen(sizes[i].w);
        const char *after = skip_sp(p + n);
        if (word_eq(p, n, sizes[i].w) && !is_id2((unsigned char)p[n]) &&
            (*after == '[' || (word_eq(after, 3, "ptr") && !is_id2((unsigned char)after[3])))) {
            op->size = sizes[i].size;
            p = skip_sp(p + n);
            if (word_eq(p, 3, "ptr") && !is_id2((unsigned char)p[3]))
                p = skip_sp(p + 3);
            break;
        }
    }

    if ((lower((unsigned char)p[0]) == 'g' || lower((unsigned char)p[0]) == 'f') &&
        lower((unsigned char)p[1]) == 's' && p[2] == ':') {
        op->seg = lower((unsigned char)p[0]) == 'g' ? 0x65 : 0x64;     // gs:[0x30] -- the TEB of the thread
        p = skip_sp(p + 3);
        if (*p != '[')
            asm_error(as, "после %cs: нужен адрес в [ ]", p[-3]);
    }
    if (*p == '[') {
        op->kind = OP_MEM;
        p++;
        parse_terms(as, &p, op, 1);
        p = skip_sp(p);
        if (*p != ']')
            asm_error(as, "ожидалось ']'");
        p++;
        if (op->sym && op->base < 0 && op->index < 0)
            op->base = REG_RIP;             // [label] means rip-relative
        if (op->base == REG_RIP && op->index >= 0)
            asm_error(as, "rip нельзя сочетать с индексом");
        if (op->scale != 1 && op->scale != 2 && op->scale != 4 && op->scale != 8)
            asm_error(as, "масштаб бывает 1, 2, 4 или 8");
        if (op->index == 4)
            asm_error(as, "rsp не может быть индексом");
        return skip_sp(p);
    }

    const char *s = p;
    while (is_id2((unsigned char)*p))
        p++;
    if (p > s && is_id1((unsigned char)*s) && parse_reg(s, (int)(p - s), op))
        return skip_sp(p);
    p = s;
    op->kind = OP_IMM;
    parse_terms(as, &p, op, 0);
    return skip_sp(p);
}

// ---- encoding ----

static int fits8(i64 v)  { return v >= -128 && v <= 127; }
static int fits32(i64 v) { return v >= -2147483648LL && v <= 2147483647LL; }

static void check_rex_conflict(Asm *as, Operand *a, Operand *b, int rex)
{
    if (rex && ((a && a->high8) || (b && b->high8)))
        asm_error(as, "ah, bh, ch, dh нельзя сочетать с новыми регистрами");
}

static void modrm(Asm *as, int reg_field, Operand *rm);

// Emits: [prefix] [REX] opcode ModRM [SIB] [disp] ; the caller then adds an immediate.
// reg_field: register number or /digit. rm: register or memory operand.
static void encode(Asm *as, int prefix, int rexw, const u8 *opc, int opclen, int reg_field,
                   Operand *reg_op, Operand *rm, int imm_bytes)
{
    int rex = rexw ? 8 : 0;
    if (reg_field & 8) rex |= 4;
    if (rm->kind == OP_REG || rm->kind == OP_XMM) {
        if (rm->reg & 8) rex |= 1;
    } else {
        if (rm->index >= 0 && (rm->index & 8)) rex |= 2;
        if (rm->base >= 0 && rm->base != REG_RIP && (rm->base & 8)) rex |= 1;
    }
    int force = (reg_op && reg_op->rex8) || (rm->kind == OP_REG && rm->rex8);
    check_rex_conflict(as, reg_op, rm->kind == OP_REG ? rm : 0, rex || force);

    if (rm->kind == OP_MEM && rm->seg)
        emit8(as, rm->seg);
    if (prefix)
        emit8(as, prefix);
    if (rex || force)
        emit8(as, 0x40 | rex);
    for (int i = 0; i < opclen; i++)
        emit8(as, opc[i]);
    (void)imm_bytes;
    modrm(as, reg_field, rm);
}

// ModRM [SIB] [disp] for a register number (or /digit) and a register or memory operand
static void modrm(Asm *as, int reg_field, Operand *rm)
{
    int r = reg_field & 7;
    if (rm->kind == OP_REG || rm->kind == OP_XMM) {
        emit8(as, 0xC0 | (r << 3) | (rm->reg & 7));
        return;
    }

    // memory
    int has_sym = rm->sym != 0;
    if (rm->base == REG_RIP) {
        emit8(as, (r << 3) | 5);
        as->disp_pos = as->pos[SEC_TEXT];
        as->disp_sym = rm->sym;
        as->disp_sym_len = rm->sym_len;
        as->disp_add = rm->disp;
        emit32(as, 0);                  // fixed after the immediate is known
        return;
    }
    i64 disp = rm->disp;
    if (has_sym) {
        // absolute address inside a base/index expression
        disp += (i64)symbol_address(as, rm->sym, rm->sym_len);
        if (as->pass == 2 && !fits32(disp))
            asm_error(as, "адрес не помещается в 32 бита, используйте [rip+имя]");
    }
    if (rm->base < 0) {
        // [index*scale + disp32] or [disp32]
        int idx = rm->index >= 0 ? rm->index & 7 : 4;
        int ss = rm->scale == 8 ? 3 : rm->scale == 4 ? 2 : rm->scale == 2 ? 1 : 0;
        emit8(as, (r << 3) | 4);
        emit8(as, (ss << 6) | (idx << 3) | 5);
        emit32(as, (u64)disp);
        return;
    }
    int mod;
    if (!has_sym && disp == 0 && (rm->base & 7) != 5) mod = 0;
    else if (!has_sym && fits8(disp)) mod = 1;
    else mod = 2;
    if (rm->index >= 0 || (rm->base & 7) == 4) {
        int idx = rm->index >= 0 ? rm->index & 7 : 4;
        int ss = rm->scale == 8 ? 3 : rm->scale == 4 ? 2 : rm->scale == 2 ? 1 : 0;
        emit8(as, (mod << 6) | (r << 3) | 4);
        emit8(as, (ss << 6) | (idx << 3) | (rm->base & 7));
    } else {
        emit8(as, (mod << 6) | (r << 3) | (rm->base & 7));
    }
    if (mod == 1)
        emit8(as, (int)(disp & 0xFF));
    else if (mod == 2)
        emit32(as, (u64)disp);
}

// Resolves a pending RIP-relative displacement once the instruction ends.
static void finish_rip(Asm *as)
{
    if (as->disp_pos < 0)
        return;
    int at = as->disp_pos;
    as->disp_pos = -1;
    if (as->pass < 2)
        return;
    u64 end = current_address(as);
    u64 target;
    if (!as->disp_sym) {
        target = end;               // [rip+N]
    } else if (as->disp_import) {
        Import *im = find_import(as, as->disp_sym, as->disp_sym_len);
        target = (u64)(as->slots + 8 * im->slot);
    } else {
        Label *l = find_label(as, as->disp_sym, as->disp_sym_len);
        if (!l || !l->defined) {
            Import *im = find_import(as, as->disp_sym, as->disp_sym_len);
            if (!im)
                asm_error(as, "неизвестное имя %s", cc_strndup(as->disp_sym, as->disp_sym_len));
            asm_error(as, "%s — функция системы: её можно вызвать (call) или взять адрес (lea), но не читать",
                      cc_strndup(as->disp_sym, as->disp_sym_len));
        }
        target = (u64)(as->base[l->section] + l->offset);
    }
    i64 rel = (i64)(target + (u64)as->disp_add) - (i64)end;
    if (!fits32(rel))
        asm_error(as, "слишком далеко для rip-адреса");
    patch32(as, at, (u64)rel);
}

static int op_size(Asm *as, Operand *a, Operand *b)
{
    int s = 0;
    if (a && a->kind == OP_REG) s = a->size;
    else if (b && b->kind == OP_REG) s = b->size;
    else if (a && a->size) s = a->size;
    else if (b && b->size && b->kind == OP_MEM) s = b->size;
    if (!s)
        asm_error(as, "укажите размер: byte, word, dword или qword ptr");
    return s;
}

// prefix and REX.W for an operand size
static int size_prefix(int size) { return size == 2 ? 0x66 : 0; }
static int size_w(int size)      { return size == 8; }

static void emit_imm(Asm *as, Operand *op, int bytes)
{
    i64 v = op->imm;
    if (op->sym && as->pass == 2) {        // a .tls label: its offset in the thread's block
        Label *l = find_label(as, op->sym, op->sym_len);
        if (l && l->defined && l->section == SEC_TLS) {
            v += l->offset;
            if (bytes == 1) emit8(as, (int)(v & 0xFF));
            else if (bytes == 2) emit16(as, (u64)v);
            else if (bytes == 4) emit32(as, (u64)v);
            else emit64(as, (u64)v);
            return;
        }
    }
    if (op->sym && as->pe && as->pass == 1)
        as->pass1_abs++;                // a base relocation comes (wcc --tiny: the header keeps room for its directory)
    if (op->sym && as->pe && as->pass == 2) {
        // An absolute address inside a DLL: store preferred VA and add a base
        // relocation so the loader fixes it up. Only 64-bit (dq / mov imm64).
        Label *l = find_label(as, op->sym, op->sym_len);
        if (!l || !l->defined)
            asm_error(as, "в DLL нельзя взять абсолютный адрес внешнего имени %s — вызывайте его",
                      cc_strndup(op->sym, op->sym_len));
        if (bytes != 8)
            asm_error(as, "в DLL абсолютный адрес помещается только в dq (8 байт)");
        record_base_reloc(as);
        u64 rva = (u64)(as->base[l->section] + l->offset) - (u64)as->image_buf;
        v = op->imm + (i64)(as->image_base + rva);
        emit64(as, (u64)v);
        return;
    }
    if (op->sym)
        v += (i64)symbol_address(as, op->sym, op->sym_len);
    if (bytes == 1) emit8(as, (int)(v & 0xFF));
    else if (bytes == 2) emit16(as, (u64)v);
    else if (bytes == 4) emit32(as, (u64)v);
    else emit64(as, (u64)v);
}

// ---- instruction groups ----

static void need(Asm *as, int n, int want)
{
    if (n != want)
        asm_error(as, "нужно операндов: %d", want);
}

// add or adc sbb and sub xor cmp: /digit and base opcode
static void alu(Asm *as, int digit, Operand *ops, int n)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    int size = op_size(as, a, b);
    int base = digit * 8;
    int pfx = size_prefix(size), w = size_w(size);
    if (b->kind == OP_IMM) {
        if (a->kind == OP_IMM)
            asm_error(as, "неверные операнды");
        u8 opc;
        int ib;
        if (size == 1) { opc = 0x80; ib = 1; }
        else if (!b->sym && fits8(b->imm)) { opc = 0x83; ib = 1; }
        else { opc = 0x81; ib = size == 2 ? 2 : 4; }
        encode(as, pfx, w, &opc, 1, digit, 0, a, ib);
        emit_imm(as, b, ib);
        finish_rip(as);
        return;
    }
    if (a->kind == OP_REG && (b->kind == OP_REG || b->kind == OP_MEM)) {
        u8 opc = (u8)(base + (size == 1 ? 2 : 3));
        encode(as, pfx, w, &opc, 1, a->reg, a, b, 0);
        finish_rip(as);
        return;
    }
    if (a->kind == OP_MEM && b->kind == OP_REG) {
        u8 opc = (u8)(base + (size == 1 ? 0 : 1));
        encode(as, pfx, w, &opc, 1, b->reg, b, a, 0);
        finish_rip(as);
        return;
    }
    asm_error(as, "неверные операнды");
}

static void mov(Asm *as, Operand *ops, int n)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind == OP_XMM || b->kind == OP_XMM)
        asm_error(as, "для xmm используйте movd, movq, movss, movsd");
    int size = op_size(as, a, b);
    int pfx = size_prefix(size), w = size_w(size);
    if (b->kind == OP_IMM) {
        if (a->kind == OP_REG) {
            i64 v = b->imm;
            // symbol => always the full 64-bit form (address unknown until pass 2)
            if (size == 8 && (b->sym || !fits32(v))) {
                int rex = 0x48 | ((a->reg & 8) ? 1 : 0);
                emit8(as, rex);
                emit8(as, 0xB8 + (a->reg & 7));
                emit_imm(as, b, 8);
                return;
            }
            if (size == 8) {
                u8 opc = 0xC7;
                encode(as, 0, 1, &opc, 1, 0, 0, a, 4);
                emit_imm(as, b, 4);
                return;
            }
            // B0+r / B8+r
            if (pfx)
                emit8(as, pfx);
            if ((a->reg & 8) || a->rex8)
                emit8(as, 0x40 | ((a->reg & 8) ? 1 : 0));
            check_rex_conflict(as, a, 0, (a->reg & 8) || a->rex8);
            emit8(as, (size == 1 ? 0xB0 : 0xB8) + (a->reg & 7));
            emit_imm(as, b, size);
            return;
        }
        if (a->kind == OP_MEM) {
            u8 opc = size == 1 ? 0xC6 : 0xC7;
            int ib = size == 1 ? 1 : size == 2 ? 2 : 4;
            encode(as, pfx, w, &opc, 1, 0, 0, a, ib);
            emit_imm(as, b, ib);
            finish_rip(as);
            return;
        }
    }
    if (a->kind == OP_REG && (b->kind == OP_REG || b->kind == OP_MEM)) {
        u8 opc = size == 1 ? 0x8A : 0x8B;
        encode(as, pfx, w, &opc, 1, a->reg, a, b, 0);
        finish_rip(as);
        return;
    }
    if (a->kind == OP_MEM && b->kind == OP_REG) {
        u8 opc = size == 1 ? 0x88 : 0x89;
        encode(as, pfx, w, &opc, 1, b->reg, b, a, 0);
        finish_rip(as);
        return;
    }
    asm_error(as, "неверные операнды mov");
}

// lea r, [mem]; for an external function it becomes mov r, [rip+slot]
static void lea(Asm *as, Operand *ops, int n)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind != OP_REG || b->kind != OP_MEM || a->size < 2)
        asm_error(as, "lea: нужен регистр и адрес");
    u8 opc = 0x8D;
    int import = 0;
    // Only from the second pass on: in the first, a function defined further down the
    // text is not known yet and must not turn into an import (the same size either way).
    if (as->pass > 0 && b->base == REG_RIP && b->sym && !is_local_symbol(as, b->sym, b->sym_len)) {
        if (!find_import(as, b->sym, b->sym_len))
            asm_error(as, "неизвестное имя %s", cc_strndup(b->sym, b->sym_len));
        if (b->disp)
            asm_error(as, "к адресу функции системы нельзя прибавлять");
        opc = 0x8B;
        import = 1;
    }
    encode(as, size_prefix(a->size), size_w(a->size), &opc, 1, a->reg, a, b, 0);
    as->disp_import = import;
    finish_rip(as);
    as->disp_import = 0;
}

// movzx / movsx / movsxd
static void movx(Asm *as, Operand *ops, int n, int is_signed, int is_d)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind != OP_REG)
        asm_error(as, "первым должен быть регистр");
    int src = b->kind == OP_REG ? b->size : b->size;
    if (is_d || src == 4) {
        if (a->size != 8)
            asm_error(as, "movsxd: приёмник 64-битный");
        u8 opc = 0x63;
        encode(as, 0, 1, &opc, 1, a->reg, a, b, 0);
        finish_rip(as);
        return;
    }
    if (src != 1 && src != 2)
        asm_error(as, "укажите byte или word ptr");
    u8 opc[2] = { 0x0F, (u8)((is_signed ? 0xBE : 0xB6) + (src == 2 ? 1 : 0)) };
    encode(as, size_prefix(a->size), size_w(a->size), opc, 2, a->reg, a, b, 0);
    finish_rip(as);
}

// one-operand F6/F7 group and FE/FF inc/dec
static void unary_grp(Asm *as, Operand *ops, int n, int digit, int incdec)
{
    need(as, n, 1);
    Operand *a = &ops[0];
    int size = op_size(as, a, 0);
    u8 opc = incdec ? (size == 1 ? 0xFE : 0xFF) : (size == 1 ? 0xF6 : 0xF7);
    encode(as, size_prefix(size), size_w(size), &opc, 1, digit, 0, a, 0);
    finish_rip(as);
}

static void imul(Asm *as, Operand *ops, int n)
{
    if (n == 1) {
        unary_grp(as, ops, n, 5, 0);
        return;
    }
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind != OP_REG)
        asm_error(as, "imul: первым должен быть регистр");
    int size = a->size;
    if (n == 2 && b->kind != OP_IMM) {
        u8 opc[2] = { 0x0F, 0xAF };
        encode(as, size_prefix(size), size_w(size), opc, 2, a->reg, a, b, 0);
        finish_rip(as);
        return;
    }
    Operand *src = n == 3 ? b : a;
    Operand *imm = n == 3 ? &ops[2] : b;
    int ib = fits8(imm->imm) && !imm->sym ? 1 : (size == 2 ? 2 : 4);
    u8 opc = ib == 1 ? 0x6B : 0x69;
    encode(as, size_prefix(size), size_w(size), &opc, 1, a->reg, a, src, ib);
    emit_imm(as, imm, ib);
    finish_rip(as);
}

static void shift(Asm *as, Operand *ops, int n, int digit)
{
    Operand *a = &ops[0];
    int size = op_size(as, a, 0);
    int pfx = size_prefix(size), w = size_w(size);
    if (n == 1 || (n == 2 && ops[1].kind == OP_IMM && ops[1].imm == 1 && !ops[1].sym)) {
        u8 opc = size == 1 ? 0xD0 : 0xD1;
        encode(as, pfx, w, &opc, 1, digit, 0, a, 0);
    } else if (ops[1].kind == OP_REG && ops[1].reg == 1 && ops[1].size == 1) {
        u8 opc = size == 1 ? 0xD2 : 0xD3;
        encode(as, pfx, w, &opc, 1, digit, 0, a, 0);
    } else if (ops[1].kind == OP_IMM) {
        u8 opc = size == 1 ? 0xC0 : 0xC1;
        encode(as, pfx, w, &opc, 1, digit, 0, a, 1);
        emit_imm(as, &ops[1], 1);
    } else {
        asm_error(as, "сдвиг: число или cl");
    }
    finish_rip(as);
}

static void test(Asm *as, Operand *ops, int n)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    int size = op_size(as, a, b);
    int pfx = size_prefix(size), w = size_w(size);
    if (b->kind == OP_IMM) {
        u8 opc = size == 1 ? 0xF6 : 0xF7;
        int ib = size == 1 ? 1 : size == 2 ? 2 : 4;
        encode(as, pfx, w, &opc, 1, 0, 0, a, ib);
        emit_imm(as, b, ib);
        finish_rip(as);
        return;
    }
    Operand *reg = b->kind == OP_REG ? b : a;
    Operand *rm = b->kind == OP_REG ? a : b;
    if (reg->kind != OP_REG)
        asm_error(as, "неверные операнды test");
    u8 opc = size == 1 ? 0x84 : 0x85;
    encode(as, pfx, w, &opc, 1, reg->reg, reg, rm, 0);
    finish_rip(as);
}

static void xchg(Asm *as, Operand *ops, int n)
{
    need(as, n, 2);
    Operand *reg = ops[0].kind == OP_REG ? &ops[0] : &ops[1];
    Operand *rm = ops[0].kind == OP_REG ? &ops[1] : &ops[0];
    if (reg->kind != OP_REG)
        asm_error(as, "неверные операнды xchg");
    int size = reg->size;
    u8 opc = size == 1 ? 0x86 : 0x87;
    encode(as, size_prefix(size), size_w(size), &opc, 1, reg->reg, reg, rm, 0);
    finish_rip(as);
}

static void push_pop(Asm *as, Operand *ops, int n, int is_push)
{
    need(as, n, 1);
    Operand *a = &ops[0];
    if (a->kind == OP_REG) {
        if (a->size != 8 && a->size != 2)
            asm_error(as, "push/pop работают с 64-битными регистрами");
        if (a->size == 2)
            emit8(as, 0x66);
        if (a->reg & 8)
            emit8(as, 0x41);
        emit8(as, (is_push ? 0x50 : 0x58) + (a->reg & 7));
        return;
    }
    if (a->kind == OP_IMM) {
        if (!is_push)
            asm_error(as, "pop в число невозможен");
        if (!a->sym && fits8(a->imm)) {
            emit8(as, 0x6A);
            emit_imm(as, a, 1);
        } else {
            emit8(as, 0x68);
            emit_imm(as, a, 4);
        }
        return;
    }
    u8 opc = is_push ? 0xFF : 0x8F;
    encode(as, 0, 0, &opc, 1, is_push ? 6 : 0, 0, a, 0);
    finish_rip(as);
}

// jmp / call / jcc with a label: rel32; an external name goes through its slot
static void branch(Asm *as, Operand *ops, int n, int kind, int cc)
{
    need(as, n, 1);
    Operand *a = &ops[0];
    if (a->kind == OP_IMM && a->sym && !a->imm) {
        int local = is_local_symbol(as, a->sym, a->sym_len);
        if (!local && kind != 2) {
            Import *im = as->pass > 0 ? find_import(as, a->sym, a->sym_len) : 0;
            if (as->pass > 0 && !im)
                asm_error(as, "неизвестное имя %s", cc_strndup(a->sym, a->sym_len));
            emit8(as, 0xFF);
            emit8(as, kind == 0 ? 0x15 : 0x25);     // call/jmp [rip+slot]
            int at = as->pos[SEC_TEXT];
            emit32(as, 0);
            if (as->pass == 2) {
                i64 rel = (i64)(u64)(as->slots + 8 * im->slot) - (i64)current_address(as);
                patch32(as, at, (u64)rel);
            }
            return;
        }
        if (!local && as->pass > 0)
            asm_error(as, "нет такой метки %s", cc_strndup(a->sym, a->sym_len));
        if (kind == 0) emit8(as, 0xE8);
        else if (kind == 1) emit8(as, 0xE9);
        else { emit8(as, 0x0F); emit8(as, 0x80 + cc); }
        int at = as->pos[SEC_TEXT];
        emit32(as, 0);
        if (as->pass == 2) {
            Label *l = find_label(as, a->sym, a->sym_len);
            i64 rel = (i64)(u64)(as->base[l->section] + l->offset) - (i64)current_address(as);
            patch32(as, at, (u64)rel);
        }
        return;
    }
    if (kind == 2)
        asm_error(as, "условный переход бывает только на метку");
    if (a->kind == OP_REG && a->size != 8)
        asm_error(as, "адрес перехода — 64-битный регистр");
    u8 opc = 0xFF;
    encode(as, 0, 0, &opc, 1, kind == 0 ? 2 : 4, 0, a, 0);
    finish_rip(as);
}

// SSE: prefix, opcode after 0F; dst xmm, src xmm/mem (or reversed store form)
static void sse(Asm *as, Operand *ops, int n, int pfx, int opc_load, int opc_store)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind == OP_XMM && (b->kind == OP_XMM || b->kind == OP_MEM)) {
        u8 opc[2] = { 0x0F, (u8)opc_load };
        encode(as, pfx, 0, opc, 2, a->reg, 0, b, 0);
        finish_rip(as);
        return;
    }
    if (a->kind == OP_MEM && b->kind == OP_XMM && opc_store) {
        u8 opc[2] = { 0x0F, (u8)opc_store };
        encode(as, pfx, 0, opc, 2, b->reg, 0, a, 0);
        finish_rip(as);
        return;
    }
    asm_error(as, "неверные операнды SSE");
}

// cvtsi2ss/sd xmm, r/m32/64
static void cvt_from_int(Asm *as, Operand *ops, int n, int pfx)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind != OP_XMM)
        asm_error(as, "приёмник — регистр xmm");
    int size = b->kind == OP_REG ? b->size : b->size;
    if (size != 4 && size != 8)
        asm_error(as, "источник — 32 или 64 бита");
    u8 opc[2] = { 0x0F, 0x2A };
    encode(as, pfx, size == 8, opc, 2, a->reg, 0, b, 0);
    finish_rip(as);
}

// cvttss2si / cvttsd2si r32/64, xmm/m
static void cvt_to_int(Asm *as, Operand *ops, int n, int pfx)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind != OP_REG || (a->size != 4 && a->size != 8))
        asm_error(as, "приёмник — 32- или 64-битный регистр");
    u8 opc[2] = { 0x0F, 0x2C };
    encode(as, pfx, a->size == 8, opc, 2, a->reg, a, b, 0);
    finish_rip(as);
}

// movd / movq between xmm and general registers or memory
static void movdq(Asm *as, Operand *ops, int n, int is_q)
{
    need(as, n, 2);
    Operand *a = &ops[0], *b = &ops[1];
    if (a->kind == OP_XMM && b->kind == OP_XMM) {
        if (!is_q)
            asm_error(as, "movd между xmm невозможен");
        u8 opc[2] = { 0x0F, 0x7E };
        encode(as, 0xF3, 0, opc, 2, a->reg, 0, b, 0);
        return;
    }
    if (a->kind == OP_XMM) {
        if (is_q && b->kind == OP_MEM) {
            u8 opc[2] = { 0x0F, 0x7E };         // movq xmm, m64
            encode(as, 0xF3, 0, opc, 2, a->reg, 0, b, 0);
        } else {
            u8 opc[2] = { 0x0F, 0x6E };
            encode(as, 0x66, is_q, opc, 2, a->reg, 0, b, 0);
        }
        finish_rip(as);
        return;
    }
    if (b->kind == OP_XMM) {
        if (is_q && a->kind == OP_MEM) {
            u8 opc[2] = { 0x0F, 0xD6 };         // movq m64, xmm
            encode(as, 0x66, 0, opc, 2, b->reg, 0, a, 0);
        } else {
            u8 opc[2] = { 0x0F, 0x7E };
            encode(as, 0x66, is_q, opc, 2, b->reg, 0, a, 0);
        }
        finish_rip(as);
        return;
    }
    asm_error(as, "movd/movq: нужен регистр xmm");
}

static void in_out(Asm *as, Operand *ops, int n, int is_in)
{
    need(as, n, 2);
    Operand *acc = is_in ? &ops[0] : &ops[1];
    Operand *port = is_in ? &ops[1] : &ops[0];
    if (acc->kind != OP_REG || acc->reg != 0 || acc->size == 8)
        asm_error(as, "in/out работают с al, ax или eax");
    if (acc->size == 2)
        emit8(as, 0x66);
    int wide = acc->size != 1;
    if (port->kind == OP_REG && port->reg == 2 && port->size == 2) {
        emit8(as, (is_in ? 0xEC : 0xEE) + wide);
    } else if (port->kind == OP_IMM) {
        emit8(as, (is_in ? 0xE4 : 0xE6) + wide);
        emit_imm(as, port, 1);
    } else {
        asm_error(as, "порт — число или dx");
    }
}

static const char *g_cc_names[] = {
    "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g",
};

static int cond_code(const char *s, int len)
{
    static const struct { const char *n; int cc; } alias[] = {
        { "c", 2 }, { "nae", 2 }, { "nc", 3 }, { "nb", 3 }, { "z", 4 }, { "nz", 5 }, { "na", 6 },
        { "nbe", 7 }, { "pe", 10 }, { "po", 11 }, { "nge", 12 }, { "nl", 13 }, { "ng", 14 }, { "nle", 15 },
    };
    for (int i = 0; i < 16; i++)
        if (word_eq(s, len, g_cc_names[i]))
            return i;
    for (int i = 0; i < (int)(sizeof(alias) / sizeof(alias[0])); i++)
        if (word_eq(s, len, alias[i].n))
            return alias[i].cc;
    return -1;
}

typedef struct { const char *name; u8 bytes[4]; int len; } Simple;

static const Simple g_simple[] = {
    { "ret", { 0xC3 }, 1 }, { "leave", { 0xC9 }, 1 }, { "nop", { 0x90 }, 1 }, { "hlt", { 0xF4 }, 1 },
    { "cli", { 0xFA }, 1 }, { "sti", { 0xFB }, 1 }, { "cld", { 0xFC }, 1 }, { "std", { 0xFD }, 1 },
    { "clc", { 0xF8 }, 1 }, { "stc", { 0xF9 }, 1 }, { "cmc", { 0xF5 }, 1 }, { "int3", { 0xCC }, 1 },
    { "cqo", { 0x48, 0x99 }, 2 }, { "cdq", { 0x99 }, 1 }, { "cwd", { 0x66, 0x99 }, 2 },
    { "cdqe", { 0x48, 0x98 }, 2 }, { "cwde", { 0x98 }, 1 }, { "cbw", { 0x66, 0x98 }, 2 },
    { "pushfq", { 0x9C }, 1 }, { "popfq", { 0x9D }, 1 }, { "pushf", { 0x9C }, 1 }, { "popf", { 0x9D }, 1 },
    { "cpuid", { 0x0F, 0xA2 }, 2 }, { "rdtsc", { 0x0F, 0x31 }, 2 }, { "rdmsr", { 0x0F, 0x32 }, 2 },
    { "wrmsr", { 0x0F, 0x30 }, 2 }, { "ud2", { 0x0F, 0x0B }, 2 }, { "pause", { 0xF3, 0x90 }, 2 },
    { "iretq", { 0x48, 0xCF }, 2 }, { "mfence", { 0x0F, 0xAE, 0xF0 }, 3 }, { "lfence", { 0x0F, 0xAE, 0xE8 }, 3 },
    { "sfence", { 0x0F, 0xAE, 0xF8 }, 3 },
    { "movsb", { 0xA4 }, 1 }, { "movsw", { 0x66, 0xA5 }, 2 }, { "movsq", { 0x48, 0xA5 }, 2 },
    { "stosb", { 0xAA }, 1 }, { "stosw", { 0x66, 0xAB }, 2 }, { "stosd", { 0xAB }, 1 }, { "stosq", { 0x48, 0xAB }, 2 },
    { "lodsb", { 0xAC }, 1 }, { "lodsw", { 0x66, 0xAD }, 2 }, { "lodsd", { 0xAD }, 1 }, { "lodsq", { 0x48, 0xAD }, 2 },
    { "cmpsb", { 0xA6 }, 1 }, { "scasb", { 0xAE }, 1 },
    { "cmpsw", { 0x66, 0xA7 }, 2 }, { "cmpsq", { 0x48, 0xA7 }, 2 },
    { "scasw", { 0x66, 0xAF }, 2 }, { "scasd", { 0xAF }, 1 }, { "scasq", { 0x48, 0xAF }, 2 },
    { "syscall", { 0x0F, 0x05 }, 2 }, { "rdtscp", { 0x0F, 0x01, 0xF9 }, 3 }, { "xgetbv", { 0x0F, 0x01, 0xD0 }, 3 },
    { "rdpmc", { 0x0F, 0x33 }, 2 }, { "emms", { 0x0F, 0x77 }, 2 }, { "lahf", { 0x9F }, 1 }, { "sahf", { 0x9E }, 1 },
    { "endbr64", { 0xF3, 0x0F, 0x1E, 0xFA }, 4 }, { "xlatb", { 0xD7 }, 1 },
};

// SSE arithmetic: name -> prefix, opcode
typedef struct { const char *name; int pfx; int opc; int store; } SseOp;

static const SseOp g_sse[] = {
    { "movss", 0xF3, 0x10, 0x11 }, { "movsd", 0xF2, 0x10, 0x11 },
    { "movaps", 0, 0x28, 0x29 }, { "movups", 0, 0x10, 0x11 }, { "movapd", 0x66, 0x28, 0x29 },
    { "movdqa", 0x66, 0x6F, 0x7F }, { "movdqu", 0xF3, 0x6F, 0x7F },
    { "addss", 0xF3, 0x58 }, { "addsd", 0xF2, 0x58 }, { "subss", 0xF3, 0x5C }, { "subsd", 0xF2, 0x5C },
    { "mulss", 0xF3, 0x59 }, { "mulsd", 0xF2, 0x59 }, { "divss", 0xF3, 0x5E }, { "divsd", 0xF2, 0x5E },
    { "sqrtss", 0xF3, 0x51 }, { "sqrtsd", 0xF2, 0x51 }, { "minss", 0xF3, 0x5D }, { "minsd", 0xF2, 0x5D },
    { "maxss", 0xF3, 0x5F }, { "maxsd", 0xF2, 0x5F },
    { "addps", 0, 0x58 }, { "addpd", 0x66, 0x58 }, { "mulps", 0, 0x59 }, { "mulpd", 0x66, 0x59 },
    { "subps", 0, 0x5C }, { "subpd", 0x66, 0x5C }, { "divps", 0, 0x5E }, { "divpd", 0x66, 0x5E },
    { "ucomiss", 0, 0x2E }, { "ucomisd", 0x66, 0x2E }, { "comiss", 0, 0x2F }, { "comisd", 0x66, 0x2F },
    { "cvtss2sd", 0xF3, 0x5A }, { "cvtsd2ss", 0xF2, 0x5A },
    { "xorps", 0, 0x57 }, { "xorpd", 0x66, 0x57 }, { "andps", 0, 0x54 }, { "andpd", 0x66, 0x54 },
    { "orps", 0, 0x56 }, { "orpd", 0x66, 0x56 }, { "andnps", 0, 0x55 }, { "pxor", 0x66, 0xEF },
    { "paddd", 0x66, 0xFE }, { "paddq", 0x66, 0xD4 }, { "psubd", 0x66, 0xFA }, { "pand", 0x66, 0xDB },
    { "por", 0x66, 0xEB }, { "unpcklps", 0, 0x14 }, { "unpcklpd", 0x66, 0x14 },
};

// More of SSE..SSE4.2: dst xmm, src xmm or memory [, imm8]. map: 0 = 0F, 1 = 0F 38, 2 = 0F 3A
typedef struct { const char *name; int pfx; int map; int opc; int imm; } SseEx;

static const SseEx g_sse_ex[] = {
    { "shufps", 0, 0, 0xC6, 1 }, { "shufpd", 0x66, 0, 0xC6, 1 }, { "cmpps", 0, 0, 0xC2, 1 }, { "cmppd", 0x66, 0, 0xC2, 1 },
    { "cmpss", 0xF3, 0, 0xC2, 1 }, { "cmpsd", 0xF2, 0, 0xC2, 1 }, { "pshufd", 0x66, 0, 0x70, 1 },
    { "pshuflw", 0xF2, 0, 0x70, 1 }, { "pshufhw", 0xF3, 0, 0x70, 1 },
    { "sqrtps", 0, 0, 0x51, 0 }, { "sqrtpd", 0x66, 0, 0x51, 0 }, { "rsqrtps", 0, 0, 0x52, 0 }, { "rsqrtss", 0xF3, 0, 0x52, 0 },
    { "rcpps", 0, 0, 0x53, 0 }, { "rcpss", 0xF3, 0, 0x53, 0 },
    { "minps", 0, 0, 0x5D, 0 }, { "maxps", 0, 0, 0x5F, 0 }, { "minpd", 0x66, 0, 0x5D, 0 }, { "maxpd", 0x66, 0, 0x5F, 0 },
    { "andnpd", 0x66, 0, 0x55, 0 }, { "unpckhps", 0, 0, 0x15, 0 }, { "unpckhpd", 0x66, 0, 0x15, 0 },
    { "movhlps", 0, 0, 0x12, 0 }, { "movlhps", 0, 0, 0x16, 0 },
    { "cvtdq2ps", 0, 0, 0x5B, 0 }, { "cvtps2dq", 0x66, 0, 0x5B, 0 }, { "cvttps2dq", 0xF3, 0, 0x5B, 0 },
    { "cvtps2pd", 0, 0, 0x5A, 0 }, { "cvtpd2ps", 0x66, 0, 0x5A, 0 }, { "cvtdq2pd", 0xF3, 0, 0xE6, 0 },
    { "cvttpd2dq", 0x66, 0, 0xE6, 0 }, { "cvtpd2dq", 0xF2, 0, 0xE6, 0 },
    { "haddps", 0xF2, 0, 0x7C, 0 }, { "haddpd", 0x66, 0, 0x7C, 0 }, { "hsubps", 0xF2, 0, 0x7D, 0 }, { "hsubpd", 0x66, 0, 0x7D, 0 },
    { "addsubps", 0xF2, 0, 0xD0, 0 }, { "addsubpd", 0x66, 0, 0xD0, 0 },
    { "movshdup", 0xF3, 0, 0x16, 0 }, { "movsldup", 0xF3, 0, 0x12, 0 }, { "movddup", 0xF2, 0, 0x12, 0 }, { "lddqu", 0xF2, 0, 0xF0, 0 },
    { "paddb", 0x66, 0, 0xFC, 0 }, { "paddw", 0x66, 0, 0xFD, 0 }, { "psubb", 0x66, 0, 0xF8, 0 }, { "psubw", 0x66, 0, 0xF9, 0 },
    { "psubq", 0x66, 0, 0xFB, 0 }, { "pmullw", 0x66, 0, 0xD5, 0 }, { "pmulhw", 0x66, 0, 0xE5, 0 }, { "pmulhuw", 0x66, 0, 0xE4, 0 },
    { "pmuludq", 0x66, 0, 0xF4, 0 }, { "pmaddwd", 0x66, 0, 0xF5, 0 },
    { "pcmpeqb", 0x66, 0, 0x74, 0 }, { "pcmpeqw", 0x66, 0, 0x75, 0 }, { "pcmpeqd", 0x66, 0, 0x76, 0 },
    { "pcmpgtb", 0x66, 0, 0x64, 0 }, { "pcmpgtw", 0x66, 0, 0x65, 0 }, { "pcmpgtd", 0x66, 0, 0x66, 0 }, { "pandn", 0x66, 0, 0xDF, 0 },
    { "punpcklbw", 0x66, 0, 0x60, 0 }, { "punpcklwd", 0x66, 0, 0x61, 0 }, { "punpckldq", 0x66, 0, 0x62, 0 },
    { "punpcklqdq", 0x66, 0, 0x6C, 0 }, { "punpckhbw", 0x66, 0, 0x68, 0 }, { "punpckhwd", 0x66, 0, 0x69, 0 },
    { "punpckhdq", 0x66, 0, 0x6A, 0 }, { "punpckhqdq", 0x66, 0, 0x6D, 0 },
    { "packsswb", 0x66, 0, 0x63, 0 }, { "packuswb", 0x66, 0, 0x67, 0 }, { "packssdw", 0x66, 0, 0x6B, 0 },
    { "pminub", 0x66, 0, 0xDA, 0 }, { "pmaxub", 0x66, 0, 0xDE, 0 }, { "pminsw", 0x66, 0, 0xEA, 0 }, { "pmaxsw", 0x66, 0, 0xEE, 0 },
    { "pavgb", 0x66, 0, 0xE0, 0 }, { "pavgw", 0x66, 0, 0xE3, 0 }, { "psadbw", 0x66, 0, 0xF6, 0 },
    { "paddusb", 0x66, 0, 0xDC, 0 }, { "paddusw", 0x66, 0, 0xDD, 0 }, { "psubusb", 0x66, 0, 0xD8, 0 }, { "psubusw", 0x66, 0, 0xD9, 0 },
    { "paddsb", 0x66, 0, 0xEC, 0 }, { "paddsw", 0x66, 0, 0xED, 0 }, { "psubsb", 0x66, 0, 0xE8, 0 }, { "psubsw", 0x66, 0, 0xE9, 0 },
    { "psllw", 0x66, 0, 0xF1, 0 }, { "pslld", 0x66, 0, 0xF2, 0 }, { "psllq", 0x66, 0, 0xF3, 0 },
    { "psrlw", 0x66, 0, 0xD1, 0 }, { "psrld", 0x66, 0, 0xD2, 0 }, { "psrlq", 0x66, 0, 0xD3, 0 },
    { "psraw", 0x66, 0, 0xE1, 0 }, { "psrad", 0x66, 0, 0xE2, 0 },
    { "pshufb", 0x66, 1, 0x00, 0 }, { "phaddw", 0x66, 1, 0x01, 0 }, { "phaddd", 0x66, 1, 0x02, 0 }, { "pmaddubsw", 0x66, 1, 0x04, 0 },
    { "pmulhrsw", 0x66, 1, 0x0B, 0 }, { "pabsb", 0x66, 1, 0x1C, 0 }, { "pabsw", 0x66, 1, 0x1D, 0 }, { "pabsd", 0x66, 1, 0x1E, 0 },
    { "ptest", 0x66, 1, 0x17, 0 }, { "pmulld", 0x66, 1, 0x40, 0 }, { "pmuldq", 0x66, 1, 0x28, 0 },
    { "pminsb", 0x66, 1, 0x38, 0 }, { "pminsd", 0x66, 1, 0x39, 0 }, { "pminuw", 0x66, 1, 0x3A, 0 }, { "pminud", 0x66, 1, 0x3B, 0 },
    { "pmaxsb", 0x66, 1, 0x3C, 0 }, { "pmaxsd", 0x66, 1, 0x3D, 0 }, { "pmaxuw", 0x66, 1, 0x3E, 0 }, { "pmaxud", 0x66, 1, 0x3F, 0 },
    { "pmovsxbw", 0x66, 1, 0x20, 0 }, { "pmovsxbd", 0x66, 1, 0x21, 0 }, { "pmovsxwd", 0x66, 1, 0x23, 0 }, { "pmovsxdq", 0x66, 1, 0x25, 0 },
    { "pmovzxbw", 0x66, 1, 0x30, 0 }, { "pmovzxbd", 0x66, 1, 0x31, 0 }, { "pmovzxwd", 0x66, 1, 0x33, 0 }, { "pmovzxdq", 0x66, 1, 0x35, 0 },
    { "pcmpeqq", 0x66, 1, 0x29, 0 }, { "pcmpgtq", 0x66, 1, 0x37, 0 }, { "packusdw", 0x66, 1, 0x2B, 0 },
    { "aesenc", 0x66, 1, 0xDC, 0 }, { "aesenclast", 0x66, 1, 0xDD, 0 }, { "aesdec", 0x66, 1, 0xDE, 0 },
    { "aesdeclast", 0x66, 1, 0xDF, 0 }, { "aesimc", 0x66, 1, 0xDB, 0 },
    { "roundps", 0x66, 2, 0x08, 1 }, { "roundpd", 0x66, 2, 0x09, 1 }, { "roundss", 0x66, 2, 0x0A, 1 }, { "roundsd", 0x66, 2, 0x0B, 1 },
    { "blendps", 0x66, 2, 0x0C, 1 }, { "blendpd", 0x66, 2, 0x0D, 1 }, { "pblendw", 0x66, 2, 0x0E, 1 }, { "palignr", 0x66, 2, 0x0F, 1 },
    { "dpps", 0x66, 2, 0x40, 1 }, { "dppd", 0x66, 2, 0x41, 1 }, { "mpsadbw", 0x66, 2, 0x42, 1 }, { "pclmulqdq", 0x66, 2, 0x44, 1 },
    { "pcmpestrm", 0x66, 2, 0x60, 1 }, { "pcmpestri", 0x66, 2, 0x61, 1 }, { "pcmpistrm", 0x66, 2, 0x62, 1 },
    { "pcmpistri", 0x66, 2, 0x63, 1 }, { "insertps", 0x66, 2, 0x21, 1 }, { "aeskeygenassist", 0x66, 2, 0xDF, 1 },
};

// [prefix] [REX] 0F [38|3A] opcode ModRM [imm8]
static void op0f(Asm *as, int pfx, int w, int map, int opc, int reg_field, Operand *reg_op, Operand *rm, Operand *imm)
{
    u8 o[3];
    int n = 0;
    o[n++] = 0x0F;
    if (map == 1) o[n++] = 0x38;
    if (map == 2) o[n++] = 0x3A;
    o[n++] = (u8)opc;
    encode(as, pfx, w, o, n, reg_field, reg_op, rm, imm ? 1 : 0);
    if (imm) {
        if (imm->kind != OP_IMM)
            asm_error(as, "последним нужно число");
        emit_imm(as, imm, 1);
    }
    finish_rip(as);
}

static int sse_ex(Asm *as, const char *m, int mlen, Operand *ops, int n)
{
    for (int i = 0; i < (int)(sizeof(g_sse_ex) / sizeof(g_sse_ex[0])); i++) {
        const SseEx *e = &g_sse_ex[i];
        if (!word_eq(m, mlen, e->name))
            continue;
        // psllw..psrad, pslldq, psrldq with a number: 66 0F 71/72/73 /digit ib
        if (n == 2 && ops[1].kind == OP_IMM && e->map == 0 && !e->imm) {
            static const struct { const char *name; int opc, digit; } sh[] = {
                { "psllw", 0x71, 6 }, { "pslld", 0x72, 6 }, { "psllq", 0x73, 6 }, { "psrlw", 0x71, 2 },
                { "psrld", 0x72, 2 }, { "psrlq", 0x73, 2 }, { "psraw", 0x71, 4 }, { "psrad", 0x72, 4 },
            };
            for (int k = 0; k < 8; k++)
                if (word_eq(m, mlen, sh[k].name)) {
                    op0f(as, 0x66, 0, 0, sh[k].opc, sh[k].digit, 0, &ops[0], &ops[1]);
                    return 1;
                }
        }
        need(as, n, e->imm ? 3 : 2);
        if (ops[0].kind != OP_XMM || (ops[1].kind != OP_XMM && ops[1].kind != OP_MEM))
            asm_error(as, "%s: приёмник — xmm, источник — xmm или память", e->name);
        op0f(as, e->pfx, 0, e->map, e->opc, ops[0].reg, 0, &ops[1], e->imm ? &ops[2] : 0);
        return 1;
    }
    if (word_eq(m, mlen, "pslldq") || word_eq(m, mlen, "psrldq")) {
        need(as, n, 2);
        op0f(as, 0x66, 0, 0, 0x73, word_eq(m, mlen, "pslldq") ? 7 : 3, 0, &ops[0], &ops[1]);
        return 1;
    }
    // with a general register on one side
    if (word_eq(m, mlen, "pmovmskb") || word_eq(m, mlen, "movmskps") || word_eq(m, mlen, "movmskpd")) {
        need(as, n, 2);
        if (ops[0].kind != OP_REG || ops[1].kind != OP_XMM)
            asm_error(as, "приёмник — регистр, источник — xmm");
        int pfx = word_eq(m, mlen, "movmskps") ? 0 : 0x66;
        op0f(as, pfx, ops[0].size == 8, 0, word_eq(m, mlen, "pmovmskb") ? 0xD7 : 0x50, ops[0].reg, &ops[0], &ops[1], 0);
        return 1;
    }
    if (word_eq(m, mlen, "pextrb") || word_eq(m, mlen, "pextrd") || word_eq(m, mlen, "pextrq") ||
        word_eq(m, mlen, "extractps")) {
        need(as, n, 3);
        if (ops[1].kind != OP_XMM)
            asm_error(as, "источник — xmm");
        int opc = word_eq(m, mlen, "pextrb") ? 0x14 : word_eq(m, mlen, "extractps") ? 0x17 : 0x16;
        op0f(as, 0x66, word_eq(m, mlen, "pextrq"), 2, opc, ops[1].reg, 0, &ops[0], &ops[2]);
        return 1;
    }
    if (word_eq(m, mlen, "pinsrb") || word_eq(m, mlen, "pinsrd") || word_eq(m, mlen, "pinsrq")) {
        need(as, n, 3);
        if (ops[0].kind != OP_XMM)
            asm_error(as, "приёмник — xmm");
        op0f(as, 0x66, word_eq(m, mlen, "pinsrq"), 2, word_eq(m, mlen, "pinsrb") ? 0x20 : 0x22, ops[0].reg, 0, &ops[1], &ops[2]);
        return 1;
    }
    if (word_eq(m, mlen, "pextrw")) {
        need(as, n, 3);
        op0f(as, 0x66, 0, 0, 0xC5, ops[0].reg, &ops[0], &ops[1], &ops[2]);
        return 1;
    }
    if (word_eq(m, mlen, "pinsrw")) {
        need(as, n, 3);
        op0f(as, 0x66, 0, 0, 0xC4, ops[0].reg, 0, &ops[1], &ops[2]);
        return 1;
    }
    if (word_eq(m, mlen, "cvtsd2si") || word_eq(m, mlen, "cvtss2si")) {
        need(as, n, 2);
        if (ops[0].kind != OP_REG || (ops[0].size != 4 && ops[0].size != 8))
            asm_error(as, "приёмник — 32- или 64-битный регистр");
        op0f(as, word_eq(m, mlen, "cvtsd2si") ? 0xF2 : 0xF3, ops[0].size == 8, 0, 0x2D, ops[0].reg, &ops[0], &ops[1], 0);
        return 1;
    }
    // stores and half loads: movntps m, xmm; movhps xmm, m / m, xmm ...
    static const struct { const char *name; int pfx, load, store; } half[] = {
        { "movhps", 0, 0x16, 0x17 }, { "movlps", 0, 0x12, 0x13 }, { "movhpd", 0x66, 0x16, 0x17 }, { "movlpd", 0x66, 0x12, 0x13 },
        { "movntps", 0, 0, 0x2B }, { "movntpd", 0x66, 0, 0x2B }, { "movntdq", 0x66, 0, 0xE7 },
    };
    for (int i = 0; i < (int)(sizeof(half) / sizeof(half[0])); i++)
        if (word_eq(m, mlen, half[i].name)) {
            need(as, n, 2);
            if (ops[0].kind == OP_XMM && ops[1].kind == OP_MEM && half[i].load)
                op0f(as, half[i].pfx, 0, 0, half[i].load, ops[0].reg, 0, &ops[1], 0);
            else if (ops[0].kind == OP_MEM && ops[1].kind == OP_XMM)
                op0f(as, half[i].pfx, 0, 0, half[i].store, ops[1].reg, 0, &ops[0], 0);
            else
                asm_error(as, "%s: xmm и память", half[i].name);
            return 1;
        }
    return 0;
}

// ---- AVX: VEX-coded, ymm registers (256 bits) and three operands ----
// pp: 0 none, 1 = 66, 2 = F3, 3 = F2; map: 1 = 0F, 2 = 0F 38, 3 = 0F 3A
static void vex(Asm *as, int pp, int map, int w, int l, int opc, int reg, int vvvv, Operand *rm)
{
    int r = !(reg & 8), x = 1, b = 1;
    if (rm->kind == OP_REG || rm->kind == OP_XMM) {
        b = !(rm->reg & 8);
    } else {
        if (rm->index >= 0) x = !(rm->index & 8);
        if (rm->base >= 0 && rm->base != REG_RIP) b = !(rm->base & 8);
    }
    if (x && b && !w && map == 1) {
        emit8(as, 0xC5);
        emit8(as, (r << 7) | ((~vvvv & 15) << 3) | (l << 2) | pp);
    } else {
        emit8(as, 0xC4);
        emit8(as, (r << 7) | (x << 6) | (b << 5) | map);
        emit8(as, (w << 7) | ((~vvvv & 15) << 3) | (l << 2) | pp);
    }
    emit8(as, opc);
    modrm(as, reg, rm);
}

// form: 0 three operands (dst, src1, src2/m), 1 two (dst, src/m), 2 a move (load opc, store opc2)
typedef struct { const char *name; int pp, map, opc, w, form, opc2, imm; } AvxOp;

static const AvxOp g_avx[] = {
    { "vaddps", 0, 1, 0x58 }, { "vaddpd", 1, 1, 0x58 }, { "vaddss", 2, 1, 0x58 }, { "vaddsd", 3, 1, 0x58 },
    { "vsubps", 0, 1, 0x5C }, { "vsubpd", 1, 1, 0x5C }, { "vsubss", 2, 1, 0x5C }, { "vsubsd", 3, 1, 0x5C },
    { "vmulps", 0, 1, 0x59 }, { "vmulpd", 1, 1, 0x59 }, { "vmulss", 2, 1, 0x59 }, { "vmulsd", 3, 1, 0x59 },
    { "vdivps", 0, 1, 0x5E }, { "vdivpd", 1, 1, 0x5E }, { "vdivss", 2, 1, 0x5E }, { "vdivsd", 3, 1, 0x5E },
    { "vminps", 0, 1, 0x5D }, { "vminpd", 1, 1, 0x5D }, { "vminss", 2, 1, 0x5D }, { "vminsd", 3, 1, 0x5D },
    { "vmaxps", 0, 1, 0x5F }, { "vmaxpd", 1, 1, 0x5F }, { "vmaxss", 2, 1, 0x5F }, { "vmaxsd", 3, 1, 0x5F },
    { "vandps", 0, 1, 0x54 }, { "vandpd", 1, 1, 0x54 }, { "vandnps", 0, 1, 0x55 }, { "vandnpd", 1, 1, 0x55 },
    { "vorps", 0, 1, 0x56 }, { "vorpd", 1, 1, 0x56 }, { "vxorps", 0, 1, 0x57 }, { "vxorpd", 1, 1, 0x57 },
    { "vunpcklps", 0, 1, 0x14 }, { "vunpckhps", 0, 1, 0x15 }, { "vunpcklpd", 1, 1, 0x14 }, { "vunpckhpd", 1, 1, 0x15 },
    { "vhaddps", 3, 1, 0x7C }, { "vhaddpd", 1, 1, 0x7C },
    { "vpaddb", 1, 1, 0xFC }, { "vpaddw", 1, 1, 0xFD }, { "vpaddd", 1, 1, 0xFE }, { "vpaddq", 1, 1, 0xD4 },
    { "vpsubb", 1, 1, 0xF8 }, { "vpsubw", 1, 1, 0xF9 }, { "vpsubd", 1, 1, 0xFA }, { "vpsubq", 1, 1, 0xFB },
    { "vpand", 1, 1, 0xDB }, { "vpandn", 1, 1, 0xDF }, { "vpor", 1, 1, 0xEB }, { "vpxor", 1, 1, 0xEF },
    { "vpcmpeqb", 1, 1, 0x74 }, { "vpcmpeqw", 1, 1, 0x75 }, { "vpcmpeqd", 1, 1, 0x76 }, { "vpcmpgtd", 1, 1, 0x66 },
    { "vpmullw", 1, 1, 0xD5 }, { "vpmuludq", 1, 1, 0xF4 },
    { "vshufps", 0, 1, 0xC6, 0, 0, 0, 1 }, { "vshufpd", 1, 1, 0xC6, 0, 0, 0, 1 },
    { "vcmpps", 0, 1, 0xC2, 0, 0, 0, 1 }, { "vcmppd", 1, 1, 0xC2, 0, 0, 0, 1 },
    { "vpshufb", 1, 2, 0x00 }, { "vpmulld", 1, 2, 0x40 }, { "vpermilps", 1, 2, 0x0C }, { "vpermps", 1, 2, 0x16 },
    { "vpermd", 1, 2, 0x36 }, { "vpminsd", 1, 2, 0x39 }, { "vpmaxsd", 1, 2, 0x3D },
    { "vfmadd132ps", 1, 2, 0x98, 0 }, { "vfmadd132pd", 1, 2, 0x98, 1 }, { "vfmadd213ps", 1, 2, 0xA8, 0 },
    { "vfmadd213pd", 1, 2, 0xA8, 1 }, { "vfmadd231ps", 1, 2, 0xB8, 0 }, { "vfmadd231pd", 1, 2, 0xB8, 1 },
    { "vfmadd231ss", 1, 2, 0xB9, 0 }, { "vfmadd231sd", 1, 2, 0xB9, 1 }, { "vfmadd213ss", 1, 2, 0xA9, 0 },
    { "vfmadd213sd", 1, 2, 0xA9, 1 }, { "vfmsub231ps", 1, 2, 0xBA, 0 }, { "vfmsub231pd", 1, 2, 0xBA, 1 },
    { "vfnmadd231ps", 1, 2, 0xBC, 0 }, { "vfnmadd231pd", 1, 2, 0xBC, 1 },
    { "vblendps", 1, 3, 0x0C, 0, 0, 0, 1 }, { "vblendpd", 1, 3, 0x0D, 0, 0, 0, 1 }, { "vdpps", 1, 3, 0x40, 0, 0, 0, 1 },
    { "vperm2f128", 1, 3, 0x06, 0, 0, 0, 1 }, { "vinsertf128", 1, 3, 0x18, 0, 0, 0, 1 },
    { "vsqrtps", 0, 1, 0x51, 0, 1 }, { "vsqrtpd", 1, 1, 0x51, 0, 1 }, { "vrsqrtps", 0, 1, 0x52, 0, 1 },
    { "vrcpps", 0, 1, 0x53, 0, 1 }, { "vcvtdq2ps", 0, 1, 0x5B, 0, 1 }, { "vcvtps2dq", 1, 1, 0x5B, 0, 1 },
    { "vcvttps2dq", 2, 1, 0x5B, 0, 1 }, { "vcvtps2pd", 0, 1, 0x5A, 0, 1 }, { "vcvtpd2ps", 1, 1, 0x5A, 0, 1 },
    { "vbroadcastss", 1, 2, 0x18, 0, 1 }, { "vbroadcastsd", 1, 2, 0x19, 0, 1 }, { "vpbroadcastd", 1, 2, 0x58, 0, 1 },
    { "vptest", 1, 2, 0x17, 0, 1 }, { "vroundps", 1, 3, 0x08, 0, 1, 0, 1 }, { "vroundpd", 1, 3, 0x09, 0, 1, 0, 1 },
    { "vpshufd", 1, 1, 0x70, 0, 1, 0, 1 },
    { "vmovups", 0, 1, 0x10, 0, 2, 0x11 }, { "vmovupd", 1, 1, 0x10, 0, 2, 0x11 }, { "vmovaps", 0, 1, 0x28, 0, 2, 0x29 },
    { "vmovapd", 1, 1, 0x28, 0, 2, 0x29 }, { "vmovdqu", 2, 1, 0x6F, 0, 2, 0x7F }, { "vmovdqa", 1, 1, 0x6F, 0, 2, 0x7F },
    { "vmovss", 2, 1, 0x10, 0, 2, 0x11 }, { "vmovsd", 3, 1, 0x10, 0, 2, 0x11 },
};

static int avx(Asm *as, const char *m, int mlen, Operand *ops, int n)
{
    if (word_eq(m, mlen, "vzeroupper") || word_eq(m, mlen, "vzeroall")) {
        emit8(as, 0xC5);
        emit8(as, word_eq(m, mlen, "vzeroall") ? 0xFC : 0xF8);
        emit8(as, 0x77);
        return 1;
    }
    if (word_eq(m, mlen, "vextractf128")) {             // xmm/m128, ymm, imm8
        need(as, n, 3);
        vex(as, 1, 3, 0, 1, 0x19, ops[1].reg, 0, &ops[0]);
        emit_imm(as, &ops[2], 1);
        finish_rip(as);
        return 1;
    }
    for (int i = 0; i < (int)(sizeof(g_avx) / sizeof(g_avx[0])); i++) {
        const AvxOp *e = &g_avx[i];
        if (!word_eq(m, mlen, e->name))
            continue;
        int extra = e->imm ? 1 : 0;
        if (e->form == 2 && n == 3) {                   // vmovss xmm1, xmm2, xmm3
            vex(as, e->pp, e->map, e->w, 0, e->opc, ops[0].reg, ops[1].reg, &ops[2]);
        } else if (e->form == 2) {
            need(as, n, 2);
            if (ops[0].kind == OP_XMM)
                vex(as, e->pp, e->map, e->w, ops[0].size == 32, e->opc, ops[0].reg, 0, &ops[1]);
            else if (ops[1].kind == OP_XMM)
                vex(as, e->pp, e->map, e->w, ops[1].size == 32, e->opc2, ops[1].reg, 0, &ops[0]);
            else
                asm_error(as, "%s: нужен регистр xmm или ymm", e->name);
        } else if (e->form == 1) {
            need(as, n, 2 + extra);
            if (ops[0].kind != OP_XMM)
                asm_error(as, "%s: приёмник — xmm или ymm", e->name);
            vex(as, e->pp, e->map, e->w, ops[0].size == 32, e->opc, ops[0].reg, 0, &ops[1]);
        } else {
            need(as, n, 3 + extra);
            if (ops[0].kind != OP_XMM || ops[1].kind != OP_XMM)
                asm_error(as, "%s: первые два — регистры xmm или ymm", e->name);
            vex(as, e->pp, e->map, e->w, ops[0].size == 32, e->opc, ops[0].reg, ops[1].reg, &ops[2]);
        }
        if (extra)
            emit_imm(as, &ops[n - 1], 1);
        finish_rip(as);
        return 1;
    }
    return 0;
}

// popcnt, bswap, bt, loop, rdrand, prefetch, crc32, movabs...: 1 if it was one of them
static int more_ops(Asm *as, const char *m, int mlen, Operand *ops, int n)
{
    if (word_eq(m, mlen, "popcnt") || word_eq(m, mlen, "lzcnt") || word_eq(m, mlen, "tzcnt")) {
        need(as, n, 2);
        if (ops[0].kind != OP_REG || ops[0].size < 2)
            asm_error(as, "приёмник — регистр");
        if (ops[0].size == 2)
            emit8(as, 0x66);
        int opc = word_eq(m, mlen, "popcnt") ? 0xB8 : word_eq(m, mlen, "lzcnt") ? 0xBD : 0xBC;
        op0f(as, 0xF3, ops[0].size == 8, 0, opc, ops[0].reg, &ops[0], &ops[1], 0);
        return 1;
    }
    if (word_eq(m, mlen, "bswap")) {
        need(as, n, 1);
        if (ops[0].kind != OP_REG || ops[0].size < 4)
            asm_error(as, "bswap: 32- или 64-битный регистр");
        int rex = (ops[0].size == 8 ? 8 : 0) | (ops[0].reg & 8 ? 1 : 0);
        if (rex)
            emit8(as, 0x40 | rex);
        emit8(as, 0x0F);
        emit8(as, 0xC8 + (ops[0].reg & 7));
        return 1;
    }
    static const char *bts[] = { "bt", "bts", "btr", "btc" };
    for (int i = 0; i < 4; i++)
        if (word_eq(m, mlen, bts[i])) {
            need(as, n, 2);
            int size = op_size(as, &ops[0], ops[1].kind == OP_REG ? &ops[1] : 0);
            if (size == 1)
                asm_error(as, "%s: 16, 32 или 64 бита", bts[i]);
            if (ops[1].kind == OP_IMM)
                op0f(as, size_prefix(size), size_w(size), 0, 0xBA, 4 + i, 0, &ops[0], &ops[1]);
            else
                op0f(as, size_prefix(size), size_w(size), 0, 0xA3 + 8 * i, ops[1].reg, &ops[1], &ops[0], 0);
            return 1;
        }
    static const struct { const char *name; int opc; } loops[] = {
        { "loop", 0xE2 }, { "loope", 0xE1 }, { "loopz", 0xE1 }, { "loopne", 0xE0 }, { "loopnz", 0xE0 }, { "jrcxz", 0xE3 },
        { "jecxz", 0x1E3 },
    };
    for (int i = 0; i < 7; i++)
        if (word_eq(m, mlen, loops[i].name)) {
            need(as, n, 1);
            if (ops[0].kind != OP_IMM || !ops[0].sym)
                asm_error(as, "%s: нужна метка", loops[i].name);
            if (loops[i].opc > 0xFF)
                emit8(as, 0x67);
            emit8(as, loops[i].opc & 0xFF);
            int at = as->pos[SEC_TEXT];
            emit8(as, 0);
            if (as->pass == 2) {
                Label *l = find_label(as, ops[0].sym, ops[0].sym_len);
                if (!l || !l->defined)
                    asm_error(as, "нет такой метки %s", cc_strndup(ops[0].sym, ops[0].sym_len));
                i64 rel = (i64)(u64)(as->base[l->section] + l->offset) - (i64)current_address(as);
                if (!fits8(rel))
                    asm_error(as, "%s достаёт только на 128 байт", loops[i].name);
                as->base[SEC_TEXT][at] = (u8)rel;
            }
            return 1;
        }
    if (word_eq(m, mlen, "rdrand") || word_eq(m, mlen, "rdseed")) {
        need(as, n, 1);
        if (ops[0].kind != OP_REG || ops[0].size < 2)
            asm_error(as, "приёмник — регистр");
        op0f(as, size_prefix(ops[0].size), size_w(ops[0].size), 0, 0xC7, word_eq(m, mlen, "rdrand") ? 6 : 7, 0, &ops[0], 0);
        return 1;
    }
    static const struct { const char *name; int opc, digit; } pf[] = {
        { "prefetchnta", 0x18, 0 }, { "prefetcht0", 0x18, 1 }, { "prefetcht1", 0x18, 2 }, { "prefetcht2", 0x18, 3 },
        { "prefetchw", 0x0D, 1 }, { "clflush", 0xAE, 7 },
    };
    for (int i = 0; i < 6; i++)
        if (word_eq(m, mlen, pf[i].name)) {
            need(as, n, 1);
            if (ops[0].kind != OP_MEM)
                asm_error(as, "%s: нужен адрес", pf[i].name);
            op0f(as, 0, 0, 0, pf[i].opc, pf[i].digit, 0, &ops[0], 0);
            return 1;
        }
    if (word_eq(m, mlen, "crc32")) {
        need(as, n, 2);
        if (ops[0].kind != OP_REG || ops[0].size < 4)
            asm_error(as, "crc32: приёмник — 32- или 64-битный регистр");
        int src = ops[1].kind == OP_REG ? ops[1].size : ops[1].size;
        if (!src)
            asm_error(as, "укажите размер: byte, word, dword или qword ptr");
        if (src == 2)
            emit8(as, 0x66);
        op0f(as, 0xF2, ops[0].size == 8, 1, src == 1 ? 0xF0 : 0xF1, ops[0].reg, &ops[0], &ops[1], 0);
        return 1;
    }
    static const struct { const char *name; int digit; } mx[] = {
        { "fxsave", 0 }, { "fxrstor", 1 }, { "ldmxcsr", 2 }, { "stmxcsr", 3 },
    };
    for (int i = 0; i < 4; i++)
        if (word_eq(m, mlen, mx[i].name)) {         // 0F AE /digit with an address
            need(as, n, 1);
            if (ops[0].kind != OP_MEM)
                asm_error(as, "%s: нужен адрес", mx[i].name);
            op0f(as, 0, 0, 0, 0xAE, mx[i].digit, 0, &ops[0], 0);
            return 1;
        }
    if (word_eq(m, mlen, "movabs")) {               // mov with the full 64-bit number
        need(as, n, 2);
        if (ops[0].kind != OP_REG || ops[0].size != 8 || ops[1].kind != OP_IMM)
            asm_error(as, "movabs: 64-битный регистр и число");
        emit8(as, 0x48 | ((ops[0].reg & 8) ? 1 : 0));
        emit8(as, 0xB8 + (ops[0].reg & 7));
        emit_imm(as, &ops[1], 8);
        return 1;
    }
    return 0;
}

// ---- directives ----

static void directive_data(Asm *as, const char *p, int unit)
{
    for (;;) {
        p = skip_sp(p);
        if (!*p || *p == '\n' || *p == ';')
            break;
        if (*p == '"' || (*p == '\'' && unit == 1 && p[1] && p[2] != '\'')) {
            char q = *p++;
            while (*p && *p != q && *p != '\n') {
                int c = (u8)*p++;
                if (c == '\\' && q == '"') {
                    c = (u8)*p++;
                    if (c == 'n') c = '\n';
                    else if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                    else if (c == '0') c = 0;
                }
                if (unit == 1) emit8(as, c);
                else if (unit == 2) emit16(as, (u64)c);
                else if (unit == 4) emit32(as, (u64)c);
                else emit64(as, (u64)c);
            }
            if (*p != q)
                asm_error(as, "строка не закрыта");
            p++;
        } else {
            Operand op = { 0 };
            op.base = op.index = -1;
            parse_terms(as, &p, &op, 0);
            if (op.sym && unit != 8)
                asm_error(as, "адрес помещается только в dq");
            emit_imm(as, &op, unit);
        }
        p = skip_sp(p);
        if (*p != ',')
            break;
        p++;
    }
}

static void align_section(Asm *as, int a)
{
    if (a <= 0 || (a & (a - 1)))
        asm_error(as, ".align: степень двойки");
    while (as->pos[as->section] % a)
        emit8(as, as->section == SEC_TEXT ? 0x90 : 0);
}

// ---- debug info: .dbgfile, .loc, .dbgfunc, .dbgend (codegen.c) ----

static void *grow(void *old, int n, int *cap, int size)
{
    if (n < *cap)
        return old;
    int nc = *cap ? *cap * 2 : 256;
    char *nb = cc_alloc((size_t)nc * (size_t)size);
    for (int i = 0; i < n * size; i++)
        nb[i] = ((char *)old)[i];
    *cap = nc;
    return nb;
}

// The rest of the line without the spaces around it.
static int rest_of_line(const char *p, const char **start)
{
    p = skip_sp(p);
    int n = 0;
    while (p[n] && p[n] != '\n')
        n++;
    while (n && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r'))
        n--;
    *start = p;
    return n;
}

// Which C line the code is of, for errors: every pass, every mode.
static void where_in_c(Asm *as, const char *m, int mlen, const char *p)
{
    i64 a = 0, b = 0;
    p = skip_sp(p);
    if (word_eq(m, mlen, ".dbgfile")) {
        if (parse_number(as, &p, &a) && a >= 0 && a < 256)
            as->c_file_len[a] = rest_of_line(skip_sp(p), &as->c_files[a]);
    } else if (word_eq(m, mlen, ".loc")) {
        if (parse_number(as, &p, &a) && a >= 0 && a < 256) {
            p = skip_sp(p);
            if (parse_number(as, &p, &b)) {
                as->c_file = (int)a;
                as->c_line = (int)b;
            }
        }
    } else if (word_eq(m, mlen, ".dbgend")) {
        as->c_line = 0;                     // what follows is not that function's
    }
}

static void dbg_directive(Asm *as, const char *m, int mlen, const char *p)
{
    int off = as->pos[SEC_TEXT];
    i64 a = 0, b = 0;
    p = skip_sp(p);
    if (word_eq(m, mlen, ".dbgfile")) {
        if (!parse_number(as, &p, &a) || a < 0 || a >= 256)
            asm_error(as, ".dbgfile: номер файла");
        as->dfile_len[a] = rest_of_line(p, &as->dfile[a]);
        if (a >= as->ndfile)
            as->ndfile = (int)a + 1;
    } else if (word_eq(m, mlen, ".loc")) {
        if (!parse_number(as, &p, &a))
            asm_error(as, ".loc: номер файла");
        p = skip_sp(p);
        if (!parse_number(as, &p, &b))
            asm_error(as, ".loc: строка");
        if (as->ndl && as->dlines[as->ndl - 1].off == off)
            as->ndl--;                      // no code since the last one: this line takes its place
        as->dlines = grow(as->dlines, as->ndl, &as->dlcap, (int)sizeof(DbgLine));
        DbgLine *l = &as->dlines[as->ndl++];
        l->off = off;
        l->line = (int)b;
        l->file = (int)a;
    } else if (word_eq(m, mlen, ".dbgfunc")) {
        as->dfuncs = grow(as->dfuncs, as->ndf, &as->dfcap, (int)sizeof(DbgFunc));
        DbgFunc *f = &as->dfuncs[as->ndf++];
        f->start = f->end = off;
        f->len = rest_of_line(p, &f->name);
    } else if (!word_eq(m, mlen, ".dbgend")) {      // .dbgframe .dbgvar .dbgglobal .dbgstruct .dbgfield
        as->dextra = grow(as->dextra, as->ndx, &as->dxcap, (int)sizeof(DbgExtra));
        DbgExtra *x = &as->dextra[as->ndx++];
        x->kind = word_eq(m, mlen, ".dbgframe") ? 'F' : word_eq(m, mlen, ".dbgvar") ? 'v' :
                  word_eq(m, mlen, ".dbgglobal") ? 'g' : word_eq(m, mlen, ".dbgstruct") ? 's' : 'f';
        x->func = as->ndf - 1;
        x->len = rest_of_line(p, &x->s);
    } else if (as->ndf) {                   // .dbgend
        as->dfuncs[as->ndf - 1].end = off;
    }
}

// .seh_proc KIND / .seh_scope begin, end, funclet, target / .seh_endproc (pass 1)
static void seh_directive(Asm *as, const char *m, int mlen, const char *p)
{
    int off = as->pos[SEC_TEXT];
    if (word_eq(m, mlen, ".seh_proc")) {
        i64 k = 0;
        p = skip_sp(p);
        parse_number(as, &p, &k);
        as->procs = grow(as->procs, as->nproc, &as->proccap, (int)sizeof(SehProc));
        SehProc *f = &as->procs[as->nproc++];
        SehProc z = { 0 };
        *f = z;
        f->start = f->end = off;
        f->kind = (int)k;
        // ".seh_proc 0, PAD, R1, R2...": the registers pushed after rbp (their numbers: rbx 3, rsi 6...)
        p = skip_sp(p);
        if (*p == ',') {
            i64 v = 0;
            p = skip_sp(p + 1);
            parse_number(as, &p, &v);
            f->pad = (int)v;
            for (p = skip_sp(p); *p == ',' && f->nregs < 8; p = skip_sp(p)) {
                p = skip_sp(p + 1);
                parse_number(as, &p, &v);
                f->regs[f->nregs++] = (u8)v;
            }
        }
        return;
    }
    if (!as->nproc)
        asm_error(as, "%s без .seh_proc", cc_strndup(m, mlen));
    SehProc *f = &as->procs[as->nproc - 1];
    if (word_eq(m, mlen, ".seh_endproc")) {
        f->end = off;
        return;
    }
    SehScope *sc = cc_alloc(sizeof(SehScope));
    for (int i = 0; i < 4; i++) {
        p = skip_sp(p);
        const char *s = p;
        while (is_id2((unsigned char)*p))
            p++;
        if (p == s)
            asm_error(as, ".seh_scope: четыре метки");
        sc->name[i] = s;
        sc->len[i] = (int)(p - s);
        p = skip_sp(p);
        if (*p == ',')
            p++;
    }
    if (f->tail) f->tail->next = sc; else f->scopes = sc;
    f->tail = sc;
    f->nscopes++;
}

// ---- x87: the old floating-point unit (fld, fsin, fstp...) ----
// Registers st / st(0)..st(7); memory as "dword|qword|tbyte|word ptr [...]".

static const struct { const char *name; u8 a, b; } g_x87_plain[] = {
    { "fsin", 0xD9, 0xFE }, { "fcos", 0xD9, 0xFF }, { "fsincos", 0xD9, 0xFB }, { "fsqrt", 0xD9, 0xFA },
    { "fptan", 0xD9, 0xF2 }, { "fpatan", 0xD9, 0xF3 }, { "fabs", 0xD9, 0xE1 }, { "fchs", 0xD9, 0xE0 },
    { "fld1", 0xD9, 0xE8 }, { "fldz", 0xD9, 0xEE }, { "fldpi", 0xD9, 0xEB }, { "fldl2e", 0xD9, 0xEA },
    { "fldl2t", 0xD9, 0xE9 }, { "fldlg2", 0xD9, 0xEC }, { "fldln2", 0xD9, 0xED }, { "f2xm1", 0xD9, 0xF0 },
    { "fyl2x", 0xD9, 0xF1 }, { "fyl2xp1", 0xD9, 0xF9 }, { "fscale", 0xD9, 0xFD }, { "frndint", 0xD9, 0xFC },
    { "fprem", 0xD9, 0xF8 }, { "fprem1", 0xD9, 0xF5 }, { "fxtract", 0xD9, 0xF4 }, { "ftst", 0xD9, 0xE4 },
    { "fxam", 0xD9, 0xE5 }, { "fnop", 0xD9, 0xD0 }, { "fincstp", 0xD9, 0xF7 }, { "fdecstp", 0xD9, 0xF6 },
    { "fninit", 0xDB, 0xE3 }, { "fnclex", 0xDB, 0xE2 }, { "fcompp", 0xDE, 0xD9 }, { "fucompp", 0xDA, 0xE9 },
    { "faddp", 0xDE, 0xC1 }, { "fmulp", 0xDE, 0xC9 }, { "fsubp", 0xDE, 0xE9 }, { "fsubrp", 0xDE, 0xE1 },
    { "fdivp", 0xDE, 0xF9 }, { "fdivrp", 0xDE, 0xF1 },
};

// st, st0, st(3) -> 0..7; otherwise -1
static int x87_reg(const char **pp)
{
    const char *p = skip_sp(*pp);
    if (!((p[0] == 's' || p[0] == 'S') && (p[1] == 't' || p[1] == 'T')) ||
        (is_id2((unsigned char)p[2]) && !(p[2] >= '0' && p[2] <= '7')))
        return -1;
    p += 2;
    int r = 0;
    if (*p >= '0' && *p <= '7') r = *p++ - '0';
    else if (*p == '(') {
        p = skip_sp(p + 1);
        if (*p < '0' || *p > '7') return -1;
        r = *p++ - '0';
        p = skip_sp(p);
        if (*p != ')') return -1;
        p++;
    }
    *pp = skip_sp(p);
    return r;
}

// the rest of the line after the mnemonic: up to two st registers or one memory operand
static int x87_operands(Asm *as, const char *p, int *st, Operand *mem)
{
    int n = 0;
    p = skip_sp(p);
    while (*p && *p != '\n' && *p != ';' && !(p[0] == '/' && p[1] == '/')) {
        if (n == 2) asm_error(as, "слишком много операндов");
        int r = x87_reg(&p);
        if (r >= 0) st[n++] = r;
        else {
            if (mem->kind) asm_error(as, "у команды x87 один операнд в памяти");
            p = parse_operand(as, p, mem);
            if (mem->kind != OP_MEM && mem->kind != OP_REG) asm_error(as, "ожидалась память или st(i)");
            st[n++] = -1;
        }
        p = skip_sp(p);
        if (*p == ',') p++;
    }
    return n;
}

static void x87_mem(Asm *as, u8 opc, int digit, Operand *mem)
{
    encode(as, 0, 0, &opc, 1, digit, 0, mem, 0);
    finish_rip(as);
}

static int x87(Asm *as, const char *m, int mlen, const char *p)
{
    if (mlen < 2 || (m[0] != 'f' && m[0] != 'F')) return 0;
    for (int i = 0; i < (int)(sizeof(g_x87_plain) / sizeof(g_x87_plain[0])); i++)
        if (word_eq(m, mlen, g_x87_plain[i].name)) {
            int st[2] = { -1, -1 };
            Operand mem = { 0 };
            mem.base = mem.index = -1;
            int n = x87_operands(as, p, st, &mem);
            // faddp st(i), st / fmulp st(i) — the pop forms with a register
            if (n && g_x87_plain[i].a == 0xDE && g_x87_plain[i].name[1] != 'c' && st[0] >= 0) {
                emit8(as, 0xDE);
                emit8(as, (u8)((g_x87_plain[i].b & 0xF8) | st[0]));
                return 1;
            }
            if (n) asm_error(as, "у %s нет операндов", g_x87_plain[i].name);
            emit8(as, g_x87_plain[i].a);
            emit8(as, g_x87_plain[i].b);
            return 1;
        }
    if (word_eq(m, mlen, "fwait") || word_eq(m, mlen, "wait")) { emit8(as, 0x9B); return 1; }

    // fadd, fmul, fcom, fcomp, fsub, fsubr, fdiv, fdivr: D8 /n (dword), DC /n (qword), registers D8/DC C0+
    static const char *arith[8] = { "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr" };
    int st[2] = { -1, -1 };
    Operand mem = { 0 };
    mem.base = mem.index = -1;
    for (int k = 0; k < 8; k++) {
        if (!word_eq(m, mlen, arith[k])) continue;
        int n = x87_operands(as, p, st, &mem);
        if (n == 1 && st[0] < 0) {
            if (mem.size != 4 && mem.size != 8) asm_error(as, "%s: нужен dword или qword ptr", arith[k]);
            x87_mem(as, mem.size == 4 ? 0xD8 : 0xDC, k, &mem);
        }
        else if (n == 1 || (n == 2 && st[0] == 0)) {          // fadd st(i)  =  fadd st, st(i)
            emit8(as, 0xD8);
            emit8(as, (u8)(0xC0 + k * 8 + (n == 1 ? st[0] : st[1])));
        }
        else if (n == 2 && st[1] == 0 && k != 2 && k != 3) {   // fadd st(i), st: DC, sub/div swap their r
            static const int swap[8] = { 0, 1, 2, 3, 5, 4, 7, 6 };
            emit8(as, 0xDC);
            emit8(as, (u8)(0xC0 + swap[k] * 8 + st[0]));
        }
        else if (n == 0 && k >= 2 && k <= 3) {                 // fcom = fcom st(1)
            emit8(as, 0xD8);
            emit8(as, (u8)(0xC0 + k * 8 + 1));
        }
        else asm_error(as, "%s: такие операнды не годятся", arith[k]);
        return 1;
    }

    // loads and stores: size picks the opcode
    static const struct { const char *name; u8 opc[4]; u8 dig[4]; u8 reg_a, reg_b; } ls[] = {
        //          word        dword       qword       tbyte          st(i)
        { "fld",   { 0, 0xD9, 0xDD, 0xDB }, { 0, 0, 0, 5 }, 0xD9, 0xC0 },
        { "fst",   { 0, 0xD9, 0xDD, 0 },    { 0, 2, 2, 0 }, 0xDD, 0xD0 },
        { "fstp",  { 0, 0xD9, 0xDD, 0xDB }, { 0, 3, 3, 7 }, 0xDD, 0xD8 },
        { "fild",  { 0xDF, 0xDB, 0xDF, 0 }, { 0, 0, 5, 0 }, 0, 0 },
        { "fist",  { 0xDF, 0xDB, 0, 0 },    { 2, 2, 0, 0 }, 0, 0 },
        { "fistp", { 0xDF, 0xDB, 0xDF, 0 }, { 3, 3, 7, 0 }, 0, 0 },
        { "fisttp",{ 0xDF, 0xDB, 0xDD, 0 }, { 1, 1, 1, 0 }, 0, 0 },
        { "fxch",  { 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xD9, 0xC8 },
        { "ffree", { 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xDD, 0xC0 },
        { "fucom", { 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xDD, 0xE0 },
        { "fucomp",{ 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xDD, 0xE8 },
        { "fcomi", { 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xDB, 0xF0 },
        { "fucomi",{ 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xDB, 0xE8 },
        { "fcomip",{ 0, 0, 0, 0 },          { 0, 0, 0, 0 }, 0xDF, 0xF0 },
        { "fucomip",{ 0, 0, 0, 0 },         { 0, 0, 0, 0 }, 0xDF, 0xE8 },
    };
    for (int k = 0; k < (int)(sizeof(ls) / sizeof(ls[0])); k++) {
        if (!word_eq(m, mlen, ls[k].name)) continue;
        int n = x87_operands(as, p, st, &mem);
        if (n >= 1 && st[0] < 0) {
            int slot = mem.size == 2 ? 0 : mem.size == 4 ? 1 : mem.size == 8 ? 2 : mem.size == 10 ? 3 : -1;
            if (slot < 0 || !ls[k].opc[slot]) asm_error(as, "%s: такого размера нет (word/dword/qword/tbyte ptr)", ls[k].name);
            x87_mem(as, ls[k].opc[slot], ls[k].dig[slot], &mem);
            return 1;
        }
        if (!ls[k].reg_a) asm_error(as, "%s берёт только память", ls[k].name);
        // fcomi st, st(i): register is the second; fxch / ffree / fucom: alone or none (= st(1))
        int r = n == 2 ? st[1] : n == 1 ? st[0] : (ls[k].reg_b == 0xC8 || ls[k].reg_b == 0xE0 || ls[k].reg_b == 0xE8) ? 1 : -1;
        if (r < 0) asm_error(as, "%s: нужен регистр st(i)", ls[k].name);
        emit8(as, ls[k].reg_a);
        emit8(as, (u8)(ls[k].reg_b + r));
        return 1;
    }

    // control and status words
    if (word_eq(m, mlen, "fldcw") || word_eq(m, mlen, "fnstcw") || word_eq(m, mlen, "fstcw")) {
        if (word_eq(m, mlen, "fstcw")) emit8(as, 0x9B);
        x87_operands(as, p, st, &mem);
        if (mem.kind != OP_MEM) asm_error(as, "%s: нужна память", cc_strndup(m, mlen));
        x87_mem(as, 0xD9, word_eq(m, mlen, "fldcw") ? 5 : 7, &mem);
        return 1;
    }
    if (word_eq(m, mlen, "fnstsw") || word_eq(m, mlen, "fstsw")) {
        if (word_eq(m, mlen, "fstsw")) emit8(as, 0x9B);
        x87_operands(as, p, st, &mem);
        if (mem.kind == OP_REG && mem.reg == 0 && mem.size == 2) { emit8(as, 0xDF); emit8(as, 0xE0); }
        else if (mem.kind == OP_MEM) x87_mem(as, 0xDD, 7, &mem);
        else asm_error(as, "%s: нужен ax или память", cc_strndup(m, mlen));
        return 1;
    }
    return 0;
}

// ---- one line ----

static void assemble_line(Asm *as, const char *p)
{
    p = skip_sp(p);
    // labels: "name:" possibly followed by an instruction
    for (;;) {
        const char *s = p;
        while (is_id2((unsigned char)*p))
            p++;
        if (p > s && *p == ':' && is_id1((unsigned char)*s)) {
            int len = (int)(p - s);
            Label *l = add_label(as, s, len);
            if (as->pass == 0) {
                if (l->defined)
                    asm_error(as, "метка %s определена дважды", cc_strndup(s, len));
                l->defined = 1;
            }
            l->section = as->section;
            l->offset = as->pos[as->section];
            p = skip_sp(p + 1);
            continue;
        }
        if (p > s && *p == ':' && p - s <= 3 && *s >= '0' && *s <= '9') {      // "1:" -- a numeric label
            int n = 0;
            for (const char *d = s; d < p; d++) {
                if (*d < '0' || *d > '9')
                    asm_error(as, "метка не может начинаться с цифры");
                n = n * 10 + (*d - '0');
            }
            const char *name = cc_format(".Lnum.%d.%d", n, as->numlab[n]++);
            Label *l = add_label(as, name, cc_strlen(name));
            if (as->pass == 0)
                l->defined = 1;
            l->section = as->section;
            l->offset = as->pos[as->section];
            p = skip_sp(p + 1);
            continue;
        }
        p = s;
        break;
    }
    if (!*p || *p == '\n' || *p == ';' || (p[0] == '/' && p[1] == '/') || *p == '#')
        return;

    const char *m = p;
    while (is_id2((unsigned char)*p))
        p++;
    int mlen = (int)(p - m);
    if (!mlen)
        asm_error(as, "ожидалась команда");

    // directives
    if (word_eq(m, mlen, ".text") || (word_eq(m, mlen, "section") && word_eq(skip_sp(p), 5, ".text"))) {
        as->section = SEC_TEXT;
        return;
    }
    if (word_eq(m, mlen, ".data") || (word_eq(m, mlen, "section") && word_eq(skip_sp(p), 5, ".data"))) {
        as->section = SEC_DATA;
        return;
    }
    if (word_eq(m, mlen, ".bss") || (word_eq(m, mlen, "section") && word_eq(skip_sp(p), 4, ".bss"))) {
        as->section = as->win ? SEC_BSS : SEC_DATA;
        return;
    }
    if (word_eq(m, mlen, ".tls") || (word_eq(m, mlen, "section") && word_eq(skip_sp(p), 4, ".tls"))) {
        if (!as->win)
            asm_error(as, ".tls есть только в программах для Windows");
        as->section = SEC_TLS;
        return;
    }
    if (word_eq(m, mlen, ".srcfile")) {         // .srcfile LINE NAME: the next line is that line of NAME
        i64 v = 0;
        p = skip_sp(p);
        if (!parse_number(as, &p, &v))
            asm_error(as, ".srcfile: номер строки");
        as->src_len = rest_of_line(p, &as->src_name);
        as->src_base = (int)v - (as->line + 1);
        return;
    }
    if (word_eq(m, mlen, ".seh_proc") || word_eq(m, mlen, ".seh_endproc") || word_eq(m, mlen, ".seh_scope")) {
        if (as->win && as->pass == 1)
            seh_directive(as, m, mlen, p);
        return;
    }
    if (word_eq(m, mlen, ".align") || word_eq(m, mlen, "align")) {
        i64 v;
        p = skip_sp(p);
        if (!parse_number(as, &p, &v))
            asm_error(as, "ожидалось число");
        align_section(as, (int)v);
        return;
    }
    if (word_eq(m, mlen, ".zero") || word_eq(m, mlen, "resb")) {
        i64 v;
        p = skip_sp(p);
        if (!parse_number(as, &p, &v))
            asm_error(as, "ожидалось число");
        if (as->section == SEC_BSS || as->pass < 2) {
            as->pos[as->section] += (int)v;     // nothing to write: the memory is zero already
            return;
        }
        for (i64 i = 0; i < v; i++)
            emit8(as, 0);
        return;
    }
    if (word_eq(m, mlen, ".loc") || word_eq(m, mlen, ".dbgfile") || word_eq(m, mlen, ".dbgfunc") ||
        word_eq(m, mlen, ".dbgend") || word_eq(m, mlen, ".dbgframe") || word_eq(m, mlen, ".dbgvar") ||
        word_eq(m, mlen, ".dbgglobal") || word_eq(m, mlen, ".dbgstruct") || word_eq(m, mlen, ".dbgfield")) {
        where_in_c(as, m, mlen, p);
        if (as->pe && as->pass == 1)
            dbg_directive(as, m, mlen, p);
        return;
    }
    if (word_eq(m, mlen, "db")) { directive_data(as, p, 1); return; }
    if (word_eq(m, mlen, "dw")) { directive_data(as, p, 2); return; }
    if (word_eq(m, mlen, "dd")) { directive_data(as, p, 4); return; }
    if (word_eq(m, mlen, "dq")) { directive_data(as, p, 8); return; }
    if (word_eq(m, mlen, ".gc"))
        return;                 // strip.c's mark: a piece of the library the program may leave out
    if (word_eq(m, mlen, ".alias")) {
        // .alias name, target   (an '&' before target is allowed: the library's table writes &rt_stdin)
        if (as->pass == 0) {
            const char *s = skip_sp(p), *e = s;
            while (is_id2((unsigned char)*e))
                e++;
            const char *t = skip_sp(e);
            if (*t == ',') t = skip_sp(t + 1);
            if (*t == '&') t = skip_sp(t + 1);
            const char *te = t;
            while (is_id2((unsigned char)*te))
                te++;
            if (e == s || te == t)
                asm_error(as, ".alias: нужно имя и метка");
            AliasDef *a = cc_alloc(sizeof(AliasDef));
            a->name = s; a->nlen = (int)(e - s);
            a->target = t; a->tlen = (int)(te - t);
            if (as->alias_tail) as->alias_tail->next = a; else as->aliases = a;
            as->alias_tail = a;
        }
        return;
    }
    if (word_eq(m, mlen, ".local") || word_eq(m, mlen, ".export")) {
        // .local name: not exported (static); .export name: exported (dllexport)
        const char *s = skip_sp(p), *e = s;
        while (is_id2((unsigned char)*e))
            e++;
        if (e == s)
            asm_error(as, "ожидалось имя");
        Label *l = add_label(as, s, (int)(e - s));
        if (word_eq(m, mlen, ".local"))
            l->local = 1;
        else {
            l->exported = 1;
            as->any_export = 1;
        }
        return;
    }
    if (word_eq(m, mlen, "global") || word_eq(m, mlen, "extern") || word_eq(m, mlen, "bits") ||
        word_eq(m, mlen, ".globl") || word_eq(m, mlen, "default"))
        return;         // accepted for compatibility, nothing to do

    // prefixes
    if (word_eq(m, mlen, "rep") || word_eq(m, mlen, "repe") || word_eq(m, mlen, "repz") ||
        word_eq(m, mlen, "repne") || word_eq(m, mlen, "repnz") || word_eq(m, mlen, "lock")) {
        emit8(as, word_eq(m, mlen, "lock") ? 0xF0 : (word_eq(m, mlen, "repne") || word_eq(m, mlen, "repnz")) ? 0xF2 : 0xF3);
        assemble_line(as, p);
        return;
    }

    if (as->section != SEC_TEXT)
        asm_error(as, "команды пишутся в .text");
    if (x87(as, m, mlen, p))
        return;
    if (word_eq(m, mlen, "cmpsd") && !*skip_sp(p)) {     // the string one (cmpsd xmm, xmm, n is SSE)
        emit8(as, 0xA7);
        return;
    }

    Operand ops[4];
    int n = 0;
    for (int i = 0; i < 4; i++) {
        Operand z = { 0 };
        ops[i] = z;
    }
    p = skip_sp(p);
    while (*p && *p != '\n' && *p != ';' && !(p[0] == '/' && p[1] == '/')) {
        if (n == 4)
            asm_error(as, "слишком много операндов");
        p = parse_operand(as, p, &ops[n++]);
        if (*p == ',')
            p++;
        else if (*p && *p != '\n' && *p != ';' && !(p[0] == '/' && p[1] == '/'))
            asm_error(as, "ожидалась запятая");
    }

    for (int i = 0; i < (int)(sizeof(g_simple) / sizeof(g_simple[0])); i++) {
        if (word_eq(m, mlen, g_simple[i].name)) {
            // "movsd" with operands is the SSE instruction
            if (n && word_eq(m, mlen, "movsd"))
                break;
            if (n && !word_eq(m, mlen, "ret"))
                asm_error(as, "у %s нет операндов", g_simple[i].name);
            if (n && word_eq(m, mlen, "ret")) {
                emit8(as, 0xC2);
                emit_imm(as, &ops[0], 2);
                return;
            }
            for (int k = 0; k < g_simple[i].len; k++)
                emit8(as, g_simple[i].bytes[k]);
            return;
        }
    }
    if (word_eq(m, mlen, "movsd") && n == 0) {
        emit8(as, 0xA5);
        return;
    }

    static const char *alu_names[] = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" };
    for (int i = 0; i < 8; i++)
        if (word_eq(m, mlen, alu_names[i])) {
            alu(as, i, ops, n);
            return;
        }
    static const char *shift_names[] = { "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar" };
    for (int i = 0; i < 8; i++)
        if (word_eq(m, mlen, shift_names[i])) {
            shift(as, ops, n, i == 6 ? 4 : i);
            return;
        }
    for (int i = 0; i < (int)(sizeof(g_sse) / sizeof(g_sse[0])); i++)
        if (word_eq(m, mlen, g_sse[i].name)) {
            sse(as, ops, n, g_sse[i].pfx, g_sse[i].opc, g_sse[i].store);
            return;
        }
    if (sse_ex(as, m, mlen, ops, n) || avx(as, m, mlen, ops, n) || more_ops(as, m, mlen, ops, n))
        return;

    if (word_eq(m, mlen, "mov")) { mov(as, ops, n); return; }
    if (word_eq(m, mlen, "lea")) { lea(as, ops, n); return; }
    if (word_eq(m, mlen, "movzx")) { movx(as, ops, n, 0, 0); return; }
    if (word_eq(m, mlen, "movsx")) { movx(as, ops, n, 1, 0); return; }
    if (word_eq(m, mlen, "movsxd")) { movx(as, ops, n, 1, 1); return; }
    if (word_eq(m, mlen, "test")) { test(as, ops, n); return; }
    if (word_eq(m, mlen, "xchg")) { xchg(as, ops, n); return; }
    if (word_eq(m, mlen, "not")) { unary_grp(as, ops, n, 2, 0); return; }
    if (word_eq(m, mlen, "neg")) { unary_grp(as, ops, n, 3, 0); return; }
    if (word_eq(m, mlen, "mul")) { unary_grp(as, ops, n, 4, 0); return; }
    if (word_eq(m, mlen, "div")) { unary_grp(as, ops, n, 6, 0); return; }
    if (word_eq(m, mlen, "idiv")) { unary_grp(as, ops, n, 7, 0); return; }
    if (word_eq(m, mlen, "inc")) { unary_grp(as, ops, n, 0, 1); return; }
    if (word_eq(m, mlen, "dec")) { unary_grp(as, ops, n, 1, 1); return; }
    if (word_eq(m, mlen, "imul")) { imul(as, ops, n); return; }
    if (word_eq(m, mlen, "push")) { push_pop(as, ops, n, 1); return; }
    if (word_eq(m, mlen, "pop")) { push_pop(as, ops, n, 0); return; }
    if (word_eq(m, mlen, "call")) { branch(as, ops, n, 0, 0); return; }
    if (word_eq(m, mlen, "jmp")) { branch(as, ops, n, 1, 0); return; }
    if (word_eq(m, mlen, "movd")) { movdq(as, ops, n, 0); return; }
    if (word_eq(m, mlen, "movq")) { movdq(as, ops, n, 1); return; }
    if (word_eq(m, mlen, "cvtsi2ss")) { cvt_from_int(as, ops, n, 0xF3); return; }
    if (word_eq(m, mlen, "cvtsi2sd")) { cvt_from_int(as, ops, n, 0xF2); return; }
    if (word_eq(m, mlen, "cvttss2si")) { cvt_to_int(as, ops, n, 0xF3); return; }
    if (word_eq(m, mlen, "cvttsd2si")) { cvt_to_int(as, ops, n, 0xF2); return; }
    if (word_eq(m, mlen, "in")) { in_out(as, ops, n, 1); return; }
    if (word_eq(m, mlen, "out")) { in_out(as, ops, n, 0); return; }
    // 0F xx with ModRM: cmpxchg/xadd r/m, reg; bsf/bsr/popcnt-free bit scans reg, r/m
    if (word_eq(m, mlen, "cmpxchg") || word_eq(m, mlen, "xadd")) {
        need(as, n, 2);
        if (ops[1].kind != OP_REG || ops[0].kind == OP_IMM)
            asm_error(as, "%s: приёмник — память или регистр, источник — регистр", cc_strndup(m, mlen));
        int size = ops[1].size;
        int cx = word_eq(m, mlen, "cmpxchg");
        u8 opc[2] = { 0x0F, (u8)((cx ? 0xB0 : 0xC0) + (size == 1 ? 0 : 1)) };
        encode(as, size_prefix(size), size_w(size), opc, 2, ops[1].reg, &ops[1], &ops[0], 0);
        finish_rip(as);
        return;
    }
    if (word_eq(m, mlen, "bsf") || word_eq(m, mlen, "bsr")) {
        need(as, n, 2);
        if (ops[0].kind != OP_REG || ops[0].size < 2)
            asm_error(as, "%s: приёмник — регистр", cc_strndup(m, mlen));
        u8 opc[2] = { 0x0F, (u8)(word_eq(m, mlen, "bsf") ? 0xBC : 0xBD) };
        encode(as, size_prefix(ops[0].size), size_w(ops[0].size), opc, 2, ops[0].reg, &ops[0], &ops[1], 0);
        finish_rip(as);
        return;
    }
    if (word_eq(m, mlen, "int")) {
        need(as, n, 1);
        emit8(as, 0xCD);
        emit_imm(as, &ops[0], 1);
        return;
    }
    if (mlen >= 2 && lower(m[0]) == 'j') {
        int cc = cond_code(m + 1, mlen - 1);
        if (cc >= 0) {
            branch(as, ops, n, 2, cc);
            return;
        }
    }
    if (mlen >= 4 && word_eq(m, 3, "set")) {
        int cc = cond_code(m + 3, mlen - 3);
        if (cc >= 0) {
            need(as, n, 1);
            if (op_size(as, &ops[0], 0) != 1)
                asm_error(as, "set* пишет один байт");
            u8 opc[2] = { 0x0F, (u8)(0x90 + cc) };
            encode(as, 0, 0, opc, 2, 0, 0, &ops[0], 0);
            finish_rip(as);
            return;
        }
    }
    if (mlen >= 5 && word_eq(m, 4, "cmov")) {
        int cc = cond_code(m + 4, mlen - 4);
        if (cc >= 0) {
            need(as, n, 2);
            if (ops[0].kind != OP_REG || ops[0].size < 2)
                asm_error(as, "cmov: приёмник — регистр");
            u8 opc[2] = { 0x0F, (u8)(0x40 + cc) };
            encode(as, size_prefix(ops[0].size), size_w(ops[0].size), opc, 2, ops[0].reg, &ops[0], &ops[1], 0);
            finish_rip(as);
            return;
        }
    }
    asm_error(as, "неизвестная команда %s", cc_strndup(m, mlen));
}

// After passes 0 and 1: every .alias label takes its target's place. The
// program's own label of that name, or an earlier .alias, keeps the name.
static void resolve_aliases(Asm *as)
{
    for (AliasDef *a = as->aliases; a; a = a->next) {
        Label *t = find_label(as, a->target, a->tlen);
        if (!t || !t->defined)
            continue;                       // a function the library does not have here
        Label *l = find_label(as, a->name, a->nlen);
        if (l && l->defined && l->alias != a)
            continue;
        l = add_label(as, a->name, a->nlen);
        l->defined = 1;
        l->alias = a;
        l->local = 1;
        l->section = t->section;
        l->offset = t->offset;
    }
}

static void run_pass(Asm *as, const char *text, int pass)
{
    as->pass = pass;
    as->section = SEC_TEXT;
    for (int i = 0; i < SEC_COUNT; i++)
        as->pos[i] = 0;
    for (int i = 0; i < 1000; i++)
        as->numlab[i] = 0;
    as->src_name = 0;
    as->line = 0;
    as->c_line = 0;
    as->disp_pos = -1;
    const char *p = text;
    while (*p) {
        as->line++;
        as->line_text = p;
        // copy the line so that operand parsing sees a terminator
        const char *e = p;
        while (*e && *e != '\n')
            e++;
        assemble_line(as, p);
        p = *e ? e + 1 : e;
    }
    if (pass < 2)
        resolve_aliases(as);
}

int cc_assemble_program(const char *text, const CcSymbol *externs, CcImage *out)
{
    Asm *as = cc_alloc(sizeof(Asm));
    as->externs = externs;

    run_pass(as, text, 0);
    run_pass(as, text, 1);
    as->size[SEC_TEXT] = as->pos[SEC_TEXT];
    as->size[SEC_DATA] = as->pos[SEC_DATA];

    // [ text | data (16-aligned) | import slots ]
    size_t text_size = (size_t)((as->size[SEC_TEXT] + 15) & ~15);
    size_t data_size = (size_t)((as->size[SEC_DATA] + 15) & ~15);
    size_t slot_size = (size_t)as->import_count * 8;
    size_t total = text_size + data_size + slot_size;
    u8 *mem = cc_alloc_code(total ? total : 16);
    as->base[SEC_TEXT] = mem;
    as->base[SEC_DATA] = mem + text_size;
    as->slots = mem + text_size + data_size;
    for (Import *im = as->imports; im; im = im->next) {
        u64 a = (u64)im->addr;
        for (int i = 0; i < 8; i++)
            as->slots[8 * im->slot + i] = (u8)(a >> (8 * i));
    }

    run_pass(as, text, 2);

    out->code = mem;
    out->code_size = (size_t)as->size[SEC_TEXT];
    out->data = as->base[SEC_DATA];
    out->data_size = (size_t)as->size[SEC_DATA];
    out->entry = 0;
    Label *m = find_label(as, "main", 4);
    if (m && m->defined)
        out->entry = as->base[m->section] + m->offset;
    return 0;
}

// ---- PE (.dll) writer ----

static int pe_align(int n, int a) { return (n + a - 1) & ~(a - 1); }

// The DOS stub of every program and DLL: compiler\stub.bin, 64 bytes. An MZ header whose code
// (at 0x20: mov ah, 4Ch; int 21h) ends the program at once if DOS ever runs it. The PE headers
// follow at 0x80: e_lfanew is set here.
static const u8 g_dos_stub[64] = {
    0x4d, 0x5a, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0xff, 0xff, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x4c, 0xcd, 0x21,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static void put_dos_stub(u8 *file)
{
    for (int i = 0; i < 64; i++)
        file[i] = g_dos_stub[i];
    PeDos *dos = (PeDos *)file;
    dos->e_lfanew = 0x80;
}

static void wr16(u8 *p, unsigned v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void wr32(u8 *p, u32 v) { for (int i = 0; i < 4; i++) p[i] = (u8)(v >> (8 * i)); }
static void wr64(u8 *p, u64 v) { for (int i = 0; i < 8; i++) p[i] = (u8)(v >> (8 * i)); }

// ---- .dydbg (layout in peformat.h) ----

static int dbg_strings_size(Asm *as)
{
    int n = 0;
    for (int i = 0; i < as->ndfile; i++)
        n += as->dfile_len[i] + 1;
    for (int i = 0; i < as->ndf; i++)
        n += as->dfuncs[i].len + 1;
    return n;
}

// 0: nothing to say (no C function)
static int dbg_section_size(Asm *as)
{
    if (!as->ndf)
        return 0;
    return (int)sizeof(DyDbgHeader) + as->ndfile * 4 + as->ndf * (int)sizeof(DyDbgFunc) +
           as->ndl * (int)sizeof(DyDbgLine) + dbg_strings_size(as);
}

static void dbg_write(Asm *as, u8 *p, int text_rva)
{
    DyDbgHeader *h = (DyDbgHeader *)p;
    h->magic = DYDBG_MAGIC;
    h->version = DYDBG_VERSION;
    h->nfiles = (u32)as->ndfile;
    h->nfuncs = (u32)as->ndf;
    h->nlines = (u32)as->ndl;
    h->files_off = (u32)sizeof(DyDbgHeader);
    h->funcs_off = h->files_off + h->nfiles * 4;
    h->lines_off = h->funcs_off + h->nfuncs * (u32)sizeof(DyDbgFunc);
    h->strings_off = h->lines_off + h->nlines * (u32)sizeof(DyDbgLine);
    h->strings_size = (u32)dbg_strings_size(as);
    h->text_rva = (u32)text_rva;
    h->text_size = (u32)as->pos[SEC_TEXT];
    u8 *str = p + h->strings_off;
    u32 so = 0;
    for (int i = 0; i < as->ndfile; i++) {
        wr32(p + h->files_off + 4 * i, so);
        for (int k = 0; k < as->dfile_len[i]; k++)
            str[so++] = (u8)as->dfile[i][k];
        str[so++] = 0;
    }
    int li = 0;
    for (int i = 0; i < as->ndf; i++) {
        DbgFunc *f = &as->dfuncs[i];
        DyDbgFunc *o = (DyDbgFunc *)(p + h->funcs_off) + i;
        o->start = (u32)(text_rva + f->start);
        o->end = (u32)(text_rva + f->end);
        o->name = so;
        o->flags = DYDBG_FUNC_FRAME;
        while (li < as->ndl && as->dlines[li].off < f->start)
            li++;
        if (li < as->ndl && as->dlines[li].off < f->end)
            o->flags |= DYDBG_FUNC_LINES;
        for (int k = 0; k < f->len; k++)
            str[so++] = (u8)f->name[k];
        str[so++] = 0;
    }
    for (int i = 0; i < as->ndl; i++) {
        DyDbgLine *o = (DyDbgLine *)(p + h->lines_off) + i;
        o->rva = (u32)(text_rva + as->dlines[i].off);
        o->line = (u32)as->dlines[i].line;
        o->file = (u32)as->dlines[i].file;
    }
}

// ---- wcc -g: the debug info of a Windows program as text (prog.wdbg, the IDE's debugger reads it) ----
// wdbg 1
// file N NAME                      a source of the line table (as the compiler was given it)
// func START END NAME              a C function's code [START, END) as RVAs (hex)
//   frame NSAVE PAD REG...         after its func: the registers pushed after rbp
//   var LOC LINE TYPE NAME         after its func: a parameter or local (codegen.c, .dbgvar)
// line RVA FILE LINE               from RVA on, the code is this line of file FILE
// global RVA FILE LINE TYPE NAME   a global variable, FILE its source
// struct ID SIZE s|u / field OFF BITOFF BITS TYPE NAME
char *cc_win_debug_text;

typedef struct { char *p; int len, cap; } DbgText;

static void dt_add(DbgText *t, const char *s, int n)
{
    if (t->len + n + 1 > t->cap) {
        int cap = t->cap ? t->cap : 65536;
        while (t->len + n + 1 > cap)
            cap *= 2;
        char *np = cc_alloc((size_t)cap);
        for (int i = 0; i < t->len; i++)
            np[i] = t->p[i];
        t->p = np;
        t->cap = cap;
    }
    for (int i = 0; i < n; i++)
        t->p[t->len++] = s[i];
    t->p[t->len] = 0;
}

static void dt_printf(DbgText *t, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = cc_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof(buf))
        n = (int)sizeof(buf) - 1;
    dt_add(t, buf, n);
}

static char *dbg_text(Asm *as, int text_rva)
{
    DbgText t = { 0 };
    dt_printf(&t, "wdbg 1\n");
    for (int i = 0; i < as->ndfile; i++) {
        dt_printf(&t, "file %d ", i);
        dt_add(&t, as->dfile[i], as->dfile_len[i]);
        dt_add(&t, "\n", 1);
    }
    int xi = 0;
    for (int i = 0; i < as->ndf; i++) {
        DbgFunc *f = &as->dfuncs[i];
        dt_printf(&t, "func %x %x ", text_rva + f->start, text_rva + f->end);
        dt_add(&t, f->name, f->len);
        dt_add(&t, "\n", 1);
        for (int k = 0; k < as->ndx; k++) {       // its frame and variables (in order, so mostly a scan from xi)
            DbgExtra *x = &as->dextra[(xi + k) % as->ndx];
            if (x->func != i || (x->kind != 'F' && x->kind != 'v'))
                continue;
            dt_add(&t, x->kind == 'F' ? "frame " : "var ", x->kind == 'F' ? 6 : 4);
            dt_add(&t, x->s, x->len);
            dt_add(&t, "\n", 1);
        }
        while (xi < as->ndx && as->dextra[xi].func <= i)
            xi++;
    }
    for (int i = 0; i < as->ndl; i++)
        dt_printf(&t, "line %x %d %d\n", text_rva + as->dlines[i].off, as->dlines[i].file, as->dlines[i].line);
    for (int k = 0; k < as->ndx; k++) {
        DbgExtra *x = &as->dextra[k];
        if (x->kind == 'g') {                   // LABEL LINE TYPE NAME: the label becomes its RVA
            int n = 0;
            while (n < x->len && x->s[n] != ' ')
                n++;
            Label *l = find_label(as, x->s, n);
            if (!l || !l->defined || l->section == SEC_TLS)
                continue;
            u64 rva = (u64)(as->base[l->section] + l->offset) - (u64)as->image_buf;
            dt_printf(&t, "global %x", (unsigned)rva);
            dt_add(&t, x->s + n, x->len - n);
            dt_add(&t, "\n", 1);
        } else if (x->kind == 's' || x->kind == 'f') {
            dt_add(&t, x->kind == 's' ? "struct " : "field ", x->kind == 's' ? 7 : 6);
            dt_add(&t, x->s, x->len);
            dt_add(&t, "\n", 1);
        }
    }
    return t.p;
}

typedef struct { Label *l; } ExpEnt;

static int exp_less(Label *a, Label *b)
{
    int n = a->len < b->len ? a->len : b->len;
    for (int i = 0; i < n; i++) {
        u8 ca = (u8)a->name[i], cb = (u8)b->name[i];
        if (ca != cb)
            return ca < cb;
    }
    return a->len < b->len;
}

// Merge sorts (n log n: thousands of exports and relocations); tmp holds n items.
static void sort_exports(ExpEnt *a, ExpEnt *tmp, int n)
{
    for (int w = 1; w < n; w *= 2) {
        for (int lo = 0; lo < n; lo += 2 * w) {
            int mid = lo + w < n ? lo + w : n, hi = lo + 2 * w < n ? lo + 2 * w : n;
            int i = lo, j = mid, k = lo;
            while (i < mid && j < hi)
                tmp[k++] = exp_less(a[j].l, a[i].l) ? a[j++] : a[i++];
            while (i < mid) tmp[k++] = a[i++];
            while (j < hi) tmp[k++] = a[j++];
        }
        for (int i = 0; i < n; i++)
            a[i] = tmp[i];
    }
}

static void sort_ints(int *a, int *tmp, int n)
{
    for (int w = 1; w < n; w *= 2) {
        for (int lo = 0; lo < n; lo += 2 * w) {
            int mid = lo + w < n ? lo + w : n, hi = lo + 2 * w < n ? lo + 2 * w : n;
            int i = lo, j = mid, k = lo;
            while (i < mid && j < hi)
                tmp[k++] = a[j] < a[i] ? a[j++] : a[i++];
            while (i < mid) tmp[k++] = a[i++];
            while (j < hi) tmp[k++] = a[j++];
        }
        for (int i = 0; i < n; i++)
            a[i] = tmp[i];
    }
}

// Is this .text label in the export table: functions with external linkage
// (static ones are .local); when some are __declspec(dllexport) (.export),
// only those and main.
static int exported_label(Asm *as, Label *l)
{
    if (!l->defined || l->section != SEC_TEXT || l->name[0] == '.' || l->local)
        return 0;
    return !as->any_export || l->exported || (l->len == 4 && cc_strneq(l->name, "main", 4));
}

// Builds a relocatable PE64 DLL. Exports the labels exported_label() picks;
// no limits on exports, imports or relocations.
int cc_assemble_pe(const char *text, const CcSymbol *externs, const char *dll_name,
                   u8 **out, u64 *out_size)
{
    Asm *as = cc_alloc(sizeof(Asm));
    as->externs = externs;
    as->pe = 1;
    as->image_base = PE_IMAGE_BASE;

    run_pass(as, text, 0);
    run_pass(as, text, 1);
    int text_size = as->pos[SEC_TEXT];
    int data_raw = as->pos[SEC_DATA];
    int n_imp = as->import_count;

    // collect exported labels
    int ne = 0;
    for (int b = 0; b < 256; b++)
        for (Label *l = as->labels[b]; l; l = l->next)
            if (exported_label(as, l))
                ne++;
    ExpEnt *exps = cc_alloc(sizeof(ExpEnt) * (size_t)(ne + 1));
    ne = 0;
    for (int b = 0; b < 256; b++)
        for (Label *l = as->labels[b]; l; l = l->next)
            if (exported_label(as, l))
                exps[ne++].l = l;
    if (ne > 65535)
        cc_error_at("ассемблер", 0, "в экспорт просится %d имён, PE держит до 65535", ne);
    sort_exports(exps, cc_alloc(sizeof(ExpEnt) * (size_t)(ne + 1)), ne);   // Windows needs names sorted

    Import **imps = cc_alloc(sizeof(Import *) * (size_t)(n_imp + 1));
    for (Import *im = as->imports; im; im = im->next)
        imps[im->slot] = im;

    const int SA = PE_SECT_ALIGN;
    int text_rva = SA;
    int data_rva = text_rva + pe_align(text_size, SA);

    // layout inside the .data section (offsets from data_rva)
    int o = pe_align(data_raw, 8);
    int iat_off = o;   o += (n_imp + 1) * 8;
    int ilt_off = o;   o += (n_imp + 1) * 8;
    o = pe_align(o, 4);
    int impdesc_off = o; o += 2 * (int)sizeof(PeImportDesc);
    int *imp_name_off = cc_alloc(sizeof(int) * (size_t)(n_imp + 1));
    for (int i = 0; i < n_imp; i++) {
        imp_name_off[i] = o;
        o += pe_align(2 + imps[i]->len + 1, 2);
    }
    o = pe_align(o, 2);
    int modname_off = o;
    o += pe_align(8 /* "dynasty\0" */, 2);
    o = pe_align(o, 4);
    int expdir_off = o; o += (int)sizeof(PeExportDir);
    int eat_off = o;    o += ne * 4;
    int enpt_off = o;   o += ne * 4;
    int eord_off = o;   o += ne * 2;
    o = pe_align(o, 2);
    int *exp_name_off = cc_alloc(sizeof(int) * (size_t)(ne + 1));
    for (int i = 0; i < ne; i++) {
        exp_name_off[i] = o;
        o += exps[i].l->len + 1;
    }
    int dllname_off = o;
    int dlllen = cc_strlen(dll_name);
    o += dlllen + 1;
    int data_vsize = o;

    int reloc_rva = data_rva + pe_align(data_vsize, SA);
    // a relocation is 2 bytes for every 8-byte address in the image, each 4 KiB
    // page that has one adds a block header (8) and maybe a pad (2)
    int reloc_cap = pe_align((text_size + data_raw) / 4 + 12 * ((text_size + data_raw) / 4096 + 2), SA);
    int dbg_size = dbg_section_size(as);
    int capacity = reloc_rva + reloc_cap + pe_align(dbg_size, SA);

    u8 *buf = cc_alloc((size_t)capacity);
    as->image_buf = buf;
    as->base[SEC_TEXT] = buf + text_rva;
    as->base[SEC_DATA] = buf + data_rva;
    as->slots = buf + data_rva + iat_off;

    run_pass(as, text, 2);

    // import thunks and descriptor
    for (int i = 0; i < n_imp; i++) {
        u64 name_rva = (u64)(data_rva + imp_name_off[i]);
        wr64(buf + data_rva + iat_off + i * 8, name_rva);
        wr64(buf + data_rva + ilt_off + i * 8, name_rva);
        wr16(buf + data_rva + imp_name_off[i], 0);      // hint
        for (int k = 0; k < imps[i]->len; k++)
            buf[data_rva + imp_name_off[i] + 2 + k] = (u8)imps[i]->name[k];
    }
    if (n_imp) {
        PeImportDesc *d = (PeImportDesc *)(buf + data_rva + impdesc_off);
        d->OriginalFirstThunk = (u32)(data_rva + ilt_off);
        d->Name = (u32)(data_rva + modname_off);
        d->FirstThunk = (u32)(data_rva + iat_off);
        const char *mn = "dynasty";
        for (int k = 0; mn[k]; k++)
            buf[data_rva + modname_off + k] = (u8)mn[k];
    }

    // export directory
    if (ne) {
        PeExportDir *ed = (PeExportDir *)(buf + data_rva + expdir_off);
        ed->Name = (u32)(data_rva + dllname_off);
        ed->Base = 1;
        ed->NumberOfFunctions = (u32)ne;
        ed->NumberOfNames = (u32)ne;
        ed->AddressOfFunctions = (u32)(data_rva + eat_off);
        ed->AddressOfNames = (u32)(data_rva + enpt_off);
        ed->AddressOfNameOrdinals = (u32)(data_rva + eord_off);
        for (int i = 0; i < ne; i++) {
            wr32(buf + data_rva + eat_off + i * 4, (u32)(text_rva + exps[i].l->offset));
            wr32(buf + data_rva + enpt_off + i * 4, (u32)(data_rva + exp_name_off[i]));
            wr16(buf + data_rva + eord_off + i * 2, (unsigned)i);
            for (int k = 0; k < exps[i].l->len; k++)
                buf[data_rva + exp_name_off[i] + k] = (u8)exps[i].l->name[k];
        }
        for (int k = 0; k < dlllen; k++)
            buf[data_rva + dllname_off + k] = (u8)dll_name[k];
    }

    // base relocations, grouped by 4 KiB page
    int reloc_size = 0;
    {
        // gather and sort rvas
        int nr = 0;
        int *rvas = cc_alloc(sizeof(int) * (size_t)(as->base_reloc_count + 1));
        for (BaseReloc *r = as->base_relocs; r; r = r->next) {
            int base = r->section == SEC_TEXT ? text_rva : data_rva;
            rvas[nr++] = base + r->offset;
        }
        sort_ints(rvas, cc_alloc(sizeof(int) * (size_t)(nr + 1)), nr);
        u8 *rp = buf + reloc_rva;
        int i = 0;
        while (i < nr) {
            u32 page = (u32)rvas[i] & ~0xFFFu;
            int j = i;
            while (j < nr && ((u32)rvas[j] & ~0xFFFu) == page)
                j++;
            int count = j - i;
            int block = 8 + count * 2;
            if (block & 3) {                        // pad block to 4 bytes
                wr16(rp + block, (PE_REL_ABSOLUTE << 12));
                block += 2;
            }
            PeRelocBlock *rb = (PeRelocBlock *)rp;
            rb->PageRVA = page;
            rb->BlockSize = (u32)block;
            for (int k = 0; k < count; k++)
                wr16(rp + 8 + k * 2, (PE_REL_DIR64 << 12) | ((u32)rvas[i + k] & 0xFFF));
            rp += block;
            reloc_size += block;
            i = j;
        }
    }

    int n_sections = reloc_size ? 3 : 2;
    int image_end = reloc_size ? reloc_rva + pe_align(reloc_size, SA) : reloc_rva;
    int dbg_rva = image_end;                        // .dydbg last: loaders that do not know it map it as data
    if (dbg_size) {
        dbg_write(as, buf + dbg_rva, text_rva);
        n_sections++;
        image_end = dbg_rva + pe_align(dbg_size, SA);
    }

    // headers
    put_dos_stub(buf);
    u8 *nt = buf + 0x80;
    wr32(nt, PE_NT_MAGIC);
    PeFileHeader *fh = (PeFileHeader *)(nt + 4);
    fh->Machine = PE_MACHINE_X64;
    fh->NumberOfSections = (unsigned short)n_sections;
    fh->SizeOfOptionalHeader = (unsigned short)sizeof(PeOpt64);
    fh->Characteristics = PE_F_EXECUTABLE | PE_F_DLL | PE_F_LARGE_ADDR;
    PeOpt64 *op = (PeOpt64 *)(nt + 4 + sizeof(PeFileHeader));
    op->Magic = PE_MAGIC_PE32PLUS;
    op->SizeOfCode = (u32)pe_align(text_size, SA);
    op->SizeOfInitializedData = (u32)(pe_align(data_vsize, SA) + pe_align(reloc_size, SA) + pe_align(dbg_size, SA));
    op->AddressOfEntryPoint = 0;
    op->BaseOfCode = (u32)text_rva;
    op->ImageBase = PE_IMAGE_BASE;
    op->SectionAlignment = SA;
    op->FileAlignment = SA;
    op->MajorOSVersion = 6;
    op->MajorSubsystemVersion = 6;
    op->SizeOfImage = (u32)image_end;
    op->SizeOfHeaders = SA;
    op->Subsystem = 3;                              // console; irrelevant to our loader
    op->DllCharacteristics = 0x0040;               // relocatable
    op->SizeOfStackReserve = 0x100000;
    op->SizeOfStackCommit = 0x1000;
    op->SizeOfHeapReserve = 0x100000;
    op->SizeOfHeapCommit = 0x1000;
    op->NumberOfRvaAndSizes = 16;
    if (ne) {
        op->DataDirectory[PE_DIR_EXPORT].VirtualAddress = (u32)(data_rva + expdir_off);
        op->DataDirectory[PE_DIR_EXPORT].Size = (u32)sizeof(PeExportDir);
    }
    if (n_imp) {
        op->DataDirectory[PE_DIR_IMPORT].VirtualAddress = (u32)(data_rva + impdesc_off);
        op->DataDirectory[PE_DIR_IMPORT].Size = (u32)(2 * sizeof(PeImportDesc));
        op->DataDirectory[PE_DIR_IAT].VirtualAddress = (u32)(data_rva + iat_off);
        op->DataDirectory[PE_DIR_IAT].Size = (u32)((n_imp + 1) * 8);
    }
    if (reloc_size) {
        op->DataDirectory[PE_DIR_BASERELOC].VirtualAddress = (u32)reloc_rva;
        op->DataDirectory[PE_DIR_BASERELOC].Size = (u32)reloc_size;
    }

    PeSection *sec = (PeSection *)((u8 *)op + sizeof(PeOpt64));
    const char *tn = ".text";
    for (int k = 0; tn[k]; k++) sec[0].Name[k] = (u8)tn[k];
    sec[0].VirtualSize = (u32)text_size;
    sec[0].VirtualAddress = (u32)text_rva;
    sec[0].SizeOfRawData = (u32)pe_align(text_size, SA);
    sec[0].PointerToRawData = (u32)text_rva;
    sec[0].Characteristics = PE_SCN_CODE | PE_SCN_EXECUTE | PE_SCN_READ;
    const char *dn = ".data";
    for (int k = 0; dn[k]; k++) sec[1].Name[k] = (u8)dn[k];
    sec[1].VirtualSize = (u32)data_vsize;
    sec[1].VirtualAddress = (u32)data_rva;
    sec[1].SizeOfRawData = (u32)pe_align(data_vsize, SA);
    sec[1].PointerToRawData = (u32)data_rva;
    sec[1].Characteristics = PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_WRITE;
    if (reloc_size) {
        const char *rn = ".reloc";
        for (int k = 0; rn[k]; k++) sec[2].Name[k] = (u8)rn[k];
        sec[2].VirtualSize = (u32)reloc_size;
        sec[2].VirtualAddress = (u32)reloc_rva;
        sec[2].SizeOfRawData = (u32)pe_align(reloc_size, SA);
        sec[2].PointerToRawData = (u32)reloc_rva;
        sec[2].Characteristics = PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_DISCARDABLE;
    }
    if (dbg_size) {
        PeSection *ds = &sec[n_sections - 1];
        const char *gn = ".dydbg";
        for (int k = 0; gn[k]; k++) ds->Name[k] = (u8)gn[k];
        ds->VirtualSize = (u32)dbg_size;
        ds->VirtualAddress = (u32)dbg_rva;
        ds->SizeOfRawData = (u32)pe_align(dbg_size, SA);
        ds->PointerToRawData = (u32)dbg_rva;
        ds->Characteristics = PE_SCN_INIT_DATA | PE_SCN_READ;
    }

    *out = buf;
    *out_size = (u64)image_end;
    return 1;
}

// ---- Windows .exe / .dll (WinCC) ----
// Layout: headers | .text | .data (the program's data, then the import and
// export tables) | .reloc. Imports are grouped by DLL: every DLL has its run
// of slots in the IAT, ended by a zero slot. The file is aligned to 512 bytes,
// the memory to 4 KB.

#define WIN_FILE_ALIGN 0x200

// The manifest every WinCC .exe carries: UTF-8 is the program's code page (the
// ...A functions of Windows take the program's UTF-8 text as it is), the
// controls look as in today's Windows (Common Controls 6), no elevation asked.
static const char g_manifest[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
    "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n"
    "  <dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" "
    "version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>"
    "</dependentAssembly></dependency>\r\n"
    "  <application xmlns=\"urn:schemas-microsoft-com:asm.v3\"><windowsSettings>"
    "<activeCodePage xmlns=\"http://schemas.microsoft.com/SMI/2019/WindowsSettings\">UTF-8</activeCodePage>"
    "</windowsSettings></application>\r\n"
    "  <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>"
    "<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/></application></compatibility>\r\n"
    "  <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\"><security><requestedPrivileges>"
    "<requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"/></requestedPrivileges></security></trustInfo>\r\n"
    "</assembly>\r\n";

// The resource tree (.rsrc): type -> name -> language -> data. In each directory the named
// entries come first (in order of their names), then the numbered ones (in order of numbers).
// Layout: all directories, the data entries, the names, then the data (each 8-aligned).
// With p = 0 it only counts; returns the size.
static int res_order(CcResId a, CcResId b)
{
    if (a.str && !b.str) return -1;
    if (!a.str && b.str) return 1;
    if (!a.str) return a.num < b.num ? -1 : a.num > b.num;
    for (int i = 0; i < a.len && i < b.len; i++)
        if (a.str[i] != b.str[i]) return a.str[i] < b.str[i] ? -1 : 1;
    return a.len < b.len ? -1 : a.len > b.len;
}

static int res_less(const CcResource *a, const CcResource *b)
{
    int c = res_order(a->type, b->type);
    if (!c) c = res_order(a->name, b->name);
    if (!c) c = a->lang < b->lang ? -1 : a->lang > b->lang;
    return c < 0;
}

// a directory entry's name: the id, or (named) the place of the name, written at *sp
static void res_entry_name(u8 *p, u8 *e, CcResId id, int *sp)
{
    if (!id.str) {
        if (p) wr32(e, id.num);
        return;
    }
    if (p) {
        wr32(e, 0x80000000u | (u32)*sp);
        wr16(p + *sp, (unsigned)id.len);
        for (int i = 0; i < id.len; i++) wr16(p + *sp + 2 + 2 * i, id.str[i]);
    }
    *sp += 2 + 2 * id.len;
}

static int win_resources(u8 *p, int rva, const CcResource *in, int n)
{
    CcResource *r = cc_alloc(sizeof(CcResource) * (size_t)(n + 1));
    for (int i = 0; i < n; i++) {          // sorted, a few dozen at most
        int k = i;
        while (k > 0 && res_less(&in[i], &r[k - 1])) { r[k] = r[k - 1]; k--; }
        r[k] = in[i];
    }
    int ntypes = 0, nnames = 0;
    for (int i = 0; i < n; i++) {
        int new_type = !i || res_order(r[i].type, r[i - 1].type);
        ntypes += new_type;
        nnames += new_type || res_order(r[i].name, r[i - 1].name);
    }
    int de = 16 + 8 * ntypes + 16 * ntypes + 8 * nnames + 16 * nnames + 8 * n;   // the data entries
    int sp = de + 16 * n;                                                      // the names
    // the names' size first: the data comes after them
    int names_end = sp;
    for (int i = 0; i < n; i++) {
        if (r[i].type.str && (!i || res_order(r[i].type, r[i - 1].type))) names_end += 2 + 2 * r[i].type.len;
        if (r[i].name.str && (!i || res_order(r[i].type, r[i - 1].type) || res_order(r[i].name, r[i - 1].name)))
            names_end += 2 + 2 * r[i].name.len;
    }
    int dp = (names_end + 7) & ~7;
    if (!p) {
        for (int i = 0; i < n; i++) dp = (dp + r[i].size + 7) & ~7;
        return dp;
    }
    int named_types = 0;
    for (int i = 0; i < n; i++)
        if (r[i].type.str && (!i || res_order(r[i].type, r[i - 1].type))) named_types++;
    wr16(p + 12, (unsigned)named_types);
    wr16(p + 14, (unsigned)(ntypes - named_types));
    int type_dir = 16 + 8 * ntypes, ti = 0, dei = 0;
    for (int i = 0; i < n;) {
        int j = i;
        while (j < n && !res_order(r[j].type, r[i].type)) j++;
        u8 *e = p + 16 + 8 * ti++;
        res_entry_name(p, e, r[i].type, &sp);
        wr32(e + 4, 0x80000000u | (u32)type_dir);
        int nn = 0, named = 0;
        for (int k = i; k < j; k++)
            if (k == i || res_order(r[k].name, r[k - 1].name)) { nn++; named += r[k].name.str != 0; }
        wr16(p + type_dir + 12, (unsigned)named);
        wr16(p + type_dir + 14, (unsigned)(nn - named));
        int name_dir = type_dir + 16 + 8 * nn, ni = 0;
        for (int k = i; k < j;) {
            int m = k;
            while (m < j && !res_order(r[m].name, r[k].name)) m++;
            u8 *ne = p + type_dir + 16 + 8 * ni++;
            res_entry_name(p, ne, r[k].name, &sp);
            wr32(ne + 4, 0x80000000u | (u32)name_dir);
            wr16(p + name_dir + 14, (unsigned)(m - k));
            for (int q = k; q < m; q++) {
                u8 *le = p + name_dir + 16 + 8 * (q - k);
                wr32(le, r[q].lang);
                wr32(le + 4, (u32)(de + 16 * dei));
                u8 *d = p + de + 16 * dei++;
                wr32(d, (u32)(rva + dp));
                wr32(d + 4, (u32)r[q].size);
                for (int b = 0; b < r[q].size; b++) p[dp + b] = r[q].data[b];
                dp = (dp + r[q].size + 7) & ~7;
            }
            name_dir += 16 + 8 * (m - k);
            k = m;
        }
        type_dir = name_dir;
        i = j;
    }
    return dp;
}

// A name the program uses and does not define: which DLL has it? All the
// missing ones are told at once.
static void win_bind_imports(Asm *as, Import **imps, int n_imp)
{
    char missing[400];
    int nm = 0, shown = 0, pos = 0;
    for (int i = 0; i < n_imp; i++) {
        Import *im = imps[i];
        im->dll = as->win->dll_of ? as->win->dll_of(im->name, im->len) : 0;
        if (im->dll)
            continue;
        nm++;
        if (shown < 8 && pos + im->len + 4 < (int)sizeof(missing)) {
            if (shown) { missing[pos++] = ','; missing[pos++] = ' '; }
            for (int k = 0; k < im->len; k++) missing[pos++] = im->name[k];
            shown++;
        }
    }
    missing[pos] = 0;
    if (nm)
        cc_error_at(0, 0, "%s %s%s: %s ни в программе, ни в библиотеках Windows (своя DLL — ключ -l имя или #pragma comment(lib, \"имя\"); своя библиотека — её файл .lib в команде)",
                    nm == 1 ? "функция объявлена, но не написана —" : "функции объявлены, но не написаны —", missing,
                    nm > shown ? " …" : "", nm == 1 ? "её нет" : "их нет");
}

static int win_exported(Asm *as, Label *l)
{
    if (!l->defined || l->section != SEC_TEXT || l->name[0] == '.' || l->local || l->alias)
        return 0;
    if (as->win->exe)
        return l->exported;                     // an .exe exports only what asks for it
    return !as->any_export || l->exported;      // a DLL: __declspec(dllexport), or all it defines
}

// Unwind info of "push rbp; push r...; [sub rsp, 8]; mov rbp, rsp" (codes in reverse order of the prologue:
// SET_FPREG, ALLOC_SMALL, PUSH_NONVOL of each register, PUSH_NONVOL rbp); the count of codes made even.
static int unwind_codes(const SehProc *f) { return 2 + f->nregs + f->pad; }

static int unwind_size(const SehProc *f)
{
    int n = unwind_codes(f);
    return 4 + 2 * (n + (n & 1));
}

static void write_unwind(u8 *u, const SehProc *f)
{
    int ends[8], off = 1;               // after push rbp
    for (int i = 0; i < f->nregs; i++) {
        off += f->regs[i] >= 8 ? 2 : 1;
        ends[i] = off;
    }
    int alloc_end = off + (f->pad ? 4 : 0);
    int prolog = alloc_end + 3;
    u[0] = 1;
    u[1] = (u8)prolog;
    u[2] = (u8)unwind_codes(f);
    u[3] = 0x05;                        // frame register rbp, offset 0
    u8 *c = u + 4;
    *c++ = (u8)prolog; *c++ = 0x03;                         // UWOP_SET_FPREG
    if (f->pad) { *c++ = (u8)alloc_end; *c++ = 0x02; }      // UWOP_ALLOC_SMALL 8
    for (int i = f->nregs - 1; i >= 0; i--) {
        *c++ = (u8)ends[i];
        *c++ = (u8)(f->regs[i] << 4);                       // UWOP_PUSH_NONVOL
    }
    *c++ = 1; *c++ = 0x50;                                  // push rbp
}

int cc_win_import_count;

int cc_assemble_win(const char *text, const CcWinTarget *t, u8 **out, u64 *out_size)
{
    Asm *as = cc_alloc(sizeof(Asm));
    as->pe = 1;
    as->win = t;
    as->image_base = t->exe ? 0x140000000ull : 0x180000000ull;

    run_pass(as, text, 0);
    run_pass(as, text, 1);
    int text_size = as->pos[SEC_TEXT];
    int data_raw = as->pos[SEC_DATA];
    int bss_size = as->pos[SEC_BSS];
    int tls_size = as->pos[SEC_TLS];
    int n_imp = as->import_count;

    Label *entry = find_label(as, t->entry, cc_strlen(t->entry));
    if (!entry || !entry->defined || entry->section != SEC_TEXT)
        cc_error_at(0, 0, "нет точки входа %s", t->entry);

    // imports, grouped by DLL in the order the DLLs first come up
    Import **imps = cc_alloc(sizeof(Import *) * (size_t)(n_imp + 1));
    for (Import *im = as->imports; im; im = im->next)
        imps[im->slot] = im;
    win_bind_imports(as, imps, n_imp);
    const char **dlls = cc_alloc(sizeof(char *) * (size_t)(n_imp + 1));
    int *dll_first = cc_alloc(sizeof(int) * (size_t)(n_imp + 1));     // its first slot
    int ndll = 0, nslot = 0;
    Import **order = cc_alloc(sizeof(Import *) * (size_t)(n_imp + 1));  // by slot, zero slots left out
    int norder = 0;
    for (int i = 0; i < n_imp; i++) {
        int known = 0;
        for (int d = 0; d < ndll; d++)
            if (cc_streq(dlls[d], imps[i]->dll)) known = 1;
        if (known)
            continue;
        dlls[ndll] = imps[i]->dll;
        dll_first[ndll] = nslot;
        for (int j = i; j < n_imp; j++)
            if (cc_streq(imps[j]->dll, imps[i]->dll)) {
                imps[j]->slot = nslot++;
                order[norder++] = imps[j];
            }
        nslot++;                                // the zero slot that ends this DLL's run
        ndll++;
    }
    cc_win_import_count = norder;

    // exported labels
    int ne = 0;
    for (int b = 0; b < 256; b++)
        for (Label *l = as->labels[b]; l; l = l->next)
            if (win_exported(as, l)) ne++;
    ExpEnt *exps = cc_alloc(sizeof(ExpEnt) * (size_t)(ne + 1));
    ne = 0;
    for (int b = 0; b < 256; b++)
        for (Label *l = as->labels[b]; l; l = l->next)
            if (win_exported(as, l)) exps[ne++].l = l;
    if (ne > 65535)
        cc_error_at("ассемблер", 0, "в экспорт просится %d имён, PE держит до 65535", ne);
    sort_exports(exps, cc_alloc(sizeof(ExpEnt) * (size_t)(ne + 1)), ne);

    // The layout: one .text section holds the code and, after it, everything only read --
    // import and export tables, the TLS directory, .pdata, resources (as a linker merges .rdata
    // into .text). The program's writable data, .tls, .bss and base relocations get sections of
    // their own, and only when they have something: an empty program is the headers and one
    // section, 1 KB in the file.
    // wcc --tiny: sections and the file by 16 bytes, not by 4096 and 512 (link /ALIGN:16 /FILEALIGN:16).
    // Windows then maps the file as one piece: a section's place in memory is its place in the file.
    // tiny goes further: the PE headers right after the 64-byte DOS stub, only the data directories
    // up to the base relocations (the TLS one when there is TLS), no IAT directory, no second import
    // list (ILT), and one section for everything: Windows maps such a file as one writable piece anyway.
    const int SA = t->tiny ? 16 : PE_SECT_ALIGN;
    const int FA = t->tiny ? 16 : WIN_FILE_ALIGN;
    int nt_at = t->tiny ? 0x40 : 0x80;
    // tiny: directories up to the last one used -- TLS 9, base relocations 5, exceptions 3,
    // resources 2, imports 1 (exports 0); .pdata and resources are known here, relocations by pass 1
    int pdata_known = 0;
    for (int i = 0; i < as->nproc && !pdata_known; i++)
        if (as->procs[i].nscopes) pdata_known = 1;
    {
        Label *w = find_label(as, "__wcc_watch_faults", 18);
        if (w && w->defined) pdata_known = 1;
    }
    int ndirs = !t->tiny ? 16 : tls_size ? 10 : as->pass1_abs ? 6 : pdata_known ? 4
              : (t->nres || (t->exe && t->manifest)) ? 3 : ndll ? 2 : ne ? 1 : 0;
    int opt_size = (int)sizeof(PeOpt64) - (16 - ndirs) * 8;
    int hdr_room = nt_at + 4 + (int)sizeof(PeFileHeader) + opt_size + (t->tiny ? 1 : 5) * (int)sizeof(PeSection);
    int text_rva = t->tiny ? pe_align(hdr_room, SA) : SA;
    int tab_rva = text_rva + pe_align(text_size, 16);

    // the tables, right after the code (offsets from tab_rva)
    int o = 0;
    int iat_off = o;     o += nslot * 8;
    int ilt_off = o;     if (!t->tiny) o += nslot * 8;      // tiny: no ILT, Windows reads the names from the IAT
    o = pe_align(o, 4);
    int impdesc_off = o; o += (ndll + 1) * (int)sizeof(PeImportDesc);
    int *name_off = cc_alloc(sizeof(int) * (size_t)(n_imp + 1));
    for (int i = 0; i < norder; i++) {
        o = pe_align(o, 2);
        name_off[i] = o;
        o += 2 + order[i]->len + 1;
    }
    int *dllname_off = cc_alloc(sizeof(int) * (size_t)(ndll + 1));
    for (int d = 0; d < ndll; d++) {
        dllname_off[d] = o;
        o += cc_strlen(dlls[d]) + 1;
    }
    o = pe_align(o, 4);
    int expdir_off = o;  if (ne) o += (int)sizeof(PeExportDir);
    int eat_off = o;     o += ne * 4;
    int enpt_off = o;    o += ne * 4;
    int eord_off = o;    o += ne * 2;
    int *exp_name_off = cc_alloc(sizeof(int) * (size_t)(ne + 1));
    for (int i = 0; i < ne; i++) {
        exp_name_off[i] = o;
        o += exps[i].l->len + 1;
    }
    const char *own = t->dll_name ? t->dll_name : "program";
    int own_off = o;
    if (ne) o += cc_strlen(own) + 1;
    // thread-local variables: the TLS directory and its (empty) list of callbacks
    int tlsdir_off = 0;
    if (tls_size) {
        o = pe_align(o, 8);
        tlsdir_off = o;
        o += 40 + 8;
    }
    int tab_size = o;

    // .pdata: a RUNTIME_FUNCTION (12 bytes) for each C function and funclet, then the unwind
    // infos: one for all functions, one for all funclets, an own one for a function with __try.
    // Only a program that unwinds needs it: one with __try, one that watches its faults
    // (signal(): runtime\start.c), and a DLL (the program that loads it may unwind through it).
    int want_pdata = !t->exe;
    for (int i = 0; i < as->nproc && !want_pdata; i++)
        if (as->procs[i].nscopes) want_pdata = 1;
    {
        Label *w = find_label(as, "__wcc_watch_faults", 18);
        if (w && w->defined) want_pdata = 1;
    }
    int npf = want_pdata ? as->nproc : 0, pdata_size = 0;
    if (npf) {
        pdata_size = 12 * npf + 8 + 12;
        for (int i = 0; i < npf; i++)
            if (as->procs[i].nscopes)
                pdata_size += 16 + 16 * as->procs[i].nscopes;
            else if (as->procs[i].nregs)
                pdata_size += unwind_size(&as->procs[i]);
    }
    int pdata_rva = tab_rva + pe_align(tab_size, 8);

    // .rsrc: the program's resources and, in an .exe that asks for it (wcc --manifest,
    // #pragma comment(linker, "/MANIFEST")), WinCC's manifest (type 24, id 1) -- unless it has its own
    CcResource *res = cc_alloc(sizeof(CcResource) * (size_t)(t->nres + 1));
    int nres = 0, own_manifest = 0;
    for (int i = 0; i < t->nres; i++) {
        res[nres++] = t->res[i];
        if (!t->res[i].type.str && t->res[i].type.num == 24 && !t->res[i].name.str && t->res[i].name.num == 1)
            own_manifest = 1;
    }
    if (t->exe && !own_manifest && t->manifest) {     // only when the program asks for it
        CcResource *m = &res[nres++];
        m->type.num = 24;
        m->name.num = 1;
        m->data = (const u8 *)g_manifest;
        m->size = cc_strlen(g_manifest);
    }
    int rsrc_rva = pdata_rva + pe_align(pdata_size, 8);
    int rsrc_cap = nres ? win_resources(0, 0, res, nres) : 0;
    int text_end = rsrc_rva + rsrc_cap;
    // then the program's writable data, .tls, .bss (zeros: nothing of it in the file nor in buf)
    int data_rva = text_rva + pe_align(text_end - text_rva, SA);
    int tls_rva = data_rva + (data_raw ? pe_align(data_raw, SA) : 0);
    int bss_rva = tls_rva + (tls_size ? pe_align(tls_size, SA) : 0);
    int reloc_rva = bss_rva + (bss_size ? pe_align(bss_size, SA) : 0);     // last; its bytes in relocs[]
    int all_raw = text_size + data_raw + tls_size + tab_size;
    int reloc_cap = all_raw / 4 + 12 * (all_raw / 4096 + 2) + 64;
    int capacity = bss_rva;
    u8 *relocs = cc_alloc((size_t)reloc_cap);

    u8 *buf = cc_alloc((size_t)capacity);
    as->image_buf = buf;
    as->base[SEC_TEXT] = buf + text_rva;
    as->base[SEC_DATA] = buf + data_rva;
    as->base[SEC_TLS] = buf + tls_rva;
    as->base[SEC_BSS] = buf + bss_rva;
    as->slots = buf + tab_rva + iat_off;
    int sec_rva_of[SEC_COUNT];
    sec_rva_of[SEC_TEXT] = text_rva;
    sec_rva_of[SEC_DATA] = data_rva;
    sec_rva_of[SEC_BSS] = bss_rva;
    sec_rva_of[SEC_TLS] = tls_rva;

    run_pass(as, text, 2);

    // the TLS directory: the template (.tls), where the loader puts the slot number, no callbacks
    int extra_rel[4], n_extra = 0;
    if (tls_size) {
        Label *ix = find_label(as, "__wcc_tls_index", 15);
        if (!ix || !ix->defined || ix->section != SEC_DATA)
            cc_error_at(0, 0, "ассемблер: у потоковых переменных нет __wcc_tls_index");
        u8 *d = buf + tab_rva + tlsdir_off;
        wr64(d, as->image_base + (u64)tls_rva);
        wr64(d + 8, as->image_base + (u64)(tls_rva + tls_size));
        wr64(d + 16, as->image_base + (u64)(data_rva + ix->offset));
        wr64(d + 24, as->image_base + (u64)(tab_rva + tlsdir_off + 40));
        for (int k = 0; k < 4; k++)
            extra_rel[n_extra++] = tab_rva + tlsdir_off + 8 * k;
    }

    // .pdata and the unwind infos (x64): a function's frame is rbp, a funclet's rbx
    if (pdata_size) {
        u8 *pd = buf + pdata_rva;
        int info_fn = 12 * npf, info_fl = info_fn + 8, o2 = info_fl + 12;
        static const u8 fn_info[8] = { 1, 4, 2, 0x05, 4, 0x03, 1, 0x50 };
        static const u8 fl_info[12] = { 1, 9, 4, 0x03, 9, 0x03, 6, 0x02, 2, 0x30, 1, 0x50 };
        for (int k = 0; k < 8; k++) pd[info_fn + k] = fn_info[k];
        for (int k = 0; k < 12; k++) pd[info_fl + k] = fl_info[k];
        Label *handler = 0;
        for (int i = 0; i < npf; i++) {
            SehProc *f = &as->procs[i];
            u8 *rf = pd + 12 * i;
            wr32(rf, (u32)(text_rva + f->start));
            wr32(rf + 4, (u32)(text_rva + f->end));
            if (!f->nscopes && f->nregs) {
                wr32(rf + 8, (u32)(pdata_rva + o2));
                write_unwind(pd + o2, f);
                o2 += unwind_size(f);
                continue;
            }
            if (!f->nscopes) {
                wr32(rf + 8, (u32)(pdata_rva + (f->kind ? info_fl : info_fn)));
                continue;
            }
            if (!handler) {
                handler = find_label(as, "__wcc_seh_handler", 17);
                if (!handler || !handler->defined)
                    cc_error_at(0, 0, "ассемблер: у __try нет __wcc_seh_handler");
            }
            wr32(rf + 8, (u32)(pdata_rva + o2));
            u8 *u = pd + o2;
            for (int k = 0; k < 8; k++) u[k] = fn_info[k];
            u[0] = 1 | (3 << 3);                    // UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER
            wr32(u + 8, (u32)(text_rva + handler->offset));
            wr32(u + 12, (u32)f->nscopes);
            u8 *e = u + 16;
            for (SehScope *sc = f->scopes; sc; sc = sc->next, e += 16)
                for (int k = 0; k < 4; k++) {
                    u32 v = 0;
                    if (!(sc->len[k] == 1 && sc->name[k][0] == '0')) {
                        Label *l = find_label(as, sc->name[k], sc->len[k]);
                        if (!l || !l->defined || l->section != SEC_TEXT)
                            cc_error_at(0, 0, "ассемблер: .seh_scope: нет метки %s", cc_strndup(sc->name[k], sc->len[k]));
                        v = (u32)(text_rva + l->offset);
                    }
                    wr32(e + 4 * k, v);
                }
            o2 += 16 + 16 * f->nscopes;
        }
    }

    // import tables: IAT and ILT both hold the hint/name RVAs; Windows fills the IAT
    for (int i = 0; i < norder; i++) {
        u64 name_rva = (u64)(tab_rva + name_off[i]);
        wr64(buf + tab_rva + iat_off + order[i]->slot * 8, name_rva);
        if (!t->tiny)
            wr64(buf + tab_rva + ilt_off + order[i]->slot * 8, name_rva);
        wr16(buf + tab_rva + name_off[i], 0);
        for (int k = 0; k < order[i]->len; k++)
            buf[tab_rva + name_off[i] + 2 + k] = (u8)order[i]->name[k];
    }
    for (int d = 0; d < ndll; d++) {
        PeImportDesc *desc = (PeImportDesc *)(buf + tab_rva + impdesc_off) + d;
        desc->OriginalFirstThunk = t->tiny ? 0 : (u32)(tab_rva + ilt_off + dll_first[d] * 8);
        desc->Name = (u32)(tab_rva + dllname_off[d]);
        desc->FirstThunk = (u32)(tab_rva + iat_off + dll_first[d] * 8);
        for (int k = 0; dlls[d][k]; k++)
            buf[tab_rva + dllname_off[d] + k] = (u8)dlls[d][k];
    }

    if (ne) {
        PeExportDir *ed = (PeExportDir *)(buf + tab_rva + expdir_off);
        ed->Name = (u32)(tab_rva + own_off);
        ed->Base = 1;
        ed->NumberOfFunctions = (u32)ne;
        ed->NumberOfNames = (u32)ne;
        ed->AddressOfFunctions = (u32)(tab_rva + eat_off);
        ed->AddressOfNames = (u32)(tab_rva + enpt_off);
        ed->AddressOfNameOrdinals = (u32)(tab_rva + eord_off);
        for (int i = 0; i < ne; i++) {
            int sec = exps[i].l->section;
            wr32(buf + tab_rva + eat_off + i * 4, (u32)(sec_rva_of[sec] + exps[i].l->offset));
            wr32(buf + tab_rva + enpt_off + i * 4, (u32)(tab_rva + exp_name_off[i]));
            wr16(buf + tab_rva + eord_off + i * 2, (unsigned)i);
            for (int k = 0; k < exps[i].l->len; k++)
                buf[tab_rva + exp_name_off[i] + k] = (u8)exps[i].l->name[k];
        }
        for (int k = 0; own[k]; k++)
            buf[tab_rva + own_off + k] = (u8)own[k];
    }

    int rsrc_size = nres ? win_resources(buf + rsrc_rva, rsrc_rva, res, nres) : 0;

    // base relocations, grouped by 4 KB page
    int reloc_size = 0;
    {
        int nr = 0;
        int *rvas = cc_alloc(sizeof(int) * (size_t)(as->base_reloc_count + 5));
        for (BaseReloc *r = as->base_relocs; r; r = r->next) {
            if (r->section == SEC_BSS)
                cc_error_at(0, 0, "ассемблер: адрес в .bss");
            rvas[nr++] = sec_rva_of[r->section] + r->offset;
        }
        for (int k = 0; k < n_extra; k++)
            rvas[nr++] = extra_rel[k];
        sort_ints(rvas, cc_alloc(sizeof(int) * (size_t)(nr + 1)), nr);
        u8 *rp = relocs;                        // their own buffer: .reloc comes after .bss, not in buf
        for (int i = 0; i < nr;) {
            u32 page = (u32)rvas[i] & ~0xFFFu;
            int j = i;
            while (j < nr && ((u32)rvas[j] & ~0xFFFu) == page)
                j++;
            int block = 8 + (j - i) * 2;
            if (block & 3) {
                wr16(rp + block, PE_REL_ABSOLUTE << 12);
                block += 2;
            }
            PeRelocBlock *rb = (PeRelocBlock *)rp;
            rb->PageRVA = page;
            rb->BlockSize = (u32)block;
            for (int k = 0; k < j - i; k++)
                wr16(rp + 8 + k * 2, (PE_REL_DIR64 << 12) | ((u32)rvas[i + k] & 0xFFF));
            rp += block;
            reloc_size += block;
            i = j;
        }
        if (reloc_size > reloc_cap)
            cc_error_at(0, 0, "ассемблер: поправок адресов больше места под них (%d > %d)", reloc_size, reloc_cap);
    }

    // the sections that have something, in the order of their addresses; .bss has no bytes in the file.
    // .text: the code, then the tables, .pdata and resources (all read-only)
    int text_vsize = rsrc_size ? rsrc_rva + rsrc_size - text_rva
                   : pdata_size ? pdata_rva + pdata_size - text_rva : tab_rva + tab_size - text_rva;
    static const char *all_names[5] = { ".text", ".data", ".tls", ".bss", ".reloc" };
    static const u32 all_flags[5] = {
        PE_SCN_CODE | PE_SCN_EXECUTE | PE_SCN_READ,
        PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_WRITE,
        PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_WRITE,
        0x00000080 | PE_SCN_READ | PE_SCN_WRITE,        // uninitialized data
        PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_DISCARDABLE,
    };
    int all_rva[5] = { text_rva, data_rva, tls_rva, bss_rva, reloc_rva };
    int all_size[5] = { text_vsize, data_raw, tls_size, bss_size, reloc_size };
    const u8 *all_bytes[5] = { buf + text_rva, buf + data_rva, buf + tls_rva, 0, relocs };
    const char *names[5];
    u32 flags[5];
    const u8 *bytes[5];
    int sec_rva[5], sec_vsize[5], sec_raw[5], sec_ptr[5], sec_bss[5], sec_len[5];
    int nsec = 0;
    if (t->tiny) {
        // one section from the code to the last byte: tables, data, .tls, .bss as zeros, relocations
        int end = reloc_size ? reloc_rva + reloc_size : bss_size ? bss_rva + bss_size
                : tls_size ? tls_rva + tls_size : data_raw ? data_rva + data_raw : text_rva + text_vsize;
        u8 *all = cc_alloc((size_t)(end - text_rva));
        for (int k = text_rva; k < end && k < capacity; k++) all[k - text_rva] = buf[k];
        for (int k = 0; k < reloc_size; k++) all[reloc_rva - text_rva + k] = relocs[k];
        names[0] = ".text";
        flags[0] = PE_SCN_CODE | PE_SCN_INIT_DATA | PE_SCN_EXECUTE | PE_SCN_READ | PE_SCN_WRITE;
        sec_rva[0] = text_rva;
        sec_len[0] = sec_vsize[0] = end - text_rva;
        sec_bss[0] = 0;
        bytes[0] = all;
        nsec = 1;
    }
    for (int i = 0; i < 5 && !t->tiny; i++)
        if (i == 0 || all_size[i]) {
            names[nsec] = all_names[i];
            flags[nsec] = all_flags[i];
            sec_rva[nsec] = all_rva[i];
            sec_len[nsec] = all_size[i];        // the bytes it has
            sec_vsize[nsec] = all_size[i];
            sec_bss[nsec] = i == 3 && !t->tiny;
            bytes[nsec] = all_bytes[i];
            // tiny: the file is the image, so .bss is zeros in the file (Windows does not take
            // a section without bytes in such a file, not even the last one)
            if (i == 3 && t->tiny) {
                bytes[nsec] = cc_alloc((size_t)bss_size);
                flags[nsec] = PE_SCN_INIT_DATA | PE_SCN_READ | PE_SCN_WRITE;
            }
            nsec++;
        }

    // the file: headers, then each section's bytes at 512-byte steps.
    // tiny: each section lies in the file where it lies in memory, end to end
    int hdr_size = t->tiny ? text_rva
                 : pe_align(0x80 + 4 + (int)sizeof(PeFileHeader) + (int)sizeof(PeOpt64) + nsec * (int)sizeof(PeSection), FA);
    int fpos = hdr_size;
    for (int i = 0; i < nsec; i++) {
        if (t->tiny) {
            sec_raw[i] = (i + 1 < nsec ? sec_rva[i + 1] : sec_rva[i] + pe_align(sec_len[i], FA)) - sec_rva[i];
            sec_ptr[i] = sec_rva[i];
            fpos = sec_ptr[i] + sec_raw[i];
            continue;
        }
        sec_raw[i] = sec_bss[i] ? 0 : pe_align(sec_len[i], FA);
        sec_ptr[i] = sec_raw[i] ? fpos : 0;
        fpos += sec_raw[i];
    }
    // tiny: Windows does not take an image file shorter than 336 bytes -- the last section gets zeros
    // in the file only; its size in memory stays its own (made equal, a new file fails on its first start)
    const int tiny_min = 336;
    if (t->tiny && fpos < tiny_min) {
        sec_raw[nsec - 1] += tiny_min - fpos;
        fpos = tiny_min;
    }
    int image_end = sec_rva[nsec - 1] + pe_align(sec_vsize[nsec - 1] ? sec_vsize[nsec - 1] : 1, SA);
    u8 *file = cc_alloc((size_t)fpos);
    for (int i = 0; i < nsec; i++)
        for (int k = 0; k < sec_raw[i] && k < sec_vsize[i] && k < sec_len[i]; k++)
            file[sec_ptr[i] + k] = bytes[i][k];

    put_dos_stub(file);
    ((PeDos *)file)->e_lfanew = nt_at;
    u8 *nt = file + nt_at;
    wr32(nt, PE_NT_MAGIC);
    PeFileHeader *fh = (PeFileHeader *)(nt + 4);
    fh->Machine = PE_MACHINE_X64;
    fh->NumberOfSections = (unsigned short)nsec;
    fh->SizeOfOptionalHeader = (unsigned short)opt_size;
    fh->Characteristics = PE_F_EXECUTABLE | PE_F_LARGE_ADDR | (t->exe ? 0 : PE_F_DLL);
    PeOpt64 *op = (PeOpt64 *)(nt + 4 + sizeof(PeFileHeader));
    op->Magic = PE_MAGIC_PE32PLUS;
    op->MajorLinkerVersion = 1;
    op->SizeOfCode = (u32)sec_raw[0];
    for (int i = 1; i < nsec; i++) {
        if (sec_bss[i])
            op->SizeOfUninitializedData += (u32)pe_align(sec_vsize[i], FA);
        else
            op->SizeOfInitializedData += (u32)sec_raw[i];
    }
    op->AddressOfEntryPoint = (u32)(text_rva + entry->offset);
    op->BaseOfCode = (u32)text_rva;
    op->ImageBase = as->image_base;
    op->SectionAlignment = SA;
    op->FileAlignment = FA;
    op->MajorOSVersion = 6;
    op->MajorSubsystemVersion = 6;
    op->SizeOfImage = (u32)image_end;
    op->SizeOfHeaders = (u32)hdr_size;
    op->Subsystem = (unsigned short)(t->subsystem ? t->subsystem : 3);
    // high-entropy ASLR, relocatable, no-execute data; an .exe is terminal-server aware
    op->DllCharacteristics = (unsigned short)(0x0020 | 0x0040 | 0x0100 | (t->exe ? 0x8000 : 0));
    op->SizeOfStackReserve = t->stack_reserve ? t->stack_reserve : 0x800000;     // 8 MB: deep recursion and big local arrays
    op->SizeOfStackCommit = 0x1000;
    op->SizeOfHeapReserve = 0x100000;
    op->SizeOfHeapCommit = 0x1000;
    op->NumberOfRvaAndSizes = (u32)ndirs;
    if ((reloc_size && ndirs <= PE_DIR_BASERELOC) || (pdata_size && ndirs <= 3) || (rsrc_size && ndirs <= PE_DIR_RESOURCE))
        cc_error_at(0, 0, "ассемблер: --tiny: в заголовке нет места для каталога (%d)", ndirs);
    if (ne) {
        op->DataDirectory[PE_DIR_EXPORT].VirtualAddress = (u32)(tab_rva + expdir_off);
        op->DataDirectory[PE_DIR_EXPORT].Size = (u32)(own_off + cc_strlen(own) + 1 - expdir_off);
    }
    if (ndll) {
        op->DataDirectory[PE_DIR_IMPORT].VirtualAddress = (u32)(tab_rva + impdesc_off);
        op->DataDirectory[PE_DIR_IMPORT].Size = (u32)((ndll + 1) * sizeof(PeImportDesc));
        if (ndirs > PE_DIR_IAT) {
            op->DataDirectory[PE_DIR_IAT].VirtualAddress = (u32)(tab_rva + iat_off);
            op->DataDirectory[PE_DIR_IAT].Size = (u32)(nslot * 8);
        }
    }
    if (reloc_size) {
        op->DataDirectory[PE_DIR_BASERELOC].VirtualAddress = (u32)reloc_rva;
        op->DataDirectory[PE_DIR_BASERELOC].Size = (u32)reloc_size;
    }
    if (rsrc_size) {
        op->DataDirectory[PE_DIR_RESOURCE].VirtualAddress = (u32)rsrc_rva;
        op->DataDirectory[PE_DIR_RESOURCE].Size = (u32)rsrc_size;
    }
    if (pdata_size) {
        op->DataDirectory[3].VirtualAddress = (u32)pdata_rva;      // exceptions: .pdata
        op->DataDirectory[3].Size = (u32)(12 * npf);
    }
    if (tls_size) {
        op->DataDirectory[9].VirtualAddress = (u32)(tab_rva + tlsdir_off);
        op->DataDirectory[9].Size = 40;
    }

    PeSection *sec = (PeSection *)((u8 *)op + opt_size);
    for (int i = 0; i < nsec; i++) {
        for (int k = 0; names[i][k]; k++) sec[i].Name[k] = (u8)names[i][k];
        sec[i].VirtualSize = (u32)(sec_vsize[i] ? sec_vsize[i] : 1);
        sec[i].VirtualAddress = (u32)sec_rva[i];
        sec[i].SizeOfRawData = (u32)sec_raw[i];
        sec[i].PointerToRawData = (u32)sec_ptr[i];
        sec[i].Characteristics = flags[i];
    }

    cc_win_debug_text = cc_debug_info ? dbg_text(as, text_rva) : 0;
    *out = file;
    *out_size = (u64)fpos;
    return 1;
}
