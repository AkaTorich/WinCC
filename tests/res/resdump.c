// resdump file.exe: every resource of a program, one line each -- type, name, language, size and
// a checksum of the bytes; with --hex also the bytes. tests\res\compare.ps1 compares two of them.
#define UNICODE
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int g_hex;

static void id_text(char *out, LPCWSTR id)
{
    if (IS_INTRESOURCE(id)) sprintf(out, "#%u", (unsigned)(ULONG_PTR)id);
    else WideCharToMultiByte(CP_UTF8, 0, id, -1, out, 200, 0, 0);
}

static BOOL on_lang(HMODULE mod, LPCWSTR type, LPCWSTR name, WORD lang, LONG_PTR p)
{
    (void)p;
    HRSRC r = FindResourceExW(mod, type, name, lang);
    DWORD size = SizeofResource(mod, r);
    const BYTE *d = LockResource(LoadResource(mod, r));
    unsigned sum = 2166136261u;
    for (DWORD i = 0; i < size; i++) sum = (sum ^ d[i]) * 16777619u;
    char t[256], n[256];
    id_text(t, type);
    id_text(n, name);
    printf("%s %s %04x %lu %08x\n", t, n, lang, (unsigned long)size, sum);
    if (g_hex) {
        for (DWORD i = 0; i < size; i++) printf("%02x%s", d[i], i % 32 == 31 || i + 1 == size ? "\n" : " ");
    }
    return TRUE;
}

static BOOL on_name(HMODULE mod, LPCWSTR type, LPWSTR name, LONG_PTR p)
{
    EnumResourceLanguagesW(mod, type, name, on_lang, p);
    return TRUE;
}

static BOOL on_type(HMODULE mod, LPWSTR type, LONG_PTR p)
{
    EnumResourceNamesW(mod, type, on_name, p);
    return TRUE;
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("resdump файл.exe [--hex]\n"); return 2; }
    g_hex = argc > 2 && !strcmp(argv[2], "--hex");
    wchar_t path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, path, MAX_PATH);
    HMODULE mod = LoadLibraryExW(path, 0, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!mod) { printf("не открыть %s\n", argv[1]); return 1; }
    EnumResourceTypesW(mod, on_type, 0);
    return 0;
}
