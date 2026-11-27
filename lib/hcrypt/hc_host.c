// What hcrypt.c took from the DynastyOS kernel, made of Windows calls.
#include "hc_internal.h"

void *page_alloc(u64 count)
{
    return VirtualAlloc(0, (SIZE_T)(count * PAGE_SIZE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void page_free(void *addr, u64 count)
{
    (void)count;
    if (addr) VirtualFree(addr, 0, MEM_RELEASE);
}

// ---- the lanes of Argon2id: each on a thread of its own ----

typedef struct {
    void (*fn)(int i, void *ctx);
    void *ctx;
    int i;
} HcLane;

static DWORD WINAPI lane_main(void *p)
{
    HcLane *l = p;
    l->fn(l->i, l->ctx);
    return 0;
}

void smp_parallel(int n, void (*fn)(int i, void *ctx), void *ctx)
{
    if (n <= 1) {
        if (n == 1) fn(0, ctx);
        return;
    }
    HcLane *lanes = malloc(sizeof(HcLane) * (size_t)n);
    HANDLE *th = malloc(sizeof(HANDLE) * (size_t)n);
    if (!lanes || !th) {                    // no memory for them: one after another
        free(lanes); free(th);
        for (int i = 0; i < n; i++) fn(i, ctx);
        return;
    }
    for (int i = 1; i < n; i++) {
        lanes[i].fn = fn; lanes[i].ctx = ctx; lanes[i].i = i;
        th[i] = CreateThread(0, 0, lane_main, &lanes[i], 0, 0);
        if (!th[i]) fn(i, ctx);             // no thread: this one does it
    }
    fn(0, ctx);
    for (int i = 1; i < n; i++)
        if (th[i]) {
            WaitForSingleObject(th[i], INFINITE);
            CloseHandle(th[i]);
        }
    free(lanes);
    free(th);
}

// ---- entropy besides rdseed/rdrand ----

u64 timer_ticks(void) { return GetTickCount64(); }

u64 mouse_seq(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (u64)t.QuadPart ^ ((u64)GetCurrentProcessId() << 32) ^ ((u64)GetCurrentThreadId() << 16);
}

// ---- the key of local_seal: %LOCALAPPDATA%\hcrypt\<name> ----

static int key_path(const char *name, wchar_t *path, int cap, int make_dir)
{
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, (DWORD)cap);
    if (!n || n + 40 >= (DWORD)cap) return 0;
    wcscpy(path + n, L"\\hcrypt");
    if (make_dir) CreateDirectoryW(path, 0);
    n += 7;
    path[n++] = L'\\';
    for (const char *s = name; *s && n + 1 < (DWORD)cap; s++) path[n++] = (wchar_t)(u8)*s;
    path[n] = 0;
    return 1;
}

const char *fs_read(const char *name, u64 *size)
{
    static char data[64];
    wchar_t path[MAX_PATH];
    if (size) *size = 0;
    if (!key_path(name, path, MAX_PATH, 0)) return 0;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD got = 0;
    BOOL ok = ReadFile(h, data, sizeof(data), &got, 0);
    CloseHandle(h);
    if (!ok) return 0;
    if (size) *size = got;                  // more than the key: the caller sees the size is wrong
    return data;
}

int fs_write(const char *name, const char *data, u64 size)
{
    wchar_t path[MAX_PATH];
    if (!key_path(name, path, MAX_PATH, 1)) return 0;
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) return 0;   // there already: never written over
    DWORD put = 0;
    BOOL ok = WriteFile(h, data, (DWORD)size, &put, 0) && put == (DWORD)size;
    CloseHandle(h);
    if (!ok) DeleteFileW(path);
    return ok;
}
