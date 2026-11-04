// WUI — сообщение в оформлении темы (как ThemedMessageBox): окно поверх своего, пока не ответят.
#include "wui_int.h"
#include <stdlib.h>

static int g_answer;

static void answer(WuiControl *b, void *user)
{
    g_answer = (int)(INT_PTR)user;
    wui_window_close(wui_window_of(b));
}

int wui_message(WuiControl *window, const char *title, const char *text, int buttons)
{
    HWND owner = window ? wui_hwnd(window) : 0;
    // высота — по тексту: заголовок, поля, текст, строка кнопок
    float keep = wui_paint_scale;
    wui_paint_scale = window ? window->win->scale : 1;
    WuiFont f = wui_font(WUI_S->Font_Body, WUI_S->Body_Size, 400);
    float tw, th;
    wui_text_size(&f, text, 440 - 68, &tw, &th);
    wui_paint_scale = keep;
    WuiControl *d = wui_window(title, 440, (int)(th + 150), 0);
    WuiWin *w = d->win;
    for (WuiControl *k = w->titlebar->first; k; k = k->next)
        if (k->type == WUI_T_CAPTION && k->style != 2) k->visible = 0;
    SetWindowLongPtrW(w->hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
    if (owner) {                                // по середине окна-хозяина
        RECT o, r;
        GetWindowRect(owner, &o);
        GetWindowRect(w->hwnd, &r);
        SetWindowPos(w->hwnd, 0, (o.left + o.right - (r.right - r.left)) / 2, (o.top + o.bottom - (r.bottom - r.top)) / 2,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    WuiControl *c = wui_content(d);
    wui_stretch(wui_label(c, text, WUI_LABEL_BODY), 1);
    WuiControl *row = wui_row(c);
    wui_align(row, WUI_ALIGN_END);
    WuiControl *first;
    g_answer = 0;
    if (buttons == WUI_MSG_YES_NO) {
        first = wui_button(row, "Да", WUI_BUTTON_ACCENT);
        wui_on_click(first, answer, (void *)1);
        wui_on_click(wui_button(row, "Нет", WUI_BUTTON_NORMAL), answer, (void *)0);
    }
    else {
        first = wui_button(row, "ОК", WUI_BUTTON_ACCENT);
        wui_on_click(first, answer, (void *)1);
        if (buttons == WUI_MSG_OK_CANCEL) wui_on_click(wui_button(row, "Отмена", WUI_BUTTON_NORMAL), answer, (void *)0);
    }
    wui_focus(first);
    if (owner) EnableWindow(owner, FALSE);
    // свой круг сообщений, пока окно открыто
    HWND h = w->hwnd;
    MSG m;
    int quit = 0;
    while (IsWindow(h)) {
        if (GetMessageW(&m, 0, 0, 0) <= 0) { quit = 1; break; }
        if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE && m.hwnd == h) { g_answer = 0; DestroyWindow(h); continue; }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    if (quit) PostQuitMessage((int)m.wParam);
    return g_answer;
}
