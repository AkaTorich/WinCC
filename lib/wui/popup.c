// WUI — выпадающий список: своё окно поверх всех (не забирает фокус у хозяина), тень, прокрутка
// колесом, выбор мышью и клавишами. Вид — Popup раскрывающегося списка ThemeLibrary.
#include "wui_int.h"
#include <stdlib.h>
#include <string.h>

#define MARGIN_L 10                 // поле под тень: Margin="10,2,10,16"
#define MARGIN_T 2
#define MARGIN_R 10
#define MARGIN_B 16
#define MAX_H 320

static struct {
    HWND hwnd;
    WuiControl *owner;
    WuiPopupItem *items;
    int count, selected, hot;
    float scroll, scroll_max, item_h, w, h, scale;
    WuiPopupDone done;
} P;

static WuiFont item_font(int header)
{
    return wui_font(WUI_S->Field_FontFamily, WUI_S->Field_FontSize, header ? 600 : 400);
}

static WuiRect list_rect(void)
{
    WuiRect r = { MARGIN_L, MARGIN_T, P.w, P.h };
    return r;
}

static void render(void)
{
    float s = P.scale;
    int pw = (int)((P.w + MARGIN_L + MARGIN_R) * s + 0.5f), ph = (int)((P.h + MARGIN_T + MARGIN_B) * s + 0.5f);
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), pw, -ph, 1, 32 } };
    void *bits;
    HDC screen = GetDC(0), dc = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, 0, 0);
    HGDIOBJ old = SelectObject(dc, dib);
    GpBitmap *surface;
    GdipCreateBitmapFromScan0(pw, ph, pw * 4, PixelFormat32bppPARGB, bits, &surface);
    GpGraphics *g;
    GdipGetImageGraphicsContext(surface, &g);
    GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
    GdipSetTextRenderingHint(g, TextRenderingHintClearTypeGridFit);
    GdipScaleWorldTransform(g, s, s, MatrixOrderAppend);
    float keep = wui_paint_scale;
    wui_paint_scale = s;

    const WuiStyle *st = WUI_S;
    WuiRect r = list_rect();
    WuiCorners k = st->Popup_CornerRadius;
    wui_effect(g, st->Popup_Effect, st->Popup_Background, r, k, 1);
    wui_fill(g, st->Popup_Background, r, k, 1);

    WuiRect in = wui_inset(r, 5, 5, 5, 5);
    UINT state;
    GdipSaveGraphics(g, &state);
    GdipSetClipRect(g, in.x, in.y, in.w, in.h, CombineModeIntersect);
    float y = in.y - P.scroll;
    float right = P.scroll_max > 0 ? 10 : 0;
    for (int i = 0; i < P.count; i++, y += P.item_h) {
        if (y + P.item_h < in.y || y > in.y + in.h) continue;
        WuiRect ir = { in.x, y, in.w - right, P.item_h };
        WuiFont f = item_font(P.items[i].header);
        WuiColor fore = wui_brush_color(st->Text);
        if (P.items[i].header) fore = wui_alpha(fore, (int)(WUI_A(fore) * 0.45f));
        else if (i == P.selected) {
            wui_fill(g, st->Item_Selected, ir, st->Item_CornerRadius, 1);
            fore = wui_brush_color(st->Item_SelectedForeground);
        }
        else if (i == P.hot) wui_fill(g, st->Item_Hover, ir, st->Item_CornerRadius, 1);
        wui_draw_text(g, &f, P.items[i].text, wui_inset(ir, 10, 0, 10, 0), fore, 0, 0);
    }
    GdipRestoreGraphics(g, state);
    if (P.scroll_max > 0) {
        float view = in.h, th = view * view / (view + P.scroll_max);
        WuiRect t = { in.x + in.w - 7, in.y + (view - th) * P.scroll / P.scroll_max, 6, th };
        wui_fill(g, st->Scroll_Thumb, t, wui_corners(3), 1);
    }
    WuiEdges one = { 1, 1, 1, 1 };
    wui_stroke(g, st->Popup_Border, r, k, one, 1);

    wui_paint_scale = keep;
    GdipDeleteGraphics(g);
    GdipDisposeImage(surface);
    RECT wr;
    GetWindowRect(P.hwnd, &wr);
    POINT src = { 0, 0 }, at = { wr.left, wr.top };
    SIZE size = { pw, ph };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(P.hwnd, screen, &at, &size, dc, &src, 0, &bf, ULW_ALPHA);
    SelectObject(dc, old);
    DeleteObject(dib);
    DeleteDC(dc);
    ReleaseDC(0, screen);
}

static int item_at(float x, float y)
{
    WuiRect in = wui_inset(list_rect(), 5, 5, 5, 5);
    if (x < in.x || x >= in.x + in.w || y < in.y || y >= in.y + in.h) return -1;
    int i = (int)((y - in.y + P.scroll) / P.item_h);
    return i >= 0 && i < P.count && !P.items[i].header ? i : -1;
}

static void scroll_to(int i)
{
    WuiRect in = wui_inset(list_rect(), 5, 5, 5, 5);
    float top = i * P.item_h;
    if (top < P.scroll) P.scroll = top;
    if (top + P.item_h > P.scroll + in.h) P.scroll = top + P.item_h - in.h;
    if (P.scroll < 0) P.scroll = 0;
    if (P.scroll > P.scroll_max) P.scroll = P.scroll_max;
}

static void choose(int i)
{
    WuiControl *owner = P.owner;
    WuiPopupDone done = P.done;
    wui_popup_close();
    if (i >= 0 && done) done(owner, i);
}

static LRESULT CALLBACK popup_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
        int i = item_at((short)LOWORD(lp) / P.scale, (short)HIWORD(lp) / P.scale);
        if (i != P.hot) { P.hot = i; render(); }
        TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, h, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (P.hot >= 0) { P.hot = -1; render(); }
        return 0;
    case WM_LBUTTONUP: {
        int i = item_at((short)LOWORD(lp) / P.scale, (short)HIWORD(lp) / P.scale);
        if (i >= 0) choose(i);
        return 0;
    }
    case WM_MOUSEWHEEL:
        wui_popup_wheel((short)HIWORD(wp));
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(0, (LPCWSTR)IDC_ARROW));
        return 1;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void wui_popup_open(WuiControl *owner, const WuiPopupItem *items, int count, int selected, WuiPopupDone done)
{
    wui_popup_close();
    static int registered;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof wc };
        wc.lpfnWndProc = popup_proc;
        wc.hInstance = GetModuleHandleW(0);
        wc.lpszClassName = L"WuiPopup";
        wc.style = CS_DROPSHADOW * 0;
        RegisterClassExW(&wc);
        registered = 1;
    }
    WuiWin *w = owner->win;
    P.owner = owner;
    P.items = malloc(sizeof(WuiPopupItem) * (size_t)(count ? count : 1));
    for (int i = 0; i < count; i++) {
        P.items[i].text = wui_strdup(items[i].text);
        P.items[i].header = items[i].header;
    }
    P.count = count;
    P.selected = selected;
    P.hot = -1;
    P.done = done;
    P.scale = w->scale;
    float keep = wui_paint_scale;
    wui_paint_scale = w->scale;
    WuiFont f = item_font(0);
    P.item_h = wui_line_height(&f) + 12;
    float widest = owner->r.w;
    for (int i = 0; i < count; i++) {
        WuiFont fi = item_font(items[i].header);
        float tw, th;
        wui_text_size(&fi, items[i].text, 0, &tw, &th);
        if (tw + 20 + 10 + 14 > widest) widest = tw + 20 + 10 + 14;
    }
    wui_paint_scale = keep;
    float content = count * P.item_h + 10;
    P.w = widest;
    P.h = content > MAX_H ? MAX_H : content;
    P.scroll = 0;
    P.scroll_max = content - P.h > 0 ? content - P.h : 0;
    if (selected >= 0) scroll_to(selected);

    // под элементом, а не влезет на экран — над ним
    RECT wr;
    GetWindowRect(w->hwnd, &wr);
    float s = w->scale;
    int x = wr.left + (int)((owner->r.x - MARGIN_L) * s);
    int y = wr.top + (int)((owner->r.y + owner->r.h + 2 - MARGIN_T) * s);
    int pw = (int)((P.w + MARGIN_L + MARGIN_R) * s + 0.5f), ph = (int)((P.h + MARGIN_T + MARGIN_B) * s + 0.5f);
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromWindow(w->hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    if (y + ph > mi.rcWork.bottom) y = wr.top + (int)((owner->r.y - 2 - P.h - MARGIN_T) * s);
    if (x + pw > mi.rcWork.right) x = mi.rcWork.right - pw;
    P.hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"WuiPopup", L"",
                             WS_POPUP, x, y, pw, ph, w->hwnd, 0, GetModuleHandleW(0), 0);
    render();
    ShowWindow(P.hwnd, SW_SHOWNOACTIVATE);
    wui_invalidate(owner);
}

void wui_popup_close(void)
{
    if (!P.hwnd) return;
    HWND h = P.hwnd;
    P.hwnd = 0;
    DestroyWindow(h);
    for (int i = 0; i < P.count; i++) free((char *)P.items[i].text);
    free(P.items);
    P.items = 0;
    WuiControl *owner = P.owner;
    P.owner = 0;
    if (owner) wui_invalidate(owner);
}

int wui_popup_is_open(WuiControl *owner) { return P.hwnd && (!owner || P.owner == owner); }

int wui_popup_wheel(int delta)
{
    if (!P.hwnd) return 0;
    P.scroll -= delta / 120.0f * P.item_h * 3;
    if (P.scroll < 0) P.scroll = 0;
    if (P.scroll > P.scroll_max) P.scroll = P.scroll_max;
    render();
    return 1;
}

// стрелки, Enter, Escape — от окна-хозяина (у списка фокуса нет)
int wui_popup_key(int vk)
{
    if (!P.hwnd) return 0;
    int i = P.hot >= 0 ? P.hot : P.selected;
    if (vk == VK_ESCAPE) { wui_popup_close(); return 1; }
    if (vk == VK_RETURN || vk == VK_SPACE) { choose(i); return 1; }
    int step = vk == VK_DOWN ? 1 : vk == VK_UP ? -1 : vk == VK_NEXT ? 8 : vk == VK_PRIOR ? -8 : 0;
    if (vk == VK_HOME) { i = -1; step = 1; }
    if (vk == VK_END) { i = P.count; step = -1; }
    if (!step) return 1;
    int n = i + step;
    if (n < 0) n = 0;
    if (n >= P.count) n = P.count - 1;
    while (n >= 0 && n < P.count && P.items[n].header) n += step > 0 ? 1 : -1;
    if (n < 0 || n >= P.count) return 1;
    P.hot = n;
    scroll_to(n);
    render();
    return 1;
}
