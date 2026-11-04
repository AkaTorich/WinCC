// Which DLL exports a name: WinCC reads the export tables of the DLLs straight
// from their files (System32, or the program's own DLLs next to it), so no .lib
// files are needed. The first DLL in the list that has the name wins.
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "cc.h"
#include "winlink.h"

typedef struct Lib {
    char name[MAX_PATH];        // as the import table names it: "user32.dll"
    wchar_t path[MAX_PATH * 2]; // where it was found, "" = not yet looked for
    int loaded, missing;
} Lib;

static Lib g_libs[128];
static int g_nlib;

typedef struct Entry { const char *name; const char *dll; } Entry;
static Entry *g_table;          // open addressing, names of all loaded DLLs
static size_t g_cap, g_count;
static wchar_t g_dirs[16][MAX_PATH * 2];    // where a DLL is looked for before System32
static int g_ndirs;

static unsigned hash_of(const char *s, int len)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < len; i++)
        h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}

static void table_put(const char *name, const char *dll)
{
    if ((g_count + 1) * 2 > g_cap) {
        size_t cap = g_cap ? g_cap * 2 : 8192;
        Entry *t = calloc(cap, sizeof(Entry));
        for (size_t i = 0; i < g_cap; i++)
            if (g_table[i].name) {
                size_t k = hash_of(g_table[i].name, (int)strlen(g_table[i].name)) & (cap - 1);
                while (t[k].name) k = (k + 1) & (cap - 1);
                t[k] = g_table[i];
            }
        free(g_table);
        g_table = t;
        g_cap = cap;
    }
    size_t k = hash_of(name, (int)strlen(name)) & (g_cap - 1);
    while (g_table[k].name) {
        if (!strcmp(g_table[k].name, name))
            return;                         // an earlier DLL has it already
        k = (k + 1) & (g_cap - 1);
    }
    g_table[k].name = name;
    g_table[k].dll = dll;
    g_count++;
}

void winlink_add_dir(const wchar_t *dir)
{
    if (g_ndirs < 16 && dir && *dir)
        wcsncpy(g_dirs[g_ndirs++], dir, MAX_PATH * 2 - 1);
}

// "user32", "user32.dll", "user32.lib" -> "user32.dll"
void winlink_add(const char *lib)
{
    char n[MAX_PATH];
    strncpy(n, lib, MAX_PATH - 5);
    n[MAX_PATH - 5] = 0;
    size_t len = strlen(n);
    if (len > 4 && !_stricmp(n + len - 4, ".lib"))
        n[len -= 4] = 0;
    if (!(len > 4 && !_stricmp(n + len - 4, ".dll")))
        strcat(n, ".dll");
    for (int i = 0; i < g_nlib; i++)
        if (!_stricmp(g_libs[i].name, n))
            return;
    if (g_nlib < 128) {
        memset(&g_libs[g_nlib], 0, sizeof(Lib));
        strcpy(g_libs[g_nlib++].name, n);
    }
}

static int find_file(Lib *l)
{
    wchar_t wname[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, l->name, -1, wname, MAX_PATH);
    for (int i = 0; i < g_ndirs; i++) {
        swprintf(l->path, MAX_PATH * 2, L"%ls\\%ls", g_dirs[i], wname);
        if (GetFileAttributesW(l->path) != INVALID_FILE_ATTRIBUTES)
            return 1;
    }
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(l->path, MAX_PATH * 2, L"%ls\\%ls", sys, wname);
    return GetFileAttributesW(l->path) != INVALID_FILE_ATTRIBUTES;
}

// the file offset of an RVA, by the section table
static DWORD rva_to_off(const IMAGE_SECTION_HEADER *sec, int nsec, DWORD rva)
{
    for (int i = 0; i < nsec; i++)
        if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + max(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize))
            return rva - sec[i].VirtualAddress + sec[i].PointerToRawData;
    return 0;
}

static void load_exports(Lib *l)
{
    l->loaded = 1;
    if (!find_file(l)) {
        l->missing = 1;
        return;
    }
    HANDLE h = CreateFileW(l->path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) { l->missing = 1; return; }
    DWORD size = GetFileSize(h, 0), got = 0;
    unsigned char *d = malloc(size);
    if (!d || !ReadFile(h, d, size, &got, 0) || got != size) { CloseHandle(h); free(d); l->missing = 1; return; }
    CloseHandle(h);
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)d;
    if (size < 0x200 || dos->e_magic != IMAGE_DOS_SIGNATURE || (DWORD)dos->e_lfanew + 0x200 > size) { free(d); return; }
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(d + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { free(d); return; }
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    int nsec = nt->FileHeader.NumberOfSections;
    const IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir->VirtualAddress) { free(d); return; }
    DWORD eo = rva_to_off(sec, nsec, dir->VirtualAddress);
    if (!eo || eo + sizeof(IMAGE_EXPORT_DIRECTORY) > size) { free(d); return; }
    const IMAGE_EXPORT_DIRECTORY *ed = (const IMAGE_EXPORT_DIRECTORY *)(d + eo);
    DWORD no = rva_to_off(sec, nsec, ed->AddressOfNames);
    for (DWORD i = 0; no && i < ed->NumberOfNames && no + i * 4 + 4 <= size; i++) {
        DWORD so = rva_to_off(sec, nsec, *(const DWORD *)(d + no + i * 4));
        if (!so || so >= size) continue;
        const char *nm = (const char *)(d + so);
        size_t n = strnlen(nm, size - so);
        char *copy = malloc(n + 1);
        memcpy(copy, nm, n);
        copy[n] = 0;
        table_put(copy, l->name);
    }
    free(d);
}

const char *winlink_dll_of(const char *name, int len)
{
    for (int pass = 0; pass < 2; pass++) {
        if (g_cap) {
            size_t k = hash_of(name, len) & (g_cap - 1);
            while (g_table[k].name) {
                if ((int)strlen(g_table[k].name) == len && !memcmp(g_table[k].name, name, (size_t)len))
                    return g_table[k].dll;
                k = (k + 1) & (g_cap - 1);
            }
        }
        if (pass)
            break;
        int any = 0;                    // not seen yet: read the DLLs not read so far
        for (int i = 0; i < g_nlib; i++)
            if (!g_libs[i].loaded) { load_exports(&g_libs[i]); any = 1; }
        if (!any)
            break;
    }
    return 0;
}

// The DLLs asked for (-l, #pragma comment) that are not to be found: their names, or "".
const char *winlink_missing(void)
{
    static char out[1024];
    out[0] = 0;
    for (int i = 0; i < g_nlib; i++) {
        if (!g_libs[i].loaded) load_exports(&g_libs[i]);
        if (g_libs[i].missing && strlen(out) + strlen(g_libs[i].name) + 3 < sizeof(out)) {
            if (out[0]) strcat(out, ", ");
            strcat(out, g_libs[i].name);
        }
    }
    return out;
}
