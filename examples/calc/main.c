// Калькулятор на WinCC: окно, мышь, клавиатура, буфер обмена, масштаб экрана.
//   wcc main.c engine.c draw.c -o Калькулятор.exe
//   Калькулятор.exe --демо=12+3*4=   — сам «нажимает» эти кнопки (для снимков)
#include "calc.h"
#include <gdiplus.h>
#include <stdio.h>
#include <string.h>
#pragma comment(lib, "dwmapi")
#pragma comment(linker, "/MANIFEST")    // манифест WinCC: A-функции берут текст в UTF-8

HRESULT DwmSetWindowAttribute(HWND hwnd, DWORD attr, const void *value, DWORD size);

static View g_view;
static HWND g_hwnd;
static int g_animating;
static char g_demo[256];
static int g_demo_pos;

enum { TIMER_ANIM = 1, TIMER_DEMO = 2 };

static void redraw(void) { InvalidateRect(g_hwnd, NULL, FALSE); }

static void animate(void)
{
    if (!g_animating) {
        g_animating = 1;
        SetTimer(g_hwnd, TIMER_ANIM, 15, NULL);
    }
}

// один кадр: подсветка плавно догоняет мышь, вспышка нажатия гаснет
static void step_animation(void)
{
    int moving = 0;
    for (int i = 0; i < BUTTONS; i++) {
        float target = i == g_view.hot ? 1.0f : 0.0f;
        float d = target - g_view.hover[i];
        if (d > 0.01f || d < -0.01f) { g_view.hover[i] += d * 0.28f; moving = 1; }
        else g_view.hover[i] = target;
        if (i == g_view.held) g_view.press[i] = 1;
        else if (g_view.press[i] > 0.01f) { g_view.press[i] *= 0.72f; moving = 1; }
        else g_view.press[i] = 0;
    }
    if (!moving) {
        KillTimer(g_hwnd, TIMER_ANIM);
        g_animating = 0;
    }
    redraw();
}

static void act(int i)
{
    const Button *b = &g_buttons[i];
    Calc *c = &g_view.calc;
    switch (b->key) {
    case K_DIGIT: calc_digit(c, b->arg); break;
    case K_COMMA: calc_comma(c); break;
    case K_OP: calc_op(c, (char)b->arg); break;
    case K_EQ: calc_equals(c); g_view.hist_scroll = 0; break;
    case K_CLEAR: calc_clear(c); break;
    case K_BACK: calc_back(c); break;
    case K_PERCENT: calc_percent(c); break;
    case K_SIGN: calc_sign(c); break;
    }
    g_view.press[i] = 1;
    animate();
    redraw();
}

static int find_button(int key, int arg)
{
    for (int i = 0; i < BUTTONS; i++)
        if (g_buttons[i].key == key && (g_buttons[i].arg == arg || key != K_DIGIT && key != K_OP))
            return i;
    return -1;
}

static int inside(Box b, int x, int y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

static int button_at(int x, int y)
{
    for (int i = 0; i < BUTTONS; i++)
        if (inside(g_view.btn[i], x, y)) return i;
    return -1;
}

static int history_at(int x, int y)
{
    if (!g_view.show_history) return -1;
    for (int k = 0; k < g_view.hist_shown; k++)
        if (inside(g_view.hist_item[k], x, y)) return k;
    return -1;
}

// ---- буфер обмена ----

static void copy_result(void)
{
    // само число, без украшений табло: «1234,5», «9,75461057814e+17»
    double v;
    char plain[64];
    if (!calc_value(&g_view.calc, &v)) return;
    snprintf(plain, sizeof(plain), "%.15g", v);
    for (char *p = plain; *p; p++)
        if (*p == '.') *p = ',';
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    int n = MultiByteToWideChar(CP_UTF8, 0, plain, -1, 0, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)n * sizeof(WCHAR));
    if (h) {
        MultiByteToWideChar(CP_UTF8, 0, plain, -1, (WCHAR *)GlobalLock(h), n);
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
    }
    CloseClipboard();
}

static void paste_number(void)
{
    if (!OpenClipboard(g_hwnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        char text[128];
        WideCharToMultiByte(CP_UTF8, 0, (const WCHAR *)GlobalLock(h), -1, text, sizeof(text), 0, 0);
        GlobalUnlock(h);
        double v;
        if (parse_number(text, &v)) {
            calc_set(&g_view.calc, v);
            redraw();
        } else {
            MessageBeep(MB_ICONWARNING);
        }
    }
    CloseClipboard();
}

// ---- клавиатура ----

static void key_char(int ch)
{
    int i = -1;
    if (ch >= '0' && ch <= '9') i = find_button(K_DIGIT, ch - '0');
    else if (ch == ',' || ch == '.') i = find_button(K_COMMA, 0);
    else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') i = find_button(K_OP, ch);
    else if (ch == '=' || ch == '\r') i = find_button(K_EQ, 0);
    else if (ch == '\b') i = find_button(K_BACK, 0);
    else if (ch == '%') i = find_button(K_PERCENT, 0);
    else if (ch == 27) { calc_reset(&g_view.calc); i = find_button(K_CLEAR, 0); g_view.press[i] = 1; animate(); redraw(); return; }
    else if (ch == 3) { copy_result(); return; }      // Ctrl+C
    else if (ch == 22) { paste_number(); return; }    // Ctrl+V
    if (i >= 0) act(i);
}

// ---- окно ----

static void set_scale(UINT dpi)
{
    g_view.scale = dpi ? dpi / 96.0f : 1.0f;
    HICON big = make_icon((int)(32 * g_view.scale)), small = make_icon((int)(16 * g_view.scale));
    if (big) SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)big);
    if (small) SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        g_view.width = LOWORD(lp);
        g_view.height = HIWORD(lp);
        layout(&g_view);
        redraw();
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        float s = g_view.scale > 0 ? g_view.scale : 1;
        mm->ptMinTrackSize.x = (LONG)(340 * s);
        mm->ptMinTrackSize.y = (LONG)(540 * s);
        return 0;
    }
    case WM_DPICHANGED: {
        set_scale(LOWORD(wp));
        RECT *r = (RECT *)lp;
        SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;                       // фон рисует paint() целиком
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_view.width > 0 && g_view.height > 0) paint(&g_view, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int hot = button_at(x, y), hh = history_at(x, y);
        int ch = g_view.show_history && history_count() > 0 && inside(g_view.hist_clear, x, y);
        if (hot != g_view.hot || hh != g_view.hist_hot || ch != g_view.hist_clear_hot) {
            g_view.hot = hot;
            g_view.hist_hot = hh;
            g_view.hist_clear_hot = ch;
            animate();
        }
        TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_view.hot = g_view.hist_hot = -1;
        g_view.hist_clear_hot = 0;
        animate();
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            int link = button_at(p.x, p.y) >= 0 || history_at(p.x, p.y) >= 0 ||
                       (g_view.show_history && history_count() > 0 && inside(g_view.hist_clear, p.x, p.y));
            SetCursor(LoadCursorW(NULL, link ? (LPCWSTR)IDC_HAND : (LPCWSTR)IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        int i = button_at(x, y);
        if (i >= 0) {
            SetCapture(hwnd);
            g_view.held = i;
            act(i);
            return 0;
        }
        int k = history_at(x, y);
        if (k >= 0) {                   // число из журнала — снова на табло
            calc_set(&g_view.calc, history_value(g_view.hist_index[k]));
            redraw();
            return 0;
        }
        if (g_view.show_history && history_count() > 0 && inside(g_view.hist_clear, x, y)) {
            history_clear();
            g_view.hist_hot = -1;
            redraw();
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_view.held >= 0) {
            ReleaseCapture();
            g_view.held = -1;
            animate();
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (g_view.show_history) {
            g_view.hist_scroll -= GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            redraw();
        }
        return 0;
    case WM_CHAR:
        key_char((int)wp);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_DELETE) { int i = find_button(K_CLEAR, 0); if (i >= 0) act(i); }
        else if (wp == VK_F9) { int i = find_button(K_SIGN, 0); if (i >= 0) act(i); }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_ANIM) step_animation();
        else if (wp == TIMER_DEMO) {
            if (g_demo[g_demo_pos]) {
                char ch = g_demo[g_demo_pos++];
                key_char(ch == 'C' ? 27 : ch);
            } else {
                KillTimer(hwnd, TIMER_DEMO);
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    // чёткая картинка на любом масштабе экрана (Windows 10 1703 и новее)
    typedef BOOL (*SetAwareness)(HANDLE);
    SetAwareness aware = (SetAwareness)GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext");
    if (aware) aware((HANDLE)(INT_PTR)-4);          // PER_MONITOR_AWARE_V2

    const char *d = strstr(cmd, "--демо=");
    if (d) snprintf(g_demo, sizeof(g_demo), "%s", d + strlen("--демо="));

    ULONG_PTR token;
    GdiplusStartupInput gin = { 1, 0, FALSE, FALSE };
    if (GdiplusStartup(&token, &gin, 0) != 0) {
        MessageBoxA(NULL, "Не удалось запустить GDI+", "Калькулятор", MB_ICONERROR);
        return 1;
    }
    draw_start();
    calc_reset(&g_view.calc);
    g_view.hot = g_view.held = g_view.hist_hot = -1;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"WinCCCalc";
    RegisterClassExW(&wc);

    float s = GetDpiForSystem() / 96.0f;
    g_hwnd = CreateWindowExW(0, L"WinCCCalc", L"Калькулятор", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, (int)(700 * s), (int)(620 * s), NULL, NULL, inst, NULL);
    set_scale(GetDpiForWindow(g_hwnd));
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(g_hwnd, 20, &dark, sizeof(dark))))       // тёмный заголовок
        DwmSetWindowAttribute(g_hwnd, 19, &dark, sizeof(dark));
    COLORREF caption = RGB(0x1E, 0x21, 0x29);
    DwmSetWindowAttribute(g_hwnd, 35, &caption, sizeof(caption));             // цвет заголовка (Windows 11)

    // окно по центру экрана, размер — под масштаб этого экрана
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int w = (int)(700 * g_view.scale), h = (int)(620 * g_view.scale);
    SetWindowPos(g_hwnd, NULL, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, w, h, SWP_NOZORDER);
    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);
    if (g_demo[0]) SetTimer(g_hwnd, TIMER_DEMO, 90, NULL);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    draw_stop();
    GdiplusShutdown(token);
    return (int)m.wParam;
}
