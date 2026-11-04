// Оконная программа на WinCC: меню, кнопка, поле ввода, рисование и диалог открытия файла.
// Собрать:  wcc examples\window.c       (есть WinMain и нет main — получится оконная программа)
#include <windows.h>
#include <stdio.h>

enum { ID_BUTTON = 1, ID_EDIT, ID_OPEN, ID_EXIT, ID_ABOUT };

static HWND g_edit;
static int g_clicks;
static char g_file[MAX_PATH] = "файл не выбран";

static void open_file(HWND hwnd)
{
    char name[MAX_PATH] = "";
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Исходники Си\0*.c;*.h\0Все файлы\0*.*\0";
    ofn.lpstrFile = name;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        lstrcpyA(g_file, name);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

static void paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT r;
    GetClientRect(hwnd, &r);

    // круги радугой
    static const COLORREF colors[] = { RGB(230, 60, 60), RGB(240, 160, 40), RGB(230, 210, 50),
                                       RGB(70, 180, 80), RGB(60, 140, 230), RGB(130, 80, 200) };
    for (int i = 0; i < 6; i++) {
        HBRUSH b = CreateSolidBrush(colors[i]);
        HGDIOBJ old = SelectObject(dc, b);
        SelectObject(dc, GetStockObject(NULL_PEN));
        int x = 30 + i * 60;
        Ellipse(dc, x, 120, x + 50, 170);
        SelectObject(dc, old);
        DeleteObject(b);
    }

    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    HGDIOBJ oldf = SelectObject(dc, font);
    char line[400];
    sprintf(line, "Нажатий на кнопку: %d", g_clicks);
    TextOutA(dc, 30, 190, line, lstrlenA(line));
    SelectObject(dc, oldf);
    DeleteObject(font);

    RECT fr = { 30, 225, r.right - 20, r.bottom - 10 };
    DrawTextA(dc, g_file, -1, &fr, DT_WORDBREAK | DT_NOPREFIX);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        HFONT gui = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND b = CreateWindowExA(0, "BUTTON", "Нажми меня", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 30, 20, 160, 32, hwnd, (HMENU)ID_BUTTON, NULL, NULL);
        g_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "Привет из WinCC!", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 30, 65, 330, 26, hwnd, (HMENU)ID_EDIT, NULL, NULL);
        SendMessageA(b, WM_SETFONT, (WPARAM)gui, TRUE);
        SendMessageA(g_edit, WM_SETFONT, (WPARAM)gui, TRUE);

        HMENU menu = CreateMenu(), file = CreatePopupMenu(), help = CreatePopupMenu();
        AppendMenuA(file, MF_STRING, ID_OPEN, "&Открыть…");
        AppendMenuA(file, MF_SEPARATOR, 0, NULL);
        AppendMenuA(file, MF_STRING, ID_EXIT, "&Выход");
        AppendMenuA(help, MF_STRING, ID_ABOUT, "&О программе");
        AppendMenuA(menu, MF_POPUP, (UINT_PTR)file, "&Файл");
        AppendMenuA(menu, MF_POPUP, (UINT_PTR)help, "&Справка");
        SetMenu(hwnd, menu);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BUTTON: {
            char text[256];
            GetWindowTextA(g_edit, text, sizeof(text));
            g_clicks++;
            InvalidateRect(hwnd, NULL, TRUE);
            MessageBoxA(hwnd, text, "В поле написано", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case ID_OPEN:
            open_file(hwnd);
            return 0;
        case ID_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case ID_ABOUT:
            MessageBoxA(hwnd, "Окно собрано компилятором WinCC.", "О программе", MB_OK);
            return 0;
        }
        break;
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "WinCCWindow";
    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Не удалось зарегистрировать класс окна", "Ошибка", MB_ICONERROR);
        return 1;
    }
    HWND hwnd = CreateWindowExA(0, "WinCCWindow", "Окно на WinCC", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 420, 340, NULL, NULL, inst, NULL);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
