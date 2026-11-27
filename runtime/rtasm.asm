; setjmp/longjmp and the CPU helpers runtime.c needs in a Windows program (WinCC).
; The Microsoft x64 callee-saved state: rbx rbp rdi rsi r12-r15 rsp rip xmm6-15.
; .gc: each comes into a program only if the program reaches it (strip.c)

; void __wcc_finish(int code): the end of every program (start.c). What the program started --
; atexit functions, output and files written out -- the library put in __wcc_at_end; then ExitProcess.
; An atexit function that calls exit() comes back here, and the rest goes on.
.gc
__wcc_finish:
    push rbx                        ; rsp 16-aligned for the calls
    sub rsp, 32
    mov ebx, ecx
    mov rax, qword ptr [rip+@__wcc_at_end]
    test rax, rax
    jz .Lrt.fin.exit
    call rax
.Lrt.fin.exit:
    mov ecx, ebx
    call @ExitProcess
    int3

; u32 cc_thread_id(void): the id of this thread, from its block (TEB.ClientId.UniqueThread)
.gc
.local cc_thread_id
cc_thread_id:
    mov eax, dword ptr gs:[0x48]
    ret

; int cc_setjmp(CcJmpBuf *buf)   buf: 10 qwords + 10 xmm (16 bytes each) = 240 bytes
.gc
.local cc_setjmp
cc_setjmp:
    mov [rcx+0], rbx
    mov [rcx+8], rbp
    mov [rcx+16], rdi
    mov [rcx+24], rsi
    mov [rcx+32], r12
    mov [rcx+40], r13
    mov [rcx+48], r14
    mov [rcx+56], r15
    lea rdx, [rsp+8]                ; rsp after return
    mov [rcx+64], rdx
    mov rdx, [rsp]                  ; return address
    mov [rcx+72], rdx
    movdqu [rcx+80], xmm6
    movdqu [rcx+96], xmm7
    movdqu [rcx+112], xmm8
    movdqu [rcx+128], xmm9
    movdqu [rcx+144], xmm10
    movdqu [rcx+160], xmm11
    movdqu [rcx+176], xmm12
    movdqu [rcx+192], xmm13
    movdqu [rcx+208], xmm14
    movdqu [rcx+224], xmm15
    xor eax, eax
    ret

; void cc_longjmp(CcJmpBuf *buf, int value)
.gc
.local cc_longjmp
cc_longjmp:
    mov eax, edx
    test eax, eax
    jnz .Lrt.lj.nz
    inc eax
.Lrt.lj.nz:
    mov rbx, [rcx+0]
    mov rbp, [rcx+8]
    mov rdi, [rcx+16]
    mov rsi, [rcx+24]
    mov r12, [rcx+32]
    mov r13, [rcx+40]
    mov r14, [rcx+48]
    mov r15, [rcx+56]
    movdqu xmm6, [rcx+80]
    movdqu xmm7, [rcx+96]
    movdqu xmm8, [rcx+112]
    movdqu xmm9, [rcx+128]
    movdqu xmm10, [rcx+144]
    movdqu xmm11, [rcx+160]
    movdqu xmm12, [rcx+176]
    movdqu xmm13, [rcx+192]
    movdqu xmm14, [rcx+208]
    movdqu xmm15, [rcx+224]
    mov rsp, [rcx+64]
    jmp qword ptr [rcx+72]

; long _InterlockedCompareExchange(volatile long *p, long value, long expected): the old value
.gc
.local _InterlockedCompareExchange
_InterlockedCompareExchange:
    mov eax, r8d
    lock cmpxchg dword ptr [rcx], edx
    ret

; long _InterlockedExchange(volatile long *p, long value): the old value
.gc
.local _InterlockedExchange
_InterlockedExchange:
    mov eax, edx
    xchg dword ptr [rcx], eax
    ret

.gc
.local _mm_pause
_mm_pause:
    pause
    ret

; unsigned char _BitScanReverse64(unsigned long *index, u64 v): 0 when v is 0
.gc
.local _BitScanReverse64
_BitScanReverse64:
    test rdx, rdx
    jz .Lrt.bsr.zero
    bsr rax, rdx
    mov dword ptr [rcx], eax
    mov eax, 1
    ret
.Lrt.bsr.zero:
    xor eax, eax
    ret

.gc
.local _BitScanForward64
_BitScanForward64:
    test rdx, rdx
    jz .Lrt.bsf.zero
    bsf rax, rdx
    mov dword ptr [rcx], eax
    mov eax, 1
    ret
.Lrt.bsf.zero:
    xor eax, eax
    ret

; ---- Interlocked*: in 64-bit Windows they are compiler intrinsics, no DLL has them ----
; .gc: a program gets only those it calls (windows.h declares them)

.gc
InterlockedIncrement:
    mov eax, 1
    lock xadd dword ptr [rcx], eax
    inc eax
    ret

.gc
InterlockedDecrement:
    mov eax, -1
    lock xadd dword ptr [rcx], eax
    dec eax
    ret

.gc
InterlockedExchange:
    mov eax, edx
    xchg dword ptr [rcx], eax
    ret

.gc
InterlockedExchangeAdd:
    mov eax, edx
    lock xadd dword ptr [rcx], eax
    ret

.gc
InterlockedCompareExchange:
    mov eax, r8d
    lock cmpxchg dword ptr [rcx], edx
    ret

.gc
InterlockedIncrement64:
    mov eax, 1
    lock xadd qword ptr [rcx], rax
    inc rax
    ret

.gc
InterlockedDecrement64:
    mov rax, -1
    lock xadd qword ptr [rcx], rax
    dec rax
    ret

.gc
InterlockedExchange64:
    mov rax, rdx
    xchg qword ptr [rcx], rax
    ret

.gc
InterlockedExchangeAdd64:
    mov rax, rdx
    lock xadd qword ptr [rcx], rax
    ret

.gc
InterlockedCompareExchange64:
    mov rax, r8
    lock cmpxchg qword ptr [rcx], rdx
    ret

; ---- memory and strings with the processor's own string instructions (runtime.c has them in C
; for DynastyOS); rsi and rdi are callee-saved: kept in r9 / r10 ----

.gc
.local rt_memcpy
; void *rt_memcpy(void *d, const void *s, size_t n)
rt_memcpy:
    mov rax, rcx
    mov r9, rdi
    mov r10, rsi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    rep movsb
    mov rdi, r9
    mov rsi, r10
    ret

.gc
.local rt_memmove
; void *rt_memmove(void *d, const void *s, size_t n): backwards when d is inside [s, s+n)
rt_memmove:
    mov rax, rcx
    mov r9, rdi
    mov r10, rsi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    cmp rdi, rsi
    jbe 1f
    lea r11, [rsi+rcx]
    cmp rdi, r11
    jae 1f
    lea rsi, [rsi+rcx-1]
    lea rdi, [rdi+rcx-1]
    std
    rep movsb
    cld
    jmp 2f
1:
    rep movsb
2:
    mov rdi, r9
    mov rsi, r10
    ret

.gc
.local rt_memset
; void *rt_memset(void *d, int c, size_t n)
rt_memset:
    mov r9, rdi
    mov r10, rcx
    mov rdi, rcx
    mov eax, edx
    mov rcx, r8
    rep stosb
    mov rdi, r9
    mov rax, r10
    ret

.gc
.local rt_strlen
; size_t rt_strlen(const char *s): 16 bytes at a time from an aligned address (never past the page)
rt_strlen:
    mov rdx, rcx
    mov rax, rcx
    and rax, -16
    pxor xmm0, xmm0
    movdqa xmm1, xmmword ptr [rax]
    pcmpeqb xmm1, xmm0
    pmovmskb r8d, xmm1
    and ecx, 15
    shr r8d, cl
    test r8d, r8d
    jnz 3f
1:
    add rax, 16
    movdqa xmm1, xmmword ptr [rax]
    pcmpeqb xmm1, xmm0
    pmovmskb r8d, xmm1
    test r8d, r8d
    jz 1b
    bsf r8d, r8d
    add rax, r8
    sub rax, rdx
    ret
3:
    bsf eax, r8d
    ret

