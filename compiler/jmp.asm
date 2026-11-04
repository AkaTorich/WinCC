; Non-local exit for compiler errors (no C runtime in the kernel).
; Saves the Microsoft x64 callee-saved state: rbx rbp rdi rsi r12-r15 rsp rip xmm6-15.

OPTION CASEMAP:NONE

.code

; int cc_setjmp(CcJmpBuf *buf)   buf: 10 qwords + 10 xmm (16 bytes each) = 240 bytes
cc_setjmp PROC
    mov     [rcx+0],  rbx
    mov     [rcx+8],  rbp
    mov     [rcx+16], rdi
    mov     [rcx+24], rsi
    mov     [rcx+32], r12
    mov     [rcx+40], r13
    mov     [rcx+48], r14
    mov     [rcx+56], r15
    lea     rdx, [rsp+8]            ; rsp after return
    mov     [rcx+64], rdx
    mov     rdx, [rsp]              ; return address
    mov     [rcx+72], rdx
    movdqu  [rcx+80],  xmm6
    movdqu  [rcx+96],  xmm7
    movdqu  [rcx+112], xmm8
    movdqu  [rcx+128], xmm9
    movdqu  [rcx+144], xmm10
    movdqu  [rcx+160], xmm11
    movdqu  [rcx+176], xmm12
    movdqu  [rcx+192], xmm13
    movdqu  [rcx+208], xmm14
    movdqu  [rcx+224], xmm15
    xor     eax, eax
    ret
cc_setjmp ENDP

; void cc_longjmp(CcJmpBuf *buf, int value)
cc_longjmp PROC
    mov     eax, edx
    test    eax, eax
    jnz     @F
    inc     eax
@@:
    mov     rbx, [rcx+0]
    mov     rbp, [rcx+8]
    mov     rdi, [rcx+16]
    mov     rsi, [rcx+24]
    mov     r12, [rcx+32]
    mov     r13, [rcx+40]
    mov     r14, [rcx+48]
    mov     r15, [rcx+56]
    movdqu  xmm6,  [rcx+80]
    movdqu  xmm7,  [rcx+96]
    movdqu  xmm8,  [rcx+112]
    movdqu  xmm9,  [rcx+128]
    movdqu  xmm10, [rcx+144]
    movdqu  xmm11, [rcx+160]
    movdqu  xmm12, [rcx+176]
    movdqu  xmm13, [rcx+192]
    movdqu  xmm14, [rcx+208]
    movdqu  xmm15, [rcx+224]
    mov     rsp, [rcx+64]
    jmp     qword ptr [rcx+72]
cc_longjmp ENDP

END
