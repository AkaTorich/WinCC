// Minimal PE64 (.dll) structures for DyOS's own DLL writer and loader.
// Custom names so this never clashes with <windows.h> in the test host.
#pragma once

#define PE_DOS_MAGIC       0x5A4D
#define PE_NT_MAGIC        0x00004550
#define PE_MACHINE_X64     0x8664
#define PE_MAGIC_PE32PLUS  0x20B

#define PE_F_EXECUTABLE    0x0002
#define PE_F_LARGE_ADDR    0x0020
#define PE_F_DLL           0x2000

#define PE_SCN_CODE        0x00000020
#define PE_SCN_INIT_DATA   0x00000040
#define PE_SCN_DISCARDABLE 0x02000000
#define PE_SCN_EXECUTE     0x20000000
#define PE_SCN_READ        0x40000000
#define PE_SCN_WRITE       0x80000000

#define PE_DIR_EXPORT      0
#define PE_DIR_IMPORT      1
#define PE_DIR_RESOURCE    2
#define PE_DIR_BASERELOC   5
#define PE_DIR_IAT         12

#define PE_REL_ABSOLUTE    0
#define PE_REL_DIR64       10

#define PE_IMAGE_BASE      0x10000000ull
#define PE_SECT_ALIGN      0x1000

#pragma pack(push, 1)

typedef struct {
    unsigned short e_magic;
    unsigned char  e_pad[58];       // e_lfanew sits at offset 60
    unsigned int   e_lfanew;
} PeDos;

typedef struct {
    unsigned short Machine;
    unsigned short NumberOfSections;
    unsigned int   TimeDateStamp;
    unsigned int   PointerToSymbolTable;
    unsigned int   NumberOfSymbols;
    unsigned short SizeOfOptionalHeader;
    unsigned short Characteristics;
} PeFileHeader;

typedef struct {
    unsigned int VirtualAddress;
    unsigned int Size;
} PeDataDir;

typedef struct {
    unsigned short Magic;
    unsigned char  MajorLinkerVersion, MinorLinkerVersion;
    unsigned int   SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    unsigned int   AddressOfEntryPoint, BaseOfCode;
    unsigned long long ImageBase;
    unsigned int   SectionAlignment, FileAlignment;
    unsigned short MajorOSVersion, MinorOSVersion, MajorImageVersion, MinorImageVersion;
    unsigned short MajorSubsystemVersion, MinorSubsystemVersion;
    unsigned int   Win32VersionValue, SizeOfImage, SizeOfHeaders, CheckSum;
    unsigned short Subsystem, DllCharacteristics;
    unsigned long long SizeOfStackReserve, SizeOfStackCommit, SizeOfHeapReserve, SizeOfHeapCommit;
    unsigned int   LoaderFlags, NumberOfRvaAndSizes;
    PeDataDir      DataDirectory[16];
} PeOpt64;

typedef struct {
    unsigned char  Name[8];
    unsigned int   VirtualSize;
    unsigned int   VirtualAddress;
    unsigned int   SizeOfRawData;
    unsigned int   PointerToRawData;
    unsigned int   PointerToRelocations;
    unsigned int   PointerToLinenumbers;
    unsigned short NumberOfRelocations;
    unsigned short NumberOfLinenumbers;
    unsigned int   Characteristics;
} PeSection;

typedef struct {
    unsigned int   Characteristics;
    unsigned int   TimeDateStamp;
    unsigned short MajorVersion, MinorVersion;
    unsigned int   Name;
    unsigned int   Base;
    unsigned int   NumberOfFunctions;
    unsigned int   NumberOfNames;
    unsigned int   AddressOfFunctions;
    unsigned int   AddressOfNames;
    unsigned int   AddressOfNameOrdinals;
} PeExportDir;

typedef struct {
    unsigned int OriginalFirstThunk;    // ILT rva
    unsigned int TimeDateStamp;
    unsigned int ForwarderChain;
    unsigned int Name;                  // module name rva
    unsigned int FirstThunk;            // IAT rva
} PeImportDesc;

typedef struct {
    unsigned int PageRVA;
    unsigned int BlockSize;
} PeRelocBlock;

// ---- .dydbg: DyOS's own debug info (src\cc\asm.c writes it, src\debug.c reads it) ----
// A section of its own, the last one of the image; offsets are from the section's
// start, addresses are RVAs. Strings are UTF-8 with a NUL, by offset into the
// string area. Functions and lines go by address, ascending.
#define DYDBG_MAGIC        0x42445944       // "DYDB"
#define DYDBG_VERSION      1
#define DYDBG_FUNC_FRAME   1                // push rbp; mov rbp, rsp first (every C function)
#define DYDBG_FUNC_LINES   2                // has lines of the program's files

typedef struct {
    unsigned int magic, version;
    unsigned int nfiles, nfuncs, nlines;
    unsigned int files_off;                 // nfiles x u32: the name (as the compiler was given it)
    unsigned int funcs_off;                 // nfuncs x DyDbgFunc
    unsigned int lines_off;                 // nlines x DyDbgLine
    unsigned int strings_off, strings_size;
    unsigned int text_rva, text_size;       // the code section
} DyDbgHeader;

typedef struct {
    unsigned int start, end;                // [start, end): the function's code
    unsigned int name;                      // string offset
    unsigned int flags;                     // DYDBG_FUNC_*
} DyDbgFunc;

typedef struct {
    unsigned int rva;                       // the first byte of a statement's code (or of a loop's
    unsigned int line;                      // condition, a function's "{" and "}"): from here on
    unsigned int file;                      // the code is this line of this file
} DyDbgLine;

#pragma pack(pop)
