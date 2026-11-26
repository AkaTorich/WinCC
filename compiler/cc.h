// DyOS C compiler: shared declarations.
// Pipeline: preprocess + tokenize -> parse to AST -> x86-64 machine code in memory.
#pragma once

#include <stddef.h>
#include <stdarg.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef long long          i64;

// ---- host interface (kernel or Windows test harness provides these) ----
void *cc_alloc(size_t size);                // zeroed, never freed individually
void *cc_alloc_code(size_t size);           // executable memory for the program
void  cc_free_all(void);
void  cc_print(const char *text);
const char *cc_read_file(const char *name); // NUL-terminated or NULL

// ---- what the C library of programs (runtime.c) needs from the host ----
const char *cc_file_read(const char *name, size_t *size);  // the whole file (host's memory) or NULL
int  cc_file_write(const char *name, const char *data, size_t size);   // replaces the file; 1 = done
int  cc_file_delete(const char *name);                     // 1 = done
int  cc_file_rename(const char *from, const char *to);     // 1 = done
// What a name is, for errno (EISDIR, ENOTDIR): 0 nothing, 1 a file, 2 a folder.
// Optional: a host without it links to runtime.c's stand-in (-1, cannot tell).
int  cc_file_kind(const char *name);
int  cc_input_line(char *buf, int cap);     // a typed line with echo, ending in '\n'; its length, -1 at end of input
void cc_now(int t[6]);                      // local time: year, month 1-12, day, hour, minute, second
u64  cc_millis(void);                       // milliseconds from any fixed moment

// ---- tokens ----
typedef enum {
    TK_IDENT, TK_PUNCT, TK_KEYWORD, TK_NUM, TK_STR, TK_CHAR, TK_EOF
} TokenKind;

typedef struct Type Type;
typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    const char *loc;
    int len;
    i64 val;
    double fval;
    Type *num_ty;               // type of a numeric literal
    int is_unsigned, is_long;
    char *str;                  // decoded string literal: its elements, little-endian
    int str_len;                // elements, including the terminating NUL
    int str_kind;               // STR_PLAIN "", STR_U8 u8"", STR_U16 u"", STR_U32 U"", STR_WIDE L""
    const char *file;
    int line;
    int at_bol;                 // first token of a line
    int has_space;
    void *hideset;              // preprocessor: macros not to expand again
    int unit;                   // the source (translation unit) it is of: 1, 2...; set by cc.c
    int too_wide;               // L'x' beyond 16 bits in Windows: an error once it is used (not in a skipped #if)
};

// ---- types ----
typedef enum {
    TY_VOID, TY_BOOL, TY_CHAR, TY_SHORT, TY_INT, TY_LONG,
    TY_FLOAT, TY_DOUBLE, TY_ENUM, TY_PTR, TY_FUNC, TY_ARRAY, TY_STRUCT, TY_UNION,
    TY_VLA                      // int a[n]: the variable holds the address of its memory (alloca)
} TypeKind;

typedef struct Member Member;
typedef struct Obj Obj;
typedef struct Node Node;

struct Type {
    TypeKind kind;
    int size;
    int align;
    int is_unsigned;
    Type *base;                 // pointer / array element
    Token *name;                // declarator name
    int array_len;
    Member *members;
    int is_flexible;
    Type *return_ty;
    Type *params;
    int is_variadic;
    Type *next;                 // param list link
    Type *origin;
    int pack;                   // struct/union: #pragma pack in force where it was defined (0 = natural)
    // TY_VLA: its length (an expression), the hidden local that holds its size in bytes;
    // a parameter's length is read in the function's body (vla_tok), where the parameters are seen
    Node *vla_len;
    Obj *vla_size;
    Token *vla_tok;
    int is_atomic;              // _Atomic: "x += y" is one indivisible step (lock cmpxchg)
};

struct Member {
    Member *next;
    Type *ty;
    Token *name;
    int offset;
    int is_bitfield;
    int bit_offset, bit_width;
};

extern Type *ty_void, *ty_bool, *ty_char, *ty_short, *ty_int, *ty_long;
extern Type *ty_uchar, *ty_ushort, *ty_uint, *ty_ulong;
extern Type *ty_float, *ty_double;
int is_flonum(Type *ty);

// ---- AST ----
typedef enum {
    ND_NULL_EXPR, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD, ND_NEG,
    ND_BITAND, ND_BITOR, ND_BITXOR, ND_BITNOT, ND_SHL, ND_SHR,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_ASSIGN, ND_COND, ND_COMMA,
    ND_MEMBER, ND_ADDR, ND_DEREF, ND_NOT, ND_LOGAND, ND_LOGOR,
    ND_RETURN, ND_IF, ND_FOR, ND_DO, ND_SWITCH, ND_CASE, ND_BLOCK,
    ND_GOTO, ND_LABEL, ND_FUNCALL, ND_EXPR_STMT, ND_STMT_EXPR,
    ND_VAR, ND_NUM, ND_CAST, ND_MEMZERO, ND_ASM,
    ND_VLA_PTR,                 // the slot of an indirect local (a VLA, an over-aligned one): its address
    ND_ALLOCA,                  // _alloca(lhs): stack memory, aligned to val
    ND_CAS,                     // compare-exchange: lhs address, rhs address of the expected value, then the new
    ND_EXCH,                    // exchange: lhs address, rhs the new value; the old one is the result
    ND_FETCH_ADD,               // lock xadd: lhs address, rhs what to add; the old value is the result
    ND_TRY                      // __try: body; __except (cond) then, or __finally els
} NodeKind;

typedef struct AsmRef AsmRef;   // asm("... @x ..."): a local x of the function, found where the asm is
struct AsmRef {
    AsmRef *next;
    const char *name;
    int len;
    Obj *var;
};

struct Node {
    NodeKind kind;
    Node *next;
    Type *ty;
    Token *tok;
    Node *lhs, *rhs;
    Node *cond, *then, *els, *init, *inc;
    Node *body;
    Member *member;
    Type *func_ty;
    Node *args;
    Obj *ret_buffer;
    char *brk_label, *cont_label;
    char *label, *unique_label;
    Node *goto_next;
    Node *case_next, *default_case;
    i64 begin, end;             // case range
    Obj *var;
    i64 val;
    double fval;
    char *asm_str;
    AsmRef *asm_refs;
    Token *lbrace, *rbrace;     // ND_BLOCK: its braces (rbrace always, lbrace for a function's body) -- the line table
};

struct Obj {
    Obj *next;
    char *name;
    Type *ty;
    Token *tok;
    int is_local;
    int align;
    int offset;                 // locals: rbp offset
    int is_function, is_definition, is_static;
    int is_export;              // __declspec(dllexport): only such names (and main) go to a PE's export table
    int is_naked;               // __declspec(naked): no prologue or epilogue, the body is the asm it has
    int unit;                  // a file-scope static of a source: its number (1, 2...); 0 = seen by every source
    int is_tls;                 // _Thread_local, __declspec(thread): one per thread (the .tls section)
    int is_selectany;           // __declspec(selectany): several sources may define it, one stays
    int is_literal;             // a string literal: only read (Windows: it lies in .text, next to the code)
    int indirect;               // a local whose slot holds the address of its memory (a VLA, align > 16)
    int reg;                    // a local kept in a register (codegen: 1 rbx, 2 rsi, 3 rdi, 4..7 r12..r15), 0 in memory
    // globals
    char *init_data;
    struct Reloc *rel;
    // functions
    Obj *params;
    Node *body;
    Obj *locals;
    int stack_size;
    int has_alloca;             // _alloca or a VLA: rsp moves inside the body
    int has_seh;                // __try: the function gets a handler (.pdata/.xdata)
    Obj *seh_xptr, *seh_code, *seh_abn;    // __try: EXCEPTION_POINTERS* (filter), the code, AbnormalTermination
    int is_variadic;
    Obj *va_area;
    int nsave, save_pad;        // registers of its locals: pushed after rbp (save[]), and 8 bytes more to keep rbp 16-aligned
    int save[7];
    // placement after codegen
    u8 *address;
};

typedef struct Reloc {
    struct Reloc *next;
    int offset;
    char *label;
    i64 addend;
} Reloc;

// ---- driver state ----
extern const char *cc_file_name;
// The target is Windows (WinCC): long is 4 bytes, wchar_t and L"" are UTF-16,
// _WIN32/_WIN64/__WINCC__ are defined. 0 = DynastyOS, as it was.
extern int cc_target_win;
// Libraries named by #pragma comment(lib, "x") in the sources, in order.
extern const char *cc_pragma_libs[64];
extern int cc_pragma_lib_count;
// -D name=value from the host: defined in every source before its first line.
extern const char *cc_user_define_names[64], *cc_user_define_values[64];
extern int cc_user_define_count;
__declspec(noreturn) void cc_error_tok(Token *tok, const char *fmt, ...);
__declspec(noreturn) void cc_error_at(const char *file, int line, const char *fmt, ...);
// a warning does not stop the build: "file:line: предупреждение: text" goes to the hook (wcc: stderr)
extern void (*cc_warn_hook)(const char *text);
void cc_warning_at(const char *file, int line, const char *fmt, ...);
void cc_warning_tok(Token *tok, const char *fmt, ...);
// set while cc_read_file looks for an #include <...>: the system's folders before the current one
extern int cc_include_angle;

char *cc_format(const char *fmt, ...);
char *cc_join(const char *a, const char *sep, const char *b);   // any length
char *cc_strndup(const char *s, int n);
int   cc_strlen(const char *s);
int   cc_streq(const char *a, const char *b);
int   cc_strneq(const char *a, const char *b, int n);

// tokenize.c
Token *cc_tokenize(const char *file, const char *src);
int    cc_equal(Token *tok, const char *op);
Token *cc_skip(Token *tok, const char *op);
int    cc_consume(Token **rest, Token *tok, const char *op);
void   cc_convert_keywords(Token *tok);
// String literal kinds; elements are char (plain, u8), char16_t (u: UTF-16),
// char32_t (U) or wchar_t (L, 32 bits): both of the last hold code points.
enum { STR_PLAIN, STR_U8, STR_U16, STR_U32, STR_WIDE };
int    cc_str_elem_size(int kind);
void   cc_widen_string(Token *t, int kind);   // re-read a narrow literal as that kind

// preprocess.c
Token *cc_preprocess(Token *tok);
void   cc_define_macro(const char *name, const char *body);

// type.c
int   is_integer(Type *ty);
int   is_numeric(Type *ty);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
Type *vla_of(Type *base, Node *len, Token *tok);
Type *enum_type(void);
Type *struct_type(void);
void  add_type(Node *node);
int   is_compatible(Type *t1, Type *t2);

// parse.c
Obj *cc_parse(Token *tok);
extern char *cc_file_asm;           // file-scope asm("...") of all sources, or 0
// Each source is its own translation unit: its tokens carry its number
// (Token.unit), and it starts with a fresh file scope (typedefs, tags,
// enums, statics); names with external linkage are shared.

// codegen.c: AST -> assembly text
char *cc_codegen_text(Obj *prog);
// wcc -g: frames, variables and types for the debugger (.dbgvar...; asm.c writes them out, cc_win_debug_text);
// wcc -O0: no locals in registers
extern int cc_debug_info, cc_no_regalloc;
extern int cc_entry_args[4];        // parameters of main, wmain, WinMain, wWinMain; -1: none (codegen.c)
// asm.c, wcc -g: the debug info of the last cc_assemble_win (the ".wdbg" text), or 0
extern char *cc_win_debug_text;

typedef struct {
    u8 *code;
    size_t code_size;
    u8 *data;
    size_t data_size;
    void *entry;                // address of main or NULL
} CcImage;

typedef struct { const char *name; void *addr; } CcSymbol;

// asm.c: assembly text -> machine code in memory, or a relocatable PE64 DLL
typedef struct Asm Asm;
int cc_assemble_program(const char *text, const CcSymbol *externs, CcImage *out);
int cc_assemble_pe(const char *text, const CcSymbol *externs, const char *dll_name,
                   u8 **out, u64 *out_size);

// A resource of a Windows program: its type and name are numbers or UTF-16 names (str, len units).
typedef struct { u32 num; const u16 *str; int len; } CcResId;
typedef struct { CcResId type, name; u16 lang; const u8 *data; int size; } CcResource;
int  cc_res_id_equal(CcResId a, CcResId b);
// rc.c: a .rc script (its text in UTF-8) or a .res of rc.exe adds to the list; cc_resources gives it
// (with the string tables); cc_resources_reset empties it. Files it names come through cc_read_binary.
void cc_compile_rc(const char *name, const char *text);
void cc_read_res(const char *name);
int  cc_resources(CcResource **out);
void cc_resources_reset(void);
extern const u8 *(*cc_read_binary)(const char *name, int *size);

// A Windows program or DLL (WinCC): the library is inside it, names it does not
// define come from the system's DLLs (dll_of says which DLL exports a name).
typedef struct {
    int exe;                    // 1 .exe, 0 .dll
    int subsystem;              // 3 console, 2 windows; 0 = by the program's entry (WinMain: windows)
    const char *entry;          // the entry label (cc_build_win fills it in)
    const char *dll_name;       // a DLL's own name, for its export table
    const char *(*dll_of)(const char *name, int len);
    u64 stack_reserve;          // 0 = 1 MB
    const CcResource *res;      // resources of .rc and .res files (cc_build_win fills them in)
    int nres;
    int manifest;               // WinCC's manifest in an .exe: wcc --manifest or #pragma comment(linker, "/MANIFEST")
    int tiny;                   // wcc --tiny: sections and the file aligned by 16 bytes (the file maps as one piece)
} CcWinTarget;
int cc_assemble_win(const char *text, const CcWinTarget *t, u8 **out, u64 *out_size);

// headers.c, runtime.c
const char *cc_builtin_header(const char *name);
const CcSymbol *cc_runtime_symbols(void);
void rt_heap_reset(void *arena, size_t size);
// Runs a program's main: exit() comes back here, atexit functions run and
// files left open are written out. Returns the exit code.
int  rt_run_main(int (*entry)(void));
// A CPU fault in a running program: SIGFPE 8, SIGILL 4, SIGSEGV 11. If the
// program set a handler with signal(), rt_signal_call runs it (it may
// longjmp away); when it returns, the host ends the program.
int  rt_signal_caught(int sig);
void rt_signal_call(int sig);

// format.c
int cc_vformat(char *buf, int cap, const char *fmt, va_list ap);

// cc.c: driver. Sources: C files and .asm files, joined into one program.
typedef struct { const char *name; const char *text; } CcSource;
// Returns 1 on success; on failure the message is in cc_last_error.
int cc_build(const CcSource *sources, int count, const CcSymbol *externs, CcImage *out, char **asm_text);
// Compiles the sources into a relocatable PE64 DLL (bytes in *out, length *out_size).
int cc_build_dll(const CcSource *sources, int count, const CcSymbol *externs,
                 const char *dll_name, u8 **out, u64 *out_size);
// A Windows .exe or .dll: sources[first_lib..] are the library (runtime\), its
// functions are never exported. The program's own entry (main, wmain, WinMain,
// wWinMain, a DLL's DllMain) is found and started by the library's start.c.
int cc_build_win(const CcSource *sources, int count, int first_lib, CcWinTarget *t,
                 u8 **out, u64 *out_size, char **asm_text);
extern int cc_first_lib_unit;       // codegen: units from this number on are the library (0 = none)
// wcc --lib: the programmer's own library (the "WCCLIB 1" text: its C files after the preprocessor, its .asm files,
// the #pragma comment(lib) names it uses) -- a program takes it with "wcc main.c mylib.lib"
int cc_build_lib(const CcSource *sources, int count, char **out);
// A library in source form: #pragma comment(lib, "name") of a compiled unit asks for it; the hook
// gives its C files (count, 0 = it is a DLL), they are compiled as library units too.
extern int (*cc_source_lib)(const char *name, const CcSource **files);
extern int cc_pragma_manifest;      // #pragma comment(linker, "/MANIFEST"): the program asks for WinCC's manifest
extern int cc_pragma_subsystem;     // #pragma comment(linker, "/SUBSYSTEM:WINDOWS|CONSOLE"): 2 or 3, 0 = none
char *cc_strip_unused(const char *asm_text);    // strip.c: without the .gc pieces nothing reaches
char *cc_peephole(const char *asm_text);        // peephole.c: push/pop pairs into moves, jumps to the next line out
extern char cc_last_error[512];
extern const char *cc_last_loc;     // the error's place in the source text, or 0
