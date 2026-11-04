// Compiler driver: C and .asm sources -> one program in memory.
#include "cc.h"

typedef struct { u64 regs[10]; u8 xmm[160]; } CcJmpBuf;
int  cc_setjmp(CcJmpBuf *buf);
__declspec(noreturn) void cc_longjmp(CcJmpBuf *buf, int value);

int cc_target_win;
char *cc_file_asm;
const char *cc_pragma_libs[64];
int cc_pragma_lib_count;
const char *cc_user_define_names[64], *cc_user_define_values[64];
int cc_user_define_count;
char cc_last_error[512];
const char *cc_last_loc;            // where in the source the error is (a token's place), or 0
static CcJmpBuf g_jmp;

static void format_into(char *buf, int cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    cc_vformat(buf, cap, fmt, ap);
    va_end(ap);
}

__declspec(noreturn) static void fail(const char *file, int line, const char *fmt, va_list ap)
{
    char msg[400];
    cc_vformat(msg, sizeof(msg), fmt, ap);
    if (file && line > 0)
        format_into(cc_last_error, sizeof(cc_last_error), "%s:%d: %s", file, line, msg);
    else
        format_into(cc_last_error, sizeof(cc_last_error), "%s", msg);
    cc_longjmp(&g_jmp, 1);
}

__declspec(noreturn) void cc_error_at(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    cc_last_loc = 0;
    va_start(ap, fmt);
    fail(file, line, fmt, ap);
}

__declspec(noreturn) void cc_error_tok(Token *tok, const char *fmt, ...)
{
    va_list ap;
    cc_last_loc = tok ? tok->loc : 0;
    va_start(ap, fmt);
    fail(tok ? tok->file : 0, tok ? tok->line : 0, fmt, ap);
}

void (*cc_warn_hook)(const char *text);

static void warn(const char *file, int line, const char *fmt, va_list ap)
{
    if (!cc_warn_hook)
        return;
    char msg[400], all[512];
    cc_vformat(msg, sizeof(msg), fmt, ap);
    if (file && line > 0)
        format_into(all, sizeof(all), "%s:%d: предупреждение: %s", file, line, msg);
    else
        format_into(all, sizeof(all), "предупреждение: %s", msg);
    cc_warn_hook(all);
}

void cc_warning_at(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    warn(file, line, fmt, ap);
    va_end(ap);
}

void cc_warning_tok(Token *tok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    warn(tok ? tok->file : 0, tok ? tok->line : 0, fmt, ap);
    va_end(ap);
}

static int ends_with(const char *s, const char *suffix)
{
    int n = cc_strlen(s), m = cc_strlen(suffix);
    if (n < m)
        return 0;
    for (int i = 0; i < m; i++) {
        char a = s[n - m + i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b)
            return 0;
    }
    return 1;
}

// Front end shared by cc_build and cc_build_dll: sources -> assembly text.
static char *compile_to_asm(const CcSource *sources, int count)
{
    // Each C file is its own translation unit: preprocessed alone (fresh
    // macros, #pragma once, include guards), its tokens marked with its
    // number; then all units become one stream for the parser.
    Token head = { 0 };
    Token *cur = &head;
    Token *eof = 0;
    int asm_len = 0;
    for (int i = 0; i < count; i++) {
        if (ends_with(sources[i].name, ".asm") || ends_with(sources[i].name, ".s")) {
            asm_len += cc_strlen(sources[i].text) + cc_strlen(sources[i].name) + 24;
            continue;
        }
        Token *t = cc_preprocess(cc_tokenize(sources[i].name, sources[i].text));
        for (; t->kind != TK_EOF; t = t->next) {
            t->unit = i + 1;
            cur = cur->next = t;
        }
        eof = t;
    }
    // libraries in source form the units asked for (#pragma comment(lib)); they may ask for more
    for (int done = 0, unit = count; cc_source_lib && done < cc_pragma_lib_count; done++) {
        const CcSource *files;
        int n = cc_source_lib(cc_pragma_libs[done], &files);
        for (int k = 0; k < n; k++) {
            Token *t = cc_preprocess(cc_tokenize(files[k].name, files[k].text));
            unit++;
            for (; t->kind != TK_EOF; t = t->next) {
                t->unit = unit;
                cur = cur->next = t;
            }
            eof = t;
        }
    }
    if (!eof)
        eof = cc_preprocess(cc_tokenize("<пусто>", ""));
    cur->next = eof;
    Token *tok = head.next;

    Obj *prog = cc_parse(tok);
    char *text = cc_codegen_text(prog);

    // append the .asm sources
    int text_len = cc_strlen(text);
    char *all = cc_alloc((size_t)text_len + (size_t)asm_len + 16);
    int pos = 0;
    for (int i = 0; i < text_len; i++)
        all[pos++] = text[i];
    for (int i = 0; i < count; i++) {
        if (!(ends_with(sources[i].name, ".asm") || ends_with(sources[i].name, ".s")))
            continue;
        // an error in it names the .asm file and its line (asm.c, .srcfile)
        const char *s = ".text\n.srcfile 1 ";
        while (*s)
            all[pos++] = *s++;
        for (s = sources[i].name; *s; s++)
            all[pos++] = *s;
        all[pos++] = '\n';
        for (const char *p = sources[i].text; *p; p++)
            all[pos++] = *p;
        all[pos++] = '\n';
    }
    all[pos] = 0;
    return all;
}

int cc_build(const CcSource *sources, int count, const CcSymbol *externs, CcImage *out, char **asm_text)
{
    cc_last_error[0] = 0;
    if (cc_setjmp(&g_jmp))
        return 0;
    char *all = compile_to_asm(sources, count);
    if (asm_text)
        *asm_text = all;
    cc_assemble_program(all, externs, out);
    return 1;
}

// ---- Windows (WinCC) ----

int cc_first_lib_unit;
int (*cc_source_lib)(const char *name, const CcSource **files);
int cc_pragma_subsystem;

// Is "name:" a line of the assembly text (a function the program defines)?
static int has_label(const char *text, const char *name)
{
    int n = cc_strlen(name);
    for (const char *p = text; *p;) {
        if (cc_strneq(p, name, n) && p[n] == ':' && (p[n + 1] == '\n' || p[n + 1] == '\r' || !p[n + 1]))
            return 1;
        while (*p && *p != '\n')
            p++;
        if (*p)
            p++;
    }
    return 0;
}

// The entry Windows calls: a few instructions that hand the program's own
// entry to the library (start.c), which starts it.
static const char *win_entry_stub(CcWinTarget *t, const char *text)
{
    if (!t->exe) {
        t->entry = "__wcc_dllstart";
        // BOOL WINAPI (HINSTANCE, DWORD reason, void *): rcx, rdx, r8 stay; r9 = the DLL's DllMain or 0
        return cc_format("\n.text\n.local __wcc_dllstart\n__wcc_dllstart:\n  %s\n  jmp __wcc_dllentry\n",
                         has_label(text, "DllMain") ? "lea r9, [rip+DllMain]" : "xor r9d, r9d");
    }
    static const char *entries[4] = { "main", "wmain", "WinMain", "wWinMain" };
    int has[4];
    for (int i = 0; i < 4; i++)
        has[i] = has_label(text, entries[i]);
    if (!t->subsystem)                          // #pragma comment(linker, "/SUBSYSTEM:WINDOWS") of a header
        t->subsystem = cc_pragma_subsystem;
    if (!t->subsystem)                          // by the entry: WinMain alone makes a windowed program
        t->subsystem = (has[2] || has[3]) && !has[0] && !has[1] ? 2 : 3;
    static const int console_order[4] = { 0, 1, 2, 3 }, window_order[4] = { 2, 3, 0, 1 };
    const int *order = t->subsystem == 2 ? window_order : console_order;
    int kind = -1;
    for (int i = 0; i < 4 && kind < 0; i++)
        if (has[order[i]])
            kind = order[i];
    if (kind < 0)
        cc_error_at(0, 0, "нет функции main (для окон — WinMain)");
    t->entry = "__wcc_start";
    // the frame of a C function, with its unwind info: an exception is searched for through it
    return cc_format("\n.text\n.local __wcc_start\n__wcc_start:\n.seh_proc 0\n"
                     "  push rbp\n  mov rbp, rsp\n  sub rsp, 32\n  mov ecx, %d\n  lea rdx, [rip+%s]\n"
                     "  call __wcc_startup\n  int3\n.seh_endproc\n",
                     kind, entries[kind]);
}

int cc_build_win(const CcSource *sources, int count, int first_lib, CcWinTarget *t,
                 u8 **out, u64 *out_size, char **asm_text)
{
    cc_last_error[0] = 0;
    if (cc_setjmp(&g_jmp)) {
        cc_first_lib_unit = 0;
        return 0;
    }
    cc_first_lib_unit = first_lib > 0 && first_lib < count ? first_lib + 1 : 0;
    char *all = compile_to_asm(sources, count);
    cc_first_lib_unit = 0;
    all = cc_join(all, "", win_entry_stub(t, all));
    all = cc_strip_unused(all);         // the library's functions the program never reaches stay out
    if (asm_text)
        *asm_text = all;
    // not a tail call: an assembler error comes back to cc_setjmp above, so this frame must still be here
    volatile int ok = cc_assemble_win(all, t, out, out_size);
    return ok;
}

int cc_build_dll(const CcSource *sources, int count, const CcSymbol *externs,
                 const char *dll_name, u8 **out, u64 *out_size)
{
    cc_last_error[0] = 0;
    if (cc_setjmp(&g_jmp))
        return 0;
    char *all = compile_to_asm(sources, count);
    volatile int ok = cc_assemble_pe(all, externs, dll_name, out, out_size);   // not a tail call: see above
    return ok;
}
