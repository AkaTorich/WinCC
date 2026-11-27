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

static int is_resource_file(const char *name) { return ends_with(name, ".rc") || ends_with(name, ".res"); }

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
        if (is_resource_file(sources[i].name))
            continue;
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
    char *all = cc_peephole(compile_to_asm(sources, count));
    if (asm_text)
        *asm_text = all;
    cc_assemble_program(all, externs, out);
    return 1;
}

// ---- Windows (WinCC) ----

int cc_first_lib_unit;
int (*cc_source_lib)(const char *name, const CcSource **files);
int cc_pragma_subsystem;
int cc_pragma_manifest;

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
    // the frame of a C function, with its unwind info: an exception is searched for through it.
    // An entry without parameters is called as it is and its code goes to ExitProcess (start.c,
    // __wcc_finish): nothing of the library comes in that the program does not call itself.
    // With parameters, the start of that kind makes them first: __wcc_start_main (argc, argv in
    // UTF-8), __wcc_start_wmain, __wcc_start_winmain (instance and line), __wcc_start_wwinmain.
    if (cc_entry_args[kind] <= 0)
        return cc_format("\n.text\n.local __wcc_start\n__wcc_start:\n.seh_proc 0\n"
                         "  push rbp\n  mov rbp, rsp\n  sub rsp, 32\n  call %s\n  mov ecx, eax\n"
                         "  call __wcc_finish\n  int3\n.seh_endproc\n",
                         entries[kind]);
    static const char *starts[4] = { "__wcc_start_main", "__wcc_start_wmain", "__wcc_start_winmain", "__wcc_start_wwinmain" };
    return cc_format("\n.text\n.local __wcc_start\n__wcc_start:\n.seh_proc 0\n"
                     "  push rbp\n  mov rbp, rsp\n  sub rsp, 32\n  lea rcx, [rip+%s]\n"
                     "  call %s\n  int3\n.seh_endproc\n",
                     entries[kind], starts[kind]);
}

// ---- wcc --lib: a library of the programmer's own (prog.lib, WinCC's format, not Microsoft's) ----
// WCCLIB 1
// uses NAME                  a #pragma comment(lib) of its sources (a DLL or a library in source form)
// source BYTES NAME          then BYTES of text: a C file after the preprocessor (#line keeps its file
//                            and lines for errors and the debugger), or an .asm file as it is
// A program takes it as a library: its functions nobody calls stay out (strip.c).

typedef struct { char *p; int len, cap; } LibBuf;

static void lb_add(LibBuf *b, const char *s, int n)
{
    if (b->len + n + 1 > b->cap) {
        int cap = b->cap ? b->cap : 65536;
        while (b->len + n + 1 > cap)
            cap *= 2;
        char *np = cc_alloc((size_t)cap);
        for (int i = 0; i < b->len; i++)
            np[i] = b->p[i];
        b->p = np;
        b->cap = cap;
    }
    for (int i = 0; i < n; i++)
        b->p[b->len++] = s[i];
    b->p[b->len] = 0;
}

static void lb_str(LibBuf *b, const char *s) { lb_add(b, s, cc_strlen(s)); }

static int is_hex_digit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

// a string literal again from its elements (adjacent literals are one token by now): printable ASCII as it is,
// the rest as escapes -- \ooo in a narrow one, \xHHHH in a wide one (a hex digit after it opens a new literal)
static void lb_string(LibBuf *b, Token *t)
{
    static const char *prefix[] = { "", "u8", "u", "U", "L" };
    const char *pre = prefix[t->str_kind];
    int es = cc_str_elem_size(t->str_kind);
    lb_str(b, pre);
    lb_add(b, "\"", 1);
    int hex_open = 0;
    for (int i = 0; i + 1 < t->str_len; i++) {
        u32 v = 0;
        for (int k = 0; k < es; k++)
            v |= (u32)(u8)t->str[i * es + k] << (8 * k);
        if (v >= 0x20 && v < 0x7F && v != '"' && v != '\\') {
            if (hex_open && is_hex_digit((int)v)) {
                lb_add(b, "\" ", 2);
                lb_str(b, pre);
                lb_add(b, "\"", 1);
            }
            char c = (char)v;
            lb_add(b, &c, 1);
            hex_open = 0;
        } else if (es == 1) {
            char o[4] = { '\\', (char)('0' + ((v >> 6) & 3)), (char)('0' + ((v >> 3) & 7)), (char)('0' + (v & 7)) };
            lb_add(b, o, 4);                // \ooo: always three digits, so a digit after it stays a digit
            hex_open = 0;
        } else {
            lb_str(b, cc_format("\\x%x", (unsigned)v));
            hex_open = 1;
        }
    }
    lb_add(b, "\"", 1);
}

static void lb_quoted_path(LibBuf *b, const char *s)
{
    lb_add(b, "\"", 1);
    for (; *s; s++) {
        if (*s == '\\' || *s == '"')
            lb_add(b, "\\", 1);
        lb_add(b, s, 1);
    }
    lb_add(b, "\"", 1);
}

// A C file after the preprocessor, as text a later compile reads the same way
static char *preprocessed_text(const char *name, const char *text)
{
    Token *t = cc_preprocess(cc_tokenize(name, text));
    LibBuf b = { 0 };
    const char *file = 0;
    int line = 0;
    for (; t->kind != TK_EOF; t = t->next) {
        const char *tf = t->file ? t->file : "";
        if (!file || (tf != file && !cc_streq(tf, file))) {
            lb_str(&b, cc_format("\n#line %d ", t->line));
            lb_quoted_path(&b, tf);
            lb_add(&b, "\n", 1);
            file = tf;
            line = t->line;
        } else if (t->line != line) {
            if (t->line > line && t->line - line <= 8)
                for (; line < t->line; line++)
                    lb_add(&b, "\n", 1);
            else
                lb_str(&b, cc_format("\n#line %d\n", t->line));
            line = t->line;
        } else
            lb_add(&b, " ", 1);
        if (t->kind == TK_STR)
            lb_string(&b, t);
        else
            lb_add(&b, t->loc, t->len);
    }
    lb_add(&b, "\n", 1);
    return b.p;
}

int cc_build_lib(const CcSource *sources, int count, char **out)
{
    cc_last_error[0] = 0;
    if (cc_setjmp(&g_jmp))
        return 0;
    // the whole library must compile now: its errors are its own, not the programs' that take it
    compile_to_asm(sources, count);
    LibBuf b = { 0 };
    lb_str(&b, "WCCLIB 1\n");
    for (int i = 0; i < cc_pragma_lib_count; i++)
        lb_str(&b, cc_format("uses %s\n", cc_pragma_libs[i]));
    for (int i = 0; i < count; i++) {
        if (is_resource_file(sources[i].name))
            continue;
        int asm_file = ends_with(sources[i].name, ".asm") || ends_with(sources[i].name, ".s");
        const char *text = asm_file ? sources[i].text : preprocessed_text(sources[i].name, sources[i].text);
        lb_str(&b, cc_format("source %d %s\n", cc_strlen(text), sources[i].name));
        lb_str(&b, text);
    }
    *out = b.p;
    return 1;
}

int cc_build_win(const CcSource *sources, int count, int first_lib, CcWinTarget *t,
                 u8 **out, u64 *out_size, char **asm_text)
{
    cc_last_error[0] = 0;
    if (cc_setjmp(&g_jmp)) {
        cc_first_lib_unit = 0;
        return 0;
    }
    // resources first: .rc scripts and .res files of rc.exe
    cc_resources_reset();
    for (int i = 0; i < count; i++) {
        if (ends_with(sources[i].name, ".rc"))
            cc_compile_rc(sources[i].name, sources[i].text);
        else if (ends_with(sources[i].name, ".res"))
            cc_read_res(sources[i].name);
    }
    CcResource *res;
    t->nres = cc_resources(&res);
    t->res = res;
    cc_first_lib_unit = first_lib > 0 && first_lib < count ? first_lib + 1 : 0;
    for (int i = 0; i < 4; i++)
        cc_entry_args[i] = -1;
    char *all = compile_to_asm(sources, count);
    cc_first_lib_unit = 0;
    if (cc_pragma_manifest)
        t->manifest = 1;
    all = cc_join(all, "", win_entry_stub(t, all));
    all = cc_strip_unused(all);         // the library's functions the program never reaches stay out
    // nothing in the program needs an end (no output, files, atexit: need_end did not stay) -- the
    // entry calls ExitProcess itself, and __wcc_finish with __wcc_at_end goes too
    if (t->exe && !has_label(all, "need_end")) {
        // the entry's line (win_entry_stub) is rewritten in place: both are 20 characters with the newline
        static const char from[] = "  call __wcc_finish\n", to[] = "  call @ExitProcess\n";
        for (char *p = all; *p; p++)
            if (cc_strneq(p, from, 20))
                for (int k = 0; k < 20; k++) p[k] = to[k];
        all = cc_strip_unused(all);
    }
    all = cc_peephole(all);
    if (asm_text)
        *asm_text = all;
    // not a tail call: an assembler error comes back to cc_setjmp above, so this frame must still be here
    if (t->tiny && t->exe) {
        // --tiny: the 16-byte layout only when it is smaller (zeros of .bss lie in such a file,
        // so a program with big zero-filled arrays is smaller the usual way); a DLL stays as usual
        u8 *small;
        u64 small_size;
        volatile int ok = cc_assemble_win(all, t, &small, &small_size);
        char *small_dbg = cc_win_debug_text;
        t->tiny = 0;
        if (ok)
            ok = cc_assemble_win(all, t, out, out_size);
        t->tiny = 1;
        if (ok && small_size < *out_size) {
            *out = small;
            *out_size = small_size;
            cc_win_debug_text = small_dbg;
        }
        return ok;
    }
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
