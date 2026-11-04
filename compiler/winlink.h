// winlink.c: which DLL exports a name (read from the DLL files themselves).
#pragma once
#include <wchar.h>

void winlink_add(const char *lib);              // "user32" / "user32.dll": in the order of search
void winlink_add_dir(const wchar_t *dir);       // a folder to look for DLLs in before System32
const char *winlink_dll_of(const char *name, int len);
const char *winlink_missing(void);              // asked-for DLLs that are nowhere, or ""
