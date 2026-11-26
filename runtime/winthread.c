// Threads of a Windows program (WinCC): the lock of stdio (runtime.c), the C11
// threads of threads.h (thrd_, mtx_, cnd_, tss_, call_once) and the Microsoft
// ones of process.h (_beginthreadex, _spawnv...).
#include "rtwin.h"

typedef void *HANDLE;
typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef struct { u64 opaque[5]; } CRITICAL_SECTION;     // 40 bytes on x64
typedef struct { void *ptr; } CONDITION_VARIABLE;
typedef struct {
    DWORD cb; WCHAR *lpReserved, *lpDesktop, *lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    unsigned short wShowWindow, cbReserved2; u8 *lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOW;
typedef struct { HANDLE hProcess, hThread; DWORD dwProcessId, dwThreadId; } PROCESS_INFORMATION;

void InitializeCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
BOOL TryEnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
void InitializeConditionVariable(CONDITION_VARIABLE *cv);
BOOL SleepConditionVariableCS(CONDITION_VARIABLE *cv, CRITICAL_SECTION *cs, DWORD ms);
void WakeConditionVariable(CONDITION_VARIABLE *cv);
void WakeAllConditionVariable(CONDITION_VARIABLE *cv);
HANDLE CreateThread(void *sa, size_t stack, DWORD (*start)(void *), void *arg, DWORD flags, DWORD *id);
void ExitThread(DWORD code);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
BOOL GetExitCodeThread(HANDLE h, DWORD *code);
BOOL GetExitCodeProcess(HANDLE h, DWORD *code);
BOOL CloseHandle(HANDLE h);
DWORD GetCurrentThreadId(void);
DWORD GetCurrentProcessId(void);
HANDLE GetCurrentThread(void);
HANDLE GetCurrentProcess(void);
BOOL DuplicateHandle(HANDLE sp, HANDLE h, HANDLE tp, HANDLE *out, DWORD access, BOOL inherit, DWORD options);
void Sleep(DWORD ms);
BOOL SwitchToThread(void);
DWORD TlsAlloc(void);
BOOL TlsFree(DWORD i);
void *TlsGetValue(DWORD i);
BOOL TlsSetValue(DWORD i, void *v);
BOOL CreateProcessW(const WCHAR *app, WCHAR *cmd, void *pa, void *ta, BOOL inherit, DWORD flags,
                    void *env, const WCHAR *dir, STARTUPINFOW *si, PROCESS_INFORMATION *pi);
DWORD SearchPathW(const WCHAR *path, const WCHAR *file, const WCHAR *ext, DWORD cap, WCHAR *out, WCHAR **part);

void *rt_malloc(size_t n);
void *rt_calloc(size_t n, size_t size);
void rt_free(void *p);
int rt_fflush(void *f);
void rt_exit(int code);
extern _Thread_local int rt_errno;

// ---- a lock made ready on its first use ----

typedef struct { volatile long state; CRITICAL_SECTION cs; } Lazy;

static CRITICAL_SECTION *lazy(Lazy *l)
{
    if (l->state != 2) {
        if (_InterlockedCompareExchange(&l->state, 1, 0) == 0) {
            InitializeCriticalSection(&l->cs);
            l->state = 2;
        } else {
            while (l->state != 2)
                _mm_pause();
        }
    }
    return &l->cs;
}

// The lock of stdio: it asks nothing of Windows. The thread that holds it may take it again
// (fflush inside fclose, printf in a signal handler). Another thread waits on the spot;
// a program that starts threads through threads.h or process.h gets a gentler wait
// (g_io_wait: the thread gives its time away) — only it has the Windows calls for that.
static volatile long g_io_owner;        // 0 free, else the id of the thread that holds it
static int g_io_depth;
static void (*g_io_wait)(void);
u32 cc_thread_id(void);                 // rtasm.asm: from the thread's block, no call to Windows

void cc_io_lock(void)
{
    long me = (long)cc_thread_id();
    if (g_io_owner == me) {
        g_io_depth++;
        return;
    }
    for (int spins = 0; _InterlockedCompareExchange(&g_io_owner, me, 0) != 0; spins++) {
        if (g_io_wait && spins > 64) g_io_wait();
        else _mm_pause();
    }
    g_io_depth = 1;
}

void cc_io_unlock(void)
{
    if (--g_io_depth == 0)
        _InterlockedExchange(&g_io_owner, 0);
}

static void io_wait_yield(void) { SwitchToThread(); }

// ---- threads.h ----

enum { thrd_success = 0, thrd_nomem = 1, thrd_timedout = 2, thrd_busy = 3, thrd_error = 4 };
enum { mtx_plain = 1, mtx_timed = 2, mtx_recursive = 0x100 };

typedef struct { HANDLE handle; DWORD id; } RtThrd;
typedef struct { CRITICAL_SECTION cs; int type, ready; } RtMtx;
typedef struct { CONDITION_VARIABLE cv; } RtCnd;
typedef struct { volatile long state; } RtOnce;
typedef struct { i64 tv_sec; long tv_nsec; } RtTimespec;
int rt_timespec_get(RtTimespec *ts, int base);

// tss: a TLS slot of Windows, with its destructor (run when a thread of thrd_create ends)
#define TSS_MAX 64
static DWORD g_tss_slot[TSS_MAX];
static void (*g_tss_dtor[TSS_MAX])(void *);
static int g_tss_used[TSS_MAX];

static void tss_cleanup(void)
{
    for (int round = 0; round < 4; round++) {           // a destructor may set another value: again
        int any = 0;
        for (int i = 0; i < TSS_MAX; i++) {
            if (!g_tss_used[i] || !g_tss_dtor[i])
                continue;
            void *v = TlsGetValue(g_tss_slot[i]);
            if (v) {
                TlsSetValue(g_tss_slot[i], 0);
                g_tss_dtor[i](v);
                any = 1;
            }
        }
        if (!any)
            break;
    }
}

typedef struct { int (*fn)(void *); void *arg; } Start;

static DWORD thrd_start(void *p)
{
    Start s = *(Start *)p;
    rt_free(p);
    int r = s.fn(s.arg);
    tss_cleanup();
    return (DWORD)r;
}

int rt_thrd_create(RtThrd *t, int (*fn)(void *), void *arg)
{
    Start *s = rt_malloc(sizeof(Start));
    if (!s) return thrd_nomem;
    s->fn = fn;
    s->arg = arg;
    DWORD id = 0;
    g_io_wait = io_wait_yield;
    HANDLE h = CreateThread(0, 0, thrd_start, s, 0, &id);
    if (!h) {
        rt_free(s);
        return thrd_error;
    }
    t->handle = h;
    t->id = id;
    return thrd_success;
}

int rt_thrd_join(RtThrd t, int *res)
{
    if (WaitForSingleObject(t.handle, 0xFFFFFFFFu) != 0)
        return thrd_error;
    DWORD code = 0;
    GetExitCodeThread(t.handle, &code);
    CloseHandle(t.handle);
    if (res) *res = (int)code;
    return thrd_success;
}

int rt_thrd_detach(RtThrd t) { return CloseHandle(t.handle) ? thrd_success : thrd_error; }

RtThrd rt_thrd_current(void)
{
    RtThrd t;
    t.handle = GetCurrentThread();
    t.id = GetCurrentThreadId();
    return t;
}

int rt_thrd_equal(RtThrd a, RtThrd b) { return a.id == b.id; }

static DWORD ms_of(const RtTimespec *d)
{
    i64 ms = d->tv_sec * 1000 + (d->tv_nsec + 999999) / 1000000;
    return ms < 0 ? 0 : ms > 0x7FFFFFFF ? 0x7FFFFFFF : (DWORD)ms;
}

int rt_thrd_sleep(const RtTimespec *d, RtTimespec *rem)
{
    Sleep(ms_of(d));
    if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    return 0;
}

void rt_thrd_yield(void) { SwitchToThread(); }

void rt_thrd_exit(int res)
{
    tss_cleanup();
    ExitThread((DWORD)res);
}

// how long until an absolute moment of TIME_UTC (timedlock, timedwait)
static DWORD ms_until(const RtTimespec *at)
{
    RtTimespec now;
    rt_timespec_get(&now, 1);
    RtTimespec d;
    d.tv_sec = at->tv_sec - now.tv_sec;
    d.tv_nsec = at->tv_nsec - now.tv_nsec;
    if (d.tv_nsec < 0) { d.tv_nsec += 1000000000; d.tv_sec--; }
    if (d.tv_sec < 0) return 0;
    return ms_of(&d);
}

int rt_mtx_init(RtMtx *m, int type)
{
    InitializeCriticalSection(&m->cs);
    m->type = type;
    m->ready = 1;
    return thrd_success;
}

int rt_mtx_lock(RtMtx *m) { EnterCriticalSection(&m->cs); return thrd_success; }
int rt_mtx_trylock(RtMtx *m) { return TryEnterCriticalSection(&m->cs) ? thrd_success : thrd_busy; }
int rt_mtx_unlock(RtMtx *m) { LeaveCriticalSection(&m->cs); return thrd_success; }

int rt_mtx_timedlock(RtMtx *m, const RtTimespec *at)
{
    for (;;) {
        if (TryEnterCriticalSection(&m->cs))
            return thrd_success;
        if (!ms_until(at))
            return thrd_timedout;
        Sleep(1);
    }
}

void rt_mtx_destroy(RtMtx *m)
{
    if (m->ready)
        DeleteCriticalSection(&m->cs);
    m->ready = 0;
}

int rt_cnd_init(RtCnd *c) { InitializeConditionVariable(&c->cv); return thrd_success; }
int rt_cnd_signal(RtCnd *c) { WakeConditionVariable(&c->cv); return thrd_success; }
int rt_cnd_broadcast(RtCnd *c) { WakeAllConditionVariable(&c->cv); return thrd_success; }
void rt_cnd_destroy(RtCnd *c) { (void)c; }

int rt_cnd_wait(RtCnd *c, RtMtx *m)
{
    return SleepConditionVariableCS(&c->cv, &m->cs, 0xFFFFFFFFu) ? thrd_success : thrd_error;
}

int rt_cnd_timedwait(RtCnd *c, RtMtx *m, const RtTimespec *at)
{
    return SleepConditionVariableCS(&c->cv, &m->cs, ms_until(at)) ? thrd_success : thrd_timedout;
}

int rt_tss_create(int *key, void (*dtor)(void *))
{
    for (int i = 0; i < TSS_MAX; i++)
        if (!g_tss_used[i]) {
            DWORD slot = TlsAlloc();
            if (slot == 0xFFFFFFFFu)
                return thrd_error;
            g_tss_slot[i] = slot;
            g_tss_dtor[i] = dtor;
            g_tss_used[i] = 1;
            *key = i;
            return thrd_success;
        }
    return thrd_error;
}

void *rt_tss_get(int key)
{
    return key >= 0 && key < TSS_MAX && g_tss_used[key] ? TlsGetValue(g_tss_slot[key]) : 0;
}

int rt_tss_set(int key, void *v)
{
    if (key < 0 || key >= TSS_MAX || !g_tss_used[key]) return thrd_error;
    return TlsSetValue(g_tss_slot[key], v) ? thrd_success : thrd_error;
}

void rt_tss_delete(int key)
{
    if (key >= 0 && key < TSS_MAX && g_tss_used[key]) {
        TlsFree(g_tss_slot[key]);
        g_tss_used[key] = 0;
    }
}

// call_once: the first caller runs fn, the others wait until it has
void rt_call_once(RtOnce *flag, void (*fn)(void))
{
    if (flag->state == 2)
        return;
    if (_InterlockedCompareExchange(&flag->state, 1, 0) == 0) {
        fn();
        _InterlockedExchange(&flag->state, 2);
        return;
    }
    while (flag->state != 2)
        Sleep(0);
}

// ---- process.h ----

typedef struct { unsigned (*fn)(void *); void *arg; } StartEx;

static DWORD start_ex(void *p)
{
    StartEx s = *(StartEx *)p;
    rt_free(p);
    return s.fn(s.arg);
}

// _beginthreadex: a thread of start(arg); its handle (CloseHandle it), 0 on failure
u64 rt__beginthreadex(void *security, unsigned stack, unsigned (*start)(void *), void *arg, unsigned flags, unsigned *id)
{
    StartEx *s = rt_malloc(sizeof(StartEx));
    if (!s) { rt_errno = 12; return 0; }
    s->fn = start;
    s->arg = arg;
    DWORD tid = 0;
    g_io_wait = io_wait_yield;
    HANDLE h = CreateThread(security, stack, start_ex, s, flags, &tid);
    if (!h) {
        rt_free(s);
        rt_errno = 11;          // EAGAIN
        return 0;
    }
    if (id) *id = tid;
    return (u64)h;
}

typedef struct { void (*fn)(void *); void *arg; } StartOld;

static DWORD start_old(void *p)
{
    StartOld s = *(StartOld *)p;
    rt_free(p);
    s.fn(s.arg);
    return 0;
}

// _beginthread: the old way; the handle closes itself when the thread ends (-1 on failure)
u64 rt__beginthread(void (*start)(void *), unsigned stack, void *arg)
{
    StartOld *s = rt_malloc(sizeof(StartOld));
    if (!s) { rt_errno = 12; return (u64)-1; }
    s->fn = start;
    s->arg = arg;
    g_io_wait = io_wait_yield;
    HANDLE h = CreateThread(0, stack, start_old, s, 0, 0);
    if (!h) {
        rt_free(s);
        return (u64)-1;
    }
    CloseHandle(h);             // the thread lives on; its handle is not needed (as Microsoft's)
    return (u64)h;
}

void rt__endthreadex(unsigned code) { ExitThread(code); }
void rt__endthread(void) { ExitThread(0); }
int rt__getpid(void) { return (int)GetCurrentProcessId(); }

// ---- _spawnv and its kin: another program, with its arguments ----
// mode: _P_WAIT 0 (its exit code), _P_NOWAIT 1 (its handle), _P_OVERLAY 2 (this one ends), _P_DETACH 4

static int add_arg(char *line, int n, int cap, const char *a)
{
    int quote = !*a;
    for (const char *p = a; *p; p++)
        if (*p == ' ' || *p == '\t' || *p == '"') quote = 1;
    if (n && n < cap) line[n++] = ' ';
    if (quote && n < cap) line[n++] = '"';
    for (const char *p = a; *p; p++) {
        int slashes = 0;
        while (*p == '\\') { slashes++; p++; }
        if (!*p) {                  // backslashes before the closing quote: doubled
            for (int i = 0; i < (quote ? 2 * slashes : slashes) && n < cap; i++) line[n++] = '\\';
            break;
        }
        if (*p == '"') {
            for (int i = 0; i < 2 * slashes + 1 && n < cap; i++) line[n++] = '\\';
        } else {
            for (int i = 0; i < slashes && n < cap; i++) line[n++] = '\\';
        }
        if (n < cap) line[n++] = *p;
    }
    if (quote && n < cap) line[n++] = '"';
    return n;
}

static i64 spawn(int mode, const char *path, const char *const *argv, int search)
{
    int cap = 32768;
    char *line = rt_malloc((size_t)cap + 1);
    if (!line) { rt_errno = 12; return -1; }
    int n = 0;
    for (int i = 0; argv && argv[i]; i++)
        n = add_arg(line, n, cap, argv[i]);
    if (!argv || !argv[0])
        n = add_arg(line, n, cap, path);
    line[n] = 0;
    WCHAR *wp = cc_wide(path, -1, 0), *wl = cc_wide(line, -1, 0);
    rt_free(line);
    WCHAR found[1024];
    const WCHAR *app = wp;
    if (search && wp) {
        static const WCHAR exe[] = { '.', 'e', 'x', 'e', 0 };
        if (SearchPathW(0, wp, exe, 1024, found, 0))
            app = found;
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    u8 *z = (u8 *)&si;
    for (size_t i = 0; i < sizeof(si); i++) z[i] = 0;
    si.cb = sizeof(si);
    rt_fflush(0);
    BOOL ok = wp && wl && CreateProcessW(app, wl, 0, 0, 1, mode == 4 ? 0x00000008 : 0, 0, 0, &si, &pi);
    rt_free(wp);
    rt_free(wl);
    if (!ok) { rt_errno = 2; return -1; }
    CloseHandle(pi.hThread);
    if (mode == 1)
        return (i64)pi.hProcess;
    if (mode == 4) {
        CloseHandle(pi.hProcess);
        return 0;
    }
    WaitForSingleObject(pi.hProcess, 0xFFFFFFFFu);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    if (mode == 2)
        rt_exit((int)code);
    return (i64)code;
}

i64 rt__spawnv(int mode, const char *path, const char *const *argv) { return spawn(mode, path, argv, 0); }
i64 rt__spawnvp(int mode, const char *path, const char *const *argv) { return spawn(mode, path, argv, 1); }
i64 rt__execv(const char *path, const char *const *argv) { return spawn(2, path, argv, 0); }
i64 rt__execvp(const char *path, const char *const *argv) { return spawn(2, path, argv, 1); }

static i64 spawn_list(int mode, const char *path, va_list ap, int search)
{
    const char *args[256];
    int n = 0;
    const char *a;
    while ((a = va_arg(ap, const char *)) != 0 && n < 255)
        args[n++] = a;
    args[n] = 0;
    return spawn(mode, path, args, search);
}

i64 rt__spawnl(int mode, const char *path, ...)
{
    va_list ap;
    va_start(ap, path);
    i64 r = spawn_list(mode, path, ap, 0);
    va_end(ap);
    return r;
}

i64 rt__spawnlp(int mode, const char *path, ...)
{
    va_list ap;
    va_start(ap, path);
    i64 r = spawn_list(mode, path, ap, 1);
    va_end(ap);
    return r;
}

i64 rt__execl(const char *path, ...)
{
    va_list ap;
    va_start(ap, path);
    i64 r = spawn_list(2, path, ap, 0);
    va_end(ap);
    return r;
}

i64 rt__execlp(const char *path, ...)
{
    va_list ap;
    va_start(ap, path);
    i64 r = spawn_list(2, path, ap, 1);
    va_end(ap);
    return r;
}

// _cwait: wait for a program of _P_NOWAIT; its exit code into *status
i64 rt__cwait(int *status, i64 handle, int action)
{
    (void)action;
    if (WaitForSingleObject((HANDLE)handle, 0xFFFFFFFFu) != 0) { rt_errno = 10; return -1; }
    DWORD code = 0;
    GetExitCodeProcess((HANDLE)handle, &code);
    if (status) *status = (int)code;
    CloseHandle((HANDLE)handle);
    return handle;
}

#define F(name, fn) ".alias " name ", " #fn "\n"
asm(
    F("thrd_create", rt_thrd_create) F("thrd_join", rt_thrd_join) F("thrd_detach", rt_thrd_detach)
    F("thrd_current", rt_thrd_current) F("thrd_equal", rt_thrd_equal) F("thrd_sleep", rt_thrd_sleep)
    F("thrd_yield", rt_thrd_yield) F("thrd_exit", rt_thrd_exit)
    F("mtx_init", rt_mtx_init) F("mtx_lock", rt_mtx_lock) F("mtx_trylock", rt_mtx_trylock)
    F("mtx_timedlock", rt_mtx_timedlock) F("mtx_unlock", rt_mtx_unlock) F("mtx_destroy", rt_mtx_destroy)
    F("cnd_init", rt_cnd_init) F("cnd_signal", rt_cnd_signal) F("cnd_broadcast", rt_cnd_broadcast)
    F("cnd_wait", rt_cnd_wait) F("cnd_timedwait", rt_cnd_timedwait) F("cnd_destroy", rt_cnd_destroy)
    F("tss_create", rt_tss_create) F("tss_get", rt_tss_get) F("tss_set", rt_tss_set) F("tss_delete", rt_tss_delete)
    F("call_once", rt_call_once)
    F("_beginthreadex", rt__beginthreadex) F("_beginthread", rt__beginthread)
    F("_endthreadex", rt__endthreadex) F("_endthread", rt__endthread)
    F("_getpid", rt__getpid) F("getpid", rt__getpid)
    F("_spawnv", rt__spawnv) F("spawnv", rt__spawnv) F("_spawnvp", rt__spawnvp) F("spawnvp", rt__spawnvp)
    F("_spawnl", rt__spawnl) F("spawnl", rt__spawnl) F("_spawnlp", rt__spawnlp) F("spawnlp", rt__spawnlp)
    F("_execv", rt__execv) F("execv", rt__execv) F("_execvp", rt__execvp) F("execvp", rt__execvp)
    F("_execl", rt__execl) F("execl", rt__execl) F("_execlp", rt__execlp) F("execlp", rt__execlp)
    F("_cwait", rt__cwait) F("cwait", rt__cwait)
);
#undef F
