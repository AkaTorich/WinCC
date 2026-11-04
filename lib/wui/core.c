// WUI — общее: запуск GDI+, текущая тема и её запоминание, строки UTF-8/UTF-16.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const WuiTheme *wui_t = &wui_theme_list[0];
static int g_ready;
static ULONG_PTR g_token;
static char g_key[128];

void wui_init(void)
{
    if (g_ready) return;
    g_ready = 1;
    // каждый монитор — свой масштаб (Windows 10 1703 и новее); на старых — как получится
    HMODULE u = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *SetCtx)(HANDLE);
    SetCtx set = (SetCtx)GetProcAddress(u, "SetProcessDpiAwarenessContext");
    if (!set || !set((HANDLE)(INT_PTR)-4))                 // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        SetProcessDPIAware();
    GdiplusStartupInput in = { 1 };
    GdiplusStartup(&g_token, &in, NULL);
}

char *wui_strdup(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    memcpy(d, s, n);
    return d;
}

wchar_t *wui_wide(const char *s)
{
    if (!s) s = "";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, 0, 0);
    wchar_t *w = malloc(sizeof(wchar_t) * (size_t)(n + 1));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    w[n] = 0;
    return w;
}

char *wui_utf8(const wchar_t *w, int len)
{
    if (len < 0) len = (int)wcslen(w);
    int n = WideCharToMultiByte(CP_UTF8, 0, w, len, 0, 0, 0, 0);
    char *s = malloc((size_t)n + 1);
    WideCharToMultiByte(CP_UTF8, 0, w, len, s, n, 0, 0);
    s[n] = 0;
    return s;
}

// ---- темы ----

int wui_theme_count(void) { return wui_theme_total; }

static int find(const char *id)
{
    for (int i = 0; i < wui_theme_total; i++)
        if (id && lstrcmpiA(wui_theme_list[i].id, id) == 0)
            return i;
    return -1;
}

const char *wui_theme_id(int i) { return i >= 0 && i < wui_theme_total ? wui_theme_list[i].id : ""; }
const char *wui_theme_name(int i) { return i >= 0 && i < wui_theme_total ? wui_theme_list[i].name : ""; }
const char *wui_theme_group(int i) { return i >= 0 && i < wui_theme_total ? wui_theme_list[i].group : ""; }
int wui_theme_current(void) { return (int)(wui_t - wui_theme_list); }

// Выбор хранится так же, как у ThemeLibrary: %APPDATA%\ThemeLibrary\<ключ>.theme, одна строка — Id темы.
static wchar_t *theme_file(int make_folder)
{
    static wchar_t path[MAX_PATH * 2];
    wchar_t app[MAX_PATH];
    if (!g_key[0] || !GetEnvironmentVariableW(L"APPDATA", app, MAX_PATH)) return 0;
    wchar_t *key = wui_wide(g_key);
    swprintf(path, MAX_PATH * 2, L"%ls\\ThemeLibrary", app);
    if (make_folder) CreateDirectoryW(path, 0);
    swprintf(path, MAX_PATH * 2, L"%ls\\ThemeLibrary\\%ls.theme", app, key);
    free(key);
    return path;
}

void wui_theme_load(const char *key)
{
    if (!key || !*key || g_key[0]) return;
    strncpy(g_key, key, sizeof g_key - 1);
    wchar_t *f = theme_file(0);
    if (!f) return;
    HANDLE h = CreateFileW(f, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    char buf[128] = { 0 };
    DWORD got = 0;
    ReadFile(h, buf, sizeof buf - 1, &got, 0);
    CloseHandle(h);
    char *s = buf;
    if ((unsigned char)s[0] == 0xEF) s += 3;                // метка UTF-8
    for (int i = (int)strlen(s) - 1; i >= 0 && (s[i] == '\r' || s[i] == '\n' || s[i] == ' '); i--) s[i] = 0;
    int i = find(s);
    if (i >= 0) wui_t = &wui_theme_list[i];
}

void wui_theme_set(const char *id)
{
    int i = find(id);
    if (i < 0) return;
    wui_t = &wui_theme_list[i];
    for (WuiWin *w = wui_windows; w; w = w->next)
        wui_window_retheme(w);
    wchar_t *f = theme_file(1);
    if (!f) return;
    HANDLE h = CreateFileW(f, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD put;
    WriteFile(h, wui_t->id, (DWORD)strlen(wui_t->id), &put, 0);
    CloseHandle(h);
}

static WuiColor pick(int which)
{
    switch (which) {
    case WUI_TEXT_DIM: return wui_t->c[WUI_C_TEXTDIM];
    case WUI_ACCENT: return wui_t->c[WUI_C_ACCENT];
    case WUI_ACCENT2: return wui_t->c[WUI_C_ACCENT2];
    case WUI_SUCCESS: return wui_t->c[WUI_C_SUCCESS];
    case WUI_WARNING: return wui_t->c[WUI_C_WARNING];
    case WUI_DANGER: return wui_t->c[WUI_C_DANGER];
    case WUI_SURFACE: return wui_t->c[WUI_C_SURFACE];
    default: return wui_t->c[WUI_C_TEXT];
    }
}

ARGB wui_argb(int which) { return pick(which); }

COLORREF wui_color(int which)
{
    WuiColor c = pick(which);
    return RGB(WUI_R(c), WUI_G(c), WUI_B(c));
}
