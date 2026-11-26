// The last pass over the assembly text: small sure rewrites of what the code generator leaves.
//   push rax ... pop rcx   ->  mov rcx, rax ...   (rdx too) when nothing between touches the stack,
//                             calls, jumps, or uses that register (also not implicitly: rep, div...)
//   jmp L / L:             ->  L:                 (a jump to the very next line)
// Only lines the code generator writes are touched: an instruction is a line that starts with two
// spaces; a label, a directive or anything else stops a search (.loc lines are passed over).
#include "cc.h"

typedef struct { const char *s; int n; int dead; } Line;

static int starts(const Line *l, const char *w)
{
    int n = cc_strlen(w);
    return l->n >= n && cc_strneq(l->s, w, n);
}

static int is_instr(const Line *l) { return l->n > 2 && l->s[0] == ' ' && l->s[1] == ' ' && l->s[2] != ' '; }
static int is_loc(const Line *l) { return starts(l, ".loc "); }

static int ident_char(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '@'; }

// does the line name this register (as a whole word)?
static int names(const Line *l, const char *reg)
{
    int n = cc_strlen(reg);
    for (int i = 0; i + n <= l->n; i++)
        if (cc_strneq(l->s + i, reg, n) && (i == 0 || !ident_char(l->s[i - 1])) &&
            (i + n == l->n || !ident_char(l->s[i + n])))
            return 1;
    return 0;
}

// the mnemonic of an instruction line: its first word
static int mnemonic_is(const Line *l, const char *m)
{
    int n = cc_strlen(m);
    return l->n >= 2 + n && cc_strneq(l->s + 2, m, n) && (l->n == 2 + n || l->s[2 + n] == ' ');
}

static int mnemonic_starts(const Line *l, const char *m)
{
    int n = cc_strlen(m);
    return l->n >= 2 + n && cc_strneq(l->s + 2, m, n);
}

static int uses_rcx(const Line *l)
{
    return names(l, "rcx") || names(l, "ecx") || names(l, "cx") || names(l, "cl") || names(l, "ch") ||
           mnemonic_starts(l, "rep") || mnemonic_starts(l, "loop") || mnemonic_is(l, "jrcxz") ||
           mnemonic_is(l, "jecxz") || mnemonic_is(l, "syscall") || mnemonic_is(l, "cpuid") ||
           mnemonic_is(l, "rdtscp") || mnemonic_is(l, "xgetbv");
}

static int one_operand(const Line *l)
{
    for (int i = 2; i < l->n; i++)
        if (l->s[i] == ',')
            return 0;
    return 1;
}

static int uses_rdx(const Line *l)
{
    return names(l, "rdx") || names(l, "edx") || names(l, "dx") || names(l, "dl") || names(l, "dh") ||
           mnemonic_is(l, "cqo") || mnemonic_is(l, "cdq") || mnemonic_is(l, "cwd") || mnemonic_is(l, "div") ||
           mnemonic_is(l, "idiv") || (mnemonic_is(l, "mul") && one_operand(l)) ||
           (mnemonic_is(l, "imul") && one_operand(l)) || mnemonic_is(l, "rdtsc") || mnemonic_is(l, "rdtscp") ||
           mnemonic_is(l, "cpuid") || mnemonic_is(l, "xgetbv") || mnemonic_starts(l, "rep");
}

// an instruction a push/pop pair may stay around: no stack, no control flow, no raw bytes
static int plain(const Line *l)
{
    static const char *bad[] = { "push", "pop", "call", "ret", "jmp", "int", "db", "dw", "dd", "dq", "enter",
                                 "leave", "syscall", "lock", 0 };
    for (int i = 0; bad[i]; i++)
        if (mnemonic_is(l, bad[i]))
            return 0;
    if (l->s[2] == 'j')                     // jcc
        return 0;
    return !names(l, "rsp") && !names(l, "esp") && !names(l, "sp");
}

static int push_pop(Line *ln, int n)
{
    int changed = 0;
    for (int i = 0; i < n; i++) {
        if (ln[i].dead || !(ln[i].n == 10 && cc_strneq(ln[i].s, "  push rax", 10)))
            continue;
        int rcx = 0, rdx = 0;
        for (int j = i + 1; j < n && j < i + 64; j++) {
            Line *l = &ln[j];
            if (l->dead || is_loc(l))
                continue;
            if (!is_instr(l))
                break;
            if (mnemonic_is(l, "pop")) {
                const char *t = l->n == 9 && cc_strneq(l->s, "  pop rcx", 9) ? "  mov rcx, rax" :
                                l->n == 9 && cc_strneq(l->s, "  pop rdx", 9) ? "  mov rdx, rax" : 0;
                if (t && ((t[7] == 'c' && !rcx) || (t[7] == 'd' && !rdx))) {
                    ln[i].s = t;
                    ln[i].n = cc_strlen(t);
                    l->dead = 1;
                    changed = 1;
                }
                break;
            }
            if (!plain(l))
                break;
            rcx |= uses_rcx(l);
            rdx |= uses_rdx(l);
        }
    }
    return changed;
}

static int jump_next(Line *ln, int n)
{
    int changed = 0;
    for (int i = 0; i < n; i++) {
        Line *l = &ln[i];
        if (l->dead || !starts(l, "  jmp "))
            continue;
        const char *target = l->s + 6;
        int tn = l->n - 6;
        int j = i + 1;
        while (j < n && (ln[j].dead || is_loc(&ln[j])))
            j++;
        if (j < n && ln[j].n == tn + 1 && cc_strneq(ln[j].s, target, tn) && ln[j].s[tn] == ':') {
            l->dead = 1;
            changed = 1;
        }
    }
    return changed;
}

char *cc_peephole(const char *text)
{
    int count = 1;
    for (const char *p = text; *p; p++)
        count += *p == '\n';
    Line *ln = cc_alloc(sizeof(Line) * (size_t)count);
    int n = 0;
    for (const char *p = text; *p;) {
        const char *e = p;
        while (*e && *e != '\n')
            e++;
        int len = (int)(e - p);
        if (len && p[len - 1] == '\r')
            len--;
        ln[n].s = p;
        ln[n].n = len;
        n++;
        p = *e ? e + 1 : e;
    }
    for (int round = 0; round < 8; round++) {
        int c = push_pop(ln, n);
        c |= jump_next(ln, n);
        if (!c)
            break;
    }
    int size = 1;
    for (int i = 0; i < n; i++)
        if (!ln[i].dead)
            size += ln[i].n + 1;
    char *out = cc_alloc((size_t)size);
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (ln[i].dead)
            continue;
        for (int j = 0; j < ln[i].n; j++)
            out[k++] = ln[i].s[j];
        out[k++] = '\n';
    }
    out[k] = 0;
    return out;
}
