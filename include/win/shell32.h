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

// shlobj.h: choosing a folder (SHBrowseForFolder) and the item ids of the shell
#define BIF_RETURNONLYFSDIRS 0x00000001
#define BIF_DONTGOBELOWDOMAIN 0x00000002
#define BIF_STATUSTEXT 0x00000004
#define BIF_RETURNFSANCESTORS 0x00000008
#define BIF_EDITBOX 0x00000010
#define BIF_VALIDATE 0x00000020
#define BIF_NEWDIALOGSTYLE 0x00000040
#define BIF_USENEWUI (BIF_NEWDIALOGSTYLE | BIF_EDITBOX)
#define BIF_BROWSEINCLUDEURLS 0x00000080
#define BIF_UAHINT 0x00000100
#define BIF_NONEWFOLDERBUTTON 0x00000200
#define BIF_NOTRANSLATETARGETS 0x00000400
#define BIF_BROWSEFORCOMPUTER 0x00001000
#define BIF_BROWSEFORPRINTER 0x00002000
#define BIF_BROWSEINCLUDEFILES 0x00004000
#define BIF_SHAREABLE 0x00008000
#define BFFM_INITIALIZED 1
#define BFFM_SELCHANGED 2
#define BFFM_SETSTATUSTEXTA (WM_USER + 100)
#define BFFM_ENABLEOK (WM_USER + 101)
#define BFFM_SETSELECTIONA (WM_USER + 102)
#define BFFM_SETSELECTIONW (WM_USER + 103)
#define BFFM_SETSTATUSTEXTW (WM_USER + 104)

#pragma pack(push, 1)
typedef struct _SHITEMID { USHORT cb; BYTE abID[1]; } SHITEMID, *LPSHITEMID;
typedef struct _ITEMIDLIST { SHITEMID mkid; } ITEMIDLIST, *LPITEMIDLIST;
#pragma pack(pop)
typedef const ITEMIDLIST *LPCITEMIDLIST;
typedef ITEMIDLIST *PIDLIST_ABSOLUTE;
typedef const ITEMIDLIST *PCIDLIST_ABSOLUTE;

typedef int (CALLBACK *BFFCALLBACK)(HWND hwnd, UINT msg, LPARAM lparam, LPARAM data);
typedef struct _browseinfoA {
    HWND hwndOwner; PCIDLIST_ABSOLUTE pidlRoot; LPSTR pszDisplayName; LPCSTR lpszTitle; UINT ulFlags;
    BFFCALLBACK lpfn; LPARAM lParam; int iImage;
} BROWSEINFOA, *PBROWSEINFOA, *LPBROWSEINFOA;
typedef struct _browseinfoW {
    HWND hwndOwner; PCIDLIST_ABSOLUTE pidlRoot; LPWSTR pszDisplayName; LPCWSTR lpszTitle; UINT ulFlags;
    BFFCALLBACK lpfn; LPARAM lParam; int iImage;
} BROWSEINFOW, *PBROWSEINFOW, *LPBROWSEINFOW;

PIDLIST_ABSOLUTE SHBrowseForFolderA(LPBROWSEINFOA bi);
PIDLIST_ABSOLUTE SHBrowseForFolderW(LPBROWSEINFOW bi);
BOOL SHGetPathFromIDListA(PCIDLIST_ABSOLUTE pidl, LPSTR path);
BOOL SHGetPathFromIDListW(PCIDLIST_ABSOLUTE pidl, LPWSTR path);
BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR path, int csidl, BOOL create);
BOOL SHGetSpecialFolderPathW(HWND hwnd, LPWSTR path, int csidl, BOOL create);
HRESULT SHGetSpecialFolderLocation(HWND hwnd, int csidl, PIDLIST_ABSOLUTE *pidl);
void ILFree(PIDLIST_ABSOLUTE pidl);

// ole32: the shell's memory (a PIDL is freed with CoTaskMemFree) and COM's start
#define COINIT_APARTMENTTHREADED 0x2
#define COINIT_MULTITHREADED 0x0
#define COINIT_DISABLE_OLE1DDE 0x4
LPVOID CoTaskMemAlloc(SIZE_T size);
LPVOID CoTaskMemRealloc(LPVOID p, SIZE_T size);
void CoTaskMemFree(LPVOID p);
HRESULT CoInitialize(LPVOID reserved);
HRESULT CoInitializeEx(LPVOID reserved, DWORD flags);
void CoUninitialize(void);

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
#define BROWSEINFO __AW(BROWSEINFO)
#define PBROWSEINFO __AW(PBROWSEINFO)
#define LPBROWSEINFO __AW(LPBROWSEINFO)
#define SHBrowseForFolder __AW(SHBrowseForFolder)
#define SHGetPathFromIDList __AW(SHGetPathFromIDList)
#define SHGetSpecialFolderPath __AW(SHGetSpecialFolderPath)
#define BFFM_SETSTATUSTEXT __AW(BFFM_SETSTATUSTEXT)
#define BFFM_SETSELECTION __AW(BFFM_SETSELECTION)
