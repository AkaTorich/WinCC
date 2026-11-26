// windows.h: user32 -- windows, messages, controls, menus, the mouse and keyboard, the clipboard.
#pragma once

typedef LRESULT (*WNDPROC)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
typedef INT_PTR (*DLGPROC)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
typedef void (*TIMERPROC)(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);
typedef BOOL (*WNDENUMPROC)(HWND hwnd, LPARAM lp);
typedef LRESULT (*HOOKPROC)(int code, WPARAM wp, LPARAM lp);
typedef BOOL (*MONITORENUMPROC)(HMONITOR m, HDC dc, LPRECT r, LPARAM lp);

// ---- window messages ----
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_ENABLE 0x000A
#define WM_SETREDRAW 0x000B
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUERYENDSESSION 0x0011
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SYSCOLORCHANGE 0x0015
#define WM_SHOWWINDOW 0x0018
#define WM_SETTINGCHANGE 0x001A
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_MOUSEACTIVATE 0x0021
#define WM_GETMINMAXINFO 0x0024
#define WM_DRAWITEM 0x002B
#define WM_MEASUREITEM 0x002C
#define WM_SETFONT 0x0030
#define WM_GETFONT 0x0031
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_NOTIFY 0x004E
#define WM_HELP 0x0053
#define WM_CONTEXTMENU 0x007B
#define WM_STYLECHANGED 0x007D
#define WM_DISPLAYCHANGE 0x007E
#define WM_GETICON 0x007F
#define WM_SETICON 0x0080
#define WM_NCCREATE 0x0081
#define WM_NCDESTROY 0x0082
#define WM_NCCALCSIZE 0x0083
#define WM_NCHITTEST 0x0084
#define WM_NCPAINT 0x0085
#define WM_NCACTIVATE 0x0086
#define WM_GETDLGCODE 0x0087
#define WM_NCMOUSEMOVE 0x00A0
#define WM_NCLBUTTONDOWN 0x00A1
#define WM_NCLBUTTONUP 0x00A2
#define WM_NCLBUTTONDBLCLK 0x00A3
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_DEADCHAR 0x0103
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_UNICHAR 0x0109
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_HSCROLL 0x0114
#define WM_VSCROLL 0x0115
#define WM_INITMENU 0x0116
#define WM_INITMENUPOPUP 0x0117
#define WM_MENUSELECT 0x011F
#define WM_MENUCHAR 0x0120
#define WM_ENTERIDLE 0x0121
#define WM_CTLCOLORMSGBOX 0x0132
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_CTLCOLORBTN 0x0135
#define WM_CTLCOLORDLG 0x0136
#define WM_CTLCOLORSCROLLBAR 0x0137
#define WM_CTLCOLORSTATIC 0x0138
#define WM_MOUSEFIRST 0x0200
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_MOUSEHWHEEL 0x020E
#define WM_MOUSELAST 0x020E
#define WM_PARENTNOTIFY 0x0210
#define WM_ENTERMENULOOP 0x0211
#define WM_EXITMENULOOP 0x0212
#define WM_SIZING 0x0214
#define WM_CAPTURECHANGED 0x0215
#define WM_MOVING 0x0216
#define WM_ENTERSIZEMOVE 0x0231
#define WM_EXITSIZEMOVE 0x0232
#define WM_DROPFILES 0x0233
#define WM_MOUSEHOVER 0x02A1
#define WM_MOUSELEAVE 0x02A3
#define WM_DPICHANGED 0x02E0
#define WM_CUT 0x0300
#define WM_COPY 0x0301
#define WM_PASTE 0x0302
#define WM_CLEAR 0x0303
#define WM_UNDO 0x0304
#define WM_HOTKEY 0x0312
#define WM_PRINTCLIENT 0x0318
#define WM_USER 0x0400
#define WM_APP 0x8000

// ---- window styles ----
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_GROUP 0x00020000L
#define WS_TABSTOP 0x00010000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_TILED WS_OVERLAPPED
#define WS_ICONIC WS_MINIMIZE
#define WS_SIZEBOX WS_THICKFRAME
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_TILEDWINDOW WS_OVERLAPPEDWINDOW
#define WS_POPUPWINDOW (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW WS_CHILD
#define WS_EX_DLGMODALFRAME 0x00000001L
#define WS_EX_NOPARENTNOTIFY 0x00000004L
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_ACCEPTFILES 0x00000010L
#define WS_EX_TRANSPARENT 0x00000020L
#define WS_EX_MDICHILD 0x00000040L
#define WS_EX_TOOLWINDOW 0x00000080L
#define WS_EX_WINDOWEDGE 0x00000100L
#define WS_EX_CLIENTEDGE 0x00000200L
#define WS_EX_CONTEXTHELP 0x00000400L
#define WS_EX_RIGHT 0x00001000L
#define WS_EX_LEFT 0x00000000L
#define WS_EX_CONTROLPARENT 0x00010000L
#define WS_EX_STATICEDGE 0x00020000L
#define WS_EX_APPWINDOW 0x00040000L
#define WS_EX_LAYERED 0x00080000
#define WS_EX_COMPOSITED 0x02000000L
#define WS_EX_NOACTIVATE 0x08000000L
#define WS_EX_OVERLAPPEDWINDOW (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020
#define CS_CLASSDC 0x0040
#define CS_PARENTDC 0x0080
#define CS_NOCLOSE 0x0200
#define CS_SAVEBITS 0x0800
#define CS_GLOBALCLASS 0x4000
#define CS_DROPSHADOW 0x00020000

#define CW_USEDEFAULT ((int)0x80000000)
#define HWND_DESKTOP ((HWND)0)
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)
#define HWND_MESSAGE ((HWND)-3)
#define HWND_BROADCAST ((HWND)0xffff)

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWMINNOACTIVE 7
#define SW_SHOWNA 8
#define SW_RESTORE 9
#define SW_SHOWDEFAULT 10
#define SW_FORCEMINIMIZE 11

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080
#define SWP_NOOWNERZORDER 0x0200

#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWL_ID (-12)
#define GWL_USERDATA (-21)
#define GWLP_WNDPROC (-4)
#define GWLP_HINSTANCE (-6)
#define GWLP_HWNDPARENT (-8)
#define GWLP_USERDATA (-21)
#define GWLP_ID (-12)
#define GCLP_HBRBACKGROUND (-10)
#define GCLP_HCURSOR (-12)
#define GCLP_HICON (-14)
#define GCLP_HICONSM (-34)
#define GCLP_WNDPROC (-24)
#define DWLP_MSGRESULT 0

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002

#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2
#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define SC_SIZE 0xF000
#define SC_MOVE 0xF010
#define SC_MINIMIZE 0xF020
#define SC_MAXIMIZE 0xF030
#define SC_CLOSE 0xF060
#define SC_RESTORE 0xF120
#define HTERROR (-2)
#define HTTRANSPARENT (-1)
#define HTNOWHERE 0
#define HTCLIENT 1
#define HTCAPTION 2
#define HTLEFT 10
#define HTRIGHT 11
#define HTTOP 12
#define HTTOPLEFT 13
#define HTTOPRIGHT 14
#define HTBOTTOM 15
#define HTBOTTOMLEFT 16
#define HTBOTTOMRIGHT 17
#define MA_ACTIVATE 1
#define MA_ACTIVATEANDEAT 2
#define MA_NOACTIVATE 3
#define MA_NOACTIVATEANDEAT 4
#define ULW_COLORKEY 0x01
#define ULW_ALPHA 0x02
#define ULW_OPAQUE 0x04
#define RDW_INVALIDATE 0x0001
#define RDW_INTERNALPAINT 0x0002
#define RDW_ERASE 0x0004
#define RDW_ALLCHILDREN 0x0080
#define RDW_UPDATENOW 0x0100
#define RDW_ERASENOW 0x0200
#define RDW_FRAME 0x0400

// ---- message boxes ----
#define MB_OK 0x00000000L
#define MB_OKCANCEL 0x00000001L
#define MB_ABORTRETRYIGNORE 0x00000002L
#define MB_YESNOCANCEL 0x00000003L
#define MB_YESNO 0x00000004L
#define MB_RETRYCANCEL 0x00000005L
#define MB_CANCELTRYCONTINUE 0x00000006L
#define MB_ICONHAND 0x00000010L
#define MB_ICONQUESTION 0x00000020L
#define MB_ICONEXCLAMATION 0x00000030L
#define MB_ICONASTERISK 0x00000040L
#define MB_ICONWARNING MB_ICONEXCLAMATION
#define MB_ICONERROR MB_ICONHAND
#define MB_ICONSTOP MB_ICONHAND
#define MB_ICONINFORMATION MB_ICONASTERISK
#define MB_DEFBUTTON1 0x00000000L
#define MB_DEFBUTTON2 0x00000100L
#define MB_DEFBUTTON3 0x00000200L
#define MB_APPLMODAL 0x00000000L
#define MB_SYSTEMMODAL 0x00001000L
#define MB_TASKMODAL 0x00002000L
#define MB_SETFOREGROUND 0x00010000L
#define MB_TOPMOST 0x00040000L
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#define IDCLOSE 8
#define IDHELP 9
#define IDTRYAGAIN 10
#define IDCONTINUE 11

// ---- cursors and icons of the system ----
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_CROSS MAKEINTRESOURCE(32515)
#define IDC_UPARROW MAKEINTRESOURCE(32516)
#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_SIZENESW MAKEINTRESOURCE(32643)
#define IDC_SIZEWE MAKEINTRESOURCE(32644)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_HAND MAKEINTRESOURCE(32649)
#define IDC_APPSTARTING MAKEINTRESOURCE(32650)
#define IDC_HELP MAKEINTRESOURCE(32651)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define IDI_HAND MAKEINTRESOURCE(32513)
#define IDI_QUESTION MAKEINTRESOURCE(32514)
#define IDI_EXCLAMATION MAKEINTRESOURCE(32515)
#define IDI_ASTERISK MAKEINTRESOURCE(32516)
#define IDI_WINLOGO MAKEINTRESOURCE(32517)
#define IDI_SHIELD MAKEINTRESOURCE(32518)
#define IDI_WARNING IDI_EXCLAMATION
#define IDI_ERROR IDI_HAND
#define IDI_INFORMATION IDI_ASTERISK

#define IMAGE_BITMAP 0
#define IMAGE_ICON 1
#define IMAGE_CURSOR 2
#define LR_DEFAULTCOLOR 0x00000000
#define LR_LOADFROMFILE 0x00000010
#define LR_LOADTRANSPARENT 0x00000020
#define LR_DEFAULTSIZE 0x00000040
#define LR_CREATEDIBSECTION 0x00002000
#define LR_SHARED 0x00008000
#define ICON_SMALL 0
#define ICON_BIG 1

// ---- system colors and metrics ----
#define COLOR_SCROLLBAR 0
#define COLOR_BACKGROUND 1
#define COLOR_ACTIVECAPTION 2
#define COLOR_INACTIVECAPTION 3
#define COLOR_MENU 4
#define COLOR_WINDOW 5
#define COLOR_WINDOWFRAME 6
#define COLOR_MENUTEXT 7
#define COLOR_WINDOWTEXT 8
#define COLOR_CAPTIONTEXT 9
#define COLOR_ACTIVEBORDER 10
#define COLOR_INACTIVEBORDER 11
#define COLOR_APPWORKSPACE 12
#define COLOR_HIGHLIGHT 13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_BTNFACE 15
#define COLOR_BTNSHADOW 16
#define COLOR_GRAYTEXT 17
#define COLOR_BTNTEXT 18
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_BTNHIGHLIGHT 20
#define COLOR_3DDKSHADOW 21
#define COLOR_3DLIGHT 22
#define COLOR_INFOTEXT 23
#define COLOR_INFOBK 24
#define COLOR_HOTLIGHT 26
#define COLOR_GRADIENTACTIVECAPTION 27
#define COLOR_GRADIENTINACTIVECAPTION 28
#define COLOR_MENUHILIGHT 29
#define COLOR_MENUBAR 30
#define COLOR_DESKTOP COLOR_BACKGROUND
#define COLOR_3DFACE COLOR_BTNFACE
#define COLOR_3DSHADOW COLOR_BTNSHADOW
#define COLOR_3DHIGHLIGHT COLOR_BTNHIGHLIGHT

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXVSCROLL 2
#define SM_CYHSCROLL 3
#define SM_CYCAPTION 4
#define SM_CXBORDER 5
#define SM_CYBORDER 6
#define SM_CXDLGFRAME 7
#define SM_CYDLGFRAME 8
#define SM_CXICON 11
#define SM_CYICON 12
#define SM_CXCURSOR 13
#define SM_CYCURSOR 14
#define SM_CYMENU 15
#define SM_CXFULLSCREEN 16
#define SM_CYFULLSCREEN 17
#define SM_MOUSEPRESENT 19
#define SM_CXMIN 28
#define SM_CYMIN 29
#define SM_CXSIZE 30
#define SM_CYSIZE 31
#define SM_CXFRAME 32
#define SM_CYFRAME 33
#define SM_CXMINTRACK 34
#define SM_CYMINTRACK 35
#define SM_CXDOUBLECLK 36
#define SM_CYDOUBLECLK 37
#define SM_CXSMICON 49
#define SM_CYSMICON 50
#define SM_CXMAXIMIZED 61
#define SM_CYMAXIMIZED 62
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79
#define SM_CMONITORS 80
#define SPI_GETWORKAREA 0x0030
#define SPI_GETNONCLIENTMETRICS 0x0029
#define MONITOR_DEFAULTTONULL 0
#define MONITOR_DEFAULTTOPRIMARY 1
#define MONITOR_DEFAULTTONEAREST 2

// ---- keys and the mouse ----
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010
#define MK_XBUTTON1 0x0020
#define MK_XBUTTON2 0x0040
#define WHEEL_DELTA 120
#define GET_WHEEL_DELTA_WPARAM(w) ((short)HIWORD(w))
#define GET_KEYSTATE_WPARAM(w) (LOWORD(w))
#define MOD_ALT 0x0001
#define MOD_CONTROL 0x0002
#define MOD_SHIFT 0x0004
#define MOD_WIN 0x0008
#define MOD_NOREPEAT 0x4000
#define TME_HOVER 0x00000001
#define TME_LEAVE 0x00000002
#define HOVER_DEFAULT 0xFFFFFFFF

// ---- menus ----
#define MF_INSERT 0x00000000L
#define MF_CHANGE 0x00000080L
#define MF_APPEND 0x00000100L
#define MF_DELETE 0x00000200L
#define MF_REMOVE 0x00001000L
#define MF_BYCOMMAND 0x00000000L
#define MF_BYPOSITION 0x00000400L
#define MF_SEPARATOR 0x00000800L
#define MF_ENABLED 0x00000000L
#define MF_GRAYED 0x00000001L
#define MF_DISABLED 0x00000002L
#define MF_UNCHECKED 0x00000000L
#define MF_CHECKED 0x00000008L
#define MF_STRING 0x00000000L
#define MF_BITMAP 0x00000004L
#define MF_OWNERDRAW 0x00000100L
#define MF_POPUP 0x00000010L
#define MF_MENUBARBREAK 0x00000020L
#define MF_MENUBREAK 0x00000040L
#define MF_DEFAULT 0x00001000L
#define MF_RIGHTJUSTIFY 0x00004000L
#define TPM_LEFTBUTTON 0x0000L
#define TPM_RIGHTBUTTON 0x0002L
#define TPM_LEFTALIGN 0x0000L
#define TPM_CENTERALIGN 0x0004L
#define TPM_RIGHTALIGN 0x0008L
#define TPM_TOPALIGN 0x0000L
#define TPM_VCENTERALIGN 0x0010L
#define TPM_BOTTOMALIGN 0x0020L
#define TPM_NONOTIFY 0x0080L
#define TPM_RETURNCMD 0x0100L

// ---- controls: buttons, edits, statics, lists, combo boxes, scroll bars ----
#define BS_PUSHBUTTON 0x00000000L
#define BS_DEFPUSHBUTTON 0x00000001L
#define BS_CHECKBOX 0x00000002L
#define BS_AUTOCHECKBOX 0x00000003L
#define BS_RADIOBUTTON 0x00000004L
#define BS_3STATE 0x00000005L
#define BS_AUTO3STATE 0x00000006L
#define BS_GROUPBOX 0x00000007L
#define BS_USERBUTTON 0x00000008L
#define BS_AUTORADIOBUTTON 0x00000009L
#define BS_OWNERDRAW 0x0000000BL
#define BS_LEFTTEXT 0x00000020L
#define BS_TEXT 0x00000000L
#define BS_ICON 0x00000040L
#define BS_BITMAP 0x00000080L
#define BS_LEFT 0x00000100L
#define BS_RIGHT 0x00000200L
#define BS_CENTER 0x00000300L
#define BS_TOP 0x00000400L
#define BS_BOTTOM 0x00000800L
#define BS_VCENTER 0x00000C00L
#define BS_PUSHLIKE 0x00001000L
#define BS_MULTILINE 0x00002000L
#define BS_NOTIFY 0x00004000L
#define BS_FLAT 0x00008000L
#define BN_CLICKED 0
#define BN_DOUBLECLICKED 5
#define BN_SETFOCUS 6
#define BN_KILLFOCUS 7
#define BM_GETCHECK 0x00F0
#define BM_SETCHECK 0x00F1
#define BM_GETSTATE 0x00F2
#define BM_SETSTATE 0x00F3
#define BM_SETSTYLE 0x00F4
#define BM_CLICK 0x00F5
#define BM_SETIMAGE 0x00F7
#define BST_UNCHECKED 0x0000
#define BST_CHECKED 0x0001
#define BST_INDETERMINATE 0x0002
#define BST_PUSHED 0x0004
#define BST_FOCUS 0x0008

#define ES_LEFT 0x0000L
#define ES_CENTER 0x0001L
#define ES_RIGHT 0x0002L
#define ES_MULTILINE 0x0004L
#define ES_UPPERCASE 0x0008L
#define ES_LOWERCASE 0x0010L
#define ES_PASSWORD 0x0020L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_NOHIDESEL 0x0100L
#define ES_READONLY 0x0800L
#define ES_WANTRETURN 0x1000L
#define ES_NUMBER 0x2000L
#define EN_SETFOCUS 0x0100
#define EN_KILLFOCUS 0x0200
#define EN_CHANGE 0x0300
#define EN_UPDATE 0x0400
#define EN_ERRSPACE 0x0500
#define EN_MAXTEXT 0x0501
#define EN_HSCROLL 0x0601
#define EN_VSCROLL 0x0602
#define EM_GETSEL 0x00B0
#define EM_SETSEL 0x00B1
#define EM_GETRECT 0x00B2
#define EM_SCROLL 0x00B5
#define EM_LINESCROLL 0x00B6
#define EM_SCROLLCARET 0x00B7
#define EM_GETMODIFY 0x00B8
#define EM_SETMODIFY 0x00B9
#define EM_GETLINECOUNT 0x00BA
#define EM_LINEINDEX 0x00BB
#define EM_LINELENGTH 0x00C1
#define EM_REPLACESEL 0x00C2
#define EM_GETLINE 0x00C4
#define EM_LIMITTEXT 0x00C5
#define EM_SETLIMITTEXT EM_LIMITTEXT
#define EM_CANUNDO 0x00C6
#define EM_UNDO 0x00C7
#define EM_LINEFROMCHAR 0x00C9
#define EM_SETTABSTOPS 0x00CB
#define EM_SETPASSWORDCHAR 0x00CC
#define EM_SETREADONLY 0x00CF
#define EM_GETLIMITTEXT 0x00D5

#define SS_LEFT 0x00000000L
#define SS_CENTER 0x00000001L
#define SS_RIGHT 0x00000002L
#define SS_ICON 0x00000003L
#define SS_BLACKRECT 0x00000004L
#define SS_GRAYRECT 0x00000005L
#define SS_WHITERECT 0x00000006L
#define SS_BLACKFRAME 0x00000007L
#define SS_ETCHEDHORZ 0x00000010L
#define SS_ETCHEDVERT 0x00000011L
#define SS_ETCHEDFRAME 0x00000012L
#define SS_SIMPLE 0x0000000BL
#define SS_LEFTNOWORDWRAP 0x0000000CL
#define SS_OWNERDRAW 0x0000000DL
#define SS_BITMAP 0x0000000EL
#define SS_NOPREFIX 0x00000080L
#define SS_NOTIFY 0x00000100L
#define SS_CENTERIMAGE 0x00000200L
#define SS_SUNKEN 0x00001000L
#define SS_ENDELLIPSIS 0x00004000L
#define STM_SETICON 0x0170
#define STM_SETIMAGE 0x0172
#define STN_CLICKED 0

#define LBS_NOTIFY 0x0001L
#define LBS_SORT 0x0002L
#define LBS_NOREDRAW 0x0004L
#define LBS_MULTIPLESEL 0x0008L
#define LBS_HASSTRINGS 0x0040L
#define LBS_USETABSTOPS 0x0080L
#define LBS_NOINTEGRALHEIGHT 0x0100L
#define LBS_EXTENDEDSEL 0x0800L
#define LBS_DISABLENOSCROLL 0x1000L
#define LBS_STANDARD (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)
#define LB_ADDSTRING 0x0180
#define LB_INSERTSTRING 0x0181
#define LB_DELETESTRING 0x0182
#define LB_RESETCONTENT 0x0184
#define LB_SETSEL 0x0185
#define LB_SETCURSEL 0x0186
#define LB_GETSEL 0x0187
#define LB_GETCURSEL 0x0188
#define LB_GETTEXT 0x0189
#define LB_GETTEXTLEN 0x018A
#define LB_GETCOUNT 0x018B
#define LB_SELECTSTRING 0x018C
#define LB_GETTOPINDEX 0x018E
#define LB_FINDSTRING 0x018F
#define LB_GETSELCOUNT 0x0190
#define LB_GETSELITEMS 0x0191
#define LB_SETTOPINDEX 0x0197
#define LB_GETITEMDATA 0x0199
#define LB_SETITEMDATA 0x019A
#define LB_SETITEMHEIGHT 0x01A0
#define LB_FINDSTRINGEXACT 0x01A2
#define LB_OKAY 0
#define LB_ERR (-1)
#define LBN_ERRSPACE (-2)
#define LBN_SELCHANGE 1
#define LBN_DBLCLK 2
#define LBN_SELCANCEL 3
#define LBN_SETFOCUS 4
#define LBN_KILLFOCUS 5

#define CBS_SIMPLE 0x0001L
#define CBS_DROPDOWN 0x0002L
#define CBS_DROPDOWNLIST 0x0003L
#define CBS_OWNERDRAWFIXED 0x0010L
#define CBS_AUTOHSCROLL 0x0040L
#define CBS_SORT 0x0100L
#define CBS_HASSTRINGS 0x0200L
#define CBS_NOINTEGRALHEIGHT 0x0400L
#define CB_GETEDITSEL 0x0140
#define CB_LIMITTEXT 0x0141
#define CB_SETEDITSEL 0x0142
#define CB_ADDSTRING 0x0143
#define CB_DELETESTRING 0x0144
#define CB_GETCOUNT 0x0146
#define CB_GETCURSEL 0x0147
#define CB_GETLBTEXT 0x0148
#define CB_GETLBTEXTLEN 0x0149
#define CB_INSERTSTRING 0x014A
#define CB_RESETCONTENT 0x014B
#define CB_FINDSTRING 0x014C
#define CB_SELECTSTRING 0x014D
#define CB_SETCURSEL 0x014E
#define CB_SHOWDROPDOWN 0x014F
#define CB_GETITEMDATA 0x0150
#define CB_SETITEMDATA 0x0151
#define CB_GETDROPPEDSTATE 0x0157
#define CB_FINDSTRINGEXACT 0x0158
#define CB_OKAY 0
#define CB_ERR (-1)
#define CBN_SELCHANGE 1
#define CBN_DBLCLK 2
#define CBN_SETFOCUS 3
#define CBN_KILLFOCUS 4
#define CBN_EDITCHANGE 5
#define CBN_EDITUPDATE 6
#define CBN_DROPDOWN 7
#define CBN_CLOSEUP 8
#define CBN_SELENDOK 9

#define SBS_HORZ 0x0000L
#define SBS_VERT 0x0001L
#define SB_HORZ 0
#define SB_VERT 1
#define SB_CTL 2
#define SB_BOTH 3
#define SB_LINEUP 0
#define SB_LINELEFT 0
#define SB_LINEDOWN 1
#define SB_LINERIGHT 1
#define SB_PAGEUP 2
#define SB_PAGELEFT 2
#define SB_PAGEDOWN 3
#define SB_PAGERIGHT 3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK 5
#define SB_TOP 6
#define SB_LEFT 6
#define SB_BOTTOM 7
#define SB_RIGHT 7
#define SB_ENDSCROLL 8
#define SIF_RANGE 0x0001
#define SIF_PAGE 0x0002
#define SIF_POS 0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS 0x0010
#define SIF_ALL (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)
#define ESB_ENABLE_BOTH 0x0000
#define ESB_DISABLE_BOTH 0x0003

#define DS_SETFONT 0x40L
#define DS_MODALFRAME 0x80L
#define DS_CENTER 0x0800L
#define DLGC_WANTARROWS 0x0001
#define DLGC_WANTTAB 0x0002
#define DLGC_WANTALLKEYS 0x0004
#define DLGC_WANTCHARS 0x0080

#define DT_TOP 0x00000000
#define DT_LEFT 0x00000000
#define DT_CENTER 0x00000001
#define DT_RIGHT 0x00000002
#define DT_VCENTER 0x00000004
#define DT_BOTTOM 0x00000008
#define DT_WORDBREAK 0x00000010
#define DT_SINGLELINE 0x00000020
#define DT_EXPANDTABS 0x00000040
#define DT_TABSTOP 0x00000080
#define DT_NOCLIP 0x00000100
#define DT_EXTERNALLEADING 0x00000200
#define DT_CALCRECT 0x00000400
#define DT_NOPREFIX 0x00000800
#define DT_EDITCONTROL 0x00002000
#define DT_PATH_ELLIPSIS 0x00004000
#define DT_END_ELLIPSIS 0x00008000
#define DT_WORD_ELLIPSIS 0x00040000
#define BDR_RAISEDOUTER 0x0001
#define BDR_SUNKENOUTER 0x0002
#define BDR_RAISEDINNER 0x0004
#define BDR_SUNKENINNER 0x0008
#define EDGE_RAISED (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP (BDR_RAISEDOUTER | BDR_SUNKENINNER)
#define BF_LEFT 0x0001
#define BF_TOP 0x0002
#define BF_RIGHT 0x0004
#define BF_BOTTOM 0x0008
#define BF_RECT (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)

#define CF_TEXT 1
#define CF_BITMAP 2
#define CF_DIB 8
#define CF_UNICODETEXT 13
#define CF_HDROP 15

// ---- structures ----
typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
    DWORD lPrivate;
} MSG, *PMSG, *LPMSG;

typedef struct tagWNDCLASSA {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCSTR lpszMenuName, lpszClassName;
} WNDCLASSA, *PWNDCLASSA, *LPWNDCLASSA;
typedef struct tagWNDCLASSW {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCWSTR lpszMenuName, lpszClassName;
} WNDCLASSW, *PWNDCLASSW, *LPWNDCLASSW;
typedef struct tagWNDCLASSEXA {
    UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCSTR lpszMenuName, lpszClassName; HICON hIconSm;
} WNDCLASSEXA, *PWNDCLASSEXA, *LPWNDCLASSEXA;
typedef struct tagWNDCLASSEXW {
    UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
    LPCWSTR lpszMenuName, lpszClassName; HICON hIconSm;
} WNDCLASSEXW, *PWNDCLASSEXW, *LPWNDCLASSEXW;

typedef struct tagPAINTSTRUCT {
    HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore, fIncUpdate; BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagCREATESTRUCTA {
    LPVOID lpCreateParams; HINSTANCE hInstance; HMENU hMenu; HWND hwndParent;
    int cy, cx, y, x; LONG style; LPCSTR lpszName, lpszClass; DWORD dwExStyle;
} CREATESTRUCTA, *LPCREATESTRUCTA;
typedef struct tagCREATESTRUCTW {
    LPVOID lpCreateParams; HINSTANCE hInstance; HMENU hMenu; HWND hwndParent;
    int cy, cx, y, x; LONG style; LPCWSTR lpszName, lpszClass; DWORD dwExStyle;
} CREATESTRUCTW, *LPCREATESTRUCTW;

typedef struct tagMINMAXINFO {
    POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;

typedef struct tagWINDOWPOS {
    HWND hwnd, hwndInsertAfter; int x, y, cx, cy; UINT flags;
} WINDOWPOS, *LPWINDOWPOS, *PWINDOWPOS;

// WM_NCCALCSIZE with wparam TRUE: the new, old and client rectangles
typedef struct tagNCCALCSIZE_PARAMS {
    RECT rgrc[3]; PWINDOWPOS lppos;
} NCCALCSIZE_PARAMS, *LPNCCALCSIZE_PARAMS;

// WM_SIZING: which edge is dragged
#define WMSZ_LEFT 1
#define WMSZ_RIGHT 2
#define WMSZ_TOP 3
#define WMSZ_TOPLEFT 4
#define WMSZ_TOPRIGHT 5
#define WMSZ_BOTTOM 6
#define WMSZ_BOTTOMLEFT 7
#define WMSZ_BOTTOMRIGHT 8

// SetLayeredWindowAttributes
#define LWA_COLORKEY 0x00000001
#define LWA_ALPHA 0x00000002

// MsgWaitForMultipleObjects: what wakes the wait
#define QS_KEY 0x0001
#define QS_MOUSEMOVE 0x0002
#define QS_MOUSEBUTTON 0x0004
#define QS_POSTMESSAGE 0x0008
#define QS_TIMER 0x0010
#define QS_PAINT 0x0020
#define QS_SENDMESSAGE 0x0040
#define QS_HOTKEY 0x0080
#define QS_ALLPOSTMESSAGE 0x0100
#define QS_RAWINPUT 0x0400
#define QS_TOUCH 0x0800
#define QS_POINTER 0x1000
#define QS_MOUSE (QS_MOUSEMOVE | QS_MOUSEBUTTON)
#define QS_INPUT (QS_MOUSE | QS_KEY | QS_RAWINPUT | QS_TOUCH | QS_POINTER)
#define QS_ALLEVENTS (QS_INPUT | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_HOTKEY)
#define QS_ALLINPUT (QS_INPUT | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_HOTKEY | QS_SENDMESSAGE)
#define MWMO_WAITALL 0x0001
#define MWMO_ALERTABLE 0x0002
#define MWMO_INPUTAVAILABLE 0x0004

// per-monitor DPI (Windows 10)
typedef struct DPI_AWARENESS_CONTEXT__ *DPI_AWARENESS_CONTEXT;
#define DPI_AWARENESS_CONTEXT_UNAWARE ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((DPI_AWARENESS_CONTEXT)-3)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#define DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED ((DPI_AWARENESS_CONTEXT)-5)

typedef struct tagWINDOWPLACEMENT {
    UINT length, flags, showCmd; POINT ptMinPosition, ptMaxPosition; RECT rcNormalPosition;
} WINDOWPLACEMENT, *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;

typedef struct tagSCROLLINFO {
    UINT cbSize, fMask; int nMin, nMax; UINT nPage; int nPos, nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;
typedef const SCROLLINFO *LPCSCROLLINFO;

typedef struct tagNMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR, *LPNMHDR;

typedef struct tagDRAWITEMSTRUCT {
    UINT CtlType, CtlID, itemID, itemAction, itemState;
    HWND hwndItem; HDC hDC; RECT rcItem; ULONG_PTR itemData;
} DRAWITEMSTRUCT, *PDRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;

typedef struct tagTRACKMOUSEEVENT { DWORD cbSize, dwFlags; HWND hwndTrack; DWORD dwHoverTime; } TRACKMOUSEEVENT, *LPTRACKMOUSEEVENT;

typedef struct tagMONITORINFO { DWORD cbSize; RECT rcMonitor, rcWork; DWORD dwFlags; } MONITORINFO, *LPMONITORINFO;

typedef struct tagACCEL { BYTE fVirt; WORD key, cmd; } ACCEL, *LPACCEL;
#define FVIRTKEY TRUE
#define FSHIFT 0x04
#define FCONTROL 0x08
#define FALT 0x10

// ---- functions ----
int MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type);
int MessageBoxW(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type);
BOOL MessageBeep(UINT type);
ATOM RegisterClassA(const WNDCLASSA *wc);
ATOM RegisterClassW(const WNDCLASSW *wc);
ATOM RegisterClassExA(const WNDCLASSEXA *wc);
ATOM RegisterClassExW(const WNDCLASSEXW *wc);
BOOL UnregisterClassA(LPCSTR name, HINSTANCE inst);
BOOL UnregisterClassW(LPCWSTR name, HINSTANCE inst);
BOOL GetClassInfoExA(HINSTANCE inst, LPCSTR name, LPWNDCLASSEXA wc);
BOOL GetClassInfoExW(HINSTANCE inst, LPCWSTR name, LPWNDCLASSEXW wc);
HWND CreateWindowExA(DWORD ex_style, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h,
                     HWND parent, HMENU menu, HINSTANCE inst, LPVOID param);
HWND CreateWindowExW(DWORD ex_style, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y, int w, int h,
                     HWND parent, HMENU menu, HINSTANCE inst, LPVOID param);
#define CreateWindowA(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExA(0L, c, n, s, x, y, w, h, p, m, i, l)
#define CreateWindowW(c, n, s, x, y, w, h, p, m, i, l) CreateWindowExW(0L, c, n, s, x, y, w, h, p, m, i, l)
BOOL DestroyWindow(HWND hwnd);
BOOL ShowWindow(HWND hwnd, int cmd);
BOOL UpdateWindow(HWND hwnd);
BOOL GetMessageA(LPMSG msg, HWND hwnd, UINT first, UINT last);
BOOL GetMessageW(LPMSG msg, HWND hwnd, UINT first, UINT last);
BOOL PeekMessageA(LPMSG msg, HWND hwnd, UINT first, UINT last, UINT remove);
BOOL PeekMessageW(LPMSG msg, HWND hwnd, UINT first, UINT last, UINT remove);
BOOL TranslateMessage(const MSG *msg);
LRESULT DispatchMessageA(const MSG *msg);
LRESULT DispatchMessageW(const MSG *msg);
BOOL WaitMessage(void);
void PostQuitMessage(int code);
BOOL PostMessageA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL PostMessageW(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL PostThreadMessageA(DWORD thread, UINT msg, WPARAM wp, LPARAM lp);
BOOL PostThreadMessageW(DWORD thread, UINT msg, WPARAM wp, LPARAM lp);
LRESULT SendMessageA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT SendMessageW(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT DefWindowProcA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT DefWindowProcW(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CallWindowProcA(WNDPROC fn, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CallWindowProcW(WNDPROC fn, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL IsDialogMessageA(HWND dlg, LPMSG msg);
BOOL IsDialogMessageW(HWND dlg, LPMSG msg);
HACCEL CreateAcceleratorTableA(LPACCEL a, int n);
HACCEL CreateAcceleratorTableW(LPACCEL a, int n);
int TranslateAcceleratorA(HWND hwnd, HACCEL acc, LPMSG msg);
int TranslateAcceleratorW(HWND hwnd, HACCEL acc, LPMSG msg);
BOOL DestroyAcceleratorTable(HACCEL acc);

HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT ps);
BOOL EndPaint(HWND hwnd, const PAINTSTRUCT *ps);
BOOL InvalidateRect(HWND hwnd, const RECT *r, BOOL erase);
BOOL ValidateRect(HWND hwnd, const RECT *r);
BOOL RedrawWindow(HWND hwnd, const RECT *r, HRGN rgn, UINT flags);
BOOL GetUpdateRect(HWND hwnd, LPRECT r, BOOL erase);
HDC GetDC(HWND hwnd);
HDC GetWindowDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC dc);
BOOL GetClientRect(HWND hwnd, LPRECT r);
BOOL GetWindowRect(HWND hwnd, LPRECT r);
BOOL SetWindowPos(HWND hwnd, HWND after, int x, int y, int w, int h, UINT flags);
BOOL MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL repaint);
BOOL GetWindowPlacement(HWND hwnd, WINDOWPLACEMENT *wp);
BOOL SetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT *wp);
BOOL AdjustWindowRect(LPRECT r, DWORD style, BOOL menu);
BOOL AdjustWindowRectEx(LPRECT r, DWORD style, BOOL menu, DWORD ex_style);
BOOL ScreenToClient(HWND hwnd, LPPOINT p);
BOOL ClientToScreen(HWND hwnd, LPPOINT p);
int MapWindowPoints(HWND from, HWND to, LPPOINT p, UINT n);
HWND WindowFromPoint(POINT p);
HWND ChildWindowFromPoint(HWND parent, POINT p);
BOOL SetWindowTextA(HWND hwnd, LPCSTR text);
BOOL SetWindowTextW(HWND hwnd, LPCWSTR text);
int GetWindowTextA(HWND hwnd, LPSTR buf, int cap);
int GetWindowTextW(HWND hwnd, LPWSTR buf, int cap);
int GetWindowTextLengthA(HWND hwnd);
int GetWindowTextLengthW(HWND hwnd);
HWND GetDlgItem(HWND dlg, int id);
int GetDlgCtrlID(HWND hwnd);
BOOL SetDlgItemTextA(HWND dlg, int id, LPCSTR text);
BOOL SetDlgItemTextW(HWND dlg, int id, LPCWSTR text);
UINT GetDlgItemTextA(HWND dlg, int id, LPSTR buf, int cap);
UINT GetDlgItemTextW(HWND dlg, int id, LPWSTR buf, int cap);
BOOL SetDlgItemInt(HWND dlg, int id, UINT value, BOOL is_signed);
UINT GetDlgItemInt(HWND dlg, int id, BOOL *ok, BOOL is_signed);
BOOL CheckDlgButton(HWND dlg, int id, UINT check);
UINT IsDlgButtonChecked(HWND dlg, int id);
BOOL CheckRadioButton(HWND dlg, int first, int last, int check);
LRESULT SendDlgItemMessageA(HWND dlg, int id, UINT msg, WPARAM wp, LPARAM lp);
LRESULT SendDlgItemMessageW(HWND dlg, int id, UINT msg, WPARAM wp, LPARAM lp);
BOOL EnableWindow(HWND hwnd, BOOL enable);
BOOL IsWindowEnabled(HWND hwnd);
BOOL IsWindowVisible(HWND hwnd);
BOOL IsWindow(HWND hwnd);
BOOL IsIconic(HWND hwnd);
BOOL IsZoomed(HWND hwnd);
HWND SetFocus(HWND hwnd);
HWND GetFocus(void);
HWND GetActiveWindow(void);
HWND SetActiveWindow(HWND hwnd);
HWND GetForegroundWindow(void);
BOOL SetForegroundWindow(HWND hwnd);
BOOL BringWindowToTop(HWND hwnd);
BOOL FlashWindow(HWND hwnd, BOOL invert);
HWND GetParent(HWND hwnd);
HWND SetParent(HWND child, HWND parent);
HWND GetDesktopWindow(void);
HWND GetWindow(HWND hwnd, UINT cmd);
HWND FindWindowA(LPCSTR cls, LPCSTR name);
HWND FindWindowW(LPCWSTR cls, LPCWSTR name);
HWND FindWindowExA(HWND parent, HWND after, LPCSTR cls, LPCSTR name);
HWND FindWindowExW(HWND parent, HWND after, LPCWSTR cls, LPCWSTR name);
BOOL EnumWindows(WNDENUMPROC fn, LPARAM lp);
BOOL EnumChildWindows(HWND parent, WNDENUMPROC fn, LPARAM lp);
LONG GetWindowLongA(HWND hwnd, int index);
LONG GetWindowLongW(HWND hwnd, int index);
LONG SetWindowLongA(HWND hwnd, int index, LONG value);
LONG SetWindowLongW(HWND hwnd, int index, LONG value);
LONG_PTR GetWindowLongPtrA(HWND hwnd, int index);
LONG_PTR GetWindowLongPtrW(HWND hwnd, int index);
LONG_PTR SetWindowLongPtrA(HWND hwnd, int index, LONG_PTR value);
LONG_PTR SetWindowLongPtrW(HWND hwnd, int index, LONG_PTR value);
ULONG_PTR GetClassLongPtrA(HWND hwnd, int index);
ULONG_PTR GetClassLongPtrW(HWND hwnd, int index);
ULONG_PTR SetClassLongPtrA(HWND hwnd, int index, LONG_PTR value);
ULONG_PTR SetClassLongPtrW(HWND hwnd, int index, LONG_PTR value);
int GetClassNameA(HWND hwnd, LPSTR buf, int cap);
int GetClassNameW(HWND hwnd, LPWSTR buf, int cap);
BOOL SetPropA(HWND hwnd, LPCSTR name, HANDLE data);
BOOL SetPropW(HWND hwnd, LPCWSTR name, HANDLE data);
HANDLE GetPropA(HWND hwnd, LPCSTR name);
HANDLE GetPropW(HWND hwnd, LPCWSTR name);
HANDLE RemovePropA(HWND hwnd, LPCSTR name);
HANDLE RemovePropW(HWND hwnd, LPCWSTR name);

HCURSOR LoadCursorA(HINSTANCE inst, LPCSTR name);
HCURSOR LoadCursorW(HINSTANCE inst, LPCWSTR name);
HICON LoadIconA(HINSTANCE inst, LPCSTR name);
HICON LoadIconW(HINSTANCE inst, LPCWSTR name);
HANDLE LoadImageA(HINSTANCE inst, LPCSTR name, UINT type, int w, int h, UINT flags);
HANDLE LoadImageW(HINSTANCE inst, LPCWSTR name, UINT type, int w, int h, UINT flags);
BOOL DestroyIcon(HICON icon);
BOOL DestroyCursor(HCURSOR cursor);
BOOL DrawIcon(HDC dc, int x, int y, HICON icon);
BOOL DrawIconEx(HDC dc, int x, int y, HICON icon, int w, int h, UINT step, HBRUSH brush, UINT flags);
HCURSOR SetCursor(HCURSOR cursor);
HCURSOR GetCursor(void);
int ShowCursor(BOOL show);
BOOL GetCursorPos(LPPOINT p);
BOOL SetCursorPos(int x, int y);
BOOL ClipCursor(const RECT *r);
HWND SetCapture(HWND hwnd);
HWND GetCapture(void);
BOOL ReleaseCapture(void);
BOOL TrackMouseEvent(LPTRACKMOUSEEVENT t);
UINT GetCaretBlinkTime(void);
SHORT GetKeyState(int key);
SHORT GetAsyncKeyState(int key);
BOOL GetKeyboardState(PBYTE keys);
UINT MapVirtualKeyA(UINT code, UINT type);
UINT MapVirtualKeyW(UINT code, UINT type);
#define KEYEVENTF_EXTENDEDKEY 0x0001
#define KEYEVENTF_KEYUP 0x0002
#define KEYEVENTF_UNICODE 0x0004
#define KEYEVENTF_SCANCODE 0x0008
void keybd_event(BYTE vk, BYTE scan, DWORD flags, ULONG_PTR extra);
void mouse_event(DWORD flags, DWORD dx, DWORD dy, DWORD data, ULONG_PTR extra);
BOOL RegisterHotKey(HWND hwnd, int id, UINT mods, UINT vk);
BOOL UnregisterHotKey(HWND hwnd, int id);

UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT ms, TIMERPROC fn);
BOOL KillTimer(HWND hwnd, UINT_PTR id);
int GetSystemMetrics(int index);
BOOL SystemParametersInfoA(UINT action, UINT param, PVOID data, UINT win_ini);
BOOL SystemParametersInfoW(UINT action, UINT param, PVOID data, UINT win_ini);
DWORD GetSysColor(int index);
HBRUSH GetSysColorBrush(int index);
UINT GetDpiForWindow(HWND hwnd);
UINT GetDpiForSystem(void);
BOOL SetProcessDPIAware(void);
BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value);
DPI_AWARENESS_CONTEXT SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT value);
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL wait_all, DWORD ms, DWORD wake);
DWORD MsgWaitForMultipleObjectsEx(DWORD count, const HANDLE *handles, DWORD ms, DWORD wake, DWORD flags);
BOOL SetLayeredWindowAttributes(HWND hwnd, COLORREF key, BYTE alpha, DWORD flags);
BOOL GetLayeredWindowAttributes(HWND hwnd, COLORREF *key, BYTE *alpha, DWORD *flags);
HMONITOR MonitorFromWindow(HWND hwnd, DWORD flags);
HMONITOR MonitorFromPoint(POINT p, DWORD flags);
BOOL GetMonitorInfoA(HMONITOR m, LPMONITORINFO info);
BOOL GetMonitorInfoW(HMONITOR m, LPMONITORINFO info);
BOOL EnumDisplayMonitors(HDC dc, LPCRECT clip, MONITORENUMPROC fn, LPARAM lp);

HMENU CreateMenu(void);
HMENU CreatePopupMenu(void);
BOOL DestroyMenu(HMENU menu);
BOOL AppendMenuA(HMENU menu, UINT flags, UINT_PTR id, LPCSTR text);
BOOL AppendMenuW(HMENU menu, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL InsertMenuA(HMENU menu, UINT pos, UINT flags, UINT_PTR id, LPCSTR text);
BOOL InsertMenuW(HMENU menu, UINT pos, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL ModifyMenuA(HMENU menu, UINT pos, UINT flags, UINT_PTR id, LPCSTR text);
BOOL ModifyMenuW(HMENU menu, UINT pos, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL DeleteMenu(HMENU menu, UINT pos, UINT flags);
BOOL RemoveMenu(HMENU menu, UINT pos, UINT flags);
BOOL SetMenu(HWND hwnd, HMENU menu);
HMENU GetMenu(HWND hwnd);
HMENU GetSubMenu(HMENU menu, int pos);
HMENU GetSystemMenu(HWND hwnd, BOOL revert);
int GetMenuItemCount(HMENU menu);
DWORD CheckMenuItem(HMENU menu, UINT id, UINT check);
BOOL CheckMenuRadioItem(HMENU menu, UINT first, UINT last, UINT check, UINT flags);
BOOL EnableMenuItem(HMENU menu, UINT id, UINT enable);
BOOL DrawMenuBar(HWND hwnd);
BOOL TrackPopupMenu(HMENU menu, UINT flags, int x, int y, int reserved, HWND hwnd, const RECT *r);

BOOL OpenClipboard(HWND hwnd);
BOOL CloseClipboard(void);
BOOL EmptyClipboard(void);
HANDLE SetClipboardData(UINT format, HANDLE data);
HANDLE GetClipboardData(UINT format);
BOOL IsClipboardFormatAvailable(UINT format);

int FillRect(HDC dc, const RECT *r, HBRUSH brush);
int FrameRect(HDC dc, const RECT *r, HBRUSH brush);
BOOL InvertRect(HDC dc, const RECT *r);
BOOL DrawFocusRect(HDC dc, const RECT *r);
BOOL DrawEdge(HDC dc, LPRECT r, UINT edge, UINT flags);
int DrawTextA(HDC dc, LPCSTR text, int n, LPRECT r, UINT format);
int DrawTextW(HDC dc, LPCWSTR text, int n, LPRECT r, UINT format);
BOOL SetRect(LPRECT r, int left, int top, int right, int bottom);
BOOL SetRectEmpty(LPRECT r);
BOOL CopyRect(LPRECT d, const RECT *s);
BOOL InflateRect(LPRECT r, int dx, int dy);
BOOL OffsetRect(LPRECT r, int dx, int dy);
BOOL IntersectRect(LPRECT d, const RECT *a, const RECT *b);
BOOL UnionRect(LPRECT d, const RECT *a, const RECT *b);
BOOL PtInRect(const RECT *r, POINT p);
BOOL IsRectEmpty(const RECT *r);
BOOL EqualRect(const RECT *a, const RECT *b);

int GetScrollPos(HWND hwnd, int bar);
int SetScrollPos(HWND hwnd, int bar, int pos, BOOL redraw);
BOOL GetScrollRange(HWND hwnd, int bar, LPINT min, LPINT max);
BOOL SetScrollRange(HWND hwnd, int bar, int min, int max, BOOL redraw);
BOOL GetScrollInfo(HWND hwnd, int bar, LPSCROLLINFO info);
int SetScrollInfo(HWND hwnd, int bar, LPCSCROLLINFO info, BOOL redraw);
BOOL ShowScrollBar(HWND hwnd, int bar, BOOL show);
BOOL EnableScrollBar(HWND hwnd, UINT bar, UINT arrows);
BOOL ScrollWindow(HWND hwnd, int dx, int dy, const RECT *r, const RECT *clip);

int wsprintfA(LPSTR buf, LPCSTR fmt, ...);
int wsprintfW(LPWSTR buf, LPCWSTR fmt, ...);
int wvsprintfA(LPSTR buf, LPCSTR fmt, va_list ap);
int wvsprintfW(LPWSTR buf, LPCWSTR fmt, va_list ap);
LPSTR CharUpperA(LPSTR s);
LPWSTR CharUpperW(LPWSTR s);
LPSTR CharLowerA(LPSTR s);
LPWSTR CharLowerW(LPWSTR s);
LPSTR CharNextA(LPCSTR s);
LPWSTR CharNextW(LPCWSTR s);
BOOL IsCharAlphaA(CHAR c);
BOOL IsCharAlphaW(WCHAR c);
BOOL IsCharAlphaNumericA(CHAR c);
BOOL IsCharAlphaNumericW(WCHAR c);
HKL GetKeyboardLayout(DWORD thread);
BOOL ExitWindowsEx(UINT flags, DWORD reason);
BOOL LockWorkStation(void);
HHOOK SetWindowsHookExA(int id, HOOKPROC fn, HINSTANCE mod, DWORD thread);
HHOOK SetWindowsHookExW(int id, HOOKPROC fn, HINSTANCE mod, DWORD thread);
BOOL UnhookWindowsHookEx(HHOOK hook);
LRESULT CallNextHookEx(HHOOK hook, int code, WPARAM wp, LPARAM lp);
UINT RegisterWindowMessageA(LPCSTR name);
UINT RegisterWindowMessageW(LPCWSTR name);

#define MessageBox __AW(MessageBox)
#define WNDCLASS __AW(WNDCLASS)
#define PWNDCLASS __AW(PWNDCLASS)
#define LPWNDCLASS __AW(LPWNDCLASS)
#define WNDCLASSEX __AW(WNDCLASSEX)
#define PWNDCLASSEX __AW(PWNDCLASSEX)
#define LPWNDCLASSEX __AW(LPWNDCLASSEX)
#define CREATESTRUCT __AW(CREATESTRUCT)
#define LPCREATESTRUCT __AW(LPCREATESTRUCT)
#define RegisterClass __AW(RegisterClass)
#define RegisterClassEx __AW(RegisterClassEx)
#define UnregisterClass __AW(UnregisterClass)
#define GetClassInfoEx __AW(GetClassInfoEx)
#define CreateWindowEx __AW(CreateWindowEx)
#define CreateWindow __AW(CreateWindow)
#define GetMessage __AW(GetMessage)
#define PeekMessage __AW(PeekMessage)
#define DispatchMessage __AW(DispatchMessage)
#define PostMessage __AW(PostMessage)
#define PostThreadMessage __AW(PostThreadMessage)
#define SendMessage __AW(SendMessage)
#define DefWindowProc __AW(DefWindowProc)
#define CallWindowProc __AW(CallWindowProc)
#define IsDialogMessage __AW(IsDialogMessage)
#define CreateAcceleratorTable __AW(CreateAcceleratorTable)
#define TranslateAccelerator __AW(TranslateAccelerator)
#define SetWindowText __AW(SetWindowText)
#define GetWindowText __AW(GetWindowText)
#define GetWindowTextLength __AW(GetWindowTextLength)
#define SetDlgItemText __AW(SetDlgItemText)
#define GetDlgItemText __AW(GetDlgItemText)
#define SendDlgItemMessage __AW(SendDlgItemMessage)
#define FindWindow __AW(FindWindow)
#define FindWindowEx __AW(FindWindowEx)
#define GetWindowLong __AW(GetWindowLong)
#define SetWindowLong __AW(SetWindowLong)
#define GetWindowLongPtr __AW(GetWindowLongPtr)
#define SetWindowLongPtr __AW(SetWindowLongPtr)
#define GetClassLongPtr __AW(GetClassLongPtr)
#define SetClassLongPtr __AW(SetClassLongPtr)
#define GetClassName __AW(GetClassName)
#define SetProp __AW(SetProp)
#define GetProp __AW(GetProp)
#define RemoveProp __AW(RemoveProp)
#define LoadCursor __AW(LoadCursor)
#define LoadIcon __AW(LoadIcon)
#define LoadImage __AW(LoadImage)
#define MapVirtualKey __AW(MapVirtualKey)
#define SystemParametersInfo __AW(SystemParametersInfo)
#define GetMonitorInfo __AW(GetMonitorInfo)
#define AppendMenu __AW(AppendMenu)
#define InsertMenu __AW(InsertMenu)
#define ModifyMenu __AW(ModifyMenu)
#define DrawText __AW(DrawText)
#define wsprintf __AW(wsprintf)
#define wvsprintf __AW(wvsprintf)
#define CharUpper __AW(CharUpper)
#define CharLower __AW(CharLower)
#define CharNext __AW(CharNext)
#define IsCharAlpha __AW(IsCharAlpha)
#define SetWindowsHookEx __AW(SetWindowsHookEx)
#define RegisterWindowMessage __AW(RegisterWindowMessage)
