// Windows test host for the DyOS compiler.
//   cctest file.c [more.c file.asm ...]   compile and run main()
//   cctest -S file.c                      print the generated assembly
//   cctest --exe out.exe a.c b.c          build a ready DyOS program (PE, exports main)
//   cctest --run out.exe                  run such a program as the system does (map, relocate, imports)
//   cctest --prof --run out.exe           the same with a sampling profile of its functions at the end
//   cctest --tests <dir>                  run every <dir>/*.c, each main() returns the failure count
//                                         (a first line "// with: b.part c.part" adds <dir>/b.part, <dir>/c.part
//                                         as more sources, each its own translation unit)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "cc.h"
#include "peformat.h"

// ---- host interface ----

static char *g_arena;
static size_t g_arena_used, g_arena_size;

void *cc_alloc(size_t size)
{
    size = (size + 15) & ~(size_t)15;
    if (!g_arena || g_arena_used + size > g_arena_size) {
        size_t chunk = size > (64u << 20) ? size : (64u << 20);
        g_arena = VirtualAlloc(0, chunk, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!g_arena) {
            fprintf(stderr, "out of memory\n");
            ExitProcess(3);
        }
        g_arena_used = 0;
        g_arena_size = chunk;
    }
    void *p = g_arena + g_arena_used;
    g_arena_used += size;
    return p;       // VirtualAlloc memory is already zero
}

void cc_free_all(void)
{
    g_arena = 0;
}

void *cc_alloc_code(size_t size)
{
    return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

void cc_print(const char *text)
{
    fputs(text, stdout);
}

static char g_dir[MAX_PATH];

const char *cc_read_file(const char *name)
{
    char path[MAX_PATH * 2];
    FILE *f = fopen(name, "rb");
    if (!f && g_dir[0]) {
        snprintf(path, sizeof(path), "%s\\%s", g_dir, name);
        f = fopen(path, "rb");
    }
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = cc_alloc((size_t)n + 1);
    fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

// ---- the C library's hooks ----
// A program's files (the system's paths, "desktop/a.txt") live flat in
// obj\files next to cctest.exe: '/', '\' and ':' become '_'.

static int host_path(const char *name, wchar_t *out, int cap)
{
    wchar_t dir[MAX_PATH];
    DWORD n = GetModuleFileNameW(0, dir, MAX_PATH);
    while (n > 0 && dir[n - 1] != L'\\') n--;
    dir[n] = 0;
    wcscat(dir, L"obj");
    CreateDirectoryW(dir, 0);
    wcscat(dir, L"\\files");
    CreateDirectoryW(dir, 0);
    char flat[600];
    int i = 0;
    for (; name[i] && i < (int)sizeof(flat) - 1; i++)
        flat[i] = name[i] == '/' || name[i] == '\\' || name[i] == ':' ? '_' : name[i];
    flat[i] = 0;
    wchar_t wname[600];
    if (!MultiByteToWideChar(CP_UTF8, 0, flat, -1, wname, 600))
        return 0;
    if ((int)(wcslen(dir) + wcslen(wname) + 2) > cap)
        return 0;
    swprintf(out, cap, L"%ls\\%ls", dir, wname);
    return 1;
}

const char *cc_file_read(const char *name, size_t *size)
{
    static char *last;
    wchar_t path[1024];
    if (!host_path(name, path, 1024))
        return 0;
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    free(last);                 // the library copies what it gets at once
    last = malloc((size_t)n + 1);
    size_t got = fread(last, 1, (size_t)n, f);
    fclose(f);
    last[got] = 0;
    if (size) *size = got;
    return last;
}

int cc_file_write(const char *name, const char *data, size_t size)
{
    wchar_t path[1024];
    if (!host_path(name, path, 1024))
        return 0;
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    size_t w = size ? fwrite(data, 1, size, f) : 0;
    fclose(f);
    return w == size;
}

int cc_file_delete(const char *name)
{
    wchar_t path[1024];
    return host_path(name, path, 1024) && DeleteFileW(path);
}

int cc_file_rename(const char *from, const char *to)
{
    wchar_t a[1024], b[1024];
    // replaces an existing target, as POSIX rename does
    return host_path(from, a, 1024) && host_path(to, b, 1024) && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING);
}

// Folders are flat here too: "dir/a.txt" is obj\files\dir_a.txt, so "dir"
// is a folder when a file "dir_..." is there (the marker "dir/.dir" too).
int cc_file_kind(const char *name)
{
    wchar_t path[1024];
    if (!host_path(name, path, 1020))
        return 0;
    DWORD a = GetFileAttributesW(path);
    if (a != INVALID_FILE_ATTRIBUTES)
        return a & FILE_ATTRIBUTE_DIRECTORY ? 2 : 1;
    wcscat(path, L"_*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(path, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    FindClose(h);
    return 2;
}

int cc_input_line(char *buf, int cap)
{
    fflush(stdout);
    if (!fgets(buf, cap, stdin))
        return -1;
    return (int)strlen(buf);
}

void cc_now(int t[6])
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    t[0] = st.wYear; t[1] = st.wMonth; t[2] = st.wDay;
    t[3] = st.wHour; t[4] = st.wMinute; t[5] = st.wSecond;
}

u64 cc_irq_off(void) { return 0; }      // one thread here: the heap lock needs nothing more
// The heap grows past the 128 MB arena below by pieces of at least 64 MB (as the system gives
// a program more pages): big programs (the ESP32-S3 emulator holds 16 MB flash, 8 MB PSRAM...).
void *cc_heap_more(size_t want, size_t *got)
{
    size_t n = want > ((size_t)64 << 20) ? (want + 0xFFFF) & ~(size_t)0xFFFF : (size_t)64 << 20;
    void *p = VirtualAlloc(0, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    *got = p ? n : 0;
    return p;
}
void cc_irq_restore(u64 f) { (void)f; }

u64 cc_millis(void)
{
    return GetTickCount64();
}

// ---- CPU faults in a program: C signals, as in the system ----
// A fault while a program runs becomes SIGFPE / SIGILL / SIGSEGV: the
// faulting thread is sent to fault_trampoline, which runs the program's
// handler; if there is none, or it returns, the program is stopped.

typedef struct { unsigned long long regs[10]; unsigned char xmm[160]; } CcJmpBuf;
int  cc_setjmp(CcJmpBuf *buf);
__declspec(noreturn) void cc_longjmp(CcJmpBuf *buf, int value);

static CcJmpBuf g_run_jmp;
static volatile int g_running, g_fault_sig;
static volatile DWORD g_fault_code;

static const char *fault_name(DWORD code)
{
    switch (code) {
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "деление на ноль";
    case EXCEPTION_INT_OVERFLOW: return "переполнение";
    case EXCEPTION_ILLEGAL_INSTRUCTION: case EXCEPTION_PRIV_INSTRUCTION: return "неверная инструкция";
    case EXCEPTION_ACCESS_VIOLATION: return "обращение к чужой памяти";
    default: return "сбой";
    }
}

static void fault_trampoline(void)
{
    if (rt_signal_caught(g_fault_sig))
        rt_signal_call(g_fault_sig);
    cc_longjmp(&g_run_jmp, 1);
}

static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    if (!g_running)
        return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = e->ExceptionRecord->ExceptionCode;
    int sig;
    switch (code) {
    case EXCEPTION_INT_DIVIDE_BY_ZERO: case EXCEPTION_INT_OVERFLOW: sig = 8; break;
    case EXCEPTION_ILLEGAL_INSTRUCTION: case EXCEPTION_PRIV_INSTRUCTION: sig = 4; break;
    case EXCEPTION_ACCESS_VIOLATION: sig = 11; break;
    default: return EXCEPTION_CONTINUE_SEARCH;
    }
    g_fault_sig = sig;
    g_fault_code = code;
    CONTEXT *c = e->ContextRecord;
    c->Rsp = ((c->Rsp - 512) & ~(DWORD64)15) - 8;      // as if called, below the faulting frame
    c->Rip = (DWORD64)fault_trampoline;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// ---- running ----

static char g_heap[128 << 20];                     // 128 MB: the SchemeEditor tests read the whole library and catalog

#define MAX_SOURCES 256

static int run_files(const char **names, int count, int show_asm, int quiet)
{
    static CcSource src[MAX_SOURCES];
    if (count > MAX_SOURCES) { printf("больше %d файлов (%d)\n", MAX_SOURCES, count); return 1; }
    for (int i = 0; i < count; i++) {
        src[i].name = names[i];
        src[i].text = cc_read_file(names[i]);
        if (!src[i].text) {
            printf("нет файла %s\n", names[i]);
            return -1;
        }
    }
    CcImage img;
    char *text = 0;
    if (!cc_build(src, count, cc_runtime_symbols(), &img, &text)) {
        printf("ОШИБКА: %s\n", cc_last_error);
        return -1;
    }
    if (show_asm) {
        fputs(text, stdout);
        return 0;
    }
    if (!img.entry) {
        printf("нет функции main\n");
        return -1;
    }
    if (!quiet)
        printf("[код %zu байт, данные %zu байт]\n", img.code_size, img.data_size);
    rt_heap_reset(g_heap, sizeof(g_heap));
    int (*entry)(void) = (int (*)(void))img.entry;
    int rc;
    g_running = 1;
    if (!cc_setjmp(&g_run_jmp)) {
        rc = rt_run_main(entry);
    } else {                    // a fault the program did not handle
        fflush(stdout);
        printf("программа остановлена: %s (0x%08lx)\n", fault_name(g_fault_code), g_fault_code);
        rc = -2;
    }
    g_running = 0;
    fflush(stdout);
    return rc;
}

// Runs a ready DyOS program (a PE built by --exe) as the system does: the image is mapped,
// relocated, its imports (module "dynasty") resolved against the C library, then main runs.
// The same exe as in the OS, without compiling it again for every run.
// ---- sampling profiler for --run (cctest --prof --run out.exe): every ~1 ms the program's thread is
// stopped and its RIP and frame chain (rbp) noted; at the end the top functions (by .dydbg) are printed ----
#define PROF_MAX (1 << 20)
static unsigned char *g_prof_base;
static unsigned g_prof_size;
static HANDLE g_prof_thread;
static volatile int g_prof_stop;
static unsigned *g_prof_self, *g_prof_incl;            // per sample: RVA (self) and per-function inclusive
static unsigned g_prof_n, g_prof_other;
static unsigned g_prof_chain[PROF_MAX / 16][16];

static void prof_report(void);

static DWORD WINAPI prof_loop(LPVOID p)
{
    (void)p;
    while (!g_prof_stop && g_prof_n < PROF_MAX / 16) {
        Sleep(1);
        if (SuspendThread(g_prof_thread) == (DWORD)-1) break;
        CONTEXT c;
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (GetThreadContext(g_prof_thread, &c)) {
            unsigned *ch = g_prof_chain[g_prof_n];
            memset(ch, 0xFF, sizeof g_prof_chain[0]);
            int k = 0;
            unsigned long long rip = c.Rip;
            if (rip >= (unsigned long long)g_prof_base && rip < (unsigned long long)g_prof_base + g_prof_size) ch[k++] = (unsigned)(rip - (unsigned long long)g_prof_base);
            else { ch[k++] = 0xFFFFFFFEu; g_prof_other++; }
            unsigned long long bp = c.Rbp;
            for (int d = 0; d < 40 && k < 16 && bp; d++) {
                MEMORY_BASIC_INFORMATION mi;
                if (!VirtualQuery((void *)bp, &mi, sizeof mi) || mi.State != MEM_COMMIT) break;
                unsigned long long ret = ((unsigned long long *)bp)[1], nbp = ((unsigned long long *)bp)[0];
                if (ret >= (unsigned long long)g_prof_base && ret < (unsigned long long)g_prof_base + g_prof_size) ch[k++] = (unsigned)(ret - (unsigned long long)g_prof_base - 1);
                if (nbp <= bp) break;
                bp = nbp;
            }
            g_prof_n++;
        }
        ResumeThread(g_prof_thread);
    }
    return 0;
}

static const DyDbgFunc *prof_func(const DyDbgHeader *h, unsigned rva)
{
    const DyDbgFunc *f = (const DyDbgFunc *)((const char *)h + h->funcs_off);
    int lo = 0, hi = (int)h->nfuncs - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (rva < f[mid].start) hi = mid - 1; else if (rva >= f[mid].end) lo = mid + 1; else return &f[mid];
    }
    return 0;
}

static void prof_report(void)
{
    const PeDos *dos = (const PeDos *)g_prof_base;
    const PeFileHeader *fh = (const PeFileHeader *)(g_prof_base + dos->e_lfanew + 4);
    const PeOpt64 *op = (const PeOpt64 *)(fh + 1);
    const PeSection *sec = (const PeSection *)((const unsigned char *)op + fh->SizeOfOptionalHeader);
    const DyDbgHeader *h = 0;
    for (int i = 0; i < fh->NumberOfSections; i++)
        if (!strncmp((const char *)sec[i].Name, ".dydbg", 6)) h = (const DyDbgHeader *)(g_prof_base + sec[i].VirtualAddress);
    if (!h || h->magic != DYDBG_MAGIC) { printf("[проф: нет .dydbg]\n"); return; }
    unsigned nf = h->nfuncs;
    unsigned *self = calloc(nf + 1, sizeof(unsigned)), *incl = calloc(nf + 1, sizeof(unsigned));
    const DyDbgFunc *f0 = (const DyDbgFunc *)((const char *)h + h->funcs_off);
    const char *strs = (const char *)h + h->strings_off;
    unsigned char *seen = calloc(nf + 1, 1);
    for (unsigned s = 0; s < g_prof_n; s++) {
        unsigned *ch = g_prof_chain[s];
        const DyDbgFunc *f = ch[0] == 0xFFFFFFFEu ? 0 : prof_func(h, ch[0]);
        if (f) self[f - f0]++;
        memset(seen, 0, nf + 1);
        for (int k = 0; k < 16 && ch[k] != 0xFFFFFFFFu; k++) {
            if (ch[k] == 0xFFFFFFFEu) continue;
            const DyDbgFunc *g = prof_func(h, ch[k]);
            if (g && !seen[g - f0]) { seen[g - f0] = 1; incl[g - f0]++; }
        }
    }
    printf("[проф: выборок %u, вне программы (рантайм) %u]\n", g_prof_n, g_prof_other);
    for (int pass = 0; pass < 2; pass++) {
        unsigned *v = pass ? incl : self;
        printf(pass ? "[проф: с вызванными]\n" : "[проф: сама функция]\n");
        for (int r = 0; r < 25; r++) {
            unsigned best = 0, bi = 0;
            for (unsigned i = 0; i < nf; i++) if (v[i] > best) { best = v[i]; bi = i; }
            if (!best) break;
            printf("  %6.2f%%  %s\n", 100.0 * best / (g_prof_n ? g_prof_n : 1), strs + f0[bi].name);
            v[bi] = 0;
        }
    }
    free(self); free(incl); free(seen);
}
static int g_prof_on;

static int run_pe(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { printf("нет файла %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); printf("не прочитать %s\n", path); return -1; }
    fclose(f);
    if (n < 0x200 || d[0] != 'M' || d[1] != 'Z') { printf("%s — не PE\n", path); return -1; }
    const PeDos *dos = (const PeDos *)d;
    long lfanew = dos->e_lfanew;
    const PeFileHeader *fh = (const PeFileHeader *)(d + dos->e_lfanew + 4);
    const PeOpt64 *op = (const PeOpt64 *)(fh + 1);
    const PeSection *sec = (const PeSection *)((const unsigned char *)op + fh->SizeOfOptionalHeader);
    unsigned char *base = VirtualAlloc(0, op->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!base) { printf("нет памяти под образ\n"); return -1; }
    memcpy(base, d, op->SizeOfHeaders < (unsigned)n ? op->SizeOfHeaders : (size_t)n);
    for (int i = 0; i < fh->NumberOfSections; i++)
        if (sec[i].SizeOfRawData && sec[i].PointerToRawData + sec[i].SizeOfRawData <= (unsigned)n)
            memcpy(base + sec[i].VirtualAddress, d + sec[i].PointerToRawData, sec[i].SizeOfRawData);
    // the headers are read from the mapped copy: the file buffer goes away now
    fh = (const PeFileHeader *)(base + lfanew + 4);
    op = (const PeOpt64 *)(fh + 1);
    free(d);
    long long delta = (long long)((unsigned long long)base - op->ImageBase);
    const PeDataDir *rd = &op->DataDirectory[PE_DIR_BASERELOC];
    for (unsigned off = 0; rd->Size && off + 8 <= rd->Size;) {
        const PeRelocBlock *b = (const PeRelocBlock *)(base + rd->VirtualAddress + off);
        if (b->BlockSize < 8) break;
        const unsigned short *e = (const unsigned short *)(b + 1);
        for (unsigned k = 0; k < (b->BlockSize - 8) / 2; k++)
            if ((e[k] >> 12) == PE_REL_DIR64)
                *(unsigned long long *)(base + b->PageRVA + (e[k] & 0xFFF)) += (unsigned long long)delta;
        off += b->BlockSize;
    }
    const PeDataDir *id = &op->DataDirectory[PE_DIR_IMPORT];
    if (id->Size) {
        const PeImportDesc *imp = (const PeImportDesc *)(base + id->VirtualAddress);
        for (; imp->Name; imp++) {
            unsigned long long *iat = (unsigned long long *)(base + imp->FirstThunk);
            const unsigned long long *ilt = (const unsigned long long *)(base + imp->OriginalFirstThunk);
            for (int k = 0; ilt[k]; k++) {
                const char *name = (const char *)(base + (unsigned)ilt[k] + 2);
                void *addr = 0;
                for (const CcSymbol *s = cc_runtime_symbols(); s->name; s++)
                    if (!strcmp(s->name, name)) { addr = s->addr; break; }
                if (!addr) { printf("нет функции %s для %s\n", name, path); return -1; }
                iat[k] = (unsigned long long)addr;
            }
        }
    }
    int (*entry)(void) = 0;
    const PeDataDir *ed = &op->DataDirectory[PE_DIR_EXPORT];
    if (ed->Size) {
        const PeExportDir *ex = (const PeExportDir *)(base + ed->VirtualAddress);
        const unsigned *names = (const unsigned *)(base + ex->AddressOfNames);
        const unsigned short *ords = (const unsigned short *)(base + ex->AddressOfNameOrdinals);
        const unsigned *funcs = (const unsigned *)(base + ex->AddressOfFunctions);
        for (unsigned i = 0; i < ex->NumberOfNames; i++)
            if (!strcmp((const char *)(base + names[i]), "main"))
                entry = (int (*)(void))(base + funcs[ords[i]]);
    }
    if (!entry) { printf("в %s нет main\n", path); return -1; }
    rt_heap_reset(g_heap, sizeof(g_heap));
    int rc;
    HANDLE prof = 0;
    if (g_prof_on) {
        g_prof_base = base; g_prof_size = op->SizeOfImage;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_prof_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
        g_prof_stop = 0;
        prof = CreateThread(0, 0, prof_loop, 0, 0, 0);
    }
    g_running = 1;
    if (!cc_setjmp(&g_run_jmp)) {
        rc = rt_run_main(entry);
    } else {
        fflush(stdout);
        printf("программа остановлена: %s (0x%08lx)\n", fault_name(g_fault_code), g_fault_code);
        rc = -2;
    }
    g_running = 0;
    if (prof) { g_prof_stop = 1; WaitForSingleObject(prof, INFINITE); CloseHandle(prof); prof_report(); }
    fflush(stdout);
    return rc;
}

// Compiles sources to a .dll, writes it, then loads it with the real Windows
// loader and calls a few exports — a strong check that our PE is well-formed.
static int build_and_load_dll(const char **names, int count)
{
    static CcSource src[MAX_SOURCES];
    if (count > MAX_SOURCES) { printf("больше %d файлов (%d)\n", MAX_SOURCES, count); return 1; }
    for (int i = 0; i < count; i++) {
        src[i].name = names[i];
        src[i].text = cc_read_file(names[i]);
        if (!src[i].text) { printf("нет файла %s\n", names[i]); return 1; }
    }
    unsigned char *dll;
    unsigned long long size;
    if (!cc_build_dll(src, count, cc_runtime_symbols(), "test.dll", &dll, &size)) {
        printf("ОШИБКА: %s\n", cc_last_error);
        return 1;
    }
    FILE *f = fopen("out.dll", "wb");
    fwrite(dll, 1, (size_t)size, f);
    fclose(f);
    printf("Записан out.dll: %llu байт\n", size);

    char full[MAX_PATH];
    GetFullPathNameA("out.dll", MAX_PATH, full, 0);
    HMODULE h = LoadLibraryA(full);
    if (!h) {
        printf("Windows LoadLibrary не смог загрузить (код %lu)\n", GetLastError());
        return 1;
    }
    printf("Windows загрузил нашу DLL по адресу %p\n", (void *)h);
    {   // the export table as Windows sees it
        const unsigned char *b = (const unsigned char *)h;
        const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(b + ((const IMAGE_DOS_HEADER *)b)->e_lfanew);
        DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        if (rva) {
            const IMAGE_EXPORT_DIRECTORY *ed = (const IMAGE_EXPORT_DIRECTORY *)(b + rva);
            const DWORD *names = (const DWORD *)(b + ed->AddressOfNames);
            printf("  экспорт (%lu):", ed->NumberOfNames);
            for (DWORD i = 0; i < ed->NumberOfNames; i++)
                printf(" %s", (const char *)(b + names[i]));
            printf("\n");
        }
    }
    static const char *fns[] = { "add", "mul", "fib", "square", 0 };
    for (int i = 0; fns[i]; i++) {
        typedef int (*Fn)(int, int);
        Fn fn = (Fn)(void *)GetProcAddress(h, fns[i]);
        if (fn)
            printf("  %s(6, 7) = %d\n", fns[i], fn(6, 7));
    }
    FreeLibrary(h);
    return 0;
}

// Compiles sources once into a ready DyOS program (PE with an exported
// main); in the OS "run file.exe" starts it without compiling.
static int build_exe(const char *out, const char **names, int count)
{
    static CcSource src[MAX_SOURCES];
    if (count > MAX_SOURCES) { printf("больше %d файлов (%d)\n", MAX_SOURCES, count); return 1; }
    for (int i = 0; i < count; i++) {
        src[i].name = names[i];
        src[i].text = cc_read_file(names[i]);
        if (!src[i].text) { printf("нет файла %s\n", names[i]); return 1; }
    }
    unsigned char *pe;
    unsigned long long size;
    if (!cc_build_dll(src, count, cc_runtime_symbols(), out, &pe, &size)) {
        printf("ОШИБКА: %s\n", cc_last_error);
        return 1;
    }
    FILE *f = fopen(out, "wb");
    if (!f) { printf("не открыть %s\n", out); return 1; }
    fwrite(pe, 1, (size_t)size, f);
    fclose(f);
    printf("Готовая программа %s: %llu байт\n", out, size);
    return 0;
}

int main(int argc, char **argv)
{
    SetConsoleOutputCP(65001);
    AddVectoredExceptionHandler(1, on_fault);
    if (argc >= 4 && !strcmp(argv[1], "--exe"))
        return build_exe(argv[2], (const char **)argv + 3, argc - 3);
    if (argc >= 3 && !strcmp(argv[1], "--dll"))
        return build_and_load_dll((const char **)argv + 2, argc - 2);
    if (argc >= 4 && !strcmp(argv[1], "--prof") && !strcmp(argv[2], "--run")) { g_prof_on = 1; argv++; argc--; }
    if (argc >= 3 && !strcmp(argv[1], "--run")) {     // a ready program (--exe): its main's code is the exit code
        int rc = run_pe(argv[2]);
        printf("\n[main вернула %d]\n", rc);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "--tests")) {
        strncpy(g_dir, argv[2], sizeof(g_dir) - 1);
        char pattern[MAX_PATH * 2];
        snprintf(pattern, sizeof(pattern), "%s\\*.c", argv[2]);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        int files = 0, bad = 0;
        if (h == INVALID_HANDLE_VALUE) {
            printf("нет тестов в %s\n", argv[2]);
            return 1;
        }
        do {
            char path[MAX_PATH * 2];
            snprintf(path, sizeof(path), "%s\\%s", argv[2], fd.cFileName);
            // "// with: b.part c.part" on the first line: more sources of the same program
            static char more[8][MAX_PATH * 2];
            const char *srcs[9] = { path };
            int nsrc = 1;
            const char *text = cc_read_file(path);
            if (text && !strncmp(text, "// with: ", 9)) {
                const char *q = text + 9;
                while (nsrc < 9) {
                    while (*q == ' ' || *q == '\t') q++;
                    if (!*q || *q == '\r' || *q == '\n') break;
                    char rel[MAX_PATH];
                    int k = 0;
                    for (; *q && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n'; q++)
                        if (k < MAX_PATH - 1) rel[k++] = *q;
                    rel[k] = 0;
                    snprintf(more[nsrc - 1], sizeof(more[0]), "%s\\%s", argv[2], rel);
                    srcs[nsrc] = more[nsrc - 1];
                    nsrc++;
                }
            }
            printf("%-24s ", fd.cFileName);
            fflush(stdout);
            int rc = run_files(srcs, nsrc, 0, 1);
            files++;
            if (rc == 0) {
                printf("ok\n");
            } else {
                printf("ПРОВАЛ (%d)\n", rc);
                bad++;
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        printf("\nФайлов: %d, с ошибками: %d\n", files, bad);
        return bad ? 1 : 0;
    }

    int show_asm = 0, first = 1;
    if (argc >= 2 && !strcmp(argv[1], "-S")) {
        show_asm = 1;
        first = 2;
    }
    if (argc <= first) {
        printf("cctest [-S] file.c [file.asm ...] | cctest --tests dir\n");
        return 1;
    }
    int rc = run_files((const char **)argv + first, argc - first, show_asm, 0);
    if (!show_asm)
        printf("\n[main вернула %d]\n", rc);
    return 0;
}
