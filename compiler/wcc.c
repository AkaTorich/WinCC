// wcc: the WinCC compiler on the command line.
//   wcc hello.c                        -> hello.exe (console; WinMain without main: a windowed program)
//   wcc a.c b.c util.asm -o prog.exe   several sources, one program
//   wcc app.c app.rc                   with resources (an icon, a version, dialogs...; a .res of rc.exe too)
//   wcc --gui app.c                    a windowed program (no console)
//   wcc --dll lib.c                    -> lib.dll
//   -l user32   -I folder   -D NAME=1   -S (the assembly text to prog.s)   --run (start it after)
// Every program carries the library (runtime\) inside; it needs only the DLLs of Windows.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc.h"
#include "winlink.h"

#define WCC_VERSION "1.4.0"

// ---- the compiler's host hooks ----

static char *g_arena;
static size_t g_arena_used, g_arena_size;

void *cc_alloc(size_t size)
{
    size = (size + 15) & ~(size_t)15;
    if (!g_arena || g_arena_used + size > g_arena_size) {
        size_t chunk = size > (64u << 20) ? size : (64u << 20);
        g_arena = VirtualAlloc(0, chunk, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!g_arena) {
            fprintf(stderr, "wcc: не хватило памяти\n");
            ExitProcess(3);
        }
        g_arena_used = 0;
        g_arena_size = chunk;
    }
    void *p = g_arena + g_arena_used;
    g_arena_used += size;
    return p;
}

void cc_free_all(void) { g_arena = 0; }
void *cc_alloc_code(size_t size) { return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); }
void cc_print(const char *text) { fputs(text, stdout); }

// The library compiled into wcc itself is used only for strtod (tokenize.c); its hooks do nothing here.
const char *cc_file_read(const char *n, size_t *s) { (void)n; (void)s; return 0; }
int cc_file_write(const char *n, const char *d, size_t s) { (void)n; (void)d; (void)s; return 0; }
int cc_file_delete(const char *n) { (void)n; return 0; }
int cc_file_rename(const char *a, const char *b) { (void)a; (void)b; return 0; }
int cc_file_kind(const char *n) { (void)n; return 0; }
int cc_input_line(char *b, int c) { (void)b; (void)c; return -1; }
void cc_now(int t[6])               // __DATE__ and __TIME__ of the preprocessor
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    t[0] = st.wYear; t[1] = st.wMonth; t[2] = st.wDay;
    t[3] = st.wHour; t[4] = st.wMinute; t[5] = st.wSecond;
}

static void print_warning(const char *text) { fprintf(stderr, "%s\n", text); }
u64 cc_millis(void) { return GetTickCount64(); }
u64 cc_irq_off(void) { return 0; }
void cc_irq_restore(u64 f) { (void)f; }
void *cc_heap_more(size_t want, size_t *got) { (void)want; *got = 0; return 0; }

// ---- files: names are UTF-8 inside the compiler ----

static wchar_t *wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, 0, 0);
    wchar_t *w = cc_alloc(sizeof(wchar_t) * (size_t)(n + 1));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static char *utf8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
    char *s = cc_alloc((size_t)n + 1);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, 0, 0);
    return s;
}

// A source as UTF-8, whatever it was saved in: UTF-8 (with a BOM or without), UTF-16 (with a
// BOM, as Notepad saves "Unicode"), or -- not valid UTF-8 -- the Russian ANSI code page 1251.
static char *read_text(const char *name)
{
    HANDLE h = CreateFileW(wide(name), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD size = GetFileSize(h, 0), got = 0;
    char *buf = cc_alloc((size_t)size + 2);
    ReadFile(h, buf, size, &got, 0);
    CloseHandle(h);
    buf[got] = buf[got + 1] = 0;
    unsigned char *b = (unsigned char *)buf;
    if (got >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
        return buf + 3;             // a UTF-8 BOM is not part of the program
    if (got >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF))) {
        int n = (int)(got - 2) / 2;
        wchar_t *w = cc_alloc(sizeof(wchar_t) * (size_t)(n + 1));
        for (int i = 0; i < n; i++)
            w[i] = b[0] == 0xFF ? (wchar_t)(b[2 + 2 * i] | b[3 + 2 * i] << 8) : (wchar_t)(b[2 + 2 * i] << 8 | b[3 + 2 * i]);
        w[n] = 0;
        return utf8(w);
    }
    if (got && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, (int)got, 0, 0)) {
        int n = MultiByteToWideChar(1251, 0, buf, (int)got, 0, 0);
        wchar_t *w = cc_alloc(sizeof(wchar_t) * (size_t)(n + 1));
        MultiByteToWideChar(1251, 0, buf, (int)got, w, n);
        w[n] = 0;
        return utf8(w);
    }
    return buf;
}

// A file as it is (icons, pictures, .res): beside the source as named, then in the -I folders.
static char *g_inc[32];             // -I folders, then the WinCC include folder
static int g_ninc;

static const u8 *read_raw(const char *name, int *size)
{
    HANDLE h = CreateFileW(wide(name), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD n = GetFileSize(h, 0), got = 0;
    u8 *buf = cc_alloc((size_t)n + 1);
    ReadFile(h, buf, n, &got, 0);
    CloseHandle(h);
    *size = (int)got;
    return buf;
}

static const u8 *read_binary(const char *name, int *size)
{
    const u8 *d = read_raw(name, size);
    for (int i = 0; !d && i < g_ninc; i++)
        d = read_raw(cc_format("%s\\%s", g_inc[i], name), size);
    return d;
}

static int write_file(const char *name, const void *data, size_t size)
{
    HANDLE h = CreateFileW(wide(name), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD put = 0;
    BOOL ok = WriteFile(h, data, (DWORD)size, &put, 0);
    CloseHandle(h);
    return ok && put == size;
}

// #include "x.h": the name as it is (beside the source: the preprocessor already joined it), then
// the folders (-I, then WinCC's include); #include <x.h>: the folders first -- a windows.h of one's
// own in the current folder does not stand in for the system's -- and the current folder last
const char *cc_read_file(const char *name)
{
    char *t = cc_include_angle ? 0 : read_text(name);
    if (t)
        return t;
    for (int i = 0; i < g_ninc; i++) {
        t = read_text(cc_format("%s\\%s", g_inc[i], name));
        if (t)
            return t;
    }
    return cc_include_angle ? read_text(name) : 0;
}

// ---- the command line ----

static void usage(void)
{
    printf("WinCC %s — компилятор Си для Windows\n\n"
           "  wcc файл.c [ещё.c файл.asm ресурсы.rc ресурсы.res ...] [параметры]\n\n"
           "  -o имя        что получится (по умолчанию — имя первого файла .exe / .dll)\n"
           "  --console     консольная программа (так по умолчанию)\n"
           "  --gui         оконная программа, без консоли (так же, если есть WinMain и нет main)\n"
           "  --dll         библиотека DLL\n"
           "  --lib         своя библиотека .lib: программа берёт её так — wcc main.c моя.lib\n"
           "  -l имя        брать функции из этой DLL (user32, gdi32, свою mylib...)\n"
           "  -I папка      где ещё искать #include\n"
           "  -D ИМЯ[=знач] определить макрос\n"
           "  -S            сохранить ассемблерный текст рядом (.s)\n"
           "  -g            сведения для отладчика WinCC рядом с программой (.wdbg)\n"
           "  -O0           без оптимизации переменных: все в памяти (для отладки)\n"
           "  --stack МБ    размер стека (по умолчанию 8)\n"
           "  --run         собрать и сразу запустить\n", WCC_VERSION);
}

static char *exe_dir(void)
{
    wchar_t p[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(0, p, MAX_PATH * 2);
    while (n > 0 && p[n - 1] != L'\\') n--;
    p[n ? n - 1 : 0] = 0;
    return utf8(p);
}

static int exists(const char *path) { return GetFileAttributesW(wide(path)) != INVALID_FILE_ATTRIBUTES; }

static char *with_ext(const char *src, const char *ext)
{
    int n = (int)strlen(src), dot = n;
    for (int i = n - 1; i >= 0 && src[i] != '\\' && src[i] != '/'; i--)
        if (src[i] == '.') { dot = i; break; }
    return cc_join(cc_strndup(src, dot), "", ext);
}

static char *folder_of(const char *path)
{
    int n = (int)strlen(path);
    while (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    return n ? cc_strndup(path, n - 1) : cc_strndup(".", 1);
}

// ---- libraries in source form: lib\<name>\*.c next to wcc.exe (#pragma comment(lib, "wui")) ----

static char *g_home;                // WinCC's folder: include\, runtime\, lib\

static const char *g_source_names[16];  // asked-for names that are such libraries (not DLLs)
static int g_source_count;

static int is_source_lib(const char *name)
{
    for (int i = 0; i < g_source_count; i++)
        if (_stricmp(g_source_names[i], name) == 0)
            return 1;
    return 0;
}

static int source_lib(const char *name, const CcSource **files)
{
    char *dir = cc_format("%s\\lib\\%s", g_home, name);
    if (is_source_lib(name) || g_source_count >= 16 || !exists(cc_format("%s\\.", dir)))
        return 0;
    g_source_names[g_source_count++] = name;
    // its C files in name order, so every build is the same
    static CcSource list[64];
    int n = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wide(cc_format("%s\\*.c", dir)), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (n < 64)
                list[n++].name = cc_format("%s\\%s", dir, utf8(fd.cFileName));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    CcSource *own = cc_alloc(sizeof(CcSource) * (size_t)(n ? n : 1));
    for (int i = 0; i < n; i++) {
        int k = i;
        for (int j = i + 1; j < n; j++)
            if (strcmp(list[j].name, list[k].name) < 0) k = j;
        CcSource s = list[i]; list[i] = list[k]; list[k] = s;
        own[i].name = list[i].name;
        own[i].text = read_text(own[i].name);
        if (!own[i].text) own[i].text = "";
    }
    *files = own;
    return n;
}

// Which DLL has a name: -l first, then #pragma comment(lib) of the sources (known
// once they are compiled -- the first question comes after that), then the usual ones.
static const char **g_defaults;

static const char *dll_of(const char *name, int len)
{
    static int ready;
    if (!ready) {
        ready = 1;
        for (int i = 0; i < cc_pragma_lib_count; i++)
            if (!is_source_lib(cc_pragma_libs[i]))
                winlink_add(cc_pragma_libs[i]);
        for (int i = 0; g_defaults[i]; i++)
            winlink_add(g_defaults[i]);
    }
    if (len == 20 && !strncmp(name, "__C_specific_handler", 20))
        return "ntdll.dll";                     // the handler of __try (ntdll is not searched otherwise)
    return winlink_dll_of(name, len);
}

// ---- the programmer's own libraries (wcc --lib, cc.c: cc_build_lib) ----

static int ends_with_ci(const char *s, const char *ext)
{
    int n = (int)strlen(s), m = (int)strlen(ext);
    return n >= m && _stricmp(s + n - m, ext) == 0;
}

static char *full_path(const char *path)
{
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetFullPathNameW(wide(path), MAX_PATH * 2, buf, 0);
    return n && n < MAX_PATH * 2 ? utf8(buf) : (char *)path;
}

// A .lib named on the command line: its files go into *list (n of them), the names it uses into the
// #pragma comment(lib) list. 0 and a message: not a WinCC library (one of Visual Studio, say).
static int read_wcclib(const char *path, CcSource *list, int *n, int cap)
{
    int size;
    const u8 *data = read_binary(path, &size);
    if (!data) {
        fprintf(stderr, "wcc: нет файла %s\n", path);
        return 0;
    }
    const char *p = (const char *)data, *end = p + size;
    if (size < 9 || strncmp(p, "WCCLIB 1\n", 9)) {
        fprintf(stderr, "wcc: %s — не библиотека WinCC (wcc --lib); библиотеки Visual Studio он не читает\n", path);
        return 0;
    }
    p += 9;
    while (p < end) {
        const char *e = p;
        while (e < end && *e != '\n') e++;
        if (!strncmp(p, "uses ", 5)) {
            const char *name = cc_strndup(p + 5, (int)(e - p - 5));
            int known = 0;
            for (int i = 0; i < cc_pragma_lib_count; i++)
                if (!_stricmp(cc_pragma_libs[i], name)) known = 1;
            if (!known && cc_pragma_lib_count < 64)
                cc_pragma_libs[cc_pragma_lib_count++] = name;
            p = e + 1;
        } else if (!strncmp(p, "source ", 7)) {
            char *q;
            long bytes = strtol(p + 7, &q, 10);
            const char *name = cc_strndup(q + 1, (int)(e - q - 1));
            const char *text = e + 1;
            if (bytes < 0 || text + bytes > end || *n >= cap) {
                fprintf(stderr, "wcc: %s испорчен\n", path);
                return 0;
            }
            list[*n].name = name;
            list[(*n)++].text = cc_strndup(text, (int)bytes);
            p = text + bytes;
        } else
            p = e + 1;
    }
    return 1;
}

static int run_program(const char *exe)
{
    // the full path: with NoDefaultCurrentDirectoryInExePath set, Windows would not look in "."
    wchar_t full[MAX_PATH * 2];
    DWORD fn = GetFullPathNameW(wide(exe), MAX_PATH * 2, full, 0);
    wchar_t *cmd = wide(cc_format("\"%s\"", fn && fn < MAX_PATH * 2 ? utf8(full) : exe));
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    fflush(stdout);
    if (!CreateProcessW(0, cmd, 0, 0, TRUE, 0, 0, 0, &si, &pi)) {
        fprintf(stderr, "wcc: не запустить %s (код %lu)\n", exe, GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

int wmain(int argc, wchar_t **wargv)
{
    SetConsoleOutputCP(CP_UTF8);
    char **argv = cc_alloc(sizeof(char *) * (size_t)(argc + 1));
    for (int i = 0; i < argc; i++)
        argv[i] = utf8(wargv[i]);

    static CcSource src[512];
    static CcSource libsrc[512];       // the files of the .lib files named: library units, after the program's own
    static const char *libfiles[64];
    int nlibsrc = 0, nlibfile = 0, make_lib = 0;
    int nsrc = 0, dll = 0, subsystem = 0, show_asm = 0, run = 0;
    u64 stack = 0;
    const char *out = 0;
    static const char *libs[128];
    int nlib = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *next = i + 1 < argc ? argv[i + 1] : 0;
        if (!strcmp(a, "-h") || !strcmp(a, "--help") || !strcmp(a, "/?")) { usage(); return 0; }
        else if (!strcmp(a, "-v") || !strcmp(a, "--version")) { printf("WinCC %s\n", WCC_VERSION); return 0; }
        else if (!strcmp(a, "-o") && next) { out = next; i++; }
        else if (!strcmp(a, "--gui")) subsystem = 2;
        else if (!strcmp(a, "--console")) subsystem = 3;
        else if (!strcmp(a, "--dll")) dll = 1;
        else if (!strcmp(a, "--lib")) make_lib = 1;
        else if (!strcmp(a, "-S")) show_asm = 1;
        else if (!strcmp(a, "-g")) cc_debug_info = 1;
        else if (!strcmp(a, "-O0")) cc_no_regalloc = 1;
        else if (!strcmp(a, "-O") || !strcmp(a, "-O1") || !strcmp(a, "-O2")) cc_no_regalloc = 0;
        else if (!strcmp(a, "--run")) run = 1;
        else if (!strcmp(a, "--stack") && next) { stack = (u64)atoi(next) << 20; i++; }
        else if (!strncmp(a, "-l", 2)) { const char *l = a[2] ? a + 2 : next; if (!a[2]) i++; if (l && nlib < 128) libs[nlib++] = l; }
        else if (!strncmp(a, "-I", 2)) { const char *d = a[2] ? a + 2 : next; if (!a[2]) i++; if (d && g_ninc < 30) g_inc[g_ninc++] = (char *)d; }
        else if (!strncmp(a, "-D", 2)) {
            const char *d = a[2] ? a + 2 : next;
            if (!a[2]) i++;
            if (d && cc_user_define_count < 64) {
                const char *eq = strchr(d, '=');
                cc_user_define_names[cc_user_define_count] = eq ? cc_strndup(d, (int)(eq - d)) : d;
                cc_user_define_values[cc_user_define_count++] = eq ? eq + 1 : "1";
            }
        }
        else if (a[0] == '-') { fprintf(stderr, "wcc: неизвестный параметр %s (wcc --help)\n", a); return 2; }
        else if (ends_with_ci(a, ".lib")) {
            if (nlibfile < 64) libfiles[nlibfile++] = a;     // read after the options: -o may come later
        }
        else {
            if (nsrc >= 500) { fprintf(stderr, "wcc: слишком много файлов\n"); return 2; }
            const char *text = read_text(a);
            if (!text) { fprintf(stderr, "wcc: нет файла %s\n", a); return 2; }
            src[nsrc].name = make_lib ? full_path(a) : a;    // a library keeps full paths: the debugger finds them later
            src[nsrc++].text = text;
        }
    }
    if (!nsrc) { usage(); return 2; }
    if (!out)
        out = with_ext(src[0].name, make_lib ? ".lib" : dll ? ".dll" : ".exe");
    for (int i = 0; i < nlibfile; i++)
        if (!read_wcclib(libfiles[i], libsrc, &nlibsrc, 512)) return 2;

    // WinCC's own folders: include\ and runtime\ next to wcc.exe (or one level up while it is being built)
    char *home = exe_dir();
    if (!exists(cc_format("%s\\runtime\\runtime.c", home)))
        home = folder_of(home);
    char *rt = cc_format("%s\\runtime", home);
    if (!exists(cc_format("%s\\runtime.c", rt))) {
        fprintf(stderr, "wcc: не найдена библиотека %s\\runtime.c\n", rt);
        return 2;
    }
    g_inc[g_ninc++] = cc_format("%s\\include", home);
    g_home = home;
    cc_source_lib = source_lib;
    cc_warn_hook = print_warning;
    cc_read_binary = read_binary;

    // the library: compiled into every program after the program's own sources
    // (first the programmer's own .lib files, then WinCC's runtime): what nobody calls stays out
    int first_lib = nsrc;
    for (int i = 0; i < nlibsrc && nsrc < 500; i++)
        src[nsrc++] = libsrc[i];
    static const char *rt_files[] = { "runtime.c", "winhost.c", "start.c", "wingdi.c", "winfile.c", "winthread.c",
                                      "winlocale.c", "conio.c", "cmath.c", "rtasm.asm" };
    for (int i = 0; i < (int)(sizeof(rt_files) / sizeof(rt_files[0])); i++) {
        src[nsrc].name = cc_format("%s\\%s", rt, rt_files[i]);
        src[nsrc].text = read_text(src[nsrc].name);
        if (!src[nsrc].text) { fprintf(stderr, "wcc: нет файла библиотеки %s\n", src[nsrc].name); return 2; }
        nsrc++;
    }

    // DLLs: the program's own folder first (its own DLLs), then System32
    winlink_add_dir(wide(folder_of(src[0].name)));
    winlink_add_dir(wide(folder_of(out)));
    for (int i = 0; i < nlib; i++)
        winlink_add(libs[i]);
    static const char *defaults[] = { "kernel32", "user32", "gdi32", "comdlg32", "shell32", "advapi32",
                                      "comctl32", "ole32", "winmm", "ws2_32", "version", 0 };

    cc_target_win = 1;
    if (make_lib) {
        // the library is the program's own sources only; the runtime and other .lib files join each program that takes it
        char *lib_text;
        DWORD l0 = GetTickCount();
        if (!cc_build_lib(src, first_lib, &lib_text)) {
            fprintf(stderr, "%s\n", cc_last_error);
            return 1;
        }
        if (!write_file(out, lib_text, strlen(lib_text))) {
            fprintf(stderr, "wcc: не записать %s\n", out);
            return 1;
        }
        printf("Готово: %s — библиотека WinCC, файлов %d, %llu байт, %lu мс\n", out, first_lib,
               (unsigned long long)strlen(lib_text), GetTickCount() - l0);
        return 0;
    }
    CcWinTarget t = { 0 };
    t.exe = !dll;
    t.subsystem = dll ? 3 : subsystem;
    t.stack_reserve = stack;
    t.dll_of = dll_of;
    {
        const char *base = out;
        for (const char *p = out; *p; p++)
            if (*p == '\\' || *p == '/') base = p + 1;
        t.dll_name = base;
    }
    g_defaults = defaults;

    u8 *image;
    u64 size;
    char *asm_text = 0;
    DWORD t0 = GetTickCount();
    if (!cc_build_win(src, nsrc, first_lib, &t, &image, &size, &asm_text)) {
        fprintf(stderr, "%s\n", cc_last_error);
        const char *miss = winlink_missing();
        if (*miss)
            fprintf(stderr, "wcc: не найдены библиотеки: %s\n", miss);
        if (show_asm && asm_text)
            write_file(with_ext(out, ".s"), asm_text, strlen(asm_text));
        return 1;
    }
    if (show_asm)
        write_file(with_ext(out, ".s"), asm_text, strlen(asm_text));
    if (!write_file(out, image, (size_t)size)) {
        fprintf(stderr, "wcc: не записать %s (программа ещё запущена?)\n", out);
        return 1;
    }
    // -g: the debugger's file next to the program; without -g an old one would lie about the new code
    const char *wdbg = with_ext(out, ".wdbg");
    if (cc_debug_info && cc_win_debug_text)
        write_file(wdbg, cc_win_debug_text, strlen(cc_win_debug_text));
    else
        DeleteFileW(wide(wdbg));
    printf("Готово: %s — %s, %llu байт, %lu мс\n", out,
           dll ? "библиотека DLL" : t.subsystem == 2 ? "оконная программа" : "консольная программа",
           (unsigned long long)size, GetTickCount() - t0);
    if (run && !dll)
        return run_program(out);
    return 0;
}
