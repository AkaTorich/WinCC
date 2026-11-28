// windows.h: the types, structures and macros all parts share.
#pragma once

// one calling convention in x64: these say nothing
#define WINAPI
#define WINAPIV
#define APIENTRY
#define CALLBACK
#define PASCAL
#define WINBASEAPI
#define WINUSERAPI
#define WINGDIAPI
#define WINADVAPI
#define SHSTDAPI_(t) t
#define DECLSPEC_IMPORT
#define DECLSPEC_NORETURN
#define FAR
#define NEAR
#define CONST const
#define VOID void
#define IN
#define OUT
#define OPTIONAL
#define UNREFERENCED_PARAMETER(p) ((void)(p))

#ifndef NULL
#define NULL ((void *)0)
#endif
#define TRUE 1
#define FALSE 0

typedef unsigned char BYTE, UCHAR, BOOLEAN, *PBYTE, *LPBYTE, *PUCHAR;
typedef unsigned short WORD, USHORT, *PWORD, *LPWORD, *PUSHORT;
typedef unsigned long DWORD, ULONG, *PDWORD, *LPDWORD, *PULONG;
typedef int BOOL, INT, *PBOOL, *LPBOOL, *PINT, *LPINT;
typedef unsigned int UINT, *PUINT, *LPUINT;
typedef long LONG, *PLONG, *LPLONG;
typedef short SHORT, *PSHORT;
typedef char CHAR, *PCHAR, *LPCH, *PCH, *LPSTR, *PSTR, *NPSTR;
typedef const char *LPCSTR, *PCSTR, *LPCCH;
typedef wchar_t WCHAR, *PWCHAR, *LPWCH, *PWCH, *LPWSTR, *PWSTR, *NWPSTR;
typedef const wchar_t *LPCWSTR, *PCWSTR, *LPCWCH;
typedef float FLOAT;
typedef double DOUBLE;
typedef long long LONGLONG, LONG64, INT64, *PLONGLONG, *PLONG64;
typedef unsigned long long ULONGLONG, DWORDLONG, DWORD64, ULONG64, UINT64, *PULONGLONG, *PDWORD64;
typedef signed char INT8;
typedef unsigned char UINT8;
typedef short INT16;
typedef unsigned short UINT16;
typedef int INT32, LONG32;
typedef unsigned int UINT32, ULONG32, DWORD32;
typedef long long INT_PTR, LONG_PTR, SSIZE_T, *PINT_PTR, *PLONG_PTR;
typedef unsigned long long UINT_PTR, ULONG_PTR, DWORD_PTR, SIZE_T, *PUINT_PTR, *PULONG_PTR, *PDWORD_PTR, *PSIZE_T;
typedef void *PVOID, *LPVOID;
typedef const void *LPCVOID, *PCVOID;

typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef WORD ATOM;
typedef DWORD COLORREF, *LPCOLORREF;
typedef LONG HRESULT;
typedef DWORD LCID;
typedef WORD LANGID;
typedef LONG LSTATUS;
typedef DWORD ACCESS_MASK, REGSAM;

typedef void *HANDLE, **PHANDLE, **LPHANDLE;
typedef HANDLE HWND, HINSTANCE, HMODULE, HDC, HGDIOBJ, HBRUSH, HPEN, HFONT, HBITMAP, HICON, HCURSOR,
               HMENU, HRGN, HPALETTE, HKEY, HGLOBAL, HLOCAL, HACCEL, HMONITOR, HRSRC, HDROP, HHOOK,
               HTHEME, HKL, HDWP, HGLRC, HENHMETAFILE, HMETAFILE;
typedef HKEY *PHKEY;
typedef int HFILE;

#ifdef UNICODE
typedef WCHAR TCHAR, *PTCHAR, *LPTSTR, *PTSTR;
typedef const WCHAR *LPCTSTR, *PCTSTR;
typedef WCHAR TBYTE;
#define __TEXT(q) L##q
#define __AW(name) name##W
#else
typedef char TCHAR, *PTCHAR, *LPTSTR, *PTSTR;
typedef const char *LPCTSTR, *PCTSTR;
typedef unsigned char TBYTE;
#define __TEXT(q) q
#define __AW(name) name##A
#endif
#define TEXT(q) __TEXT(q)

// ---- numbers inside numbers ----
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define MAKELRESULT(l, h) ((LRESULT)(DWORD)MAKELONG(l, h))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) (LOBYTE((rgb) >> 16))
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005L)
#ifndef NOMINMAX
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#endif

#define ZeroMemory(d, n) memset((d), 0, (n))
#define SecureZeroMemory(d, n) memset((d), 0, (n))
#define FillMemory(d, n, v) memset((d), (v), (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))

#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE __AW(MAKEINTRESOURCE)
#define MAKEINTATOM(i) ((LPTSTR)((ULONG_PTR)((WORD)(i))))
#define IS_INTRESOURCE(r) ((((ULONG_PTR)(r)) >> 16) == 0)

// ---- shared structures ----
typedef struct tagPOINT { LONG x, y; } POINT, *PPOINT, *LPPOINT, POINTL, *PPOINTL;
typedef struct tagPOINTS { SHORT x, y; } POINTS, *PPOINTS;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *PSIZE, *LPSIZE, SIZEL;
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT, RECTL;
typedef const RECT *LPCRECT;

typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *PFILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;
typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    struct { DWORD LowPart; LONG HighPart; } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; };
    struct { DWORD LowPart; DWORD HighPart; } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;
typedef struct _GUID { unsigned long Data1; unsigned short Data2, Data3; unsigned char Data4[8]; } GUID, IID, CLSID;
typedef char CCHAR;

// a doubly linked list as Windows keeps them (the loader's modules, ntdll.h)
typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink, *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal, InternalHigh;
    union { struct { DWORD Offset, OffsetHigh; }; PVOID Pointer; };
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

// ---- error codes (GetLastError) ----
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define ERROR_INVALID_FUNCTION 1L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_TOO_MANY_OPEN_FILES 4L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_OUTOFMEMORY 14L
#define ERROR_NO_MORE_FILES 18L
#define ERROR_SHARING_VIOLATION 32L
#define ERROR_HANDLE_EOF 38L
#define ERROR_FILE_EXISTS 80L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_BROKEN_PIPE 109L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_INVALID_NAME 123L
#define ERROR_MOD_NOT_FOUND 126L
#define ERROR_PROC_NOT_FOUND 127L
#define ERROR_DIR_NOT_EMPTY 145L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_ENVVAR_NOT_FOUND 203L
#define ERROR_MORE_DATA 234L
#define ERROR_NO_MORE_ITEMS 259L
#define ERROR_OPERATION_ABORTED 995L
#define ERROR_IO_INCOMPLETE 996L
#define ERROR_IO_PENDING 997L
#define ERROR_TIMEOUT 1460L
