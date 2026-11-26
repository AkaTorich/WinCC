// Windows API from a WinCC program: kernel32 files and threads, the registry,
// a hidden window and its messages, GDI text in UTF-8, wide strings, UTF-8 as
// the program's code page (the manifest).
#include "test.h"
#ifndef _WIN32                          // DynastyOS (cctest): no Windows there
int main(void) { return 0; }
#else
#include <windows.h>
#include <string.h>
#pragma comment(linker, "/MANIFEST")    // WinCC's manifest: UTF-8 is the code page (asked for, not by default)

static volatile LONG g_sum;
static CRITICAL_SECTION g_cs;

static DWORD WINAPI worker(LPVOID param)
{
    int n = (int)(INT_PTR)param;
    for (int i = 0; i < 1000; i++) {
        EnterCriticalSection(&g_cs);
        g_sum += n;
        LeaveCriticalSection(&g_cs);
    }
    return (DWORD)n * 10;
}

static int g_created, g_user_msg;
static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CREATE) g_created = 1;
    if (msg == WM_USER + 7) { g_user_msg = (int)wp + (int)lp; return 99; }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(void)
{
    // files through kernel32, a Cyrillic name
    const char *name = "проба winapi.txt";
    HANDLE h = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT(1, h != INVALID_HANDLE_VALUE);
    DWORD put = 0;
    ASSERT(1, WriteFile(h, "Привет", 12, &put, NULL));
    ASSERT(12, put);
    CloseHandle(h);
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(L"проба winapi.txt", &fd);
    ASSERT(1, f != INVALID_HANDLE_VALUE);
    ASSERT(12, fd.nFileSizeLow);
    ASSERT(0, wcscmp(fd.cFileName, L"проба winapi.txt"));
    FindClose(f);
    ASSERT(1, GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES);
    ASSERT(1, DeleteFileA(name));
    ASSERT(ERROR_FILE_NOT_FOUND, (DeleteFileA(name), GetLastError()));

    // threads and a critical section
    InitializeCriticalSection(&g_cs);
    HANDLE t[3];
    for (int i = 0; i < 3; i++)
        t[i] = CreateThread(NULL, 0, worker, (LPVOID)(INT_PTR)(i + 1), 0, NULL);
    ASSERT(WAIT_OBJECT_0, WaitForMultipleObjects(3, t, TRUE, 10000));
    DWORD code = 0;
    GetExitCodeThread(t[2], &code);
    ASSERT(30, code);
    for (int i = 0; i < 3; i++) CloseHandle(t[i]);
    DeleteCriticalSection(&g_cs);
    ASSERT(6000, g_sum);

    // time, the heap
    LARGE_INTEGER fq, c1;
    ASSERT(1, QueryPerformanceFrequency(&fq) && fq.QuadPart > 0);
    QueryPerformanceCounter(&c1);
    ASSERT(1, c1.QuadPart > 0);
    char *p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 100);
    ASSERT(0, p[99]);
    HeapFree(GetProcessHeap(), 0, p);

    // the registry: a value Windows always has
    HKEY key;
    ASSERT(ERROR_SUCCESS, RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key));
    char product[256];
    DWORD size = sizeof(product), type = 0;
    ASSERT(ERROR_SUCCESS, RegQueryValueExA(key, "ProductName", NULL, &type, (LPBYTE)product, &size));
    ASSERT(REG_SZ, type);
    ASSERT(1, strstr(product, "Windows") != 0);
    RegCloseKey(key);

    // UTF-8 is the code page of the program (the manifest)
    ASSERT(65001, GetACP());
    WCHAR w[32];
    ASSERT(7, MultiByteToWideChar(CP_ACP, 0, "Привет", -1, w, 32));
    ASSERT(0x41F, w[0]);

    // a hidden window: its class, its procedure, messages
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "WinCCTest";
    ASSERT(1, RegisterClassExA(&wc) != 0);
    HWND hwnd = CreateWindowExA(0, "WinCCTest", "Проверка", WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, wc.hInstance, NULL);
    ASSERT(1, hwnd != NULL);
    ASSERT(1, g_created);
    ASSERT(99, (int)SendMessageA(hwnd, WM_USER + 7, 3, 4));
    ASSERT(7, g_user_msg);
    char title[64];
    GetWindowTextA(hwnd, title, sizeof(title));
    ASSERT(0, strcmp(title, "Проверка"));
    WCHAR wtitle[64];
    GetWindowTextW(hwnd, wtitle, 64);
    ASSERT(0x41F, wtitle[0]);

    // GDI: UTF-8 text measures as the same text in UTF-16
    HDC dc = GetDC(hwnd);
    SIZE s8, s16;
    ASSERT(1, GetTextExtentPoint32A(dc, "Ширина", 12, &s8));
    ASSERT(1, GetTextExtentPoint32W(dc, L"Ширина", 6, &s16));
    ASSERT(s16.cx, s8.cx);
    RECT r = { 0, 0, 0, 0 };
    DrawTextA(dc, "Текст", -1, &r, DT_CALCRECT | DT_SINGLELINE);
    ASSERT(1, r.right > 0);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);

    // PostQuitMessage and the message loop
    PostQuitMessage(5);
    MSG m;
    ASSERT(0, GetMessageA(&m, NULL, 0, 0));
    ASSERT(5, (int)m.wParam);
    return g_fail;
}
#endif
