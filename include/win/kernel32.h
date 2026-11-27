// windows.h: kernel32 -- files, memory, processes, threads, time, the console, DLLs.
#pragma once

#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0x00000000L
#define WAIT_ABANDONED 0x00000080L
#define WAIT_TIMEOUT 0x00000102L
#define WAIT_FAILED ((DWORD)0xFFFFFFFF)
#define WAIT_IO_COMPLETION 0x000000C0L
#define MAXIMUM_WAIT_OBJECTS 64
#define STILL_ACTIVE 259

#define GENERIC_READ 0x80000000L
#define GENERIC_WRITE 0x40000000L
#define GENERIC_EXECUTE 0x20000000L
#define GENERIC_ALL 0x10000000L
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_SYSTEM 0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#define FILE_FLAG_WRITE_THROUGH 0x80000000
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE 0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED 0x00000002
#define DRIVE_UNKNOWN 0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE 2
#define DRIVE_FIXED 3
#define DRIVE_REMOTE 4
#define DRIVE_CDROM 5
#define DRIVE_RAMDISK 6

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define HEAP_ZERO_MEMORY 0x00000008
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040
#define GHND (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR (GMEM_FIXED | GMEM_ZEROINIT)
#define LMEM_FIXED 0x0000
#define LMEM_MOVEABLE 0x0002
#define LMEM_ZEROINIT 0x0040
#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)

#define CP_ACP 0
#define CP_OEMCP 1
#define CP_UTF7 65000
#define CP_UTF8 65001
#define MB_PRECOMPOSED 0x00000001
#define MB_ERR_INVALID_CHARS 0x00000008
#define WC_NO_BEST_FIT_CHARS 0x00000400

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080
#define ENABLE_PROCESSED_INPUT 0x0001
#define ENABLE_LINE_INPUT 0x0002
#define ENABLE_ECHO_INPUT 0x0004
#define ENABLE_WINDOW_INPUT 0x0008
#define ENABLE_MOUSE_INPUT 0x0010
#define ENABLE_QUICK_EDIT_MODE 0x0040
#define ENABLE_EXTENDED_FLAGS 0x0080
#define ENABLE_PROCESSED_OUTPUT 0x0001
#define ENABLE_WRAP_AT_EOL_OUTPUT 0x0002
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define KEY_EVENT 0x0001
#define MOUSE_EVENT 0x0002
#define WINDOW_BUFFER_SIZE_EVENT 0x0004
#define CTRL_C_EVENT 0
#define CTRL_BREAK_EVENT 1
#define CTRL_CLOSE_EVENT 2

#define CREATE_SUSPENDED 0x00000004
#define DETACHED_PROCESS 0x00000008
#define CREATE_NEW_CONSOLE 0x00000010
#define CREATE_UNICODE_ENVIRONMENT 0x00000400
#define CREATE_NO_WINDOW 0x08000000
#define NORMAL_PRIORITY_CLASS 0x00000020
#define STARTF_USESHOWWINDOW 0x00000001
#define STARTF_USESTDHANDLES 0x00000100
#define PROCESS_TERMINATE 0x0001
#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define SYNCHRONIZE 0x00100000L
#define PROCESS_ALL_ACCESS 0x001FFFFF
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_LOWEST (-2)

#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#define FORMAT_MESSAGE_FROM_STRING 0x00000400
#define FORMAT_MESSAGE_FROM_HMODULE 0x00000800
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
#define FORMAT_MESSAGE_ARGUMENT_ARRAY 0x00002000
#define FORMAT_MESSAGE_MAX_WIDTH_MASK 0x000000FF
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))

#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1
#define DLL_THREAD_ATTACH 2
#define DLL_THREAD_DETACH 3

typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID param);
typedef BOOL (*PHANDLER_ROUTINE)(DWORD ctrl_type);
typedef INT_PTR (*FARPROC)(void);

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess, hThread;
    DWORD dwProcessId, dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _STARTUPINFOA {
    DWORD cb; LPSTR lpReserved, lpDesktop, lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow, cbReserved2; LPBYTE lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;
typedef struct _STARTUPINFOW {
    DWORD cb; LPWSTR lpReserved, lpDesktop, lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow, cbReserved2; LPBYTE lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    WCHAR cFileName[MAX_PATH];
    WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

typedef struct _CRITICAL_SECTION {
    PVOID DebugInfo;
    LONG LockCount, RecursionCount;
    HANDLE OwningThread, LockSemaphore;
    ULONG_PTR SpinCount;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;
typedef struct _RTL_SRWLOCK { PVOID Ptr; } SRWLOCK, *PSRWLOCK;
typedef struct _RTL_CONDITION_VARIABLE { PVOID Ptr; } CONDITION_VARIABLE, *PCONDITION_VARIABLE;
#define SRWLOCK_INIT { 0 }
#define CONDITION_VARIABLE_INIT { 0 }

typedef struct _MEMORYSTATUSEX {
    DWORD dwLength, dwMemoryLoad;
    DWORDLONG ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile, ullTotalVirtual, ullAvailVirtual,
              ullAvailExtendedVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;

typedef struct _SYSTEM_INFO {
    union { DWORD dwOemId; struct { WORD wProcessorArchitecture, wReserved; }; };
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress, lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors, dwProcessorType, dwAllocationGranularity;
    WORD wProcessorLevel, wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef struct _COORD { SHORT X, Y; } COORD, *PCOORD;
typedef struct _SMALL_RECT { SHORT Left, Top, Right, Bottom; } SMALL_RECT, *PSMALL_RECT;
typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
    COORD dwSize, dwCursorPosition;
    WORD wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;
typedef struct _CONSOLE_CURSOR_INFO { DWORD dwSize; BOOL bVisible; } CONSOLE_CURSOR_INFO, *PCONSOLE_CURSOR_INFO;
typedef struct _CHAR_INFO { union { WCHAR UnicodeChar; CHAR AsciiChar; } Char; WORD Attributes; } CHAR_INFO, *PCHAR_INFO;
typedef struct _KEY_EVENT_RECORD {
    BOOL bKeyDown;
    WORD wRepeatCount, wVirtualKeyCode, wVirtualScanCode;
    union { WCHAR UnicodeChar; CHAR AsciiChar; } uChar;
    DWORD dwControlKeyState;
} KEY_EVENT_RECORD;
typedef struct _MOUSE_EVENT_RECORD {
    COORD dwMousePosition;
    DWORD dwButtonState, dwControlKeyState, dwEventFlags;
} MOUSE_EVENT_RECORD;
typedef struct _INPUT_RECORD {
    WORD EventType;
    union { KEY_EVENT_RECORD KeyEvent; MOUSE_EVENT_RECORD MouseEvent; COORD WindowBufferSizeEvent; } Event;
} INPUT_RECORD, *PINPUT_RECORD;

// ---- functions ----
DWORD GetLastError(void);
void SetLastError(DWORD code);
void ExitProcess(UINT code);
BOOL TerminateProcess(HANDLE process, UINT code);
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
DWORD GetCurrentProcessId(void);
DWORD GetCurrentThreadId(void);
LPSTR GetCommandLineA(void);
LPWSTR GetCommandLineW(void);
HMODULE GetModuleHandleA(LPCSTR name);
HMODULE GetModuleHandleW(LPCWSTR name);
DWORD GetModuleFileNameA(HMODULE m, LPSTR buf, DWORD size);
DWORD GetModuleFileNameW(HMODULE m, LPWSTR buf, DWORD size);
HMODULE LoadLibraryA(LPCSTR name);
HMODULE LoadLibraryW(LPCWSTR name);
BOOL FreeLibrary(HMODULE m);
FARPROC GetProcAddress(HMODULE m, LPCSTR name);
void Sleep(DWORD ms);
DWORD SleepEx(DWORD ms, BOOL alertable);
DWORD GetTickCount(void);
ULONGLONG GetTickCount64(void);
BOOL QueryPerformanceCounter(LARGE_INTEGER *count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq);
void GetLocalTime(LPSYSTEMTIME t);
void GetSystemTime(LPSYSTEMTIME t);
BOOL SetLocalTime(const SYSTEMTIME *t);
void GetSystemTimeAsFileTime(LPFILETIME t);
BOOL FileTimeToSystemTime(const FILETIME *f, LPSYSTEMTIME t);
BOOL SystemTimeToFileTime(const SYSTEMTIME *t, LPFILETIME f);
BOOL FileTimeToLocalFileTime(const FILETIME *f, LPFILETIME local);
void GetSystemInfo(LPSYSTEM_INFO info);
BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX m);
BOOL Beep(DWORD freq, DWORD ms);

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE tmpl);
HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE tmpl);
BOOL ReadFile(HANDLE h, LPVOID buf, DWORD size, LPDWORD read, LPOVERLAPPED ov);
BOOL WriteFile(HANDLE h, LPCVOID data, DWORD size, LPDWORD written, LPOVERLAPPED ov);
BOOL CloseHandle(HANDLE h);
BOOL DuplicateHandle(HANDLE sp, HANDLE h, HANDLE tp, LPHANDLE out, DWORD access, BOOL inherit, DWORD options);
DWORD GetFileSize(HANDLE h, LPDWORD high);
BOOL GetFileSizeEx(HANDLE h, PLARGE_INTEGER size);
DWORD SetFilePointer(HANDLE h, LONG dist, PLONG high, DWORD method);
BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER dist, PLARGE_INTEGER pos, DWORD method);
BOOL SetEndOfFile(HANDLE h);
BOOL FlushFileBuffers(HANDLE h);
BOOL GetFileTime(HANDLE h, LPFILETIME created, LPFILETIME accessed, LPFILETIME written);
BOOL SetFileTime(HANDLE h, const FILETIME *created, const FILETIME *accessed, const FILETIME *written);
BOOL DeleteFileA(LPCSTR name);
BOOL DeleteFileW(LPCWSTR name);
BOOL CopyFileA(LPCSTR from, LPCSTR to, BOOL fail_if_exists);
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL fail_if_exists);
BOOL MoveFileA(LPCSTR from, LPCSTR to);
BOOL MoveFileW(LPCWSTR from, LPCWSTR to);
BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags);
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags);
BOOL CreateDirectoryA(LPCSTR name, LPSECURITY_ATTRIBUTES sa);
BOOL CreateDirectoryW(LPCWSTR name, LPSECURITY_ATTRIBUTES sa);
BOOL RemoveDirectoryA(LPCSTR name);
BOOL RemoveDirectoryW(LPCWSTR name);
DWORD GetFileAttributesA(LPCSTR name);
DWORD GetFileAttributesW(LPCWSTR name);
BOOL SetFileAttributesA(LPCSTR name, DWORD attrs);
BOOL SetFileAttributesW(LPCWSTR name, DWORD attrs);
HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data);
HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data);
BOOL FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA data);
BOOL FindNextFileW(HANDLE h, LPWIN32_FIND_DATAW data);
BOOL FindClose(HANDLE h);
DWORD GetCurrentDirectoryA(DWORD size, LPSTR buf);
DWORD GetCurrentDirectoryW(DWORD size, LPWSTR buf);
BOOL SetCurrentDirectoryA(LPCSTR dir);
BOOL SetCurrentDirectoryW(LPCWSTR dir);
UINT GetSystemDirectoryA(LPSTR buf, UINT size);
UINT GetSystemDirectoryW(LPWSTR buf, UINT size);
UINT GetWindowsDirectoryA(LPSTR buf, UINT size);
UINT GetWindowsDirectoryW(LPWSTR buf, UINT size);
DWORD GetTempPathA(DWORD size, LPSTR buf);
DWORD GetTempPathW(DWORD size, LPWSTR buf);
UINT GetTempFileNameA(LPCSTR dir, LPCSTR prefix, UINT unique, LPSTR name);
UINT GetTempFileNameW(LPCWSTR dir, LPCWSTR prefix, UINT unique, LPWSTR name);
DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buf, LPSTR *file_part);
DWORD GetFullPathNameW(LPCWSTR name, DWORD size, LPWSTR buf, LPWSTR *file_part);
DWORD GetLogicalDrives(void);
DWORD GetLogicalDriveStringsA(DWORD size, LPSTR buf);
DWORD GetLogicalDriveStringsW(DWORD size, LPWSTR buf);
UINT GetDriveTypeA(LPCSTR root);
UINT GetDriveTypeW(LPCWSTR root);
BOOL GetDiskFreeSpaceExA(LPCSTR dir, PULARGE_INTEGER avail, PULARGE_INTEGER total, PULARGE_INTEGER free);
BOOL GetDiskFreeSpaceExW(LPCWSTR dir, PULARGE_INTEGER avail, PULARGE_INTEGER total, PULARGE_INTEGER free);
BOOL CreatePipe(PHANDLE read, PHANDLE write, LPSECURITY_ATTRIBUTES sa, DWORD size);
BOOL PeekNamedPipe(HANDLE h, LPVOID buf, DWORD size, LPDWORD read, LPDWORD avail, LPDWORD left);

LPVOID VirtualAlloc(LPVOID at, SIZE_T size, DWORD type, DWORD protect);
BOOL VirtualFree(LPVOID at, SIZE_T size, DWORD type);
BOOL VirtualProtect(LPVOID at, SIZE_T size, DWORD protect, PDWORD old);
HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE heap, DWORD flags, SIZE_T size);
LPVOID HeapReAlloc(HANDLE heap, DWORD flags, LPVOID p, SIZE_T size);
BOOL HeapFree(HANDLE heap, DWORD flags, LPVOID p);
SIZE_T HeapSize(HANDLE heap, DWORD flags, LPCVOID p);
HGLOBAL GlobalAlloc(UINT flags, SIZE_T size);
HGLOBAL GlobalFree(HGLOBAL h);
LPVOID GlobalLock(HGLOBAL h);
BOOL GlobalUnlock(HGLOBAL h);
SIZE_T GlobalSize(HGLOBAL h);
HLOCAL LocalAlloc(UINT flags, SIZE_T size);
HLOCAL LocalFree(HLOCAL h);

UINT GetACP(void);
UINT GetOEMCP(void);
int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR s, int n, LPWSTR out, int cap);
int WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR s, int n, LPSTR out, int cap, LPCSTR def, LPBOOL used);
int lstrlenA(LPCSTR s);
int lstrlenW(LPCWSTR s);
LPSTR lstrcpyA(LPSTR d, LPCSTR s);
LPWSTR lstrcpyW(LPWSTR d, LPCWSTR s);
LPSTR lstrcpynA(LPSTR d, LPCSTR s, int n);
LPWSTR lstrcpynW(LPWSTR d, LPCWSTR s, int n);
LPSTR lstrcatA(LPSTR d, LPCSTR s);
LPWSTR lstrcatW(LPWSTR d, LPCWSTR s);
int lstrcmpA(LPCSTR a, LPCSTR b);
int lstrcmpW(LPCWSTR a, LPCWSTR b);
int lstrcmpiA(LPCSTR a, LPCSTR b);
int lstrcmpiW(LPCWSTR a, LPCWSTR b);
void OutputDebugStringA(LPCSTR s);
void OutputDebugStringW(LPCWSTR s);
DWORD FormatMessageA(DWORD flags, LPCVOID src, DWORD id, DWORD lang, LPSTR buf, DWORD size, va_list *args);
DWORD FormatMessageW(DWORD flags, LPCVOID src, DWORD id, DWORD lang, LPWSTR buf, DWORD size, va_list *args);
BOOL GetComputerNameA(LPSTR buf, LPDWORD size);
BOOL GetComputerNameW(LPWSTR buf, LPDWORD size);
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buf, DWORD size);
DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buf, DWORD size);
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value);
BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value);
DWORD ExpandEnvironmentStringsA(LPCSTR src, LPSTR dst, DWORD size);
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD size);

BOOL CreateProcessA(LPCSTR app, LPSTR cmd, LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta, BOOL inherit,
                    DWORD flags, LPVOID env, LPCSTR dir, LPSTARTUPINFOA si, LPPROCESS_INFORMATION pi);
BOOL CreateProcessW(LPCWSTR app, LPWSTR cmd, LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta, BOOL inherit,
                    DWORD flags, LPVOID env, LPCWSTR dir, LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi);
HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD pid);
BOOL GetExitCodeProcess(HANDLE h, LPDWORD code);
UINT WinExec(LPCSTR cmd, UINT show);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID param, DWORD flags, LPDWORD id);
void ExitThread(DWORD code);
BOOL GetExitCodeThread(HANDLE h, LPDWORD code);
DWORD ResumeThread(HANDLE h);
DWORD SuspendThread(HANDLE h);
BOOL SetThreadPriority(HANDLE h, int priority);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
DWORD WaitForMultipleObjects(DWORD n, const HANDLE *hs, BOOL all, DWORD ms);
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCSTR name);
HANDLE CreateEventW(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCWSTR name);
BOOL SetEvent(HANDLE h);
BOOL ResetEvent(HANDLE h);
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES sa, BOOL owner, LPCSTR name);
HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES sa, BOOL owner, LPCWSTR name);
BOOL ReleaseMutex(HANDLE h);
HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES sa, LONG initial, LONG max_count, LPCSTR name);
HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES sa, LONG initial, LONG max_count, LPCWSTR name);
BOOL ReleaseSemaphore(HANDLE h, LONG count, LPLONG previous);
void InitializeCriticalSection(LPCRITICAL_SECTION cs);
void EnterCriticalSection(LPCRITICAL_SECTION cs);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs);
void LeaveCriticalSection(LPCRITICAL_SECTION cs);
void DeleteCriticalSection(LPCRITICAL_SECTION cs);
void InitializeSRWLock(PSRWLOCK l);
void AcquireSRWLockExclusive(PSRWLOCK l);
void ReleaseSRWLockExclusive(PSRWLOCK l);
void AcquireSRWLockShared(PSRWLOCK l);
void ReleaseSRWLockShared(PSRWLOCK l);
void InitializeConditionVariable(PCONDITION_VARIABLE c);
BOOL SleepConditionVariableCS(PCONDITION_VARIABLE c, PCRITICAL_SECTION cs, DWORD ms);
BOOL SleepConditionVariableSRW(PCONDITION_VARIABLE c, PSRWLOCK l, DWORD ms, ULONG flags);

// atomic operations: in 64-bit Windows compiler intrinsics -- in WinCC the library's own (runtime\rtasm.asm)
LONG InterlockedIncrement(LONG volatile *p);
LONG InterlockedDecrement(LONG volatile *p);
LONG InterlockedExchange(LONG volatile *p, LONG value);
LONG InterlockedExchangeAdd(LONG volatile *p, LONG value);
LONG InterlockedCompareExchange(LONG volatile *p, LONG exchange, LONG comparand);
LONG64 InterlockedIncrement64(LONG64 volatile *p);
LONG64 InterlockedDecrement64(LONG64 volatile *p);
LONG64 InterlockedExchange64(LONG64 volatile *p, LONG64 value);
LONG64 InterlockedExchangeAdd64(LONG64 volatile *p, LONG64 value);
LONG64 InterlockedCompareExchange64(LONG64 volatile *p, LONG64 exchange, LONG64 comparand);
#define InterlockedAdd(p, v) (InterlockedExchangeAdd((p), (v)) + (v))
#define InterlockedAdd64(p, v) (InterlockedExchangeAdd64((p), (v)) + (v))
#define InterlockedExchangePointer(p, v) ((PVOID)InterlockedExchange64((LONG64 volatile *)(p), (LONG64)(v)))
#define InterlockedCompareExchangePointer(p, e, c)     ((PVOID)InterlockedCompareExchange64((LONG64 volatile *)(p), (LONG64)(e), (LONG64)(c)))
#define _InterlockedIncrement InterlockedIncrement
#define _InterlockedDecrement InterlockedDecrement
#define _InterlockedExchange InterlockedExchange
#define _InterlockedExchangeAdd InterlockedExchangeAdd
#define _InterlockedCompareExchange InterlockedCompareExchange
#define _InterlockedIncrement64 InterlockedIncrement64
#define _InterlockedDecrement64 InterlockedDecrement64
#define _InterlockedExchange64 InterlockedExchange64
#define _InterlockedExchangeAdd64 InterlockedExchangeAdd64
#define _InterlockedCompareExchange64 InterlockedCompareExchange64
void WakeConditionVariable(PCONDITION_VARIABLE c);
void WakeAllConditionVariable(PCONDITION_VARIABLE c);
#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)
DWORD TlsAlloc(void);
LPVOID TlsGetValue(DWORD i);
BOOL TlsSetValue(DWORD i, LPVOID v);
BOOL TlsFree(DWORD i);

HANDLE GetStdHandle(DWORD which);
BOOL SetStdHandle(DWORD which, HANDLE h);
BOOL AllocConsole(void);
BOOL FreeConsole(void);
BOOL AttachConsole(DWORD pid);
HWND GetConsoleWindow(void);
BOOL SetConsoleTitleA(LPCSTR title);
BOOL SetConsoleTitleW(LPCWSTR title);
DWORD GetConsoleTitleA(LPSTR title, DWORD size);
DWORD GetConsoleTitleW(LPWSTR title, DWORD size);
BOOL SetConsoleOutputCP(UINT cp);
BOOL SetConsoleCP(UINT cp);
UINT GetConsoleOutputCP(void);
UINT GetConsoleCP(void);
BOOL GetConsoleMode(HANDLE h, LPDWORD mode);
BOOL SetConsoleMode(HANDLE h, DWORD mode);
BOOL SetConsoleTextAttribute(HANDLE h, WORD attrs);
BOOL SetConsoleCursorPosition(HANDLE h, COORD pos);
BOOL GetConsoleCursorInfo(HANDLE h, PCONSOLE_CURSOR_INFO info);
BOOL SetConsoleCursorInfo(HANDLE h, const CONSOLE_CURSOR_INFO *info);
BOOL GetConsoleScreenBufferInfo(HANDLE h, PCONSOLE_SCREEN_BUFFER_INFO info);
BOOL SetConsoleScreenBufferSize(HANDLE h, COORD size);
BOOL SetConsoleWindowInfo(HANDLE h, BOOL absolute, const SMALL_RECT *rect);
BOOL WriteConsoleA(HANDLE h, const void *text, DWORD n, LPDWORD written, LPVOID reserved);
BOOL WriteConsoleW(HANDLE h, const void *text, DWORD n, LPDWORD written, LPVOID reserved);
BOOL ReadConsoleA(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, LPVOID control);
BOOL ReadConsoleW(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, LPVOID control);
BOOL ReadConsoleInputA(HANDLE h, PINPUT_RECORD buf, DWORD n, LPDWORD read);
BOOL ReadConsoleInputW(HANDLE h, PINPUT_RECORD buf, DWORD n, LPDWORD read);
BOOL PeekConsoleInputW(HANDLE h, PINPUT_RECORD buf, DWORD n, LPDWORD read);
BOOL GetNumberOfConsoleInputEvents(HANDLE h, LPDWORD n);
BOOL FlushConsoleInputBuffer(HANDLE h);
BOOL FillConsoleOutputCharacterA(HANDLE h, CHAR c, DWORD n, COORD at, LPDWORD written);
BOOL FillConsoleOutputCharacterW(HANDLE h, WCHAR c, DWORD n, COORD at, LPDWORD written);
BOOL FillConsoleOutputAttribute(HANDLE h, WORD attr, DWORD n, COORD at, LPDWORD written);
BOOL WriteConsoleOutputW(HANDLE h, const CHAR_INFO *buf, COORD size, COORD at, PSMALL_RECT region);
BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE fn, BOOL add);

#define GetModuleHandle __AW(GetModuleHandle)
#define GetModuleFileName __AW(GetModuleFileName)
#define GetCommandLine __AW(GetCommandLine)
#define LoadLibrary __AW(LoadLibrary)
#define CreateFile __AW(CreateFile)
#define DeleteFile __AW(DeleteFile)
#define CopyFile __AW(CopyFile)
#define MoveFile __AW(MoveFile)
#define MoveFileEx __AW(MoveFileEx)
#define CreateDirectory __AW(CreateDirectory)
#define RemoveDirectory __AW(RemoveDirectory)
#define GetFileAttributes __AW(GetFileAttributes)
#define SetFileAttributes __AW(SetFileAttributes)
#define WIN32_FIND_DATA __AW(WIN32_FIND_DATA)
#define PWIN32_FIND_DATA __AW(PWIN32_FIND_DATA)
#define LPWIN32_FIND_DATA __AW(LPWIN32_FIND_DATA)
#define FindFirstFile __AW(FindFirstFile)
#define FindNextFile __AW(FindNextFile)
#define GetCurrentDirectory __AW(GetCurrentDirectory)
#define SetCurrentDirectory __AW(SetCurrentDirectory)
#define GetTempPath __AW(GetTempPath)
#define GetTempFileName __AW(GetTempFileName)
#define GetFullPathName __AW(GetFullPathName)
#define GetLogicalDriveStrings __AW(GetLogicalDriveStrings)
#define GetDriveType __AW(GetDriveType)
#define GetDiskFreeSpaceEx __AW(GetDiskFreeSpaceEx)
#define lstrlen __AW(lstrlen)
#define lstrcpy __AW(lstrcpy)
#define lstrcpyn __AW(lstrcpyn)
#define lstrcat __AW(lstrcat)
#define lstrcmp __AW(lstrcmp)
#define lstrcmpi __AW(lstrcmpi)
#define OutputDebugString __AW(OutputDebugString)
#define FormatMessage __AW(FormatMessage)
#define GetComputerName __AW(GetComputerName)
#define GetEnvironmentVariable __AW(GetEnvironmentVariable)
#define SetEnvironmentVariable __AW(SetEnvironmentVariable)
#define ExpandEnvironmentStrings __AW(ExpandEnvironmentStrings)
#define STARTUPINFO __AW(STARTUPINFO)
#define LPSTARTUPINFO __AW(LPSTARTUPINFO)
#define CreateProcess __AW(CreateProcess)
#define CreateEvent __AW(CreateEvent)
#define CreateMutex __AW(CreateMutex)
#define CreateSemaphore __AW(CreateSemaphore)
#define SetConsoleTitle __AW(SetConsoleTitle)
#define GetConsoleTitle __AW(GetConsoleTitle)
#define WriteConsole __AW(WriteConsole)
#define ReadConsole __AW(ReadConsole)
#define ReadConsoleInput __AW(ReadConsoleInput)
#define FillConsoleOutputCharacter __AW(FillConsoleOutputCharacter)
