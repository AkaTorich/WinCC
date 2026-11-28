// ntdll.h — родной слой Windows: функции ntdll.dll (Nt…, Rtl…, Ldr…) и их структуры. Под kernel32 лежит
// именно он: CreateFile зовёт NtCreateFile, ReadProcessMemory — NtReadVirtualMemory и так далее.
// Подключает windows.h и берёт функции из ntdll.dll сам.
//
//   #include <ntdll.h>
//   UNICODE_STRING name; OBJECT_ATTRIBUTES oa; IO_STATUS_BLOCK io; HANDLE h;
//   RtlInitUnicodeString(&name, L"\\??\\C:\\temp\\a.txt");          // пути — в виде NT: \??\C:\...
//   InitializeObjectAttributes(&oa, &name, OBJ_CASE_INSENSITIVE, 0, 0);
//   NTSTATUS st = NtCreateFile(&h, FILE_GENERIC_WRITE, &oa, &io, 0, FILE_ATTRIBUTE_NORMAL, 0,
//                              FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, 0, 0);
//   if (!NT_SUCCESS(st)) printf("ошибка %08lX, по-виндовому %lu\n", st, RtlNtStatusToDosError(st));
//
// Функции возвращают NTSTATUS: 0 и другие неотрицательные — удача, отрицательные (0xC…) — ошибка.
// Строки — UNICODE_STRING: длины в байтах, не в символах, ноль в конце не обязателен.
// Структуры, которые Microsoft не описывает (PEB, TEB, LDR_DATA_TABLE_ENTRY…), даны до тех полей,
// что стоят на своих местах во всех Windows 10 и 11; дальше них в структуре есть ещё поля.
#pragma once
#include <windows.h>
#include <intrin.h>
#pragma comment(lib, "ntdll")

#ifndef NTAPI
#define NTAPI
#endif

// ---- NTSTATUS ----

typedef LONG NTSTATUS, *PNTSTATUS;
typedef LONG KPRIORITY;
typedef ULONG_PTR KAFFINITY;
typedef BOOLEAN *PBOOLEAN;
typedef CRITICAL_SECTION RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#define NT_INFORMATION(s) ((((ULONG)(s)) >> 30) == 1)
#define NT_WARNING(s) ((((ULONG)(s)) >> 30) == 2)
#define NT_ERROR(s) ((((ULONG)(s)) >> 30) == 3)

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_WAIT_0 ((NTSTATUS)0x00000000L)
#define STATUS_WAIT_1 ((NTSTATUS)0x00000001L)
#define STATUS_ABANDONED ((NTSTATUS)0x00000080L)
#define STATUS_USER_APC ((NTSTATUS)0x000000C0L)
#define STATUS_ALERTED ((NTSTATUS)0x00000101L)
#define STATUS_TIMEOUT ((NTSTATUS)0x00000102L)
#define STATUS_PENDING ((NTSTATUS)0x00000103L)
#define STATUS_REPARSE ((NTSTATUS)0x00000104L)
#define STATUS_MORE_ENTRIES ((NTSTATUS)0x00000105L)
#define STATUS_NOT_ALL_ASSIGNED ((NTSTATUS)0x00000106L)
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005L)
#define STATUS_NO_MORE_FILES ((NTSTATUS)0x80000006L)
#define STATUS_NO_MORE_ENTRIES ((NTSTATUS)0x8000001AL)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_INFO_CLASS ((NTSTATUS)0xC0000003L)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008L)
#define STATUS_INVALID_CID ((NTSTATUS)0xC000000BL)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#define STATUS_NO_SUCH_FILE ((NTSTATUS)0xC000000FL)
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010L)
#define STATUS_END_OF_FILE ((NTSTATUS)0xC0000011L)
#define STATUS_NO_MEMORY ((NTSTATUS)0xC0000017L)
#define STATUS_CONFLICTING_ADDRESSES ((NTSTATUS)0xC0000018L)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_TYPE_MISMATCH ((NTSTATUS)0xC0000024L)
#define STATUS_OBJECT_NAME_INVALID ((NTSTATUS)0xC0000033L)
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034L)
#define STATUS_OBJECT_NAME_COLLISION ((NTSTATUS)0xC0000035L)
#define STATUS_OBJECT_PATH_INVALID ((NTSTATUS)0xC0000039L)
#define STATUS_OBJECT_PATH_NOT_FOUND ((NTSTATUS)0xC000003AL)
#define STATUS_OBJECT_PATH_SYNTAX_BAD ((NTSTATUS)0xC000003BL)
#define STATUS_SHARING_VIOLATION ((NTSTATUS)0xC0000043L)
#define STATUS_DELETE_PENDING ((NTSTATUS)0xC0000056L)
#define STATUS_PRIVILEGE_NOT_HELD ((NTSTATUS)0xC0000061L)
#define STATUS_PROCEDURE_NOT_FOUND ((NTSTATUS)0xC000007AL)
#define STATUS_INVALID_IMAGE_FORMAT ((NTSTATUS)0xC000007BL)
#define STATUS_DISK_FULL ((NTSTATUS)0xC000007FL)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009AL)
#define STATUS_FILE_IS_A_DIRECTORY ((NTSTATUS)0xC00000BAL)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBL)
#define STATUS_INTERNAL_ERROR ((NTSTATUS)0xC00000E5L)
#define STATUS_DIRECTORY_NOT_EMPTY ((NTSTATUS)0xC0000101L)
#define STATUS_NOT_A_DIRECTORY ((NTSTATUS)0xC0000103L)
#define STATUS_PROCESS_IS_TERMINATING ((NTSTATUS)0xC000010AL)
#define STATUS_CANCELLED ((NTSTATUS)0xC0000120L)
#define STATUS_DLL_NOT_FOUND ((NTSTATUS)0xC0000135L)
#define STATUS_ENTRYPOINT_NOT_FOUND ((NTSTATUS)0xC0000139L)
#define STATUS_NOT_FOUND ((NTSTATUS)0xC0000225L)
#define STATUS_HEAP_CORRUPTION ((NTSTATUS)0xC0000374L)
#define STATUS_STACK_BUFFER_OVERRUN ((NTSTATUS)0xC0000409L)
// STATUS_ACCESS_VIOLATION, STATUS_STACK_OVERFLOW, STATUS_INTEGER_DIVIDE_BY_ZERO — в win\except.h

// ---- строки, объекты, ввод-вывод ----

typedef struct _UNICODE_STRING {
    USHORT Length;              // байт в строке (без нуля в конце)
    USHORT MaximumLength;       // байт в Buffer
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING, *PSTRING, ANSI_STRING, *PANSI_STRING, OEM_STRING, *POEM_STRING;
typedef const STRING *PCSTRING;
typedef const ANSI_STRING *PCANSI_STRING;
typedef const char *PCSZ;

// строка из литерала на месте: UNICODE_STRING s = RTL_CONSTANT_STRING(L"...");
#define RTL_CONSTANT_STRING(s) { sizeof(s) - sizeof((s)[0]), sizeof(s), (s) }

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;               // путь в ObjectName — от этой папки (0 — от корня)
    PUNICODE_STRING ObjectName;
    ULONG Attributes;                   // OBJ_…
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

#define OBJ_INHERIT 0x00000002L
#define OBJ_PERMANENT 0x00000010L
#define OBJ_EXCLUSIVE 0x00000020L
#define OBJ_CASE_INSENSITIVE 0x00000040L
#define OBJ_OPENIF 0x00000080L
#define OBJ_OPENLINK 0x00000100L
#define OBJ_KERNEL_HANDLE 0x00000200L
#define OBJ_FORCE_ACCESS_CHECK 0x00000400L
#define OBJ_IGNORE_IMPERSONATED_DEVICEMAP 0x00000800L
#define OBJ_DONT_REPARSE 0x00001000L
#define OBJ_VALID_ATTRIBUTES 0x00001FF2L

#define InitializeObjectAttributes(p, n, a, r, s) do { \
    (p)->Length = sizeof(OBJECT_ATTRIBUTES); (p)->RootDirectory = (r); (p)->Attributes = (a); \
    (p)->ObjectName = (n); (p)->SecurityDescriptor = (s); (p)->SecurityQualityOfService = 0; } while (0)

typedef struct _IO_STATUS_BLOCK {
    union { NTSTATUS Status; PVOID Pointer; };
    ULONG_PTR Information;              // сколько байт прочитано или записано; что сделал NtCreateFile (FILE_CREATED…)
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef void (NTAPI *PIO_APC_ROUTINE)(PVOID context, PIO_STATUS_BLOCK io, ULONG reserved);

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;               // номер процесса
    HANDLE UniqueThread;                // номер потока
} CLIENT_ID, *PCLIENT_ID;

// сам процесс и сам поток — без открытия
#define NtCurrentProcess() ((HANDLE)(LONG_PTR)-1)
#define NtCurrentThread() ((HANDLE)(LONG_PTR)-2)
#define ZwCurrentProcess() NtCurrentProcess()
#define ZwCurrentThread() NtCurrentThread()

#ifndef RtlZeroMemory
#define RtlZeroMemory(d, n) memset((d), 0, (n))
#define RtlFillMemory(d, n, v) memset((d), (v), (n))
#define RtlCopyMemory(d, s, n) memcpy((d), (s), (n))
#define RtlMoveMemory(d, s, n) memmove((d), (s), (n))
#define RtlEqualMemory(a, b, n) (!memcmp((a), (b), (n)))
#endif

// ---- блоки процесса и потока ----

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;           // модули в порядке загрузки: первый — сама программа
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
    PVOID EntryInProgress;
    BOOLEAN ShutdownInProgress;
    HANDLE ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

// один загруженный модуль (exe или dll); списки PEB_LDR_DATA ведут на поля …Links
typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;                 // C:\Windows\System32\kernel32.dll
    UNICODE_STRING BaseDllName;                 // kernel32.dll
    ULONG Flags;
    USHORT ObsoleteLoadCount;
    USHORT TlsIndex;
    LIST_ENTRY HashLinks;
    ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef struct _CURDIR {
    UNICODE_STRING DosPath;
    HANDLE Handle;
} CURDIR, *PCURDIR;

typedef struct _RTL_DRIVE_LETTER_CURDIR {
    USHORT Flags;
    USHORT Length;
    ULONG TimeStamp;
    STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

// то, с чем процесс запущен: путь, командная строка, папка, окружение, стандартные ввод и вывод
typedef struct _RTL_USER_PROCESS_PARAMETERS {
    ULONG MaximumLength;
    ULONG Length;
    ULONG Flags;
    ULONG DebugFlags;
    HANDLE ConsoleHandle;
    ULONG ConsoleFlags;
    HANDLE StandardInput;
    HANDLE StandardOutput;
    HANDLE StandardError;
    CURDIR CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    PVOID Environment;                          // строки "ИМЯ=значение" UTF-16 подряд, в конце пустая
    ULONG StartingX, StartingY, CountX, CountY, CountCharsX, CountCharsY;
    ULONG FillAttribute;
    ULONG WindowFlags;
    ULONG ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopInfo;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
    RTL_DRIVE_LETTER_CURDIR CurrentDirectories[32];
    ULONG_PTR EnvironmentSize;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

// блок процесса (PEB): NtCurrentPeb() или PROCESS_BASIC_INFORMATION.PebBaseAddress
typedef struct _PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;                      // то, что отвечает IsDebuggerPresent
    BOOLEAN BitField;
    UCHAR Padding0[4];
    HANDLE Mutant;
    PVOID ImageBaseAddress;                     // адрес exe, как GetModuleHandle(0)
    PPEB_LDR_DATA Ldr;                          // загруженные модули
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    PVOID SubSystemData;
    PVOID ProcessHeap;                          // как GetProcessHeap()
    PRTL_CRITICAL_SECTION FastPebLock;
    PVOID AtlThunkSListPtr;
    PVOID IFEOKey;
    ULONG CrossProcessFlags;
    UCHAR Padding1[4];
    PVOID KernelCallbackTable;
    ULONG SystemReserved;
    ULONG AtlThunkSListPtr32;
    PVOID ApiSetMap;
    ULONG TlsExpansionCounter;
    UCHAR Padding2[4];
    PVOID TlsBitmap;
    ULONG TlsBitmapBits[2];
    PVOID ReadOnlySharedMemoryBase;
    PVOID SharedData;
    PVOID *ReadOnlyStaticServerData;
    PVOID AnsiCodePageData;
    PVOID OemCodePageData;
    PVOID UnicodeCaseTableData;
    ULONG NumberOfProcessors;
    ULONG NtGlobalFlag;
    LARGE_INTEGER CriticalSectionTimeout;
    SIZE_T HeapSegmentReserve;
    SIZE_T HeapSegmentCommit;
    SIZE_T HeapDeCommitTotalFreeThreshold;
    SIZE_T HeapDeCommitFreeBlockThreshold;
    ULONG NumberOfHeaps;
    ULONG MaximumNumberOfHeaps;
    PVOID *ProcessHeaps;
    PVOID GdiSharedHandleTable;
    PVOID ProcessStarterHelper;
    ULONG GdiDCAttributeList;
    UCHAR Padding3[4];
    PRTL_CRITICAL_SECTION LoaderLock;
    ULONG OSMajorVersion;
    ULONG OSMinorVersion;
    USHORT OSBuildNumber;
    USHORT OSCSDVersion;
    ULONG OSPlatformId;
    ULONG ImageSubsystem;                       // 2 — оконная программа, 3 — консольная
    ULONG ImageSubsystemMajorVersion;
    ULONG ImageSubsystemMinorVersion;
    UCHAR Padding4[4];
    KAFFINITY ActiveProcessAffinityMask;
    ULONG GdiHandleBuffer[60];
    PVOID PostProcessInitRoutine;
    PVOID TlsExpansionBitmap;
    ULONG TlsExpansionBitmapBits[32];
    ULONG SessionId;
} PEB, *PPEB;

// блок потока (TEB) — начало: NtCurrentTeb() (intrin.h)
typedef struct _TEB {
    NT_TIB NtTib;
    PVOID EnvironmentPointer;
    CLIENT_ID ClientId;                         // номера процесса и потока
    PVOID ActiveRpcHandle;
    PVOID ThreadLocalStoragePointer;
    PPEB ProcessEnvironmentBlock;
    ULONG LastErrorValue;                       // то, что отвечает GetLastError
    ULONG CountOfOwnedCriticalSections;
} TEB, *PTEB;

#define NtCurrentPeb() ((PPEB)__readgsqword(0x60))

// ---- что можно спросить у системы, процесса, потока, объекта ----

typedef enum _SYSTEM_INFORMATION_CLASS {
    SystemBasicInformation = 0,
    SystemProcessorInformation = 1,
    SystemPerformanceInformation = 2,
    SystemTimeOfDayInformation = 3,
    SystemProcessInformation = 5,               // все процессы и их потоки: SYSTEM_PROCESS_INFORMATION
    SystemProcessorPerformanceInformation = 8,  // время каждого ядра
    SystemModuleInformation = 11,
    SystemHandleInformation = 16,
    SystemInterruptInformation = 23,
    SystemExceptionInformation = 33,
    SystemRegistryQuotaInformation = 37,
    SystemLookasideInformation = 45,
    SystemExtendedHandleInformation = 64,
    SystemCodeIntegrityInformation = 103,
    SystemPolicyInformation = 134
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_BASIC_INFORMATION {
    ULONG Reserved;
    ULONG TimerResolution;
    ULONG PageSize;
    ULONG NumberOfPhysicalPages;
    ULONG LowestPhysicalPageNumber;
    ULONG HighestPhysicalPageNumber;
    ULONG AllocationGranularity;
    ULONG_PTR MinimumUserModeAddress;
    ULONG_PTR MaximumUserModeAddress;
    KAFFINITY ActiveProcessorsAffinityMask;
    CCHAR NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION, *PSYSTEM_BASIC_INFORMATION;

typedef struct _SYSTEM_TIMEOFDAY_INFORMATION {
    LARGE_INTEGER BootTime;                     // когда система запущена (сотни наносекунд с 1601 года)
    LARGE_INTEGER CurrentTime;
    LARGE_INTEGER TimeZoneBias;
    ULONG TimeZoneId;
    ULONG Reserved;
    ULONGLONG BootTimeBias;
    ULONGLONG SleepTimeBias;
} SYSTEM_TIMEOFDAY_INFORMATION, *PSYSTEM_TIMEOFDAY_INFORMATION;

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;                   // вместе с IdleTime
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, *PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef enum _KTHREAD_STATE {
    Initialized, Ready, Running, Standby, Terminated, Waiting, Transition, DeferredReady, GateWaitObsolete,
    WaitingForProcessInSwap
} KTHREAD_STATE;

typedef struct _SYSTEM_THREAD_INFORMATION {
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER CreateTime;
    ULONG WaitTime;
    PVOID StartAddress;
    CLIENT_ID ClientId;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
    ULONG ContextSwitches;
    ULONG ThreadState;                          // KTHREAD_STATE
    ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;

// один процесс в ответе SystemProcessInformation; за ним — его потоки (Threads[NumberOfThreads]),
// следующий процесс — через NextEntryOffset байт от начала этого, у последнего NextEntryOffset = 0
typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;                   // имя exe; у процесса 0 (System Idle) пустое
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;        // кто запустил
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
    SYSTEM_THREAD_INFORMATION Threads[1];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0,                // PROCESS_BASIC_INFORMATION
    ProcessIoCounters = 2,                      // IO_COUNTERS
    ProcessVmCounters = 3,
    ProcessTimes = 4,                           // KERNEL_USER_TIMES
    ProcessBasePriority = 5,
    ProcessDebugPort = 7,
    ProcessHandleCount = 20,                    // ULONG
    ProcessAffinityMask = 21,
    ProcessPriorityBoost = 22,
    ProcessSessionInformation = 24,
    ProcessWow64Information = 26,               // ULONG_PTR: не 0 — 32-битный процесс
    ProcessImageFileName = 27,                  // UNICODE_STRING и путь вида \Device\HarddiskVolume3\…
    ProcessBreakOnTermination = 29,
    ProcessDebugObjectHandle = 30,
    ProcessDebugFlags = 31,
    ProcessImageFileNameWin32 = 43,             // UNICODE_STRING и путь вида C:\…
    ProcessCommandLineInformation = 60          // UNICODE_STRING — командная строка
} PROCESSINFOCLASS;

typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;                        // STATUS_PENDING (0x103), пока процесс жив
    PPEB PebBaseAddress;                        // адрес PEB в том процессе
    KAFFINITY AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;

typedef struct _KERNEL_USER_TIMES {
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER ExitTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
} KERNEL_USER_TIMES, *PKERNEL_USER_TIMES;

typedef struct _IO_COUNTERS {
    ULONGLONG ReadOperationCount;
    ULONGLONG WriteOperationCount;
    ULONGLONG OtherOperationCount;
    ULONGLONG ReadTransferCount;
    ULONGLONG WriteTransferCount;
    ULONGLONG OtherTransferCount;
} IO_COUNTERS, *PIO_COUNTERS;

typedef enum _THREADINFOCLASS {
    ThreadBasicInformation = 0,                 // THREAD_BASIC_INFORMATION
    ThreadTimes = 1,                            // KERNEL_USER_TIMES
    ThreadPriority = 2,
    ThreadBasePriority = 3,
    ThreadAffinityMask = 4,
    ThreadQuerySetWin32StartAddress = 9,        // PVOID: где поток начал
    ThreadIsIoPending = 16,
    ThreadHideFromDebugger = 17,
    ThreadIsTerminated = 20,
    ThreadNameInformation = 38                  // UNICODE_STRING — имя потока
} THREADINFOCLASS;

typedef struct _THREAD_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PTEB TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

typedef enum _OBJECT_INFORMATION_CLASS {
    ObjectBasicInformation = 0,                 // OBJECT_BASIC_INFORMATION
    ObjectNameInformation = 1,                  // OBJECT_NAME_INFORMATION
    ObjectTypeInformation = 2                   // начинается с UNICODE_STRING — имени вида ("File", "Event"…)
} OBJECT_INFORMATION_CLASS;

typedef struct _OBJECT_BASIC_INFORMATION {
    ULONG Attributes;
    ACCESS_MASK GrantedAccess;
    ULONG HandleCount;
    ULONG PointerCount;
    ULONG Reserved[10];
} OBJECT_BASIC_INFORMATION, *POBJECT_BASIC_INFORMATION;

typedef struct _OBJECT_NAME_INFORMATION {
    UNICODE_STRING Name;                        // буфер имени идёт сразу за структурой
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

typedef enum _MEMORY_INFORMATION_CLASS {
    MemoryBasicInformation = 0                  // MEMORY_BASIC_INFORMATION
} MEMORY_INFORMATION_CLASS;

typedef enum _SECTION_INHERIT { ViewShare = 1, ViewUnmap = 2 } SECTION_INHERIT;
typedef enum _EVENT_TYPE { NotificationEvent = 0, SynchronizationEvent = 1 } EVENT_TYPE;
typedef enum _WAIT_TYPE { WaitAll = 0, WaitAny = 1 } WAIT_TYPE;

typedef void (NTAPI *PPS_APC_ROUTINE)(PVOID arg1, PVOID arg2, PVOID arg3);
typedef NTSTATUS (NTAPI *PUSER_THREAD_START_ROUTINE)(PVOID arg);

// NtCreateThreadEx, flags
#define THREAD_CREATE_FLAGS_CREATE_SUSPENDED 0x00000001
#define THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH 0x00000002
#define THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER 0x00000004

// ---- файлы ----

// NtCreateFile, disposition: что делать, если файл есть или его нет
#define FILE_SUPERSEDE 0x00000000
#define FILE_OPEN 0x00000001
#define FILE_CREATE 0x00000002
#define FILE_OPEN_IF 0x00000003
#define FILE_OVERWRITE 0x00000004
#define FILE_OVERWRITE_IF 0x00000005
// options
#define FILE_DIRECTORY_FILE 0x00000001
#define FILE_WRITE_THROUGH 0x00000002
#define FILE_SEQUENTIAL_ONLY 0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING 0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT 0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020  // без него чтение и запись идут асинхронно
#define FILE_NON_DIRECTORY_FILE 0x00000040
#define FILE_CREATE_TREE_CONNECTION 0x00000080
#define FILE_COMPLETE_IF_OPLOCKED 0x00000100
#define FILE_NO_EA_KNOWLEDGE 0x00000200
#define FILE_OPEN_REMOTE_INSTANCE 0x00000400
#define FILE_RANDOM_ACCESS 0x00000800
#define FILE_DELETE_ON_CLOSE 0x00001000
#define FILE_OPEN_BY_FILE_ID 0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT 0x00004000
#define FILE_NO_COMPRESSION 0x00008000
#define FILE_RESERVE_OPFILTER 0x00100000
#define FILE_OPEN_REPARSE_POINT 0x00200000
#define FILE_OPEN_NO_RECALL 0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY 0x00800000
#define FILE_VALID_OPTION_FLAGS 0x00FFFFFF
// IO_STATUS_BLOCK.Information после NtCreateFile
#define FILE_SUPERSEDED 0x00000000
#define FILE_OPENED 0x00000001
#define FILE_CREATED 0x00000002
#define FILE_OVERWRITTEN 0x00000003
#define FILE_EXISTS 0x00000004
#define FILE_DOES_NOT_EXIST 0x00000005

typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,               // FILE_DIRECTORY_INFORMATION
    FileFullDirectoryInformation = 2,
    FileBothDirectoryInformation = 3,
    FileBasicInformation = 4,                   // FILE_BASIC_INFORMATION
    FileStandardInformation = 5,                // FILE_STANDARD_INFORMATION
    FileInternalInformation = 6,
    FileEaInformation = 7,
    FileAccessInformation = 8,
    FileNameInformation = 9,                    // FILE_NAME_INFORMATION
    FileRenameInformation = 10,
    FileLinkInformation = 11,
    FileNamesInformation = 12,                  // FILE_NAMES_INFORMATION
    FileDispositionInformation = 13,            // FILE_DISPOSITION_INFORMATION
    FilePositionInformation = 14,               // FILE_POSITION_INFORMATION
    FileFullEaInformation = 15,
    FileModeInformation = 16,
    FileAlignmentInformation = 17,
    FileAllInformation = 18,
    FileAllocationInformation = 19,
    FileEndOfFileInformation = 20,              // FILE_END_OF_FILE_INFORMATION
    FileAlternateNameInformation = 21,
    FileStreamInformation = 22,
    FilePipeInformation = 23,
    FilePipeLocalInformation = 24,
    FilePipeRemoteInformation = 25,
    FileMailslotQueryInformation = 26,
    FileMailslotSetInformation = 27,
    FileCompressionInformation = 28,
    FileObjectIdInformation = 29,
    FileCompletionInformation = 30,
    FileMoveClusterInformation = 31,
    FileQuotaInformation = 32,
    FileReparsePointInformation = 33,
    FileNetworkOpenInformation = 34,            // FILE_NETWORK_OPEN_INFORMATION
    FileAttributeTagInformation = 35,
    FileTrackingInformation = 36,
    FileIdBothDirectoryInformation = 37,
    FileIdFullDirectoryInformation = 38
} FILE_INFORMATION_CLASS;

typedef struct _FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;                 // сотни наносекунд с 1601 года, как FILETIME
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef struct _FILE_STANDARD_INFORMATION {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;                    // размер файла
    ULONG NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN Directory;
} FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

typedef struct _FILE_POSITION_INFORMATION { LARGE_INTEGER CurrentByteOffset; } FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;
typedef struct _FILE_END_OF_FILE_INFORMATION { LARGE_INTEGER EndOfFile; } FILE_END_OF_FILE_INFORMATION, *PFILE_END_OF_FILE_INFORMATION;
typedef struct _FILE_DISPOSITION_INFORMATION { BOOLEAN DeleteFile; } FILE_DISPOSITION_INFORMATION, *PFILE_DISPOSITION_INFORMATION;
typedef struct _FILE_NAME_INFORMATION { ULONG FileNameLength; WCHAR FileName[1]; } FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

// ответы NtQueryDirectoryFile: записи подряд, следующая — через NextEntryOffset байт (0 — последняя);
// FileName без нуля в конце, длина в байтах — FileNameLength
typedef struct _FILE_DIRECTORY_INFORMATION {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_DIRECTORY_INFORMATION, *PFILE_DIRECTORY_INFORMATION;

typedef struct _FILE_NAMES_INFORMATION {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NAMES_INFORMATION, *PFILE_NAMES_INFORMATION;

typedef enum _FSINFOCLASS {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation = 2,
    FileFsSizeInformation = 3,                  // FILE_FS_SIZE_INFORMATION
    FileFsDeviceInformation = 4,
    FileFsAttributeInformation = 5,
    FileFsControlInformation = 6,
    FileFsFullSizeInformation = 7
} FS_INFORMATION_CLASS;

typedef struct _FILE_FS_SIZE_INFORMATION {
    LARGE_INTEGER TotalAllocationUnits;
    LARGE_INTEGER AvailableAllocationUnits;
    ULONG SectorsPerAllocationUnit;
    ULONG BytesPerSector;
} FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;

// ---- реестр: пути вида \Registry\Machine\SOFTWARE\… и \Registry\User\<SID>\… ----

typedef enum _KEY_INFORMATION_CLASS {
    KeyBasicInformation = 0,                    // KEY_BASIC_INFORMATION
    KeyNodeInformation = 1,
    KeyFullInformation = 2,
    KeyNameInformation = 3
} KEY_INFORMATION_CLASS;

typedef enum _KEY_VALUE_INFORMATION_CLASS {
    KeyValueBasicInformation = 0,               // KEY_VALUE_BASIC_INFORMATION
    KeyValueFullInformation = 1,                // KEY_VALUE_FULL_INFORMATION
    KeyValuePartialInformation = 2              // KEY_VALUE_PARTIAL_INFORMATION
} KEY_VALUE_INFORMATION_CLASS;

typedef struct _KEY_BASIC_INFORMATION {
    LARGE_INTEGER LastWriteTime;
    ULONG TitleIndex;
    ULONG NameLength;                           // в байтах
    WCHAR Name[1];
} KEY_BASIC_INFORMATION, *PKEY_BASIC_INFORMATION;

typedef struct _KEY_VALUE_BASIC_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;                                 // REG_SZ, REG_DWORD…
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_BASIC_INFORMATION, *PKEY_VALUE_BASIC_INFORMATION;

typedef struct _KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;                           // от начала структуры
    ULONG DataLength;
    ULONG NameLength;
    WCHAR Name[1];
} KEY_VALUE_FULL_INFORMATION, *PKEY_VALUE_FULL_INFORMATION;

typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataLength;
    UCHAR Data[1];
} KEY_VALUE_PARTIAL_INFORMATION, *PKEY_VALUE_PARTIAL_INFORMATION;

#define REG_OPTION_NON_VOLATILE 0x00000000L
#define REG_OPTION_VOLATILE 0x00000001L
#define REG_CREATED_NEW_KEY 0x00000001L
#define REG_OPENED_EXISTING_KEY 0x00000002L

// ---- версия ----

typedef struct _RTL_OSVERSIONINFOW {
    ULONG dwOSVersionInfoSize;                  // sizeof — заполнить перед RtlGetVersion
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
} RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;

typedef struct _RTL_OSVERSIONINFOEXW {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
    USHORT wServicePackMajor;
    USHORT wServicePackMinor;
    USHORT wSuiteMask;
    UCHAR wProductType;
    UCHAR wReserved;
} RTL_OSVERSIONINFOEXW, *PRTL_OSVERSIONINFOEXW;

// ---- привилегии для RtlAdjustPrivilege ----

#define SE_CREATE_TOKEN_PRIVILEGE 2
#define SE_ASSIGNPRIMARYTOKEN_PRIVILEGE 3
#define SE_LOCK_MEMORY_PRIVILEGE 4
#define SE_INCREASE_QUOTA_PRIVILEGE 5
#define SE_TCB_PRIVILEGE 7
#define SE_SECURITY_PRIVILEGE 8
#define SE_TAKE_OWNERSHIP_PRIVILEGE 9
#define SE_LOAD_DRIVER_PRIVILEGE 10
#define SE_SYSTEM_PROFILE_PRIVILEGE 11
#define SE_SYSTEMTIME_PRIVILEGE 12
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE 13
#define SE_INC_BASE_PRIORITY_PRIVILEGE 14
#define SE_CREATE_PAGEFILE_PRIVILEGE 15
#define SE_BACKUP_PRIVILEGE 17
#define SE_RESTORE_PRIVILEGE 18
#define SE_SHUTDOWN_PRIVILEGE 19
#define SE_DEBUG_PRIVILEGE 20
#define SE_SYSTEM_ENVIRONMENT_PRIVILEGE 22
#define SE_CHANGE_NOTIFY_PRIVILEGE 23
#define SE_UNDOCK_PRIVILEGE 25
#define SE_MANAGE_VOLUME_PRIVILEGE 28
#define SE_IMPERSONATE_PRIVILEGE 29
#define SE_CREATE_GLOBAL_PRIVILEGE 30
#define SE_INC_WORKING_SET_PRIVILEGE 33
#define SE_TIME_ZONE_PRIVILEGE 34
#define SE_CREATE_SYMBOLIC_LINK_PRIVILEGE 35

// ======================================================================
// функции ntdll.dll
// ======================================================================

// объекты
NTSTATUS NTAPI NtClose(HANDLE handle);
NTSTATUS NTAPI NtDuplicateObject(HANDLE source_process, HANDLE source, HANDLE target_process, PHANDLE target,
                                 ACCESS_MASK access, ULONG attributes, ULONG options);
NTSTATUS NTAPI NtQueryObject(HANDLE handle, OBJECT_INFORMATION_CLASS what, PVOID info, ULONG size, PULONG returned);
NTSTATUS NTAPI NtWaitForSingleObject(HANDLE handle, BOOLEAN alertable, PLARGE_INTEGER timeout);
NTSTATUS NTAPI NtWaitForMultipleObjects(ULONG count, HANDLE *handles, WAIT_TYPE type, BOOLEAN alertable,
                                        PLARGE_INTEGER timeout);
NTSTATUS NTAPI NtSignalAndWaitForSingleObject(HANDLE signal, HANDLE wait, BOOLEAN alertable, PLARGE_INTEGER timeout);
// время ожидания и задержки — в сотнях наносекунд: меньше нуля — сколько ждать (-10000 = 1 мс), больше — до какого момента

// события, мьютексы, семафоры
NTSTATUS NTAPI NtCreateEvent(PHANDLE event, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, EVENT_TYPE type, BOOLEAN initial);
NTSTATUS NTAPI NtOpenEvent(PHANDLE event, ACCESS_MASK access, POBJECT_ATTRIBUTES oa);
NTSTATUS NTAPI NtSetEvent(HANDLE event, PLONG previous);
NTSTATUS NTAPI NtResetEvent(HANDLE event, PLONG previous);
NTSTATUS NTAPI NtClearEvent(HANDLE event);
NTSTATUS NTAPI NtPulseEvent(HANDLE event, PLONG previous);
NTSTATUS NTAPI NtCreateMutant(PHANDLE mutant, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, BOOLEAN owner);
NTSTATUS NTAPI NtReleaseMutant(HANDLE mutant, PLONG previous);
NTSTATUS NTAPI NtCreateSemaphore(PHANDLE semaphore, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, LONG initial, LONG maximum);
NTSTATUS NTAPI NtReleaseSemaphore(HANDLE semaphore, LONG count, PLONG previous);

// система и время
NTSTATUS NTAPI NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS what, PVOID info, ULONG size, PULONG returned);
NTSTATUS NTAPI NtQuerySystemTime(PLARGE_INTEGER time);              // UTC, сотни наносекунд с 1601 года
NTSTATUS NTAPI NtQueryPerformanceCounter(PLARGE_INTEGER counter, PLARGE_INTEGER frequency);
NTSTATUS NTAPI NtQueryTimerResolution(PULONG maximum, PULONG minimum, PULONG current);   // сотни наносекунд
NTSTATUS NTAPI NtSetTimerResolution(ULONG desired, BOOLEAN set, PULONG current);
NTSTATUS NTAPI NtDelayExecution(BOOLEAN alertable, PLARGE_INTEGER interval);
NTSTATUS NTAPI NtYieldExecution(void);

// процессы
NTSTATUS NTAPI NtOpenProcess(PHANDLE process, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PCLIENT_ID id);
NTSTATUS NTAPI NtTerminateProcess(HANDLE process, NTSTATUS exit_status);
NTSTATUS NTAPI NtSuspendProcess(HANDLE process);
NTSTATUS NTAPI NtResumeProcess(HANDLE process);
NTSTATUS NTAPI NtQueryInformationProcess(HANDLE process, PROCESSINFOCLASS what, PVOID info, ULONG size, PULONG returned);
NTSTATUS NTAPI NtSetInformationProcess(HANDLE process, PROCESSINFOCLASS what, PVOID info, ULONG size);
NTSTATUS NTAPI NtOpenProcessToken(HANDLE process, ACCESS_MASK access, PHANDLE token);

// потоки
NTSTATUS NTAPI NtCreateThreadEx(PHANDLE thread, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, HANDLE process,
                                PUSER_THREAD_START_ROUTINE start, PVOID arg, ULONG flags, SIZE_T zero_bits,
                                SIZE_T stack_size, SIZE_T maximum_stack_size, PVOID attribute_list);
NTSTATUS NTAPI NtOpenThread(PHANDLE thread, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PCLIENT_ID id);
NTSTATUS NTAPI NtTerminateThread(HANDLE thread, NTSTATUS exit_status);
NTSTATUS NTAPI NtSuspendThread(HANDLE thread, PULONG previous_count);
NTSTATUS NTAPI NtResumeThread(HANDLE thread, PULONG previous_count);
NTSTATUS NTAPI NtAlertThread(HANDLE thread);
NTSTATUS NTAPI NtTestAlert(void);
NTSTATUS NTAPI NtQueueApcThread(HANDLE thread, PPS_APC_ROUTINE routine, PVOID arg1, PVOID arg2, PVOID arg3);
NTSTATUS NTAPI NtGetContextThread(HANDLE thread, PCONTEXT context);
NTSTATUS NTAPI NtSetContextThread(HANDLE thread, PCONTEXT context);
NTSTATUS NTAPI NtQueryInformationThread(HANDLE thread, THREADINFOCLASS what, PVOID info, ULONG size, PULONG returned);
NTSTATUS NTAPI NtSetInformationThread(HANDLE thread, THREADINFOCLASS what, PVOID info, ULONG size);

// память
NTSTATUS NTAPI NtAllocateVirtualMemory(HANDLE process, PVOID *base, ULONG_PTR zero_bits, PSIZE_T size,
                                       ULONG type, ULONG protect);
NTSTATUS NTAPI NtFreeVirtualMemory(HANDLE process, PVOID *base, PSIZE_T size, ULONG type);
NTSTATUS NTAPI NtProtectVirtualMemory(HANDLE process, PVOID *base, PSIZE_T size, ULONG protect, PULONG old_protect);
NTSTATUS NTAPI NtQueryVirtualMemory(HANDLE process, PVOID base, MEMORY_INFORMATION_CLASS what, PVOID info,
                                    SIZE_T size, PSIZE_T returned);
NTSTATUS NTAPI NtReadVirtualMemory(HANDLE process, PVOID base, PVOID buffer, SIZE_T size, PSIZE_T read);
NTSTATUS NTAPI NtWriteVirtualMemory(HANDLE process, PVOID base, PVOID buffer, SIZE_T size, PSIZE_T written);
NTSTATUS NTAPI NtFlushInstructionCache(HANDLE process, PVOID base, SIZE_T size);
NTSTATUS NTAPI NtCreateSection(PHANDLE section, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PLARGE_INTEGER maximum_size,
                               ULONG protect, ULONG attributes, HANDLE file);
NTSTATUS NTAPI NtMapViewOfSection(HANDLE section, HANDLE process, PVOID *base, ULONG_PTR zero_bits, SIZE_T commit,
                                  PLARGE_INTEGER offset, PSIZE_T view_size, SECTION_INHERIT inherit,
                                  ULONG type, ULONG protect);
NTSTATUS NTAPI NtUnmapViewOfSection(HANDLE process, PVOID base);

// файлы
NTSTATUS NTAPI NtCreateFile(PHANDLE file, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PIO_STATUS_BLOCK io,
                            PLARGE_INTEGER allocation, ULONG attributes, ULONG share, ULONG disposition,
                            ULONG options, PVOID ea, ULONG ea_size);
NTSTATUS NTAPI NtOpenFile(PHANDLE file, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PIO_STATUS_BLOCK io,
                          ULONG share, ULONG options);
NTSTATUS NTAPI NtReadFile(HANDLE file, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io,
                          PVOID buffer, ULONG size, PLARGE_INTEGER offset, PULONG key);
NTSTATUS NTAPI NtWriteFile(HANDLE file, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io,
                           PVOID buffer, ULONG size, PLARGE_INTEGER offset, PULONG key);
NTSTATUS NTAPI NtFlushBuffersFile(HANDLE file, PIO_STATUS_BLOCK io);
NTSTATUS NTAPI NtCancelIoFile(HANDLE file, PIO_STATUS_BLOCK io);
NTSTATUS NTAPI NtQueryInformationFile(HANDLE file, PIO_STATUS_BLOCK io, PVOID info, ULONG size, FILE_INFORMATION_CLASS what);
NTSTATUS NTAPI NtSetInformationFile(HANDLE file, PIO_STATUS_BLOCK io, PVOID info, ULONG size, FILE_INFORMATION_CLASS what);
NTSTATUS NTAPI NtQueryDirectoryFile(HANDLE dir, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io,
                                    PVOID info, ULONG size, FILE_INFORMATION_CLASS what, BOOLEAN single,
                                    PUNICODE_STRING mask, BOOLEAN restart);
NTSTATUS NTAPI NtQueryAttributesFile(POBJECT_ATTRIBUTES oa, PFILE_BASIC_INFORMATION info);
NTSTATUS NTAPI NtQueryFullAttributesFile(POBJECT_ATTRIBUTES oa, PFILE_NETWORK_OPEN_INFORMATION info);
NTSTATUS NTAPI NtQueryVolumeInformationFile(HANDLE file, PIO_STATUS_BLOCK io, PVOID info, ULONG size, FS_INFORMATION_CLASS what);
NTSTATUS NTAPI NtDeleteFile(POBJECT_ATTRIBUTES oa);
NTSTATUS NTAPI NtDeviceIoControlFile(HANDLE file, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io,
                                     ULONG code, PVOID in, ULONG in_size, PVOID out, ULONG out_size);
NTSTATUS NTAPI NtFsControlFile(HANDLE file, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io,
                               ULONG code, PVOID in, ULONG in_size, PVOID out, ULONG out_size);

// реестр
NTSTATUS NTAPI NtCreateKey(PHANDLE key, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, ULONG title_index,
                           PUNICODE_STRING class_name, ULONG options, PULONG disposition);
NTSTATUS NTAPI NtOpenKey(PHANDLE key, ACCESS_MASK access, POBJECT_ATTRIBUTES oa);
NTSTATUS NTAPI NtDeleteKey(HANDLE key);
NTSTATUS NTAPI NtFlushKey(HANDLE key);
NTSTATUS NTAPI NtQueryKey(HANDLE key, KEY_INFORMATION_CLASS what, PVOID info, ULONG size, PULONG returned);
NTSTATUS NTAPI NtEnumerateKey(HANDLE key, ULONG index, KEY_INFORMATION_CLASS what, PVOID info, ULONG size, PULONG returned);
NTSTATUS NTAPI NtQueryValueKey(HANDLE key, PUNICODE_STRING name, KEY_VALUE_INFORMATION_CLASS what, PVOID info,
                               ULONG size, PULONG returned);
NTSTATUS NTAPI NtEnumerateValueKey(HANDLE key, ULONG index, KEY_VALUE_INFORMATION_CLASS what, PVOID info,
                                   ULONG size, PULONG returned);
NTSTATUS NTAPI NtSetValueKey(HANDLE key, PUNICODE_STRING name, ULONG title_index, ULONG type, PVOID data, ULONG size);
NTSTATUS NTAPI NtDeleteValueKey(HANDLE key, PUNICODE_STRING name);

// строки
void NTAPI RtlInitUnicodeString(PUNICODE_STRING s, PCWSTR text);
void NTAPI RtlInitAnsiString(PANSI_STRING s, PCSZ text);
void NTAPI RtlFreeUnicodeString(PUNICODE_STRING s);                 // только то, что выделила сама ntdll
void NTAPI RtlFreeAnsiString(PANSI_STRING s);
NTSTATUS NTAPI RtlAnsiStringToUnicodeString(PUNICODE_STRING to, PCANSI_STRING from, BOOLEAN allocate);
NTSTATUS NTAPI RtlUnicodeStringToAnsiString(PANSI_STRING to, PCUNICODE_STRING from, BOOLEAN allocate);
NTSTATUS NTAPI RtlUTF8ToUnicodeN(PWSTR to, ULONG to_bytes, PULONG written, const CHAR *from, ULONG from_bytes);
NTSTATUS NTAPI RtlUnicodeToUTF8N(PCHAR to, ULONG to_bytes, PULONG written, const WCHAR *from, ULONG from_bytes);
LONG NTAPI RtlCompareUnicodeString(PCUNICODE_STRING a, PCUNICODE_STRING b, BOOLEAN case_insensitive);
BOOLEAN NTAPI RtlEqualUnicodeString(PCUNICODE_STRING a, PCUNICODE_STRING b, BOOLEAN case_insensitive);
// C:\путь -> \??\C:\путь; nt.Buffer потом освободить RtlFreeUnicodeString
BOOLEAN NTAPI RtlDosPathNameToNtPathName_U(PCWSTR dos, PUNICODE_STRING nt, PWSTR *file_part, PVOID relative);

// ошибки, версия, процесс
ULONG NTAPI RtlNtStatusToDosError(NTSTATUS status);                 // код для GetLastError и FormatMessage
NTSTATUS NTAPI RtlGetLastNtStatus(void);
NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW info);             // настоящая версия, без поправок совместимости
void NTAPI RtlGetNtVersionNumbers(PULONG major, PULONG minor, PULONG build);   // в build верхние биты — признаки
PPEB NTAPI RtlGetCurrentPeb(void);
NTSTATUS NTAPI RtlAdjustPrivilege(ULONG privilege, BOOLEAN enable, BOOLEAN current_thread, PBOOLEAN was_enabled);
__declspec(noreturn) void NTAPI RtlExitUserProcess(NTSTATUS exit_status);

// кучи, числа
PVOID NTAPI RtlCreateHeap(ULONG flags, PVOID base, SIZE_T reserve, SIZE_T commit, PVOID lock, PVOID parameters);
PVOID NTAPI RtlDestroyHeap(PVOID heap);
PVOID NTAPI RtlAllocateHeap(PVOID heap, ULONG flags, SIZE_T size);
BOOLEAN NTAPI RtlFreeHeap(PVOID heap, ULONG flags, PVOID p);
ULONG NTAPI RtlRandomEx(PULONG seed);                               // не для ключей шифрования
ULONG NTAPI RtlComputeCrc32(ULONG initial, const void *data, ULONG size);

// загрузчик
NTSTATUS NTAPI LdrLoadDll(PWSTR search_path, PULONG flags, PUNICODE_STRING name, PVOID *module);
NTSTATUS NTAPI LdrUnloadDll(PVOID module);
NTSTATUS NTAPI LdrGetDllHandle(PWSTR search_path, PULONG flags, PUNICODE_STRING name, PVOID *module);
NTSTATUS NTAPI LdrGetProcedureAddress(PVOID module, PANSI_STRING name, ULONG ordinal, PVOID *address);

// отладчик
ULONG DbgPrint(PCSTR format, ...);                                  // в отладчик (DebugView), а не в консоль
void NTAPI DbgBreakPoint(void);
