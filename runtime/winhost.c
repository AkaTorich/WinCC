// The host of runtime.c in a Windows program (WinCC): the console, files, the
// clock, memory, the environment and CPU faults -- all through kernel32.
// Text inside the library is UTF-8; Windows gets and gives UTF-16.
#include "rtwin.h"

// ---- kernel32: only what is used here ----
typedef void *HANDLE;
typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef struct { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct {
    DWORD cb; WCHAR *lpReserved, *lpDesktop, *lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    unsigned short wShowWindow, cbReserved2; u8 *lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOW;
typedef struct { HANDLE hProcess, hThread; DWORD dwProcessId, dwThreadId; } PROCESS_INFORMATION;

#define INVALID_HANDLE ((HANDLE)(i64)-1)
#define STD_INPUT ((DWORD)-10)
#define STD_OUTPUT ((DWORD)-11)
#define CP_UTF8 65001
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define SHARE_RW 3u
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define ATTR_NORMAL 0x80
#define ATTR_DIRECTORY 0x10
#define NO_ATTRIBUTES 0xFFFFFFFFu
#define MOVE_REPLACE 1
#define MEM_COMMIT_RESERVE 0x3000
#define PAGE_RW 4
#define INFINITE_WAIT 0xFFFFFFFFu

HANDLE GetStdHandle(DWORD which);
BOOL GetConsoleMode(HANDLE h, DWORD *mode);
BOOL WriteConsoleW(HANDLE h, const void *text, DWORD count, DWORD *written, void *reserved);
BOOL ReadConsoleW(HANDLE h, void *buf, DWORD count, DWORD *read, void *control);
BOOL WriteFile(HANDLE h, const void *data, DWORD size, DWORD *written, void *overlapped);
BOOL ReadFile(HANDLE h, void *buf, DWORD size, DWORD *read, void *overlapped);
int  MultiByteToWideChar(unsigned cp, DWORD flags, const char *s, int n, WCHAR *out, int cap);
int  WideCharToMultiByte(unsigned cp, DWORD flags, const WCHAR *s, int n, char *out, int cap, const char *def, BOOL *used);
HANDLE CreateFileW(const WCHAR *name, DWORD access, DWORD share, void *sa, DWORD disposition, DWORD flags, HANDLE tmpl);
BOOL CloseHandle(HANDLE h);
BOOL GetFileSizeEx(HANDLE h, i64 *size);
BOOL DeleteFileW(const WCHAR *name);
BOOL MoveFileExW(const WCHAR *from, const WCHAR *to, DWORD flags);
DWORD GetFileAttributesW(const WCHAR *name);
void GetLocalTime(SYSTEMTIME *t);
typedef struct { DWORD lo, hi; } FILETIME;
void GetSystemTimeAsFileTime(FILETIME *ft);
BOOL FileTimeToSystemTime(const FILETIME *ft, SYSTEMTIME *st);
BOOL SystemTimeToFileTime(const SYSTEMTIME *st, FILETIME *ft);
BOOL SystemTimeToTzSpecificLocalTime(const void *zone, const SYSTEMTIME *utc, SYSTEMTIME *local);
u64  GetTickCount64(void);
BOOL QueryPerformanceCounter(i64 *count);
BOOL QueryPerformanceFrequency(i64 *freq);
unsigned GetConsoleCP(void);
unsigned GetConsoleOutputCP(void);
BOOL SetConsoleCP(unsigned cp);
BOOL SetConsoleOutputCP(unsigned cp);
void *VirtualAlloc(void *at, size_t size, DWORD type, DWORD protect);
DWORD GetEnvironmentVariableW(const WCHAR *name, WCHAR *buf, DWORD cap);
BOOL CreateProcessW(const WCHAR *app, WCHAR *cmd, void *pa, void *ta, BOOL inherit, DWORD flags,
                    void *env, const WCHAR *dir, STARTUPINFOW *si, PROCESS_INFORMATION *pi);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
BOOL GetExitCodeProcess(HANDLE h, DWORD *code);
void ExitProcess(unsigned code);

void *rt_malloc(size_t n);
void rt_free(void *p);
int rt_fflush(void *f);

// ---- UTF-8 <-> UTF-16 ----

// UTF-16 of a UTF-8 text (n bytes, -1 = up to the NUL), on the heap with a NUL; its length in *len
static WCHAR *wide_of(const char *s, int n, int *len)
{
    int k = MultiByteToWideChar(CP_UTF8, 0, s, n, 0, 0);
    WCHAR *w = rt_malloc(((size_t)k + 1) * sizeof(WCHAR));
    if (!w) return 0;
    if (k) MultiByteToWideChar(CP_UTF8, 0, s, n, w, k);
    w[k] = 0;
    if (len) *len = n < 0 && k ? k - 1 : k;
    return w;
}

// UTF-8 of a UTF-16 text (n units, -1 = up to the NUL) into buf (cap bytes, with a NUL); its length
static int utf8_into(const WCHAR *w, int n, char *buf, int cap)
{
    int k = WideCharToMultiByte(CP_UTF8, 0, w, n, buf, cap - 1, 0, 0);
    if (k < 0) k = 0;
    if (n < 0 && k) k--;            // the NUL was counted
    buf[k] = 0;
    return k;
}

// the same for the other files of the library (rtwin.h)
unsigned short *cc_wide(const char *s, int n, int *len) { return wide_of(s, n, len); }
int cc_utf8(const unsigned short *w, int n, char *buf, int cap) { return utf8_into(w, n, buf, cap); }

// ---- console ----

static int is_console(HANDLE h)
{
    DWORD mode;
    return h && h != INVALID_HANDLE && GetConsoleMode(h, &mode);
}

void cc_print(const char *text)
{
    HANDLE h = GetStdHandle(STD_OUTPUT);
    if (!h || h == INVALID_HANDLE)
        return;                     // a windowed program: nowhere to print
    int n = 0;
    while (text[n]) n++;
    DWORD done;
    if (is_console(h)) {            // the console takes UTF-16: Cyrillic shows as it is
        int len;
        WCHAR *w = wide_of(text, n, &len);
        if (w) {
            WriteConsoleW(h, w, (DWORD)len, &done, 0);
            rt_free(w);
        }
    } else {                        // a file or a pipe gets the UTF-8 bytes
        WriteFile(h, text, (DWORD)n, &done, 0);
    }
}

static char g_in[4096];             // redirected input: what ReadFile gave and is not taken yet
static int g_in_pos, g_in_len, g_in_end;

int cc_input_line(char *buf, int cap)
{
    HANDLE h = GetStdHandle(STD_INPUT);
    if (!h || h == INVALID_HANDLE || cap < 2)
        return -1;
    if (is_console(h)) {
        static WCHAR w[1024];
        DWORD got = 0;
        int max = cap / 3 < 1023 ? cap / 3 : 1023;    // UTF-8 takes up to 3 bytes for a unit
        if (max < 1) max = 1;
        if (!ReadConsoleW(h, w, (DWORD)max, &got, 0) || got == 0)
            return -1;
        if (w[0] == 26)             // Ctrl+Z, Enter: the end of input
            return -1;
        int k = 0;
        for (DWORD i = 0; i < got; i++)         // "\r\n" -> "\n"
            if (w[i] != '\r')
                w[k++] = w[i];
        return utf8_into(w, k, buf, cap);
    }
    int n = 0;
    while (n < cap - 1) {
        if (g_in_pos >= g_in_len) {
            if (g_in_end) break;
            DWORD got = 0;
            if (!ReadFile(h, g_in, sizeof(g_in), &got, 0) || got == 0) {
                g_in_end = 1;
                break;
            }
            g_in_pos = 0;
            g_in_len = (int)got;
        }
        char c = g_in[g_in_pos++];
        if (c == '\r')
            continue;
        buf[n++] = c;
        if (c == '\n')
            break;
    }
    buf[n] = 0;
    return n ? n : -1;
}

// ---- files: names are UTF-8, '/' or '\' between folders ----

static char *g_read_last;

const char *cc_file_read(const char *name, size_t *size)
{
    WCHAR *w = wide_of(name, -1, 0);
    if (!w) return 0;
    HANDLE h = CreateFileW(w, GENERIC_READ, SHARE_RW, 0, OPEN_EXISTING, ATTR_NORMAL, 0);
    rt_free(w);
    if (h == INVALID_HANDLE)
        return 0;
    i64 len = 0;
    GetFileSizeEx(h, &len);
    rt_free(g_read_last);           // the library copies what it gets at once
    g_read_last = rt_malloc((size_t)len + 1);
    if (!g_read_last) {
        CloseHandle(h);
        return 0;
    }
    size_t have = 0;
    while (have < (size_t)len) {
        DWORD got = 0;
        DWORD want = (size_t)len - have > 0x40000000 ? 0x40000000 : (DWORD)((size_t)len - have);
        if (!ReadFile(h, g_read_last + have, want, &got, 0) || !got)
            break;
        have += got;
    }
    CloseHandle(h);
    g_read_last[have] = 0;
    if (size) *size = have;
    return g_read_last;
}

int cc_file_write(const char *name, const char *data, size_t size)
{
    WCHAR *w = wide_of(name, -1, 0);
    if (!w) return 0;
    HANDLE h = CreateFileW(w, GENERIC_WRITE, SHARE_RW, 0, CREATE_ALWAYS, ATTR_NORMAL, 0);
    rt_free(w);
    if (h == INVALID_HANDLE)
        return 0;
    size_t done = 0;
    while (done < size) {
        DWORD put = 0;
        DWORD want = size - done > 0x40000000 ? 0x40000000 : (DWORD)(size - done);
        if (!WriteFile(h, data + done, want, &put, 0) || !put)
            break;
        done += put;
    }
    CloseHandle(h);
    return done == size;
}

int cc_file_delete(const char *name)
{
    WCHAR *w = wide_of(name, -1, 0);
    if (!w) return 0;
    int ok = DeleteFileW(w) != 0;
    rt_free(w);
    return ok;
}

int cc_file_rename(const char *from, const char *to)
{
    WCHAR *a = wide_of(from, -1, 0), *b = wide_of(to, -1, 0);
    int ok = a && b && MoveFileExW(a, b, MOVE_REPLACE) != 0;       // replaces, as POSIX rename does
    rt_free(a);
    rt_free(b);
    return ok;
}

int cc_file_kind(const char *name)
{
    WCHAR *w = wide_of(name, -1, 0);
    if (!w) return 0;
    DWORD a = GetFileAttributesW(w);
    rt_free(w);
    if (a == NO_ATTRIBUTES) return 0;
    return a & ATTR_DIRECTORY ? 2 : 1;
}

// ---- clock, memory ----

void cc_now(int t[6])
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    t[0] = st.wYear; t[1] = st.wMonth; t[2] = st.wDay;
    t[3] = st.wHour; t[4] = st.wMinute; t[5] = st.wSecond;
}

#define UNIX_EPOCH_IN_FILETIME 11644473600ll      // seconds from 1601-01-01 to 1970-01-01

i64 cc_utc_seconds(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    u64 v = ((u64)ft.hi << 32) | ft.lo;                    // 100 ns since 1601
    return (i64)(v / 10000000ull) - UNIX_EPOCH_IN_FILETIME;
}

// the zone and its summer time as Windows has them set
int cc_utc_offset(i64 utc)
{
    u64 v = (u64)(utc + UNIX_EPOCH_IN_FILETIME) * 10000000ull;
    FILETIME ft = { (DWORD)v, (DWORD)(v >> 32) }, lf;
    SYSTEMTIME u, l;
    if (!FileTimeToSystemTime(&ft, &u) || !SystemTimeToTzSpecificLocalTime(0, &u, &l) || !SystemTimeToFileTime(&l, &lf))
        return 0;
    u64 lv = ((u64)lf.hi << 32) | lf.lo;
    return (int)(((i64)lv - (i64)v) / 10000000);
}

// milliseconds by the performance counter: clock() moves by 1 ms, not by the 16 ms of the system tick
u64 cc_millis(void)
{
    static i64 freq;
    i64 now = 0;
    if (!freq && !QueryPerformanceFrequency(&freq))
        freq = -1;
    if (freq <= 0 || !QueryPerformanceCounter(&now))
        return GetTickCount64();
    return (u64)(now / freq * 1000 + now % freq * 1000 / freq);
}

u64 cc_irq_off(void) { return 0; }      // the heap's lock is enough here
void cc_irq_restore(u64 f) { (void)f; }

// The heap grows by pieces of at least 16 MB, as the program needs.
void *cc_heap_more(size_t want, size_t *got)
{
    size_t n = want > ((size_t)16 << 20) ? (want + 0xFFFF) & ~(size_t)0xFFFF : (size_t)16 << 20;
    void *p = VirtualAlloc(0, n, MEM_COMMIT_RESERVE, PAGE_RW);
    *got = p ? n : 0;
    return p;
}

// ---- environment, other programs, the end ----

// _wgetenv (stdlib.h of Microsoft): getenv in UTF-16, each name with its own text as well
typedef struct WEnvValue { struct WEnvValue *next; WCHAR *name, *value; } WEnvValue;
static WEnvValue *g_wenv;

static WCHAR *wcopy_n(const WCHAR *s, int n)
{
    WCHAR *r = rt_malloc(((size_t)n + 1) * sizeof(WCHAR));
    if (!r) return 0;
    for (int i = 0; i < n; i++) r[i] = s[i];
    r[n] = 0;
    return r;
}

WCHAR *rt__wgetenv(const WCHAR *name)
{
    static WCHAR wv[32768];
    if (!name) return 0;
    DWORD n = GetEnvironmentVariableW(name, wv, 32768);
    if (!n || n >= 32768)
        return 0;
    WEnvValue *e = g_wenv;
    for (; e; e = e->next) {
        int k = 0;
        while (e->name[k] && e->name[k] == name[k]) k++;
        if (!e->name[k] && !name[k])
            break;
    }
    if (e) {
        DWORD k = 0;
        while (k < n && e->value[k] == wv[k]) k++;
        if (k == n && !e->value[k])
            return e->value;            // the same as before: the same text
    } else {
        int nl = 0;
        while (name[nl]) nl++;
        e = rt_malloc(sizeof(WEnvValue));
        if (!e) return 0;
        e->name = wcopy_n(name, nl);
        if (!e->name) return 0;
        e->next = g_wenv;
        g_wenv = e;
    }
    e->value = wcopy_n(wv, (int)n);     // the old text is not freed: a pointer to it may live on
    return e->value;
}

// getenv: each name keeps its own text, so getenv("A") stays as it was after getenv("B");
// a value that changed gets a new text (the old one is not freed: a pointer to it may live on)
typedef struct EnvValue { struct EnvValue *next; char *name, *value; } EnvValue;
static EnvValue *g_env;

char *rt_getenv_win(const char *name)
{
    static WCHAR wv[32768];
    static char value[32768 * 3];
    WCHAR *w = wide_of(name, -1, 0);
    if (!w) return 0;
    DWORD n = GetEnvironmentVariableW(w, wv, 32768);
    rt_free(w);
    if (!n || n >= 32768)
        return 0;
    int len = utf8_into(wv, (int)n, value, sizeof(value));
    EnvValue *e = g_env;
    for (; e; e = e->next) {
        int k = 0;
        while (e->name[k] && e->name[k] == name[k]) k++;
        if (!e->name[k] && !name[k])
            break;
    }
    if (e) {
        int k = 0;
        while (e->value[k] && e->value[k] == value[k]) k++;
        if (!e->value[k] && !value[k])
            return e->value;            // the same as before: the same text
    } else {
        int nl = 0;
        while (name[nl]) nl++;
        e = rt_malloc(sizeof(EnvValue));
        if (!e) return 0;
        e->name = rt_malloc((size_t)nl + 1);
        if (!e->name) return 0;
        for (int i = 0; i <= nl; i++) e->name[i] = name[i];
        e->next = g_env;
        g_env = e;
    }
    e->value = rt_malloc((size_t)len + 1);
    if (!e->value) return 0;
    for (int i = 0; i <= len; i++) e->value[i] = value[i];
    return e->value;
}

// system(cmd): cmd.exe runs it and the program waits; system(NULL) says a command processor is there
int rt_system_win(const char *cmd)
{
    if (!cmd)
        return 1;
    rt_fflush(0);
    int n = 0;
    while (cmd[n]) n++;
    // the command's console works in UTF-8 while it runs: what it writes to a file is UTF-8 too
    static const char head[] = "cmd.exe /d /c chcp 65001>nul & ";
    char *line = rt_malloc(sizeof(head) + (size_t)n);
    if (!line) return -1;
    for (int i = 0; head[i]; i++) line[i] = head[i];
    for (int i = 0; i <= n; i++) line[sizeof(head) - 1 + i] = cmd[i];
    WCHAR *w = wide_of(line, -1, 0);
    rt_free(line);
    if (!w) return -1;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    u8 *z = (u8 *)&si;
    for (size_t i = 0; i < sizeof(si); i++) z[i] = 0;
    si.cb = sizeof(si);
    int rc = -1;
    unsigned in_cp = GetConsoleCP(), out_cp = GetConsoleOutputCP();     // the console is shared: back after it
    if (CreateProcessW(0, w, 0, 0, 1, 0, 0, 0, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE_WAIT);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        rc = (int)code;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    if (in_cp) SetConsoleCP(in_cp);
    if (out_cp) SetConsoleOutputCP(out_cp);
    rt_free(w);
    return rc;
}

void wcc_exit_process(int code)
{
    ExitProcess((unsigned)code);
}
