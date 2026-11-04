// The start of a Windows program (WinCC). The compiler adds a tiny entry
// (cc.c, win_entry_stub) that calls __wcc_startup with the program's own
// entry: main, wmain, WinMain or wWinMain. Here the command line becomes
// argc/argv (UTF-8, by the rules of the Microsoft C library), CPU faults
// become C signals, then the program runs and the process ends with its code.
// A DLL comes in through __wcc_dllentry: the library is made ready and the
// DLL's own DllMain, if it has one, is called.
#include "rtwin.h"

typedef void *HANDLE;
typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned short WCHAR;

WCHAR *GetCommandLineW(void);
HANDLE GetModuleHandleW(const WCHAR *name);
BOOL GetModuleHandleExW(DWORD flags, const WCHAR *name, HANDLE *module);
void *SetUnhandledExceptionFilter(void *filter);
BOOL SetThreadStackGuarantee(DWORD *size);
int WideCharToMultiByte(unsigned cp, DWORD flags, const WCHAR *s, int n, char *out, int cap, const char *def, BOOL *used);
void ExitProcess(unsigned code);

void *rt_malloc(size_t n);
int  rt_fflush(void *f);
void rt_heap_reset(void *arena, size_t size);
int  rt_run_main_args(int (*entry)(void), int argc, char **argv);
void rt_lib_init(void);
int  rt_signal_caught(int sig);
void rt_signal_call(int sig);

enum { ENTRY_MAIN, ENTRY_WMAIN, ENTRY_WINMAIN, ENTRY_WWINMAIN };

static int g_kind;
static void *g_user;
static int g_argc;
static char **g_argv;
static WCHAR **g_wargv;
static WCHAR *g_wtail;               // the command line after the program's name
static char *g_tail;

// ---- the command line ----

static int is_blank(WCHAR c) { return c == ' ' || c == '\t'; }

// One argument from *pp into out (if not 0), by the Microsoft rules: 2n backslashes
// and a quote are n backslashes and a quote that opens or closes; 2n+1 of them are
// n backslashes and a quote as it is; "" inside quotes is a quote. Its length.
static int next_arg(const WCHAR **pp, WCHAR *out)
{
    const WCHAR *p = *pp;
    int n = 0, quoted = 0;
    while (*p && (quoted || !is_blank(*p))) {
        int bs = 0;
        while (*p == '\\') { bs++; p++; }
        if (*p == '"') {
            for (int i = 0; i < bs / 2; i++) { if (out) out[n] = '\\'; n++; }
            if (bs % 2) { if (out) out[n] = '"'; n++; p++; }
            else if (quoted && p[1] == '"') { if (out) out[n] = '"'; n++; p += 2; }
            else { quoted = !quoted; p++; }
        } else {
            for (int i = 0; i < bs; i++) { if (out) out[n] = '\\'; n++; }
            if (*p && (quoted || !is_blank(*p))) { if (out) out[n] = *p; n++; p++; }
        }
    }
    *pp = p;
    return n;
}

static char *utf8_of(const WCHAR *w, int n)
{
    int k = WideCharToMultiByte(65001, 0, w, n, 0, 0, 0, 0);
    char *s = rt_malloc((size_t)k + 1);
    if (k) WideCharToMultiByte(65001, 0, w, n, s, k, 0, 0);
    s[k] = 0;
    return s;
}

static void split_command_line(void)
{
    const WCHAR *cmd = GetCommandLineW();
    const WCHAR *p = cmd;
    int cap = 8;
    g_wargv = rt_malloc(sizeof(WCHAR *) * (size_t)cap);
    g_argc = 0;
    // the program's name: up to the closing quote, or to a blank; no backslash rules
    const WCHAR *s = p;
    if (*p == '"') {
        s = ++p;
        while (*p && *p != '"') p++;
    } else {
        while (*p && !is_blank(*p)) p++;
    }
    int len = (int)(p - s);
    WCHAR *a = rt_malloc(((size_t)len + 1) * sizeof(WCHAR));
    for (int i = 0; i < len; i++) a[i] = s[i];
    a[len] = 0;
    g_wargv[g_argc++] = a;
    if (*p == '"') p++;
    while (is_blank(*p)) p++;
    g_wtail = (WCHAR *)p;
    while (*p) {
        const WCHAR *q = p;
        int n = next_arg(&q, 0);
        WCHAR *arg = rt_malloc(((size_t)n + 1) * sizeof(WCHAR));
        next_arg(&p, arg);
        arg[n] = 0;
        if (g_argc + 2 > cap) {
            cap *= 2;
            WCHAR **bigger = rt_malloc(sizeof(WCHAR *) * (size_t)cap);
            for (int i = 0; i < g_argc; i++) bigger[i] = g_wargv[i];
            g_wargv = bigger;
        }
        g_wargv[g_argc++] = arg;
        while (is_blank(*p)) p++;
    }
    g_wargv[g_argc] = 0;
    g_argv = rt_malloc(sizeof(char *) * ((size_t)g_argc + 1));
    for (int i = 0; i < g_argc; i++)
        g_argv[i] = utf8_of(g_wargv[i], -1);
    g_argv[g_argc] = 0;
    g_tail = utf8_of(g_wtail, -1);
}

// ---- CPU faults: C signals ----

static unsigned char *g_image;
static u64 g_image_size;
static volatile int g_fault_sig;
static volatile DWORD g_fault_code;

static const char *fault_name(DWORD code)
{
    switch (code) {
    case 0xC0000094: return "деление на ноль";
    case 0xC0000095: return "переполнение при делении";
    case 0xC000001D: case 0xC0000096: return "неверная инструкция";
    case 0xC0000005: return "обращение к чужой памяти";
    default: return "сбой";
    }
}

static void fault_trampoline(void)
{
    if (rt_signal_caught(g_fault_sig))
        rt_signal_call(g_fault_sig);        // the program's handler (it may longjmp away)
    rt_fflush(0);
    cc_print("\nпрограмма остановлена: ");
    cc_print(fault_name(g_fault_code));
    cc_print("\n");
    ExitProcess(g_fault_code);
}

typedef struct { DWORD *record; unsigned char *context; } FaultInfo;   // EXCEPTION_POINTERS

static int on_fault(FaultInfo *e)
{
    u64 *rip = (u64 *)(e->context + 0xF8);  // CONTEXT.Rip
    u64 *rsp = (u64 *)(e->context + 0x98);  // CONTEXT.Rsp
    if (*rip < (u64)g_image || *rip >= (u64)g_image + g_image_size)
        return 0;                           // not in this program: Windows and the system's code decide
    DWORD code = e->record[0];
    int sig;
    switch (code) {
    case 0xC0000094: case 0xC0000095: sig = 8; break;
    case 0xC000001D: case 0xC0000096: sig = 4; break;
    case 0xC0000005: sig = 11; break;
    case 0xC00000FD:
        // the stack is used up: no handler can run on it -- say so (on the room SetThreadStackGuarantee
        // kept) and stop
        rt_fflush(0);
        cc_print("\nпрограмма остановлена: переполнение стека (слишком глубокая рекурсия или слишком большой "
                 "массив внутри функции; больше стека даёт ключ wcc --stack МБ)\n");
        ExitProcess(code);
        return 0;
    default: return 0;
    }
    g_fault_sig = sig;
    g_fault_code = code;
    *rsp = ((*rsp - 512) & ~(u64)15) - 8;   // as if called, below the faulting frame
    *rip = (u64)fault_trampoline;
    return -1;                              // EXCEPTION_CONTINUE_EXECUTION
}

// The last word on an exception: only one no __try took (frames are unwound by .pdata)
typedef long (*TopFilter)(FaultInfo *e);
static TopFilter g_prev_filter;

static long on_unhandled(FaultInfo *e)
{
    int r = on_fault(e);
    if (r)
        return r;
    return g_prev_filter ? g_prev_filter(e) : 0;        // another module's: its own filter decides
}

static void watch_faults(void)
{
    HANDLE h = 0;
    // FROM_ADDRESS | UNCHANGED_REFCOUNT: the module this code is in (the exe or the DLL)
    if (!GetModuleHandleExW(4 | 2, (const WCHAR *)(void *)watch_faults, &h) || !h)
        return;
    g_image = (unsigned char *)h;
    unsigned lfanew = *(unsigned *)(g_image + 0x3C);
    g_image_size = *(unsigned *)(g_image + lfanew + 24 + 56);     // OptionalHeader.SizeOfImage
    TopFilter prev = (TopFilter)SetUnhandledExceptionFilter((void *)on_unhandled);
    if (prev != (TopFilter)on_unhandled)
        g_prev_filter = prev;
    DWORD keep = 64 * 1024;                 // room on the stack for the handler when the stack runs out
    SetThreadStackGuarantee(&keep);
}

// ---- the program ----

static int call_user(void)
{
    switch (g_kind) {
    case ENTRY_WMAIN:
        return ((int (*)(int, WCHAR **))g_user)(g_argc, g_wargv);
    case ENTRY_WINMAIN:
        return ((int (*)(HANDLE, HANDLE, char *, int))g_user)(GetModuleHandleW(0), 0, g_tail, 10);
    case ENTRY_WWINMAIN:
        return ((int (*)(HANDLE, HANDLE, WCHAR *, int))g_user)(GetModuleHandleW(0), 0, g_wtail, 10);
    default:
        return ((int (*)(int, char **, char **))g_user)(g_argc, g_argv, 0);
    }
}

void __wcc_startup(int kind, void *user)
{
    g_kind = kind;
    g_user = user;
    rt_heap_reset(0, 0);                    // the heap takes memory from Windows as it grows
    split_command_line();
    watch_faults();
    int rc = rt_run_main_args(call_user, g_argc, g_argv);
    ExitProcess((unsigned)rc);
}

// DllMain of a WinCC DLL: reason 1 = loaded into a process, 0 = unloaded.
BOOL __wcc_dllentry(HANDLE dll, DWORD reason, void *reserved, void *user)
{
    if (reason == 1) {
        rt_heap_reset(0, 0);
        rt_lib_init();
        watch_faults();
    }
    BOOL ok = user ? ((BOOL (*)(HANDLE, DWORD, void *))user)(dll, reason, reserved) : 1;
    if (reason == 0)
        rt_fflush(0);                       // files the DLL changed are written out
    return ok;
}
