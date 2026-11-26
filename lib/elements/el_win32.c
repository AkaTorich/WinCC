// Платформенный слой Windows: окно, GDI+, цикл сообщений, ввод, буфер кадра.
#include "el_internal.h"
#include <ctype.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static const WCHAR *CLS = L"ElementsWindow";
static int s_cls = 0;

static void push_key(int vk, WCHAR ch) {
    KeyEv k;
    k.vk = vk; k.ch = ch; k.down = 1; k.mods = el_mods();
    VPUSH(g.keys, k);
    g.dirty = 1;
}

static void monitor_work(HWND hwnd, MONITORINFO *mi) {
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    memset(mi, 0, sizeof *mi);
    mi->cbSize = sizeof *mi;
    GetMonitorInfo(mon, mi);
}

static LRESULT nc_hittest(HWND hwnd, LPARAM lp) {
    POINT pt;
    RECT wr;
    int cx, cy, w, h, b;
    pt.x = (short)LOWORD(lp); pt.y = (short)HIWORD(lp);
    GetWindowRect(hwnd, &wr);
    cx = pt.x - wr.left; cy = pt.y - wr.top;
    w = wr.right - wr.left; h = wr.bottom - wr.top;
    b = (int)(6 * g.dpi);
    if (g.resizable) {               // края окна тянутся для любой формы (у безрамочных — по непрозрачным краям)
        int L = cx < b, R = cx >= w - b, T = cy < b, B = cy >= h - b;
        if (T && L) return HTTOPLEFT;
        if (T && R) return HTTOPRIGHT;
        if (B && L) return HTBOTTOMLEFT;
        if (B && R) return HTBOTTOMRIGHT;
        if (L) return HTLEFT;
        if (R) return HTRIGHT;
        if (T) return HTTOP;
        if (B) return HTBOTTOM;
    }
    if (valid(g.drag_region)) {
        Element *dr = E(g.drag_region);
        if (cx >= dr->ax && cx < dr->ax + dr->aw && cy >= dr->ay && cy < dr->ay + dr->ah) {
            int a = hit_test(g.window, cx, cy);
            int k = valid(a) ? E(a)->kind : 0;
            int ctrl = (k == K_BUTTON || k == K_TOOL || k == K_CHECK || k == K_RADIO || k == K_EDIT || k == K_MEMO ||
                        k == K_PASSWORD || k == K_COMBO || k == K_LIST || k == K_SLIDER || k == K_NUMBER || k == K_MENU ||
                        k == K_TAB || (k == K_PAINT && E(a)->on_click));
            if (!ctrl) return HTCAPTION;
        }
    }
    return HTCLIENT;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE:
        g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp);
        g.dirty = 1;
        return 0;
    case WM_LBUTTONDOWN: g.buttons |= 1; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); SetCapture(hwnd); g.dirty = 1; return 0;
    case WM_LBUTTONUP:   g.buttons &= ~1; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); if (!g.buttons) ReleaseCapture(); g.dirty = 1; return 0;
    case WM_RBUTTONDOWN: g.buttons |= 2; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); SetCapture(hwnd); g.dirty = 1; return 0;
    case WM_RBUTTONUP:   g.buttons &= ~2; g.mx = (short)LOWORD(lp); g.my = (short)HIWORD(lp); if (!g.buttons) ReleaseCapture(); g.dirty = 1; return 0;
    case WM_MOUSEWHEEL:  g.wheel += GET_WHEEL_DELTA_WPARAM(wp) / 120; g.dirty = 1; return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        push_key((int)wp, 0);
        if (msg == WM_SYSKEYDOWN && wp == VK_F4) return DefWindowProcW(hwnd, msg, wp, lp); // Alt+F4 → close
        return 0;
    case WM_CHAR:
        push_key(0, (WCHAR)wp);
        return 0;
    case WM_SIZE:
        g.dirty = 1;
        return 0;
    case WM_WINDOWPOSCHANGED:
        win_apply_shape();
        g.dirty = 1;
        break;                       // пусть отработает и DefWindowProc
    case WM_NCCALCSIZE:
        if (g.frameless && wp) {
            if (IsZoomed(hwnd)) {
                NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *)lp;
                MONITORINFO mi;
                monitor_work(hwnd, &mi);
                p->rgrc[0] = mi.rcWork;
            }
            return 0;                // клиент занимает всё окно (своя рамка)
        }
        break;
    case WM_NCHITTEST:
        if (!g.frameless) break;
        return nc_hittest(hwnd, lp);
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        if (g.win_minw > 0) mm->ptMinTrackSize.x = px(g.win_minw);
        if (g.win_minh > 0) mm->ptMinTrackSize.y = px(g.win_minh);
        if (g.win_maxw > 0) mm->ptMaxTrackSize.x = px(g.win_maxw);
        if (g.win_maxh > 0) mm->ptMaxTrackSize.y = px(g.win_maxh);
        if (g.frameless) {
            MONITORINFO mi;
            monitor_work(hwnd, &mi);
            mm->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mm->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mm->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mm->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0; }
    case WM_SIZING:
        if (g.aspect_w > 0 && g.aspect_h > 0) {
            RECT *r = (RECT *)lp;
            int w = r->right - r->left, h = r->bottom - r->top;
            if (wp == WMSZ_TOP || wp == WMSZ_BOTTOM) r->right = r->left + h * g.aspect_w / g.aspect_h;
            else r->bottom = r->top + w * g.aspect_h / g.aspect_w;
            return TRUE;
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g.memdc && !g.layered) BitBlt(dc, 0, 0, g.back_w, g.back_h, g.memdc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0; }
    case WM_CLOSE:
        g.close_req = 1; g.dirty = 1;
        return 0;                 // закрытие решает el_frame
    case WM_DESTROY:
        g.quit = 1;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void gdip_init(void) {
    GdiplusStartupInput si;
    // DPI-осознанность (если доступно)
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (u) {
        typedef BOOL(WINAPI *SetCtx)(HANDLE);
        typedef BOOL(WINAPI *SetDA)(void);
        SetCtx setctx = (SetCtx)GetProcAddress(u, "SetProcessDpiAwarenessContext");
        if (setctx) setctx((HANDLE)(INT_PTR)-4);          // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        else { SetDA sda = (SetDA)GetProcAddress(u, "SetProcessDPIAware"); if (sda) sda(); }
    }
    memset(&si, 0, sizeof si);
    si.GdiplusVersion = 1;
    GdiplusStartup(&g.gdip_token, &si, NULL);
    GdipCreateBitmapFromScan0(4, 4, 0, GP_PIXFMT_32BPP_PARGB, NULL, &g.measure_bmp);
    if (g.measure_bmp) GdipGetImageGraphicsContext(g.measure_bmp, &g.measure_g);
    if (g.measure_g) GdipSetTextRenderingHint(g.measure_g, GP_TEXT_ANTIALIAS_GRIDFIT);
}

void win_create(const char *title, int w, int h) {
    HINSTANCE hi = GetModuleHandle(NULL);
    HDC sdc;
    RECT r;
    DWORD style = WS_OVERLAPPEDWINDOW;
    HWND hwnd;
    HMODULE u;
    WCHAR *wt;
    int dpiX;
    if (!s_cls) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = CLS;
        wc.hbrBackground = NULL;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&wc);
        s_cls = 1;
    }
    // ищем DPI основного монитора заранее, чтобы задать физический размер
    sdc = GetDC(NULL); dpiX = GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(NULL, sdc);
    g.dpi = dpiX / 96.0;
    if (g.dpi < 1.0) g.dpi = 1.0;

    r.left = 0; r.top = 0; r.right = px(w); r.bottom = px(h);
    AdjustWindowRect(&r, style, FALSE);
    wt = wdup(title);
    hwnd = CreateWindowExW(0, CLS, wt, style,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        NULL, NULL, hi, NULL);
    free(wt);
    g.hwnd = hwnd;
    // уточняем DPI окна
    u = GetModuleHandleW(L"user32.dll");
    if (u) {
        typedef UINT(WINAPI *GetDpiW)(HWND);
        GetDpiW gd = (GetDpiW)GetProcAddress(u, "GetDpiForWindow");
        if (gd) { UINT d = gd(hwnd); if (d) g.dpi = d / 96.0; }
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void win_set_title(const char *t) {
    WCHAR *w;
    if (!g.hwnd) return;
    w = wdup(t);
    SetWindowTextW(g.hwnd, w);
    free(w);
}

void win_client_size(int *w, int *h) {
    RECT rc;
    if (g.hwnd && GetClientRect(g.hwnd, &rc)) { if (w) *w = rc.right; if (h) *h = rc.bottom; }
    else { if (w) *w = 0; if (h) *h = 0; }
}

void ensure_back(int w, int h) {
    BITMAPINFO bi;
    HDC sdc;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (g.back && g.back_w == w && g.back_h == h) return;
    if (g.g) { GdipDeleteGraphics(g.g); g.g = NULL; }
    if (g.back) { GdipDisposeImage(g.back); g.back = NULL; }
    if (g.memdc) { DeleteDC(g.memdc); g.memdc = NULL; }
    if (g.dib) { DeleteObject(g.dib); g.dib = NULL; }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;   // сверху вниз
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    sdc = GetDC(NULL);
    g.dib = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &g.dibbits, NULL, 0);
    ReleaseDC(NULL, sdc);
    g.memdc = CreateCompatibleDC(NULL);
    SelectObject(g.memdc, g.dib);
    GdipCreateBitmapFromScan0(w, h, w * 4, GP_PIXFMT_32BPP_PARGB, (BYTE *)g.dibbits, &g.back);
    GdipGetImageGraphicsContext(g.back, &g.g);
    GdipSetSmoothingMode(g.g, GP_SMOOTH_ANTIALIAS);
    GdipSetTextRenderingHint(g.g, g.layered ? GP_TEXT_ANTIALIAS : GP_TEXT_ANTIALIAS_GRIDFIT);
    GdipSetPixelOffsetMode(g.g, GP_PIXOFF_HALF);
    GdipSetInterpolationMode(g.g, GP_INTERP_HQ_BICUBIC);
    g.back_w = w; g.back_h = h;
}

void present(void) {
    if (!g.hwnd || !g.memdc) return;
    GdipFlush(g.g, GP_FLUSH);
    if (g.layered) {
        RECT wr;
        POINT pos, src;
        SIZE sz;
        BLENDFUNCTION bf;
        HDC dc;
        GetWindowRect(g.hwnd, &wr);
        pos.x = wr.left; pos.y = wr.top;
        sz.cx = g.back_w; sz.cy = g.back_h;
        src.x = 0; src.y = 0;
        bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0;
        bf.SourceConstantAlpha = (BYTE)(g.win_opacity < 0 ? 255 : g.win_opacity);
        bf.AlphaFormat = AC_SRC_ALPHA;
        dc = GetDC(NULL);
        UpdateLayeredWindow(g.hwnd, dc, &pos, &sz, g.memdc, &src, 0, &bf, ULW_ALPHA);
        ReleaseDC(NULL, dc);
    } else {
        HDC dc = GetDC(g.hwnd);
        BitBlt(dc, 0, 0, g.back_w, g.back_h, g.memdc, 0, 0, SRCCOPY);
        ReleaseDC(g.hwnd, dc);
    }
}

void win_pump(void) {
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) { g.quit = 1; break; }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

void win_wait(int timeout_ms) {
    MsgWaitForMultipleObjectsEx(0, NULL, (DWORD)timeout_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
}

// --- клавиши окна ---
int el_mods(void) {
    int m = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) m |= EL_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= EL_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000) m |= EL_ALT;
    return m;
}

static int word_is(const char *s, size_t n, const char *w) { return strlen(w) == n && _strnicmp(s, w, n) == 0; }

static int parse_vk(const char *name, int *mods) {
    const char *p = name, *key = NULL;
    size_t kn = 0;
    *mods = 0;
    // разбор "Ctrl+Shift+S"
    while (*p) {
        const char *plus = strchr(p, '+');
        size_t n = plus ? (size_t)(plus - p) : strlen(p);
        if (word_is(p, n, "ctrl")) *mods |= EL_CTRL;
        else if (word_is(p, n, "shift")) *mods |= EL_SHIFT;
        else if (word_is(p, n, "alt")) *mods |= EL_ALT;
        else { key = p; kn = n; }
        if (!plus) break;
        p = plus + 1;
    }
    if (!key || !kn) return 0;
    if (word_is(key, kn, "enter") || word_is(key, kn, "return")) return VK_RETURN;
    if (word_is(key, kn, "esc") || word_is(key, kn, "escape")) return VK_ESCAPE;
    if (word_is(key, kn, "space")) return VK_SPACE;
    if (word_is(key, kn, "tab")) return VK_TAB;
    if (word_is(key, kn, "up")) return VK_UP;
    if (word_is(key, kn, "down")) return VK_DOWN;
    if (word_is(key, kn, "left")) return VK_LEFT;
    if (word_is(key, kn, "right")) return VK_RIGHT;
    if (word_is(key, kn, "pgup")) return VK_PRIOR;
    if (word_is(key, kn, "pgdn")) return VK_NEXT;
    if (word_is(key, kn, "home")) return VK_HOME;
    if (word_is(key, kn, "end")) return VK_END;
    if (word_is(key, kn, "del") || word_is(key, kn, "delete")) return VK_DELETE;
    if (kn >= 2 && (key[0] == 'f' || key[0] == 'F')) {
        int n = atoi(key + 1);
        if (n >= 1 && n <= 24) return VK_F1 + (n - 1);
    }
    if (kn == 1) return toupper((unsigned char)key[0]);
    return 0;
}

int el_hotkey(const char *combo, el_fn fn, void *arg) {
    Hotkey h;
    if (!combo) return 0;
    h.vk = parse_vk(combo, &h.mods); h.fn = fn; h.arg = arg;
    if (!h.vk) return 0;
    VPUSH(g.hotkeys, h);
    return g.hotkeys.n;
}

// --------------------------------------------- нестандартные окна ----------

static void set_exstyle(LONG add, LONG remove) {
    LONG ex = GetWindowLong(g.hwnd, GWL_EXSTYLE);
    ex = (ex & ~remove) | add;
    SetWindowLong(g.hwnd, GWL_EXSTYLE, ex);
}

void el_frameless(int on) {
    LONG st;
    if (!g.hwnd) return;
    g.frameless = on != 0;
    st = GetWindowLong(g.hwnd, GWL_STYLE);
    if (on) st = (st & ~WS_CAPTION) | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
    else st = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    SetWindowLong(g.hwnd, GWL_STYLE, st);
    SetWindowPos(g.hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    win_apply_shape();
    g.dirty = 1;
}

void win_apply_shape(void) {
    int w = 0, h = 0, n, i;
    HRGN rgn = NULL;
    if (!g.hwnd) return;
    if (g.layered) { SetWindowRgn(g.hwnd, NULL, TRUE); return; }
    win_client_size(&w, &h);
    if (w < 1 || h < 1) return;
    switch (g.win_shape) {
    case 1: rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, px(g.win_round) * 2, px(g.win_round) * 2); break;
    case 2: rgn = CreateEllipticRgn(0, 0, w + 1, h + 1); break;
    case 3:
        n = g.region_pts.n / 2;
        if (n >= 3) {
            POINT *pts = (POINT *)malloc(sizeof(POINT) * (size_t)n);
            if (pts) {
                for (i = 0; i < n; i++) { pts[i].x = px(g.region_pts.a[i * 2]); pts[i].y = px(g.region_pts.a[i * 2 + 1]); }
                rgn = CreatePolygonRgn(pts, n, WINDING);
                free(pts);
            }
        }
        break;
    default: rgn = NULL; break;
    }
    SetWindowRgn(g.hwnd, rgn, TRUE);     // окно забирает владение rgn
}

void el_window_round(int radius) { g.win_shape = 1; g.win_round = radius; win_apply_shape(); g.dirty = 1; }

void el_window_shape(const char *shape) {
    if (!shape) return;
    if (strncmp(shape, "round", 5) == 0) {
        const char *c = strchr(shape, ':');
        g.win_shape = 1;
        g.win_round = c ? atoi(c + 1) : 16;
    }
    else if (!strcmp(shape, "ellipse")) g.win_shape = 2;
    else if (!strcmp(shape, "pixels")) {
        g.win_shape = 4; g.layered = 1;
        set_exstyle(WS_EX_LAYERED, 0);
        if (!g.frameless) el_frameless(1);
        if (g.g) GdipSetTextRenderingHint(g.g, GP_TEXT_ANTIALIAS);
    } else g.win_shape = 0;
    win_apply_shape();
    g.dirty = 1;
}

void el_window_region(const int *xy, int count) {
    int i;
    g.region_pts.n = 0;
    if (xy) for (i = 0; i < count * 2; i++) VPUSH(g.region_pts, xy[i]);
    g.win_shape = 3;
    win_apply_shape();
    g.dirty = 1;
}

void el_window_aspect(int w, int h) { g.aspect_w = w; g.aspect_h = h; }

void el_window_opacity(int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    g.win_opacity = alpha;
    if (!g.layered && g.hwnd) {          // равномерная прозрачность без послойного окна
        set_exstyle(WS_EX_LAYERED, 0);
        SetLayeredWindowAttributes(g.hwnd, 0, (BYTE)alpha, LWA_ALPHA);
    }
    g.dirty = 1;
}

void el_drag_region(el e) { if (valid(e)) g.drag_region = e; }

void el_window_minmax(int minw, int minh, int maxw, int maxh) {
    g.win_minw = minw; g.win_minh = minh; g.win_maxw = maxw; g.win_maxh = maxh;
}

void el_window_resizable(int on) {
    LONG st;
    g.resizable = on != 0;
    if (!g.hwnd) return;
    st = GetWindowLong(g.hwnd, GWL_STYLE);
    if (on) st |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    else st &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowLong(g.hwnd, GWL_STYLE, st);
    SetWindowPos(g.hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

el el_window_canvas(void (*fn)(el, int, int, void *), void *arg) {
    int h = el_alloc(K_PAINT);
    E(h)->paint_fn = fn; E(h)->paint_arg = arg; E(h)->needs_redraw = 1;
    g.win_canvas = h;
    request_redraw();
    return h;
}
