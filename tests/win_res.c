// with: win_res.rc
// Resources (.rc): Windows itself reads each one back -- icons, cursor, picture, data, strings
// in two languages, the version, both kinds of menu, accelerators, both kinds of dialog.
#ifndef _WIN32                          // DynastyOS (cctest): no Windows there
int main(void) { return 0; }
#else
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win_res.h"
#pragma comment(linker, "/MANIFEST")    // WinCC's manifest beside the program's resources (asked for)

static int failed;
#define CHECK(c) do { if (!(c)) { printf("ПРОВАЛ строка %d: %s\n", __LINE__, #c); failed++; } } while (0)

static int wide_is(const wchar_t *a, const wchar_t *b) { return wcscmp(a, b) == 0; }

static INT_PTR dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l) { (void)h; (void)m; (void)w; (void)l; return 0; }

static const void *resource(HMODULE mod, LPCWSTR name, LPCWSTR type, DWORD *size)
{
    HRSRC r = FindResourceW(mod, name, type);
    if (!r) return 0;
    *size = SizeofResource(mod, r);
    return LockResource(LoadResource(mod, r));
}

int main(void)
{
    HINSTANCE me = GetModuleHandleW(0);
    DWORD size = 0;

    // icons: the group finds the images of 16 and 32 pixels; the named one too
    HICON i16 = (HICON)LoadImageW(me, MAKEINTRESOURCEW(IDI_TEST), IMAGE_ICON, 16, 16, 0);
    HICON i32 = (HICON)LoadImageW(me, MAKEINTRESOURCEW(IDI_TEST), IMAGE_ICON, 32, 32, 0);
    CHECK(i16 && i32);
    CHECK(LoadIconW(me, L"NAMED_ICON") != 0);
    const BYTE *grp = resource(me, MAKEINTRESOURCEW(IDI_TEST), RT_GROUP_ICON, &size);
    CHECK(grp && size == 6 + 2 * 14 && grp[2] == 1 && grp[4] == 2);

    // cursor: its hot spot
    HCURSOR cur = LoadCursorW(me, MAKEINTRESOURCEW(IDC_TEST));
    CHECK(cur != 0);
    ICONINFO ii = { 0 };
    if (cur && GetIconInfo(cur, &ii)) {
        CHECK(ii.xHotspot == 5 && ii.yHotspot == 7);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
    } else
        CHECK(!"GetIconInfo");

    // picture
    HBITMAP bmp = LoadBitmapW(me, MAKEINTRESOURCEW(IDB_TEST));
    BITMAP bm = { 0 };
    CHECK(bmp && GetObjectW(bmp, sizeof(bm), &bm) && bm.bmWidth == 7 && bm.bmHeight == 3);

    // data: a file as it is, a block of numbers and strings, a type of one's own, a named one
    const BYTE *d = resource(me, MAKEINTRESOURCEW(IDR_FILE), RT_RCDATA, &size);
    CHECK(d && size == 256 * 3 + 5 && d[255] == 255 && !memcmp(d + 768, "WinCC", 5));
    d = resource(me, MAKEINTRESOURCEW(IDR_INLINE), RT_RCDATA, &size);
    static const BYTE inl[] = { 0x34, 0x12, 7, 0, 0, 0, 'a', 'b', 'c', 0x16, 0x04, 0x36, 0x04, 255, 0 };
    CHECK(d && size == sizeof(inl) && !memcmp(d, inl, sizeof(inl)));
    d = resource(me, MAKEINTRESOURCEW(IDR_OWN), L"TESTDATA", &size);
    CHECK(d && size == 6 && d[0] == 1 && d[2] == 2 && d[4] == 3);
    d = resource(me, L"\"QUOTED\"", MAKEINTRESOURCEW(300), &size);     // as rc.exe: the quotes are in the name
    CHECK(d && size == 773);

    // strings: in the neutral language and in Russian
    wchar_t s[128];
    CHECK(LoadStringW(me, IDS_HELLO, s, 128) == 12 && wide_is(s, L"Привет, мир!"));
    CHECK(LoadStringW(me, IDS_SECOND, s, 128) && wide_is(s, L"Second \"quoted\" line"));
    CHECK(LoadStringW(me, IDS_FAR, s, 128) && wide_is(s, L"Строка 1000\tс табом"));
    CHECK(LoadStringW(me, IDS_EMPTY, s, 128) == 0);
    HRSRC ru = FindResourceExW(me, RT_STRING, MAKEINTRESOURCEW(IDS_HELLO / 16 + 1), MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA));
    const WORD *blk = ru ? LockResource(LoadResource(me, ru)) : 0;
    CHECK(blk && blk[0] == 0 && blk[1] == 10 && !memcmp(blk + 2, L"Здравствуй", 20));

    // version
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(0, path, MAX_PATH);
    DWORD vsize = GetFileVersionInfoSizeW(path, 0);
    CHECK(vsize > 0);
    if (vsize) {
        BYTE *v = malloc(vsize);
        VS_FIXEDFILEINFO *fi = 0;
        wchar_t *text = 0;
        UINT n = 0;
        CHECK(GetFileVersionInfoW(path, 0, vsize, v));
        CHECK(VerQueryValueW(v, L"\\", (void **)&fi, &n) && fi->dwSignature == 0xFEEF04BD &&
              fi->dwFileVersionMS == 0x00010002 && fi->dwFileVersionLS == 0x00030004 &&
              fi->dwProductVersionMS == 0x00050006 && fi->dwFileType == VFT_APP);
        CHECK(VerQueryValueW(v, L"\\StringFileInfo\\041904b0\\FileDescription", (void **)&text, &n) &&
              wide_is(text, L"Проверка ресурсов"));
        WORD *tr = 0;
        CHECK(VerQueryValueW(v, L"\\VarFileInfo\\Translation", (void **)&tr, &n) && n == 4 && tr[0] == 0x419 && tr[1] == 1200);
        free(v);
    }

    // menu
    HMENU m = LoadMenuW(me, MAKEINTRESOURCEW(IDM_MAIN));
    CHECK(m && GetMenuItemCount(m) == 2);
    if (m) {
        HMENU file = GetSubMenu(m, 0);
        CHECK(file && GetMenuItemCount(file) == 3);
        CHECK(GetMenuItemID(file, 0) == IDM_OPEN && GetMenuItemID(file, 2) == IDM_EXIT);
        CHECK(GetMenuStringW(file, 0, s, 128, MF_BYPOSITION) && wide_is(s, L"&Открыть\tCtrl+O"));
        CHECK(GetMenuState(file, 1, MF_BYPOSITION) & MF_SEPARATOR);
        CHECK(GetMenuState(file, IDM_EXIT, MF_BYCOMMAND) & MF_GRAYED);
        CHECK(GetMenuState(m, IDM_HELP, MF_BYCOMMAND) & MF_CHECKED);
        DestroyMenu(m);
    }
    m = LoadMenuW(me, MAKEINTRESOURCEW(IDM_EX));
    CHECK(m && GetMenuItemCount(m) == 1);
    if (m) {
        HMENU view = GetSubMenu(m, 0);
        CHECK(view && GetMenuItemCount(view) == 3);
        CHECK(GetMenuItemID(view, 2) == IDM_SMALL);
        CHECK(GetMenuState(view, IDM_BIG, MF_BYCOMMAND) & MF_CHECKED);
        CHECK(GetMenuState(view, 1, MF_BYPOSITION) & MF_SEPARATOR);
        DestroyMenu(m);
    }

    // accelerators
    HACCEL acc = LoadAcceleratorsW(me, MAKEINTRESOURCEW(IDA_MAIN));
    ACCEL a[8];
    int na = acc ? CopyAcceleratorTableW(acc, a, 8) : 0;
    CHECK(na == 4);
    if (na == 4) {
        CHECK(a[0].key == 15 && a[0].cmd == IDM_OPEN && a[0].fVirt == 0);            // ^O: Ctrl+O as a character
        CHECK(a[1].key == 'q' && a[1].cmd == IDM_EXIT);
        CHECK(a[2].key == VK_F1 && (a[2].fVirt & FVIRTKEY));
        CHECK(a[3].key == 'S' && a[3].fVirt == (FVIRTKEY | FCONTROL | FSHIFT));
    }

    // dialogs: made by Windows from the templates, their items read back
    HWND dlg = CreateDialogParamW(me, MAKEINTRESOURCEW(IDD_EX), 0, dlg_proc, 0);
    CHECK(dlg != 0);
    if (dlg) {
        CHECK(GetWindowTextW(dlg, s, 128) && wide_is(s, L"Окно проверки"));
        CHECK(GetDlgItemTextW(dlg, IDOK, s, 128) && wide_is(s, L"ОК"));
        CHECK(GetDlgItem(dlg, IDC_NAME) && GetDlgItem(dlg, IDC_LIST) && GetDlgItem(dlg, IDC_ICON));
        CHECK(GetClassNameW(GetDlgItem(dlg, IDC_NAME), s, 128) && wide_is(s, L"Edit"));
        CHECK(GetClassNameW(GetDlgItem(dlg, IDC_CUSTOM), s, 128) && wide_is(s, L"Button"));
        CHECK(!(GetWindowLongW(GetDlgItem(dlg, IDCANCEL), GWL_STYLE) & WS_TABSTOP));
        CHECK(GetWindowLongW(GetDlgItem(dlg, IDOK), GWL_STYLE) & WS_TABSTOP);
        CHECK(GetWindowLongW(dlg, GWL_EXSTYLE) & WS_EX_TOOLWINDOW);
        DestroyWindow(dlg);
    }
    dlg = CreateDialogParamW(me, MAKEINTRESOURCEW(IDD_OLD), 0, dlg_proc, 0);
    CHECK(dlg != 0);
    if (dlg) {
        CHECK(GetWindowTextW(dlg, s, 128) && wide_is(s, L"Old"));
        CHECK(GetDlgItem(dlg, IDOK) != 0);
        DestroyWindow(dlg);
    }

    // the manifest of WinCC is still there (UTF-8 is the code page)
    CHECK(GetACP() == 65001);

    if (!failed) printf("ресурсы: всё сходится\n");
    return failed;
}
#endif
