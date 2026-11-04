// commctrl.h of WinCC: the common controls -- progress bar, slider, status bar, up-down,
// tooltips. Every WinCC program carries the manifest of Common Controls 6 (the look of Windows).
#pragma once
#include <windows.h>
#pragma comment(lib, "comctl32")

#define ICC_LISTVIEW_CLASSES 0x00000001
#define ICC_TREEVIEW_CLASSES 0x00000002
#define ICC_BAR_CLASSES 0x00000004
#define ICC_TAB_CLASSES 0x00000008
#define ICC_UPDOWN_CLASS 0x00000010
#define ICC_PROGRESS_CLASS 0x00000020
#define ICC_HOTKEY_CLASS 0x00000040
#define ICC_ANIMATE_CLASS 0x00000080
#define ICC_WIN95_CLASSES 0x000000FF
#define ICC_DATE_CLASSES 0x00000100
#define ICC_USEREX_CLASSES 0x00000200
#define ICC_COOL_CLASSES 0x00000400
#define ICC_STANDARD_CLASSES 0x00004000
#define ICC_LINK_CLASS 0x00008000

typedef struct tagINITCOMMONCONTROLSEX { DWORD dwSize, dwICC; } INITCOMMONCONTROLSEX, *LPINITCOMMONCONTROLSEX;
BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX *icc);
void InitCommonControls(void);

#ifdef UNICODE
#define PROGRESS_CLASS L"msctls_progress32"
#define TRACKBAR_CLASS L"msctls_trackbar32"
#define STATUSCLASSNAME L"msctls_statusbar32"
#define UPDOWN_CLASS L"msctls_updown32"
#define TOOLTIPS_CLASS L"tooltips_class32"
#define WC_LISTVIEW L"SysListView32"
#define WC_TREEVIEW L"SysTreeView32"
#define WC_TABCONTROL L"SysTabControl32"
#else
#define PROGRESS_CLASS "msctls_progress32"
#define TRACKBAR_CLASS "msctls_trackbar32"
#define STATUSCLASSNAME "msctls_statusbar32"
#define UPDOWN_CLASS "msctls_updown32"
#define TOOLTIPS_CLASS "tooltips_class32"
#define WC_LISTVIEW "SysListView32"
#define WC_TREEVIEW "SysTreeView32"
#define WC_TABCONTROL "SysTabControl32"
#endif

// progress bar
#define PBS_SMOOTH 0x01
#define PBS_VERTICAL 0x04
#define PBS_MARQUEE 0x08
#define PBM_SETRANGE (WM_USER + 1)
#define PBM_SETPOS (WM_USER + 2)
#define PBM_DELTAPOS (WM_USER + 3)
#define PBM_SETSTEP (WM_USER + 4)
#define PBM_STEPIT (WM_USER + 5)
#define PBM_SETRANGE32 (WM_USER + 6)
#define PBM_GETPOS (WM_USER + 8)
#define PBM_SETBARCOLOR (WM_USER + 9)
#define PBM_SETMARQUEE (WM_USER + 10)
#define PBM_SETBKCOLOR 0x2001

// slider (trackbar)
#define TBS_AUTOTICKS 0x0001
#define TBS_VERT 0x0002
#define TBS_HORZ 0x0000
#define TBS_NOTICKS 0x0010
#define TBM_GETPOS (WM_USER)
#define TBM_GETRANGEMIN (WM_USER + 1)
#define TBM_GETRANGEMAX (WM_USER + 2)
#define TBM_SETPOS (WM_USER + 5)
#define TBM_SETRANGE (WM_USER + 6)
#define TBM_SETRANGEMIN (WM_USER + 7)
#define TBM_SETRANGEMAX (WM_USER + 8)
#define TBM_SETTICFREQ (WM_USER + 20)
#define TBM_SETPAGESIZE (WM_USER + 21)

// status bar
#define SBARS_SIZEGRIP 0x0100
#define SB_SETTEXTA (WM_USER + 1)
#define SB_SETTEXTW (WM_USER + 11)
#define SB_SETPARTS (WM_USER + 4)
#define SB_SIMPLE (WM_USER + 9)
#define SB_SETTEXT __AW(SB_SETTEXT)

// up-down
#define UDS_WRAP 0x0001
#define UDS_SETBUDDYINT 0x0002
#define UDS_ALIGNRIGHT 0x0004
#define UDS_ARROWKEYS 0x0020
#define UDM_SETRANGE32 (WM_USER + 111)
#define UDM_SETPOS32 (WM_USER + 113)
#define UDM_GETPOS32 (WM_USER + 114)

// notifications
#define NM_FIRST 0U
#define NM_CLICK (NM_FIRST - 2)
#define NM_DBLCLK (NM_FIRST - 3)
#define NM_RETURN (NM_FIRST - 4)
#define NM_RCLICK (NM_FIRST - 5)
