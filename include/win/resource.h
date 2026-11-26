// windows.h: resources -- finding and reading them (kernel32), strings, menus, accelerators,
// dialogs, bitmaps (user32), the version (version.dll), and the numbers .rc scripts use.
#pragma once

// ---- types of resources ----
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_FONTDIR MAKEINTRESOURCE(7)
#define RT_FONT MAKEINTRESOURCE(8)
#define RT_ACCELERATOR MAKEINTRESOURCE(9)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_MESSAGETABLE MAKEINTRESOURCE(11)
#define RT_GROUP_CURSOR MAKEINTRESOURCE(12)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)
#define RT_VERSION MAKEINTRESOURCE(16)
#define RT_DLGINCLUDE MAKEINTRESOURCE(17)
#define RT_PLUGPLAY MAKEINTRESOURCE(19)
#define RT_VXD MAKEINTRESOURCE(20)
#define RT_ANICURSOR MAKEINTRESOURCE(21)
#define RT_ANIICON MAKEINTRESOURCE(22)
#define RT_HTML MAKEINTRESOURCE(23)
#define RT_MANIFEST MAKEINTRESOURCE(24)
#define CREATEPROCESS_MANIFEST_RESOURCE_ID 1
#define ISOLATIONAWARE_MANIFEST_RESOURCE_ID 2

// ---- kernel32: find, load, enumerate ----
#define LOAD_LIBRARY_AS_DATAFILE 0x00000002
#define LOAD_LIBRARY_AS_IMAGE_RESOURCE 0x00000020
#define LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE 0x00000040
#define DONT_RESOLVE_DLL_REFERENCES 0x00000001

typedef BOOL (*ENUMRESTYPEPROCA)(HMODULE module, LPSTR type, LONG_PTR param);
typedef BOOL (*ENUMRESTYPEPROCW)(HMODULE module, LPWSTR type, LONG_PTR param);
typedef BOOL (*ENUMRESNAMEPROCA)(HMODULE module, LPCSTR type, LPSTR name, LONG_PTR param);
typedef BOOL (*ENUMRESNAMEPROCW)(HMODULE module, LPCWSTR type, LPWSTR name, LONG_PTR param);
typedef BOOL (*ENUMRESLANGPROCA)(HMODULE module, LPCSTR type, LPCSTR name, WORD lang, LONG_PTR param);
typedef BOOL (*ENUMRESLANGPROCW)(HMODULE module, LPCWSTR type, LPCWSTR name, WORD lang, LONG_PTR param);

HMODULE LoadLibraryExA(LPCSTR name, HANDLE file, DWORD flags);
HMODULE LoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags);
HRSRC FindResourceA(HMODULE module, LPCSTR name, LPCSTR type);
HRSRC FindResourceW(HMODULE module, LPCWSTR name, LPCWSTR type);
HRSRC FindResourceExA(HMODULE module, LPCSTR type, LPCSTR name, WORD lang);
HRSRC FindResourceExW(HMODULE module, LPCWSTR type, LPCWSTR name, WORD lang);
HGLOBAL LoadResource(HMODULE module, HRSRC res);
LPVOID LockResource(HGLOBAL data);
DWORD SizeofResource(HMODULE module, HRSRC res);
BOOL FreeResource(HGLOBAL data);
BOOL EnumResourceTypesA(HMODULE module, ENUMRESTYPEPROCA fn, LONG_PTR param);
BOOL EnumResourceTypesW(HMODULE module, ENUMRESTYPEPROCW fn, LONG_PTR param);
BOOL EnumResourceNamesA(HMODULE module, LPCSTR type, ENUMRESNAMEPROCA fn, LONG_PTR param);
BOOL EnumResourceNamesW(HMODULE module, LPCWSTR type, ENUMRESNAMEPROCW fn, LONG_PTR param);
BOOL EnumResourceLanguagesA(HMODULE module, LPCSTR type, LPCSTR name, ENUMRESLANGPROCA fn, LONG_PTR param);
BOOL EnumResourceLanguagesW(HMODULE module, LPCWSTR type, LPCWSTR name, ENUMRESLANGPROCW fn, LONG_PTR param);
HANDLE BeginUpdateResourceA(LPCSTR file, BOOL delete_existing);
HANDLE BeginUpdateResourceW(LPCWSTR file, BOOL delete_existing);
BOOL UpdateResourceA(HANDLE update, LPCSTR type, LPCSTR name, WORD lang, LPVOID data, DWORD size);
BOOL UpdateResourceW(HANDLE update, LPCWSTR type, LPCWSTR name, WORD lang, LPVOID data, DWORD size);
BOOL EndUpdateResourceA(HANDLE update, BOOL discard);
BOOL EndUpdateResourceW(HANDLE update, BOOL discard);

// ---- user32: strings, menus, accelerators, dialogs, pictures ----
typedef struct tagACCEL { BYTE fVirt; WORD key; WORD cmd; } ACCEL, *LPACCEL;
#define FVIRTKEY 0x01
#define FNOINVERT 0x02
#define FSHIFT 0x04
#define FCONTROL 0x08
#define FALT 0x10

typedef struct tagMENUITEMINFOW {
    UINT cbSize, fMask, fType, fState, wID;
    HMENU hSubMenu;
    HBITMAP hbmpChecked, hbmpUnchecked;
    ULONG_PTR dwItemData;
    LPWSTR dwTypeData;
    UINT cch;
    HBITMAP hbmpItem;
} MENUITEMINFOW, *LPMENUITEMINFOW;
typedef const MENUITEMINFOW *LPCMENUITEMINFOW;
typedef struct tagMENUITEMINFOA {
    UINT cbSize, fMask, fType, fState, wID;
    HMENU hSubMenu;
    HBITMAP hbmpChecked, hbmpUnchecked;
    ULONG_PTR dwItemData;
    LPSTR dwTypeData;
    UINT cch;
    HBITMAP hbmpItem;
} MENUITEMINFOA, *LPMENUITEMINFOA;
typedef const MENUITEMINFOA *LPCMENUITEMINFOA;
#ifndef MIIM_STATE
#define MIIM_STATE 0x00000001
#define MIIM_ID 0x00000002
#define MIIM_SUBMENU 0x00000004
#define MIIM_CHECKMARKS 0x00000008
#define MIIM_TYPE 0x00000010
#define MIIM_DATA 0x00000020
#define MIIM_STRING 0x00000040
#define MIIM_BITMAP 0x00000080
#define MIIM_FTYPE 0x00000100
#endif
#ifndef MFT_STRING
#define MFT_STRING 0x00000000
#define MFT_BITMAP 0x00000004
#define MFT_MENUBARBREAK 0x00000020
#define MFT_MENUBREAK 0x00000040
#define MFT_OWNERDRAW 0x00000100
#define MFT_RADIOCHECK 0x00000200
#define MFT_SEPARATOR 0x00000800
#define MFT_RIGHTORDER 0x00002000
#define MFT_RIGHTJUSTIFY 0x00004000
#define MFS_GRAYED 0x00000003
#define MFS_DISABLED MFS_GRAYED
#define MFS_CHECKED 0x00000008
#define MFS_HILITE 0x00000080
#define MFS_ENABLED 0x00000000
#define MFS_UNCHECKED 0x00000000
#define MFS_UNHILITE 0x00000000
#define MFS_DEFAULT 0x00001000
#endif

int LoadStringA(HINSTANCE inst, UINT id, LPSTR buf, int cap);
int LoadStringW(HINSTANCE inst, UINT id, LPWSTR buf, int cap);
HMENU LoadMenuA(HINSTANCE inst, LPCSTR name);
HMENU LoadMenuW(HINSTANCE inst, LPCWSTR name);
HMENU LoadMenuIndirectW(const void *tmpl);
UINT GetMenuItemID(HMENU menu, int pos);
UINT GetMenuState(HMENU menu, UINT item, UINT flags);
int GetMenuStringA(HMENU menu, UINT item, LPSTR buf, int cap, UINT flags);
int GetMenuStringW(HMENU menu, UINT item, LPWSTR buf, int cap, UINT flags);
BOOL GetMenuItemInfoA(HMENU menu, UINT item, BOOL by_pos, LPMENUITEMINFOA info);
BOOL GetMenuItemInfoW(HMENU menu, UINT item, BOOL by_pos, LPMENUITEMINFOW info);
BOOL SetMenuItemInfoW(HMENU menu, UINT item, BOOL by_pos, LPCMENUITEMINFOW info);
BOOL InsertMenuItemW(HMENU menu, UINT item, BOOL by_pos, LPCMENUITEMINFOW info);
HACCEL LoadAcceleratorsA(HINSTANCE inst, LPCSTR name);
HACCEL LoadAcceleratorsW(HINSTANCE inst, LPCWSTR name);
HACCEL CreateAcceleratorTableW(LPACCEL accels, int count);
int CopyAcceleratorTableA(HACCEL table, LPACCEL out, int cap);
int CopyAcceleratorTableW(HACCEL table, LPACCEL out, int cap);
BOOL DestroyAcceleratorTable(HACCEL table);
HWND CreateDialogParamA(HINSTANCE inst, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param);
HWND CreateDialogParamW(HINSTANCE inst, LPCWSTR name, HWND parent, DLGPROC proc, LPARAM param);
HWND CreateDialogIndirectParamW(HINSTANCE inst, const void *tmpl, HWND parent, DLGPROC proc, LPARAM param);
INT_PTR DialogBoxParamA(HINSTANCE inst, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param);
INT_PTR DialogBoxParamW(HINSTANCE inst, LPCWSTR name, HWND parent, DLGPROC proc, LPARAM param);
INT_PTR DialogBoxIndirectParamW(HINSTANCE inst, const void *tmpl, HWND parent, DLGPROC proc, LPARAM param);
HBITMAP LoadBitmapA(HINSTANCE inst, LPCSTR name);
HBITMAP LoadBitmapW(HINSTANCE inst, LPCWSTR name);
typedef struct _ICONINFO { BOOL fIcon; DWORD xHotspot, yHotspot; HBITMAP hbmMask, hbmColor; } ICONINFO, *PICONINFO;
BOOL GetIconInfo(HICON icon, PICONINFO info);
BOOL EndDialog(HWND dlg, INT_PTR result);

#define CreateDialogA(i, n, p, f) CreateDialogParamA(i, n, p, f, 0)
#define CreateDialogW(i, n, p, f) CreateDialogParamW(i, n, p, f, 0)
#define DialogBoxA(i, n, p, f) DialogBoxParamA(i, n, p, f, 0)
#define DialogBoxW(i, n, p, f) DialogBoxParamW(i, n, p, f, 0)

// ---- version.dll ----
typedef struct tagVS_FIXEDFILEINFO {
    DWORD dwSignature, dwStrucVersion, dwFileVersionMS, dwFileVersionLS, dwProductVersionMS, dwProductVersionLS;
    DWORD dwFileFlagsMask, dwFileFlags, dwFileOS, dwFileType, dwFileSubtype, dwFileDateMS, dwFileDateLS;
} VS_FIXEDFILEINFO;
DWORD GetFileVersionInfoSizeA(LPCSTR file, LPDWORD handle);
DWORD GetFileVersionInfoSizeW(LPCWSTR file, LPDWORD handle);
BOOL GetFileVersionInfoA(LPCSTR file, DWORD handle, DWORD size, LPVOID data);
BOOL GetFileVersionInfoW(LPCWSTR file, DWORD handle, DWORD size, LPVOID data);
BOOL VerQueryValueA(LPCVOID block, LPCSTR path, LPVOID *out, PUINT size);
BOOL VerQueryValueW(LPCVOID block, LPCWSTR path, LPVOID *out, PUINT size);

// ---- the numbers of VERSIONINFO (winver.h) ----
#define VS_VERSION_INFO 1
#define VS_USER_DEFINED 100
#define VS_FFI_SIGNATURE 0xFEEF04BDL
#define VS_FFI_STRUCVERSION 0x00010000L
#define VS_FFI_FILEFLAGSMASK 0x0000003FL
#define VS_FF_DEBUG 0x00000001L
#define VS_FF_PRERELEASE 0x00000002L
#define VS_FF_PATCHED 0x00000004L
#define VS_FF_PRIVATEBUILD 0x00000008L
#define VS_FF_INFOINFERRED 0x00000010L
#define VS_FF_SPECIALBUILD 0x00000020L
#define VOS_UNKNOWN 0x00000000L
#define VOS_DOS 0x00010000L
#define VOS_NT 0x00040000L
#define VOS__WINDOWS32 0x00000004L
#define VOS_NT_WINDOWS32 0x00040004L
#define VOS_DOS_WINDOWS32 0x00010004L
#define VFT_UNKNOWN 0x00000000L
#define VFT_APP 0x00000001L
#define VFT_DLL 0x00000002L
#define VFT_DRV 0x00000003L
#define VFT_FONT 0x00000004L
#define VFT_VXD 0x00000005L
#define VFT_STATIC_LIB 0x00000007L
#define VFT2_UNKNOWN 0x00000000L

// ---- numbers of dialog scripts that windows.h does not have elsewhere ----
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif
#ifndef SUBLANG_NEUTRAL
#define SUBLANG_NEUTRAL 0x00
#endif
#ifndef SUBLANG_ENGLISH_US
#define SUBLANG_ENGLISH_US 0x01
#endif
#ifndef SUBLANG_RUSSIAN_RUSSIA
#define SUBLANG_RUSSIAN_RUSSIA 0x01
#endif
#ifndef LANG_ENGLISH
#define LANG_ENGLISH 0x09
#endif
#ifndef LANG_RUSSIAN
#define LANG_RUSSIAN 0x19
#endif

#define LoadLibraryEx __AW(LoadLibraryEx)
#define FindResource __AW(FindResource)
#define FindResourceEx __AW(FindResourceEx)
#define EnumResourceTypes __AW(EnumResourceTypes)
#define EnumResourceNames __AW(EnumResourceNames)
#define EnumResourceLanguages __AW(EnumResourceLanguages)
#define BeginUpdateResource __AW(BeginUpdateResource)
#define UpdateResource __AW(UpdateResource)
#define EndUpdateResource __AW(EndUpdateResource)
#define LoadString __AW(LoadString)
#define LoadMenu __AW(LoadMenu)
#define GetMenuString __AW(GetMenuString)
#define GetMenuItemInfo __AW(GetMenuItemInfo)
#define LoadAccelerators __AW(LoadAccelerators)
#define CopyAcceleratorTable __AW(CopyAcceleratorTable)
#define CreateDialogParam __AW(CreateDialogParam)
#define CreateDialog __AW(CreateDialog)
#define DialogBoxParam __AW(DialogBoxParam)
#define DialogBox __AW(DialogBox)
#define LoadBitmap __AW(LoadBitmap)
#define GetFileVersionInfoSize __AW(GetFileVersionInfoSize)
#define GetFileVersionInfo __AW(GetFileVersionInfo)
#define VerQueryValue __AW(VerQueryValue)
#define MENUITEMINFO __AW(MENUITEMINFO)
