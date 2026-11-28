// Exceptions of Windows: the codes, what a __try / __except filter sees
// (EXCEPTION_POINTERS: the record and the processor's registers), raising one,
// and the handlers of the whole program. Part of windows.h.
//
// GetExceptionCode(), GetExceptionInformation() and AbnormalTermination() are
// words of the compiler: they exist only inside __except (...) and __finally.
#pragma once

#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    (-1)

#define EXCEPTION_ACCESS_VIOLATION          0xC0000005
#define EXCEPTION_DATATYPE_MISALIGNMENT     0x80000002
#define EXCEPTION_BREAKPOINT                0x80000003
#define EXCEPTION_SINGLE_STEP               0x80000004
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     0xC000008C
#define EXCEPTION_FLT_DENORMAL_OPERAND      0xC000008D
#define EXCEPTION_FLT_DIVIDE_BY_ZERO        0xC000008E
#define EXCEPTION_FLT_INEXACT_RESULT        0xC000008F
#define EXCEPTION_FLT_INVALID_OPERATION     0xC0000090
#define EXCEPTION_FLT_OVERFLOW              0xC0000091
#define EXCEPTION_FLT_STACK_CHECK           0xC0000092
#define EXCEPTION_FLT_UNDERFLOW             0xC0000093
#define EXCEPTION_INT_DIVIDE_BY_ZERO        0xC0000094
#define EXCEPTION_INT_OVERFLOW              0xC0000095
#define EXCEPTION_PRIV_INSTRUCTION          0xC0000096
#define EXCEPTION_IN_PAGE_ERROR             0xC0000006
#define EXCEPTION_ILLEGAL_INSTRUCTION       0xC000001D
#define EXCEPTION_NONCONTINUABLE_EXCEPTION  0xC0000025
#define EXCEPTION_STACK_OVERFLOW            0xC00000FD
#define EXCEPTION_INVALID_DISPOSITION       0xC0000026
#define EXCEPTION_GUARD_PAGE                0x80000001
#define EXCEPTION_INVALID_HANDLE            0xC0000008
#define STATUS_ACCESS_VIOLATION             EXCEPTION_ACCESS_VIOLATION
#define STATUS_STACK_OVERFLOW               EXCEPTION_STACK_OVERFLOW
#define STATUS_INTEGER_DIVIDE_BY_ZERO       EXCEPTION_INT_DIVIDE_BY_ZERO

#define EXCEPTION_NONCONTINUABLE    0x1
#define EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct _EXCEPTION_RECORD {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    struct _EXCEPTION_RECORD *ExceptionRecord;
    PVOID ExceptionAddress;
    DWORD NumberParameters;
    ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

typedef struct _M128A {
    ULONGLONG Low;
    LONGLONG High;
} M128A, *PM128A;

typedef struct _XSAVE_FORMAT {
    WORD ControlWord;
    WORD StatusWord;
    BYTE TagWord;
    BYTE Reserved1;
    WORD ErrorOpcode;
    DWORD ErrorOffset;
    WORD ErrorSelector;
    WORD Reserved2;
    DWORD DataOffset;
    WORD DataSelector;
    WORD Reserved3;
    DWORD MxCsr;
    DWORD MxCsr_Mask;
    M128A FloatRegisters[8];
    M128A XmmRegisters[16];
    BYTE Reserved4[96];
} XSAVE_FORMAT, XMM_SAVE_AREA32, *PXMM_SAVE_AREA32;

// The registers of the processor (x64), 1232 bytes, aligned to 16
typedef struct _CONTEXT {
    _Alignas(16) DWORD64 P1Home;
    DWORD64 P2Home, P3Home, P4Home, P5Home, P6Home;
    DWORD ContextFlags;
    DWORD MxCsr;
    WORD SegCs, SegDs, SegEs, SegFs, SegGs, SegSs;
    DWORD EFlags;
    DWORD64 Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    DWORD64 Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi;
    DWORD64 R8, R9, R10, R11, R12, R13, R14, R15;
    DWORD64 Rip;
    union {
        XMM_SAVE_AREA32 FltSave;
        struct {
            M128A Header[2];
            M128A Legacy[8];
            M128A Xmm0, Xmm1, Xmm2, Xmm3, Xmm4, Xmm5, Xmm6, Xmm7;
            M128A Xmm8, Xmm9, Xmm10, Xmm11, Xmm12, Xmm13, Xmm14, Xmm15;
        };
    };
    M128A VectorRegister[26];
    DWORD64 VectorControl;
    DWORD64 DebugControl;
    DWORD64 LastBranchToRip, LastBranchFromRip, LastExceptionToRip, LastExceptionFromRip;
} CONTEXT, *PCONTEXT, *LPCONTEXT;

#define CONTEXT_AMD64       0x00100000L
#define CONTEXT_CONTROL     (CONTEXT_AMD64 | 0x1L)
#define CONTEXT_INTEGER     (CONTEXT_AMD64 | 0x2L)
#define CONTEXT_SEGMENTS    (CONTEXT_AMD64 | 0x4L)
#define CONTEXT_FLOATING_POINT (CONTEXT_AMD64 | 0x8L)
#define CONTEXT_FULL        (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)
#define CONTEXT_ALL         (CONTEXT_FULL | CONTEXT_SEGMENTS | CONTEXT_AMD64 | 0x10L)

// the start of a thread's block (TEB): the exception chain, the bounds of the stack, itself at Self
typedef struct _NT_TIB {
    struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList;
    PVOID StackBase;
    PVOID StackLimit;
    PVOID SubSystemTib;
    union { PVOID FiberData; DWORD Version; };
    PVOID ArbitraryUserPointer;
    struct _NT_TIB *Self;
} NT_TIB, *PNT_TIB;

typedef struct _EXCEPTION_POINTERS {
    PEXCEPTION_RECORD ExceptionRecord;
    PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;

typedef LONG (WINAPI *PTOP_LEVEL_EXCEPTION_FILTER)(PEXCEPTION_POINTERS info);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;
typedef LONG (WINAPI *PVECTORED_EXCEPTION_HANDLER)(PEXCEPTION_POINTERS info);

void WINAPI RaiseException(DWORD code, DWORD flags, DWORD nargs, const ULONG_PTR *args);
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter);
LONG WINAPI UnhandledExceptionFilter(PEXCEPTION_POINTERS info);
PVOID WINAPI AddVectoredExceptionHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
ULONG WINAPI RemoveVectoredExceptionHandler(PVOID handle);
PVOID WINAPI AddVectoredContinueHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
ULONG WINAPI RemoveVectoredContinueHandler(PVOID handle);
void WINAPI RtlCaptureContext(PCONTEXT context);
BOOL WINAPI GetThreadContext(HANDLE thread, LPCONTEXT context);
BOOL WINAPI SetThreadContext(HANDLE thread, const CONTEXT *context);
BOOL WINAPI IsDebuggerPresent(void);
void WINAPI DebugBreak(void);
USHORT WINAPI RtlCaptureStackBackTrace(ULONG skip, ULONG count, PVOID *frames, PULONG hash);
