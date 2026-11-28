// The structures of ntdll.h (and what it needs from windows.h): built by MSVC with winternl.h
// and ntstatus.h and by WinCC with its own ntdll.h, the two lists must be the same (abi.ps1).
// Only the fields winternl.h names are compared; the rest of ntdll.h's structures are checked
// on a live process by tests\win_ntdll.c.
#ifdef __WINCC__
#include <ntdll.h>
#else
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <winternl.h>
#endif
#include <stdio.h>
#include <stddef.h>

#define S(t) printf("%-52s %4d\n", #t, (int)sizeof(t))
#define O(t, m) printf("%-52s %4d\n", #t "." #m, (int)offsetof(t, m))
#define V(e) printf("%-52s %lld\n", #e, (long long)(e))

int main(void)
{
    // windows.h: what ntdll.h needs from it
    S(LIST_ENTRY); O(LIST_ENTRY, Blink);
    S(NT_TIB); O(NT_TIB, StackBase); O(NT_TIB, StackLimit); O(NT_TIB, ArbitraryUserPointer); O(NT_TIB, Self);
    S(MEMORY_BASIC_INFORMATION); O(MEMORY_BASIC_INFORMATION, AllocationBase); O(MEMORY_BASIC_INFORMATION, AllocationProtect);
    O(MEMORY_BASIC_INFORMATION, PartitionId); O(MEMORY_BASIC_INFORMATION, RegionSize); O(MEMORY_BASIC_INFORMATION, State);
    O(MEMORY_BASIC_INFORMATION, Protect); O(MEMORY_BASIC_INFORMATION, Type);
    S(CCHAR);

    // winternl.h
    S(UNICODE_STRING); O(UNICODE_STRING, MaximumLength); O(UNICODE_STRING, Buffer);
    S(STRING); S(ANSI_STRING);
    S(OBJECT_ATTRIBUTES); O(OBJECT_ATTRIBUTES, RootDirectory); O(OBJECT_ATTRIBUTES, ObjectName);
    O(OBJECT_ATTRIBUTES, Attributes); O(OBJECT_ATTRIBUTES, SecurityDescriptor); O(OBJECT_ATTRIBUTES, SecurityQualityOfService);
    S(IO_STATUS_BLOCK); O(IO_STATUS_BLOCK, Information);
    S(CLIENT_ID); O(CLIENT_ID, UniqueThread);
    O(PEB, BeingDebugged); O(PEB, Ldr); O(PEB, ProcessParameters); O(PEB, AtlThunkSListPtr); O(PEB, AtlThunkSListPtr32);
    O(PEB, PostProcessInitRoutine); O(PEB, SessionId);
    O(TEB, ProcessEnvironmentBlock);
    O(PEB_LDR_DATA, InMemoryOrderModuleList);           // winternl.h cuts it short: no sizeof
    O(LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks); O(LDR_DATA_TABLE_ENTRY, DllBase); O(LDR_DATA_TABLE_ENTRY, FullDllName);
    O(LDR_DATA_TABLE_ENTRY, TimeDateStamp);
    O(RTL_USER_PROCESS_PARAMETERS, ImagePathName); O(RTL_USER_PROCESS_PARAMETERS, CommandLine);
    S(PROCESS_BASIC_INFORMATION); O(PROCESS_BASIC_INFORMATION, PebBaseAddress); O(PROCESS_BASIC_INFORMATION, UniqueProcessId);
    S(SYSTEM_BASIC_INFORMATION); O(SYSTEM_BASIC_INFORMATION, NumberOfProcessors);
    S(SYSTEM_TIMEOFDAY_INFORMATION);
    S(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION); O(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, IdleTime);
    O(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, KernelTime); O(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION, UserTime);
    S(SYSTEM_THREAD_INFORMATION); O(SYSTEM_THREAD_INFORMATION, StartAddress); O(SYSTEM_THREAD_INFORMATION, ClientId);
    O(SYSTEM_THREAD_INFORMATION, Priority); O(SYSTEM_THREAD_INFORMATION, BasePriority);
    O(SYSTEM_THREAD_INFORMATION, ThreadState); O(SYSTEM_THREAD_INFORMATION, WaitReason);
    O(SYSTEM_PROCESS_INFORMATION, NumberOfThreads); O(SYSTEM_PROCESS_INFORMATION, ImageName);
    O(SYSTEM_PROCESS_INFORMATION, BasePriority); O(SYSTEM_PROCESS_INFORMATION, UniqueProcessId);
    O(SYSTEM_PROCESS_INFORMATION, HandleCount); O(SYSTEM_PROCESS_INFORMATION, SessionId);
    O(SYSTEM_PROCESS_INFORMATION, PeakVirtualSize); O(SYSTEM_PROCESS_INFORMATION, VirtualSize);
    O(SYSTEM_PROCESS_INFORMATION, PeakWorkingSetSize); O(SYSTEM_PROCESS_INFORMATION, WorkingSetSize);
    O(SYSTEM_PROCESS_INFORMATION, QuotaPagedPoolUsage); O(SYSTEM_PROCESS_INFORMATION, QuotaNonPagedPoolUsage);
    O(SYSTEM_PROCESS_INFORMATION, PagefileUsage); O(SYSTEM_PROCESS_INFORMATION, PeakPagefileUsage);
    O(SYSTEM_PROCESS_INFORMATION, PrivatePageCount);

    V(SystemBasicInformation); V(SystemPerformanceInformation); V(SystemTimeOfDayInformation); V(SystemProcessInformation);
    V(SystemProcessorPerformanceInformation); V(SystemInterruptInformation); V(SystemExceptionInformation);
    V(SystemRegistryQuotaInformation); V(SystemLookasideInformation); V(SystemCodeIntegrityInformation); V(SystemPolicyInformation);
    V(ProcessBasicInformation); V(ProcessDebugPort); V(ProcessWow64Information); V(ProcessImageFileName);
    V(ProcessBreakOnTermination); V(ThreadIsIoPending); V(ThreadNameInformation);
    V(ObjectBasicInformation); V(ObjectTypeInformation); V(FileDirectoryInformation);
    return 0;
}
