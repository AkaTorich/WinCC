// windows.h of WinCC: the types, constants and functions of Windows that C
// programs use most -- windows, messages, drawing, files, processes, threads,
// the console, dialogs to open/save files, the registry.
//
// Every WinCC program has the manifest that makes UTF-8 its code page: the
// ...A functions take UTF-8, so the text of the program (UTF-8 too) goes to
// MessageBoxA, CreateWindowExA... as it is, Cyrillic included. With UNICODE
// defined before this header the plain names are the ...W functions (UTF-16:
// L"..." and TEXT("...")).
#pragma once
#ifndef _WINDOWS_
#define _WINDOWS_
#define _WINDOWS_H
#define _WINDEF_
#define _WINBASE_
#define _WINUSER_
#define _WINGDI_

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

#include <sal.h>
#include "win/types.h"
#include "win/kernel32.h"
#include "win/user32.h"
#include "win/gdi32.h"
#include "win/comdlg32.h"
#include "win/shell32.h"
#include "win/advapi32.h"
#include "win/resource.h"
#include "win/except.h"

#endif
