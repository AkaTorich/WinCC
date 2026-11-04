// tchar.h of WinCC: the same text in UTF-8 (char) or, with _UNICODE, in UTF-16 (wchar_t).
#pragma once
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif
#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#endif

#ifdef _UNICODE
#ifndef _TCHAR_DEFINED
#define _TCHAR_DEFINED
typedef wchar_t TCHAR;
#endif
typedef wchar_t _TCHAR;
#define __T(x) L##x
#define _tmain wmain
#define _tWinMain wWinMain
#define _tprintf wprintf
#define _ftprintf fwprintf
#define _stprintf swprintf
#define _tscanf wscanf
#define _tcslen wcslen
#define _tcscpy wcscpy
#define _tcsncpy wcsncpy
#define _tcscat wcscat
#define _tcscmp wcscmp
#define _tcsncmp wcsncmp
#define _tcschr wcschr
#define _tcsrchr wcsrchr
#define _tcsstr wcsstr
#define _tcstol wcstol
#define _tcstoul wcstoul
#define _tcstod wcstod
#define _fgetts fgetws
#define _fputts fputws
#define _puttchar putwchar
#define _gettchar getwchar
#else
#ifndef _TCHAR_DEFINED
#define _TCHAR_DEFINED
typedef char TCHAR;
#endif
typedef char _TCHAR;
#define __T(x) x
#define _tmain main
#define _tWinMain WinMain
#define _tprintf printf
#define _ftprintf fprintf
#define _stprintf sprintf
#define _tscanf scanf
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _tcstol strtol
#define _tcstoul strtoul
#define _tcstod strtod
#define _fgetts fgets
#define _fputts fputs
#define _puttchar putchar
#define _gettchar getchar
#endif
#define _T(x) __T(x)
#define _TEXT(x) __T(x)
