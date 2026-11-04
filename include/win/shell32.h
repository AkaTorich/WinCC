// windows.h: shell32 -- opening files and links, dropped files, the tray icon, special folders.
#pragma once

#define SE_ERR_FNF 2
#define SE_ERR_PNF 3
#define SE_ERR_ACCESSDENIED 5
#define SE_ERR_NOASSOC 31
#define CSIDL_DESKTOP 0x0000
#define CSIDL_PROGRAMS 0x0002
#define CSIDL_PERSONAL 0x0005
#define CSIDL_MYDOCUMENTS CSIDL_PERSONAL
#define CSIDL_STARTUP 0x0007
#define CSIDL_DESKTOPDIRECTORY 0x0010
#define CSIDL_FONTS 0x0014
#define CSIDL_APPDATA 0x001A
#define CSIDL_LOCAL_APPDATA 0x001C
#define CSIDL_WINDOWS 0x0024
#define CSIDL_SYSTEM 0x0025
#define CSIDL_PROGRAM_FILES 0x0026
#define CSIDL_MYPICTURES 0x0027
#define CSIDL_PROFILE 0x0028
#define CSIDL_COMMON_APPDATA 0x0023
#define CSIDL_FLAG_CREATE 0x8000
#define SHGFP_TYPE_CURRENT 0
#define NIM_ADD 0x00000000
#define NIM_MODIFY 0x00000001
#define NIM_DELETE 0x00000002
#define NIF_MESSAGE 0x00000001
#define NIF_ICON 0x00000002
#define NIF_TIP 0x00000004
#define NIF_INFO 0x00000010
#define NIIF_NONE 0x00000000
#define NIIF_INFO 0x00000001
#define NIIF_WARNING 0x00000002
#define NIIF_ERROR 0x00000003
#define FO_MOVE 0x0001
#define FO_COPY 0x0002
#define FO_DELETE 0x0003
#define FO_RENAME 0x0004
#define FOF_SILENT 0x0004
#define FOF_NOCONFIRMATION 0x0010
#define FOF_ALLOWUNDO 0x0040
#define FOF_NOERRORUI 0x0400

typedef struct _NOTIFYICONDATAA {
    DWORD cbSize; HWND hWnd; UINT uID, uFlags, uCallbackMessage; HICON hIcon;
    CHAR szTip[128]; DWORD dwState, dwStateMask; CHAR szInfo[256];
    union { UINT uTimeout, uVersion; };
    CHAR szInfoTitle[64]; DWORD dwInfoFlags; GUID guidItem; HICON hBalloonIcon;
} NOTIFYICONDATAA, *PNOTIFYICONDATAA;
typedef struct _NOTIFYICONDATAW {
    DWORD cbSize; HWND hWnd; UINT uID, uFlags, uCallbackMessage; HICON hIcon;
    WCHAR szTip[128]; DWORD dwState, dwStateMask; WCHAR szInfo[256];
    union { UINT uTimeout, uVersion; };
    WCHAR szInfoTitle[64]; DWORD dwInfoFlags; GUID guidItem; HICON hBalloonIcon;
} NOTIFYICONDATAW, *PNOTIFYICONDATAW;

typedef struct _SHFILEOPSTRUCTA {
    HWND hwnd; UINT wFunc; LPCSTR pFrom, pTo; WORD fFlags; BOOL fAnyOperationsAborted;
    LPVOID hNameMappings; LPCSTR lpszProgressTitle;
} SHFILEOPSTRUCTA, *LPSHFILEOPSTRUCTA;
typedef struct _SHFILEOPSTRUCTW {
    HWND hwnd; UINT wFunc; LPCWSTR pFrom, pTo; WORD fFlags; BOOL fAnyOperationsAborted;
    LPVOID hNameMappings; LPCWSTR lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;

HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR verb, LPCSTR file, LPCSTR params, LPCSTR dir, INT show);
HINSTANCE ShellExecuteW(HWND hwnd, LPCWSTR verb, LPCWSTR file, LPCWSTR params, LPCWSTR dir, INT show);
void DragAcceptFiles(HWND hwnd, BOOL accept);
UINT DragQueryFileA(HDROP drop, UINT index, LPSTR buf, UINT cap);
UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR buf, UINT cap);
BOOL DragQueryPoint(HDROP drop, LPPOINT p);
void DragFinish(HDROP drop);
BOOL Shell_NotifyIconA(DWORD msg, PNOTIFYICONDATAA data);
BOOL Shell_NotifyIconW(DWORD msg, PNOTIFYICONDATAW data);
HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPSTR path);
HRESULT SHGetFolderPathW(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPWSTR path);
int SHFileOperationA(LPSHFILEOPSTRUCTA op);
int SHFileOperationW(LPSHFILEOPSTRUCTW op);
LPWSTR *CommandLineToArgvW(LPCWSTR cmd, int *argc);
HICON ExtractIconA(HINSTANCE inst, LPCSTR file, UINT index);
HICON ExtractIconW(HINSTANCE inst, LPCWSTR file, UINT index);

#define NOTIFYICONDATA __AW(NOTIFYICONDATA)
#define PNOTIFYICONDATA __AW(PNOTIFYICONDATA)
#define SHFILEOPSTRUCT __AW(SHFILEOPSTRUCT)
#define LPSHFILEOPSTRUCT __AW(LPSHFILEOPSTRUCT)
#define ShellExecute __AW(ShellExecute)
#define DragQueryFile __AW(DragQueryFile)
#define Shell_NotifyIcon __AW(Shell_NotifyIcon)
#define SHGetFolderPath __AW(SHGetFolderPath)
#define SHFileOperation __AW(SHFileOperation)
#define ExtractIcon __AW(ExtractIcon)
