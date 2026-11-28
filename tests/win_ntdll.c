// ntdll.h on a live process: every structure Microsoft does not describe (PEB, TEB, the loader's
// modules, the process parameters, the file, registry and process lists) is read through ntdll and
// compared with what kernel32 says; every group of functions is called once. Returns the failures.
// The layout of what winternl.h does describe is compared with MSVC by tests\winabi\abi.ps1.
#include <ntdll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int g_bad;
#define CHECK(c) do { if (!(c)) { printf("line %d: %s\n", __LINE__, #c); g_bad++; } } while (0)
#define OK(st) CHECK(NT_SUCCESS(st))

static int us_eq(const UNICODE_STRING *u, const wchar_t *s)
{
    size_t n = wcslen(s);
    return u->Length == n * 2 && !_wcsnicmp(u->Buffer, s, n);
}

static int us_ends(const UNICODE_STRING *u, const wchar_t *s)
{
    size_t n = wcslen(s), m = u->Length / 2;
    return m >= n && !_wcsnicmp(u->Buffer + m - n, s, n);
}

static void blocks(void)
{
    PPEB peb = NtCurrentPeb();
    PTEB teb = NtCurrentTeb();
    CHECK(peb == RtlGetCurrentPeb());
    CHECK(peb == teb->ProcessEnvironmentBlock);
    CHECK(peb->ImageBaseAddress == GetModuleHandleW(0));
    CHECK(peb->ProcessHeap == GetProcessHeap());
    CHECK(peb->BeingDebugged == (IsDebuggerPresent() ? 1 : 0));
    CHECK(peb->ImageSubsystem == 3);                    // a console program
    RTL_OSVERSIONINFOW v = { sizeof(v) };
    OK(RtlGetVersion(&v));
    CHECK(v.dwMajorVersion >= 10);
    CHECK(peb->OSMajorVersion == v.dwMajorVersion && peb->OSMinorVersion == v.dwMinorVersion);
    CHECK(peb->OSBuildNumber == (USHORT)v.dwBuildNumber && peb->OSPlatformId == 2);
    ULONG ma, mi, bu;
    RtlGetNtVersionNumbers(&ma, &mi, &bu);
    CHECK(ma == v.dwMajorVersion && mi == v.dwMinorVersion && (bu & 0xFFFF) == v.dwBuildNumber);
    SYSTEM_BASIC_INFORMATION sbi;
    OK(NtQuerySystemInformation(SystemBasicInformation, &sbi, sizeof(sbi), 0));
    CHECK(sbi.PageSize == 4096 && sbi.AllocationGranularity == 65536);
    CHECK(peb->NumberOfProcessors == (ULONG)sbi.NumberOfProcessors);

    CHECK(teb->NtTib.Self == &teb->NtTib);
    CHECK((ULONG_PTR)teb->ClientId.UniqueProcess == GetCurrentProcessId());
    CHECK((ULONG_PTR)teb->ClientId.UniqueThread == GetCurrentThreadId());
    int local = 0;
    CHECK((char *)&local < (char *)teb->NtTib.StackBase && (char *)&local > (char *)teb->NtTib.StackLimit);
    SetLastError(1234);
    CHECK(teb->LastErrorValue == 1234);

    // the loader's modules: the program first, ntdll among them
    PPEB_LDR_DATA ldr = peb->Ldr;
    CHECK(ldr->Initialized);
    PLDR_DATA_TABLE_ENTRY first = (PLDR_DATA_TABLE_ENTRY)ldr->InLoadOrderModuleList.Flink;
    CHECK(first->DllBase == peb->ImageBaseAddress);
    int found = 0, count = 0;
    for (PLIST_ENTRY e = ldr->InLoadOrderModuleList.Flink; e != &ldr->InLoadOrderModuleList && count < 500; e = e->Flink, count++) {
        PLDR_DATA_TABLE_ENTRY m = (PLDR_DATA_TABLE_ENTRY)e;
        if (us_eq(&m->BaseDllName, L"ntdll.dll")) {
            found = 1;
            CHECK(m->DllBase == GetModuleHandleW(L"ntdll.dll"));
            CHECK(us_ends(&m->FullDllName, L"\\system32\\ntdll.dll"));
            CHECK(m->SizeOfImage > 0x100000 && m->TimeDateStamp != 0);
        }
    }
    CHECK(found);
    // the same list in memory order: the entry starts one LIST_ENTRY before its link
    PLDR_DATA_TABLE_ENTRY mo = (PLDR_DATA_TABLE_ENTRY)((char *)ldr->InMemoryOrderModuleList.Flink - sizeof(LIST_ENTRY));
    CHECK(mo->DllBase == peb->ImageBaseAddress);

    // what the process was started with
    PRTL_USER_PROCESS_PARAMETERS pp = peb->ProcessParameters;
    CHECK(us_eq(&pp->CommandLine, GetCommandLineW()));
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(0, path, MAX_PATH);
    CHECK(us_eq(&pp->ImagePathName, path));
    GetCurrentDirectoryW(MAX_PATH, path);
    CHECK(pp->CurrentDirectory.DosPath.Length >= wcslen(path) * 2 && !_wcsnicmp(pp->CurrentDirectory.DosPath.Buffer, path, wcslen(path)));
    CHECK(pp->StandardOutput == GetStdHandle(STD_OUTPUT_HANDLE));
    const wchar_t *env = pp->Environment;
    size_t len = 0;
    while (env[len] || env[len + 1]) len++;
    CHECK(pp->EnvironmentSize >= (len + 2) * 2 && pp->EnvironmentSize < (len + 2) * 2 + 8192);
}

static void process_and_threads(void)
{
    DWORD pid = GetCurrentProcessId(), tid = GetCurrentThreadId();
    PROCESS_BASIC_INFORMATION pbi;
    ULONG got = 0;
    OK(NtQueryInformationProcess(NtCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), &got));
    CHECK(got == sizeof(pbi) && pbi.PebBaseAddress == NtCurrentPeb() && pbi.UniqueProcessId == pid);
    CHECK(pbi.ExitStatus == STATUS_PENDING);
    ULONG handles = 0;
    OK(NtQueryInformationProcess(NtCurrentProcess(), ProcessHandleCount, &handles, sizeof(handles), 0));
    CHECK(handles > 0);
    KERNEL_USER_TIMES kt;
    OK(NtQueryInformationProcess(NtCurrentProcess(), ProcessTimes, &kt, sizeof(kt), 0));
    CHECK(kt.CreateTime.QuadPart > 0 && kt.ExitTime.QuadPart == 0);
    char buf[2048];
    OK(NtQueryInformationProcess(NtCurrentProcess(), ProcessImageFileNameWin32, buf, sizeof(buf), 0));
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(0, path, MAX_PATH);
    CHECK(us_eq((PUNICODE_STRING)buf, path));
    OK(NtQueryInformationProcess(NtCurrentProcess(), ProcessCommandLineInformation, buf, sizeof(buf), 0));
    CHECK(us_eq((PUNICODE_STRING)buf, GetCommandLineW()));

    THREAD_BASIC_INFORMATION tbi;
    OK(NtQueryInformationThread(NtCurrentThread(), ThreadBasicInformation, &tbi, sizeof(tbi), 0));
    CHECK(tbi.TebBaseAddress == NtCurrentTeb() && (ULONG_PTR)tbi.ClientId.UniqueThread == tid);

    // open itself by number
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, 0, 0, 0, 0);
    CLIENT_ID cid = { (HANDLE)(ULONG_PTR)pid, 0 };
    OK(NtOpenProcess(&h, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid));
    NtClose(h);
    CLIENT_ID tcid = { 0, (HANDLE)(ULONG_PTR)tid };
    OK(NtOpenThread(&h, THREAD_QUERY_LIMITED_INFORMATION, &oa, &tcid));
    NtClose(h);
    OK(NtOpenProcessToken(NtCurrentProcess(), 8 /* TOKEN_QUERY */, &h));
    NtClose(h);
    BOOLEAN was;
    OK(RtlAdjustPrivilege(SE_CHANGE_NOTIFY_PRIVILEGE, TRUE, FALSE, &was));

    // all processes: this one with this thread
    ULONG size = 1 << 16;
    char *list = 0;
    NTSTATUS st;
    do {
        free(list);
        size *= 2;
        list = malloc(size);
        st = NtQuerySystemInformation(SystemProcessInformation, list, size, 0);
    } while (st == STATUS_INFO_LENGTH_MISMATCH);
    OK(st);
    int seen = 0, procs = 0;
    for (char *p = list; ; procs++) {
        PSYSTEM_PROCESS_INFORMATION spi = (PSYSTEM_PROCESS_INFORMATION)p;
        if ((ULONG_PTR)spi->UniqueProcessId == pid) {
            seen = 1;
            CHECK(us_ends(&spi->ImageName, wcsrchr(path, L'\\') + 1));
            CHECK(spi->NumberOfThreads >= 1 && spi->HandleCount > 0 && spi->WorkingSetSize > 0);
            CHECK(spi->CreateTime.QuadPart == kt.CreateTime.QuadPart);
            int mine = 0;
            for (ULONG i = 0; i < spi->NumberOfThreads; i++) {
                CHECK((ULONG_PTR)spi->Threads[i].ClientId.UniqueProcess == pid);
                if ((ULONG_PTR)spi->Threads[i].ClientId.UniqueThread == tid) mine = 1;
            }
            CHECK(mine);
        }
        if (!spi->NextEntryOffset) break;
        p += spi->NextEntryOffset;
    }
    CHECK(seen && procs > 5);
    free(list);

    // the time of day and every processor's times
    SYSTEM_TIMEOFDAY_INFORMATION tod;
    LARGE_INTEGER now;
    OK(NtQuerySystemInformation(SystemTimeOfDayInformation, &tod, sizeof(tod), 0));
    OK(NtQuerySystemTime(&now));
    CHECK(tod.BootTime.QuadPart < tod.CurrentTime.QuadPart && now.QuadPart - tod.CurrentTime.QuadPart < 50000000);
    SYSTEM_BASIC_INFORMATION sbi;
    NtQuerySystemInformation(SystemBasicInformation, &sbi, sizeof(sbi), 0);
    SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION cpu[256];
    got = 0;
    OK(NtQuerySystemInformation(SystemProcessorPerformanceInformation, cpu, sizeof(cpu), &got));
    CHECK(got == sbi.NumberOfProcessors * sizeof(cpu[0]));
    CHECK(cpu[0].KernelTime.QuadPart >= cpu[0].IdleTime.QuadPart);
}

static volatile LONG g_ran, g_apc;

static NTSTATUS NTAPI worker(PVOID arg)
{
    g_ran = (LONG)(ULONG_PTR)arg;
    return 77;
}

static void NTAPI apc(PVOID a, PVOID b, PVOID c)
{
    (void)b; (void)c;
    g_apc = (LONG)(ULONG_PTR)a;
}

static void threads_and_sync(void)
{
    HANDLE t;
    OK(NtCreateThreadEx(&t, THREAD_ALL_ACCESS, 0, NtCurrentProcess(), worker, (PVOID)5,
                        THREAD_CREATE_FLAGS_CREATE_SUSPENDED, 0, 0, 0, 0));
    CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL;
    OK(NtGetContextThread(t, &ctx));
    CHECK(ctx.Rip != 0 && ctx.Rsp != 0);
    CHECK(g_ran == 0);
    ULONG prev = 9;
    OK(NtResumeThread(t, &prev));
    CHECK(prev == 1);
    LARGE_INTEGER five_s = { .QuadPart = -50000000LL };
    CHECK(NtWaitForSingleObject(t, FALSE, &five_s) == STATUS_SUCCESS);
    CHECK(g_ran == 5);
    THREAD_BASIC_INFORMATION tbi;
    OK(NtQueryInformationThread(t, ThreadBasicInformation, &tbi, sizeof(tbi), 0));
    CHECK(tbi.ExitStatus == 77);
    NtClose(t);

    OK(NtQueueApcThread(NtCurrentThread(), apc, (PVOID)9, 0, 0));
    OK(NtTestAlert());
    CHECK(g_apc == 9);

    HANDLE ev, mu, se;
    LARGE_INTEGER ms = { .QuadPart = -10000 };
    OK(NtCreateEvent(&ev, EVENT_ALL_ACCESS, 0, NotificationEvent, FALSE));
    CHECK(NtWaitForSingleObject(ev, FALSE, &ms) == STATUS_TIMEOUT);
    OK(NtSetEvent(ev, 0));
    CHECK(NtWaitForSingleObject(ev, FALSE, &ms) == STATUS_SUCCESS);
    LONG was = -1;
    OK(NtResetEvent(ev, &was));
    CHECK(was == 1);
    OK(NtCreateMutant(&mu, MUTANT_ALL_ACCESS, 0, TRUE));
    OK(NtReleaseMutant(mu, &was));
    CHECK(was == 0);
    OK(NtCreateSemaphore(&se, SEMAPHORE_ALL_ACCESS, 0, 0, 2));
    OK(NtReleaseSemaphore(se, 1, &was));
    CHECK(was == 0);
    HANDLE two[2] = { ev, se };
    CHECK(NtWaitForMultipleObjects(2, two, WaitAny, FALSE, &ms) == STATUS_WAIT_1);

    HANDLE dup;
    OK(NtDuplicateObject(NtCurrentProcess(), ev, NtCurrentProcess(), &dup, 0, 0, 2 /* DUPLICATE_SAME_ACCESS */));
    OBJECT_BASIC_INFORMATION obi;
    OK(NtQueryObject(dup, ObjectBasicInformation, &obi, sizeof(obi), 0));
    CHECK(obi.HandleCount >= 2 && obi.GrantedAccess == EVENT_ALL_ACCESS);
    char type[512];
    OK(NtQueryObject(ev, ObjectTypeInformation, type, sizeof(type), 0));
    CHECK(us_eq((PUNICODE_STRING)type, L"Event"));
    NtClose(dup); NtClose(ev); NtClose(mu); NtClose(se);

    ULONG mx, mn, cur;
    OK(NtQueryTimerResolution(&mx, &mn, &cur));
    CHECK(mx >= cur && cur >= mn && mn > 0);
    LARGE_INTEGER c, f;
    OK(NtQueryPerformanceCounter(&c, &f));
    CHECK(f.QuadPart > 0 && c.QuadPart > 0);
    OK(NtDelayExecution(FALSE, &ms));
    OK(NtYieldExecution());
}

static void memory(void)
{
    PVOID base = 0;
    SIZE_T size = 65536;
    OK(NtAllocateVirtualMemory(NtCurrentProcess(), &base, 0, &size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    CHECK(base && size == 65536);
    memset(base, 0x5A, 100);
    PVOID at = base;
    SIZE_T psize = 4096;
    ULONG old = 0;
    OK(NtProtectVirtualMemory(NtCurrentProcess(), &at, &psize, PAGE_READONLY, &old));
    CHECK(old == PAGE_READWRITE);
    MEMORY_BASIC_INFORMATION mbi, k32;
    SIZE_T got = 0;
    OK(NtQueryVirtualMemory(NtCurrentProcess(), base, MemoryBasicInformation, &mbi, sizeof(mbi), &got));
    CHECK(got == sizeof(mbi) && VirtualQuery(base, &k32, sizeof(k32)) == sizeof(k32));
    CHECK(mbi.BaseAddress == base && mbi.AllocationBase == base && mbi.RegionSize == 4096);
    CHECK(mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READONLY && mbi.Type == MEM_PRIVATE);
    CHECK(mbi.AllocationProtect == PAGE_READWRITE && !memcmp(&mbi, &k32, sizeof(mbi)));
    char back[100];
    SIZE_T n = 0;
    OK(NtReadVirtualMemory(NtCurrentProcess(), base, back, 100, &n));
    CHECK(n == 100 && back[99] == 0x5A);
    char other = 7;
    OK(NtWriteVirtualMemory(NtCurrentProcess(), (char *)base + 8192, &other, 1, &n));
    CHECK(((char *)base)[8192] == 7);
    OK(NtFlushInstructionCache(NtCurrentProcess(), base, 4096));
    size = 0;
    OK(NtFreeVirtualMemory(NtCurrentProcess(), &base, &size, MEM_RELEASE));
    OK(NtQueryVirtualMemory(NtCurrentProcess(), base, MemoryBasicInformation, &mbi, sizeof(mbi), 0));
    CHECK(mbi.State == MEM_FREE);

    // a section of the paging file seen through two views
    HANDLE sec;
    LARGE_INTEGER max = { .QuadPart = 4096 };
    OK(NtCreateSection(&sec, SECTION_ALL_ACCESS, 0, &max, PAGE_READWRITE, SEC_COMMIT, 0));
    PVOID v1 = 0, v2 = 0;
    SIZE_T s1 = 0, s2 = 0;
    OK(NtMapViewOfSection(sec, NtCurrentProcess(), &v1, 0, 0, 0, &s1, ViewUnmap, 0, PAGE_READWRITE));
    OK(NtMapViewOfSection(sec, NtCurrentProcess(), &v2, 0, 0, 0, &s2, ViewUnmap, 0, PAGE_READWRITE));
    CHECK(v1 != v2 && s1 >= 4096);
    strcpy(v1, "shared");
    CHECK(!strcmp(v2, "shared"));
    OK(NtUnmapViewOfSection(NtCurrentProcess(), v1));
    OK(NtUnmapViewOfSection(NtCurrentProcess(), v2));
    NtClose(sec);
}

static void files(void)
{
    wchar_t dir[MAX_PATH], dos[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    swprintf(dos, MAX_PATH, L"%lswincc_ntdll_%lu.txt", dir, GetCurrentProcessId());
    UNICODE_STRING nt;
    CHECK(RtlDosPathNameToNtPathName_U(dos, &nt, 0, 0));
    CHECK(nt.Buffer && !wcsncmp(nt.Buffer, L"\\??\\", 4));
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &nt, OBJ_CASE_INSENSITIVE, 0, 0);
    IO_STATUS_BLOCK io;
    HANDLE h;
    OK(NtCreateFile(&h, FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &oa, &io, 0, FILE_ATTRIBUTE_NORMAL, 0,
                    FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, 0, 0));
    CHECK(io.Information == FILE_CREATED);
    char text[] = "hello ntdll";
    OK(NtWriteFile(h, 0, 0, 0, &io, text, 11, 0, 0));
    CHECK(io.Information == 11);
    OK(NtFlushBuffersFile(h, &io));
    FILE_STANDARD_INFORMATION fsi;
    OK(NtQueryInformationFile(h, &io, &fsi, sizeof(fsi), FileStandardInformation));
    CHECK(fsi.EndOfFile.QuadPart == 11 && !fsi.Directory && fsi.NumberOfLinks == 1 && !fsi.DeletePending);
    FILE_POSITION_INFORMATION pos;
    OK(NtQueryInformationFile(h, &io, &pos, sizeof(pos), FilePositionInformation));
    CHECK(pos.CurrentByteOffset.QuadPart == 11);
    pos.CurrentByteOffset.QuadPart = 6;
    OK(NtSetInformationFile(h, &io, &pos, sizeof(pos), FilePositionInformation));
    char back[16] = { 0 };
    OK(NtReadFile(h, 0, 0, 0, &io, back, sizeof(back), 0, 0));
    CHECK(io.Information == 5 && !strcmp(back, "ntdll"));
    LARGE_INTEGER at0 = { .QuadPart = 0 };
    CHECK(NtReadFile(h, 0, 0, 0, &io, back, 5, &at0, 0) == STATUS_SUCCESS && !memcmp(back, "hello", 5));
    FILE_END_OF_FILE_INFORMATION eof = { { .QuadPart = 5 } };
    OK(NtSetInformationFile(h, &io, &eof, sizeof(eof), FileEndOfFileInformation));
    FILE_BASIC_INFORMATION fbi;
    OK(NtQueryInformationFile(h, &io, &fbi, sizeof(fbi), FileBasicInformation));
    CHECK(fbi.CreationTime.QuadPart > 0 && fbi.LastWriteTime.QuadPart >= fbi.CreationTime.QuadPart);
    CHECK(fbi.FileAttributes & (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NORMAL));
    char name[600];
    OK(NtQueryInformationFile(h, &io, name, sizeof(name), FileNameInformation));
    PFILE_NAME_INFORMATION fni = (PFILE_NAME_INFORMATION)name;
    CHECK(fni->FileNameLength > 20 && fni->FileName[0] == L'\\');
    FILE_FS_SIZE_INFORMATION fs;
    OK(NtQueryVolumeInformationFile(h, &io, &fs, sizeof(fs), FileFsSizeInformation));
    CHECK(fs.BytesPerSector >= 512 && fs.TotalAllocationUnits.QuadPart > fs.AvailableAllocationUnits.QuadPart);
    NtClose(h);

    FILE_BASIC_INFORMATION qa;
    OK(NtQueryAttributesFile(&oa, &qa));
    CHECK(qa.LastWriteTime.QuadPart == fbi.LastWriteTime.QuadPart);
    FILE_NETWORK_OPEN_INFORMATION noi;
    OK(NtQueryFullAttributesFile(&oa, &noi));
    CHECK(noi.EndOfFile.QuadPart == 5 && noi.CreationTime.QuadPart == fbi.CreationTime.QuadPart);

    // the folder: find the file by its name
    wchar_t *slash = wcsrchr(nt.Buffer, L'\\');
    UNICODE_STRING dname = { (USHORT)((slash - nt.Buffer) * 2), (USHORT)((slash - nt.Buffer) * 2), nt.Buffer };
    UNICODE_STRING mask;
    RtlInitUnicodeString(&mask, slash + 1);
    OBJECT_ATTRIBUTES doa;
    InitializeObjectAttributes(&doa, &dname, OBJ_CASE_INSENSITIVE, 0, 0);
    HANDLE d;
    OK(NtOpenFile(&d, FILE_LIST_DIRECTORY | SYNCHRONIZE, &doa, &io, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT));
    char entries[4096];
    OK(NtQueryDirectoryFile(d, 0, 0, 0, &io, entries, sizeof(entries), FileDirectoryInformation, TRUE, &mask, TRUE));
    PFILE_DIRECTORY_INFORMATION fdi = (PFILE_DIRECTORY_INFORMATION)entries;
    CHECK(fdi->FileNameLength == mask.Length && !_wcsnicmp(fdi->FileName, mask.Buffer, mask.Length / 2));
    CHECK(fdi->EndOfFile.QuadPart == 5 && fdi->CreationTime.QuadPart == fbi.CreationTime.QuadPart);
    CHECK(NtQueryDirectoryFile(d, 0, 0, 0, &io, entries, sizeof(entries), FileNamesInformation, TRUE, &mask, TRUE) == STATUS_SUCCESS);
    CHECK(((PFILE_NAMES_INFORMATION)entries)->FileNameLength == mask.Length);
    NtClose(d);

    // delete it two ways: by name, and a second file by FileDispositionInformation
    OK(NtDeleteFile(&oa));
    CHECK(NtQueryAttributesFile(&oa, &qa) == STATUS_OBJECT_NAME_NOT_FOUND);
    OK(NtCreateFile(&h, DELETE | SYNCHRONIZE, &oa, &io, 0, 0, 0, FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT, 0, 0));
    FILE_DISPOSITION_INFORMATION del = { TRUE };
    OK(NtSetInformationFile(h, &io, &del, sizeof(del), FileDispositionInformation));
    NtClose(h);
    CHECK(NtQueryAttributesFile(&oa, &qa) == STATUS_OBJECT_NAME_NOT_FOUND);
    RtlFreeUnicodeString(&nt);
}

static void registry(void)
{
    // what kernel32 reads from the same key
    HANDLE key;
    UNICODE_STRING kname, vname;
    OBJECT_ATTRIBUTES oa;
    RtlInitUnicodeString(&kname, L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    InitializeObjectAttributes(&oa, &kname, OBJ_CASE_INSENSITIVE, 0, 0);
    OK(NtOpenKey(&key, KEY_READ, &oa));
    RtlInitUnicodeString(&vname, L"CurrentBuildNumber");
    char buf[512];
    ULONG got = 0;
    OK(NtQueryValueKey(key, &vname, KeyValuePartialInformation, buf, sizeof(buf), &got));
    PKEY_VALUE_PARTIAL_INFORMATION kv = (PKEY_VALUE_PARTIAL_INFORMATION)buf;
    RTL_OSVERSIONINFOW v = { sizeof(v) };
    RtlGetVersion(&v);
    CHECK(kv->Type == REG_SZ && (ULONG)wcstoul((wchar_t *)kv->Data, 0, 10) == v.dwBuildNumber);
    NtClose(key);

    // a key of its own under HKCU\Software, volatile: gone even if the test stops halfway
    HKEY sw;
    CHECK(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_ALL_ACCESS, &sw) == 0);
    RtlInitUnicodeString(&kname, L"WinCC ntdll test");
    InitializeObjectAttributes(&oa, &kname, OBJ_CASE_INSENSITIVE, (HANDLE)sw, 0);
    ULONG disp = 0;
    OK(NtCreateKey(&key, KEY_ALL_ACCESS, &oa, 0, 0, REG_OPTION_VOLATILE, &disp));
    CHECK(disp == REG_CREATED_NEW_KEY);
    RtlInitUnicodeString(&vname, L"Number");
    DWORD num = 0x12345678;
    OK(NtSetValueKey(key, &vname, 0, REG_DWORD, &num, sizeof(num)));
    OK(NtQueryValueKey(key, &vname, KeyValueFullInformation, buf, sizeof(buf), &got));
    PKEY_VALUE_FULL_INFORMATION full = (PKEY_VALUE_FULL_INFORMATION)buf;
    CHECK(full->Type == REG_DWORD && full->DataLength == 4 && full->NameLength == 12);
    CHECK(*(DWORD *)(buf + full->DataOffset) == num);
    OK(NtEnumerateValueKey(key, 0, KeyValueBasicInformation, buf, sizeof(buf), &got));
    PKEY_VALUE_BASIC_INFORMATION basic = (PKEY_VALUE_BASIC_INFORMATION)buf;
    CHECK(basic->Type == REG_DWORD && basic->NameLength == 12 && !wcsncmp(basic->Name, L"Number", 6));
    CHECK(NtEnumerateValueKey(key, 1, KeyValueBasicInformation, buf, sizeof(buf), &got) == STATUS_NO_MORE_ENTRIES);
    OK(NtQueryKey(key, KeyBasicInformation, buf, sizeof(buf), &got));
    PKEY_BASIC_INFORMATION kb = (PKEY_BASIC_INFORMATION)buf;
    CHECK(kb->NameLength == kname.Length && !wcsncmp(kb->Name, L"WinCC ntdll test", 16) && kb->LastWriteTime.QuadPart > 0);
    int seen = 0;
    for (ULONG i = 0; NT_SUCCESS(NtEnumerateKey((HANDLE)sw, i, KeyBasicInformation, buf, sizeof(buf), &got)); i++)
        if (kb->NameLength == kname.Length && !_wcsnicmp(kb->Name, L"WinCC ntdll test", 16)) seen = 1;
    CHECK(seen);
    OK(NtDeleteValueKey(key, &vname));
    CHECK(NtQueryValueKey(key, &vname, KeyValuePartialInformation, buf, sizeof(buf), &got) == STATUS_OBJECT_NAME_NOT_FOUND);
    OK(NtDeleteKey(key));
    NtClose(key);
    RegCloseKey(sw);
}

static void strings_and_rest(void)
{
    UNICODE_STRING u, u2;
    RtlInitUnicodeString(&u, L"Hello");
    CHECK(u.Length == 10 && u.MaximumLength == 12);
    UNICODE_STRING c = RTL_CONSTANT_STRING(L"hello");
    CHECK(c.Length == 10 && RtlCompareUnicodeString(&u, &c, TRUE) == 0 && RtlCompareUnicodeString(&u, &c, FALSE) != 0);
    CHECK(RtlEqualUnicodeString(&u, &c, TRUE) && !RtlEqualUnicodeString(&u, &c, FALSE));
    ANSI_STRING a, a2;
    RtlInitAnsiString(&a, "Hello");
    CHECK(a.Length == 5);
    OK(RtlAnsiStringToUnicodeString(&u2, &a, TRUE));
    CHECK(RtlEqualUnicodeString(&u, &u2, FALSE));
    RtlFreeUnicodeString(&u2);
    OK(RtlUnicodeStringToAnsiString(&a2, &u, TRUE));
    CHECK(a2.Length == 5 && !memcmp(a2.Buffer, "Hello", 5));
    RtlFreeAnsiString(&a2);
    const char *utf8 = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";      // «Привет»
    wchar_t w[16];
    ULONG n = 0;
    OK(RtlUTF8ToUnicodeN(w, sizeof(w), &n, utf8, 12));
    CHECK(n == 12 && !wcsncmp(w, L"Привет", 6));
    char back[32];
    OK(RtlUnicodeToUTF8N(back, sizeof(back), &n, w, 12));
    CHECK(n == 12 && !memcmp(back, utf8, 12));

    CHECK(RtlNtStatusToDosError(STATUS_ACCESS_DENIED) == ERROR_ACCESS_DENIED);
    CHECK(RtlNtStatusToDosError(STATUS_OBJECT_NAME_NOT_FOUND) == 2);
    CHECK(CreateFileW(L"C:\\no such folder\\no such file", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0) == INVALID_HANDLE_VALUE);
    CHECK(RtlGetLastNtStatus() == STATUS_OBJECT_PATH_NOT_FOUND);
    CHECK(NtClose((HANDLE)(LONG_PTR)0x7FFFFFF0) == STATUS_INVALID_HANDLE || IsDebuggerPresent());

    PVOID heap = RtlCreateHeap(2 /* HEAP_GROWABLE */, 0, 0, 0, 0, 0);
    CHECK(heap != 0);
    char *p = RtlAllocateHeap(heap, HEAP_ZERO_MEMORY, 1000);
    CHECK(p && p[999] == 0);
    CHECK(RtlFreeHeap(heap, 0, p));
    CHECK(RtlDestroyHeap(heap) == 0);
    ULONG seed = 1, r1 = RtlRandomEx(&seed), r2 = RtlRandomEx(&seed);
    CHECK(r1 != r2);
    CHECK(RtlComputeCrc32(0, "123456789", 9) == 0xCBF43926u);

    PVOID k32 = 0, fn = 0, ver = 0;
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, L"kernel32.dll");
    OK(LdrGetDllHandle(0, 0, &name, &k32));
    CHECK(k32 == GetModuleHandleW(L"kernel32.dll"));
    ANSI_STRING proc;
    RtlInitAnsiString(&proc, "GetTickCount");
    OK(LdrGetProcedureAddress(k32, &proc, 0, &fn));
    CHECK(fn == (PVOID)GetProcAddress(k32, "GetTickCount"));
    RtlInitUnicodeString(&name, L"version.dll");
    OK(LdrLoadDll(0, 0, &name, &ver));
    CHECK(ver != 0 && ver == GetModuleHandleW(L"version.dll"));
    OK(LdrUnloadDll(ver));
    DbgPrint("WinCC ntdll test: %d\n", 1);
}

int main(void)
{
    blocks();
    process_and_threads();
    threads_and_sync();
    memory();
    files();
    registry();
    strings_and_rest();
    if (g_bad) printf("failed: %d\n", g_bad);
    return g_bad;
}
