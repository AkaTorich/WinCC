// windows.h: comdlg32 -- the dialogs to open and save a file, to pick a color or a font.
#pragma once

#define OFN_READONLY 0x00000001
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_HIDEREADONLY 0x00000004
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_SHOWHELP 0x00000010
#define OFN_NOVALIDATE 0x00000100
#define OFN_ALLOWMULTISELECT 0x00000200
#define OFN_EXTENSIONDIFFERENT 0x00000400
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
#define OFN_CREATEPROMPT 0x00002000
#define OFN_NOREADONLYRETURN 0x00008000
#define OFN_EXPLORER 0x00080000
#define OFN_NODEREFERENCELINKS 0x00100000
#define OFN_LONGNAMES 0x00200000
#define OFN_DONTADDTORECENT 0x02000000
#define OFN_FORCESHOWHIDDEN 0x10000000
#define CC_RGBINIT 0x00000001
#define CC_FULLOPEN 0x00000002
#define CC_PREVENTFULLOPEN 0x00000004
#define CC_ANYCOLOR 0x00000100
#define CF_SCREENFONTS 0x00000001
#define CF_EFFECTS 0x00000100
#define CF_INITTOLOGFONTSTRUCT 0x00000040
#define CF_NOVERTFONTS 0x01000000

typedef UINT_PTR (*LPOFNHOOKPROC)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
typedef UINT_PTR (*LPCCHOOKPROC)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
typedef UINT_PTR (*LPCFHOOKPROC)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

typedef struct tagOFNA {
    DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance;
    LPCSTR lpstrFilter; LPSTR lpstrCustomFilter; DWORD nMaxCustFilter, nFilterIndex;
    LPSTR lpstrFile; DWORD nMaxFile; LPSTR lpstrFileTitle; DWORD nMaxFileTitle;
    LPCSTR lpstrInitialDir, lpstrTitle; DWORD Flags; WORD nFileOffset, nFileExtension;
    LPCSTR lpstrDefExt; LPARAM lCustData; LPOFNHOOKPROC lpfnHook; LPCSTR lpTemplateName;
    void *pvReserved; DWORD dwReserved, FlagsEx;
} OPENFILENAMEA, *LPOPENFILENAMEA;
typedef struct tagOFNW {
    DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance;
    LPCWSTR lpstrFilter; LPWSTR lpstrCustomFilter; DWORD nMaxCustFilter, nFilterIndex;
    LPWSTR lpstrFile; DWORD nMaxFile; LPWSTR lpstrFileTitle; DWORD nMaxFileTitle;
    LPCWSTR lpstrInitialDir, lpstrTitle; DWORD Flags; WORD nFileOffset, nFileExtension;
    LPCWSTR lpstrDefExt; LPARAM lCustData; LPOFNHOOKPROC lpfnHook; LPCWSTR lpTemplateName;
    void *pvReserved; DWORD dwReserved, FlagsEx;
} OPENFILENAMEW, *LPOPENFILENAMEW;

typedef struct tagCHOOSECOLORA {
    DWORD lStructSize; HWND hwndOwner; HWND hInstance; COLORREF rgbResult; COLORREF *lpCustColors;
    DWORD Flags; LPARAM lCustData; LPCCHOOKPROC lpfnHook; LPCSTR lpTemplateName;
} CHOOSECOLORA, *LPCHOOSECOLORA;
typedef struct tagCHOOSECOLORW {
    DWORD lStructSize; HWND hwndOwner; HWND hInstance; COLORREF rgbResult; COLORREF *lpCustColors;
    DWORD Flags; LPARAM lCustData; LPCCHOOKPROC lpfnHook; LPCWSTR lpTemplateName;
} CHOOSECOLORW, *LPCHOOSECOLORW;

typedef struct tagCHOOSEFONTA {
    DWORD lStructSize; HWND hwndOwner; HDC hDC; LPLOGFONTA lpLogFont; INT iPointSize; DWORD Flags;
    COLORREF rgbColors; LPARAM lCustData; LPCFHOOKPROC lpfnHook; LPCSTR lpTemplateName; HINSTANCE hInstance;
    LPSTR lpszStyle; WORD nFontType, ___MISSING_ALIGNMENT__; INT nSizeMin, nSizeMax;
} CHOOSEFONTA, *LPCHOOSEFONTA;
typedef struct tagCHOOSEFONTW {
    DWORD lStructSize; HWND hwndOwner; HDC hDC; LPLOGFONTW lpLogFont; INT iPointSize; DWORD Flags;
    COLORREF rgbColors; LPARAM lCustData; LPCFHOOKPROC lpfnHook; LPCWSTR lpTemplateName; HINSTANCE hInstance;
    LPWSTR lpszStyle; WORD nFontType, ___MISSING_ALIGNMENT__; INT nSizeMin, nSizeMax;
} CHOOSEFONTW, *LPCHOOSEFONTW;

BOOL GetOpenFileNameA(LPOPENFILENAMEA ofn);
BOOL GetOpenFileNameW(LPOPENFILENAMEW ofn);
BOOL GetSaveFileNameA(LPOPENFILENAMEA ofn);
BOOL GetSaveFileNameW(LPOPENFILENAMEW ofn);
BOOL ChooseColorA(LPCHOOSECOLORA cc);
BOOL ChooseColorW(LPCHOOSECOLORW cc);
BOOL ChooseFontA(LPCHOOSEFONTA cf);
BOOL ChooseFontW(LPCHOOSEFONTW cf);
DWORD CommDlgExtendedError(void);

#define OPENFILENAME __AW(OPENFILENAME)
#define LPOPENFILENAME __AW(LPOPENFILENAME)
#define CHOOSECOLOR __AW(CHOOSECOLOR)
#define LPCHOOSECOLOR __AW(LPCHOOSECOLOR)
#define CHOOSEFONT __AW(CHOOSEFONT)
#define LPCHOOSEFONT __AW(LPCHOOSEFONT)
#define GetOpenFileName __AW(GetOpenFileName)
#define GetSaveFileName __AW(GetSaveFileName)
#define ChooseColor __AW(ChooseColor)
#define ChooseFont __AW(ChooseFont)
