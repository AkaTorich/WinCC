// intrin.h: the intrinsics of Microsoft C -- single instructions of the processor
// called as functions: __cpuid, __rdtsc, _BitScanForward, __popcnt, _byteswap_ulong,
// _rotl, _umul128, _InterlockedIncrement, __readgsqword... Each is a macro with a
// few lines of asm, so it costs no call. (The SSE ones with __m128 are not here:
// write those in asm.)
#pragma once

// ---- the processor ----

// __cpuidex(info, leaf, subleaf): eax, ebx, ecx, edx of cpuid into info[0..3]
#define __cpuidex(info, leaf, sub) ((void)({ int *__ci = (info); int __cl = (leaf), __cs = (sub); \
    asm("push rbx\n mov eax, dword ptr @__cl\n mov ecx, dword ptr @__cs\n cpuid\n" \
        "mov r8, qword ptr @__ci\n mov dword ptr [r8], eax\n mov dword ptr [r8+4], ebx\n" \
        "mov dword ptr [r8+8], ecx\n mov dword ptr [r8+12], edx\n pop rbx"); 0; }))
#define __cpuid(info, leaf) __cpuidex((info), (leaf), 0)

#define __rdtsc() ({ unsigned long long __r; \
    asm("rdtsc\n shl rdx, 32\n or rax, rdx\n mov qword ptr @__r, rax"); __r; })
#define __rdtscp(aux) ({ unsigned int *__a = (aux); unsigned long long __r; \
    asm("rdtscp\n shl rdx, 32\n or rax, rdx\n mov qword ptr @__r, rax\n" \
        "mov rdx, qword ptr @__a\n mov dword ptr [rdx], ecx"); __r; })
#define _xgetbv(n) ({ unsigned int __n = (n); unsigned long long __r; \
    asm("mov ecx, dword ptr @__n\n xgetbv\n shl rdx, 32\n or rax, rdx\n mov qword ptr @__r, rax"); __r; })

#define __debugbreak() ((void)({ asm("int3"); 0; }))
#define __nop() ((void)({ asm("nop"); 0; }))
#define _mm_pause() ((void)({ asm("pause"); 0; }))
#define _mm_mfence() ((void)({ asm("mfence"); 0; }))
#define _mm_lfence() ((void)({ asm("lfence"); 0; }))
#define _mm_sfence() ((void)({ asm("sfence"); 0; }))
#define __faststorefence() ((void)({ asm("lock or dword ptr [rsp], 0"); 0; }))
#define _ReadWriteBarrier() ((void)0)       // the compiler keeps the order of memory as written
#define _ReadBarrier() ((void)0)
#define _WriteBarrier() ((void)0)
#define _mm_getcsr() ({ unsigned int __r; asm("stmxcsr dword ptr @__r"); __r; })
#define _mm_setcsr(v) ((void)({ unsigned int __v = (v); asm("ldmxcsr dword ptr @__v"); 0; }))

// the return address of this function, and the place it is kept
#define _ReturnAddress() ({ void *__r; asm("mov rax, qword ptr [rbp+8]\n mov qword ptr @__r, rax"); __r; })
#define _AddressOfReturnAddress() ({ void *__r; asm("lea rax, [rbp+8]\n mov qword ptr @__r, rax"); __r; })

// gs: the block of the thread (TEB at gs:[0x30])
#define __readgsqword(off) ({ unsigned long __o = (off); unsigned long long __r; \
    asm("mov eax, dword ptr @__o\n mov rax, qword ptr gs:[rax]\n mov qword ptr @__r, rax"); __r; })
#define __readgsdword(off) ({ unsigned long __o = (off); unsigned long __r; \
    asm("mov eax, dword ptr @__o\n mov eax, dword ptr gs:[rax]\n mov dword ptr @__r, eax"); __r; })
#define __readgsword(off) ({ unsigned long __o = (off); unsigned short __r; \
    asm("mov eax, dword ptr @__o\n mov ax, word ptr gs:[rax]\n mov word ptr @__r, ax"); __r; })
#define __readgsbyte(off) ({ unsigned long __o = (off); unsigned char __r; \
    asm("mov eax, dword ptr @__o\n mov al, byte ptr gs:[rax]\n mov byte ptr @__r, al"); __r; })
#define NtCurrentTeb() ((void *)__readgsqword(0x30))

// ---- bits ----

// _BitScanForward(&index, mask): the lowest set bit's number into index; 0 when mask is 0
#define _BitScanForward(idx, mask) ({ unsigned long *__i = (idx); unsigned long __m = (mask); unsigned char __r; \
    asm("xor ecx, ecx\n bsf eax, dword ptr @__m\n setnz cl\n jz 1f\n mov rdx, qword ptr @__i\n" \
        "mov dword ptr [rdx], eax\n 1:\n mov byte ptr @__r, cl"); __r; })
#define _BitScanReverse(idx, mask) ({ unsigned long *__i = (idx); unsigned long __m = (mask); unsigned char __r; \
    asm("xor ecx, ecx\n bsr eax, dword ptr @__m\n setnz cl\n jz 1f\n mov rdx, qword ptr @__i\n" \
        "mov dword ptr [rdx], eax\n 1:\n mov byte ptr @__r, cl"); __r; })
#define _BitScanForward64(idx, mask) ({ unsigned long *__i = (idx); unsigned long long __m = (mask); unsigned char __r; \
    asm("xor ecx, ecx\n bsf rax, qword ptr @__m\n setnz cl\n jz 1f\n mov rdx, qword ptr @__i\n" \
        "mov dword ptr [rdx], eax\n 1:\n mov byte ptr @__r, cl"); __r; })
#define _BitScanReverse64(idx, mask) ({ unsigned long *__i = (idx); unsigned long long __m = (mask); unsigned char __r; \
    asm("xor ecx, ecx\n bsr rax, qword ptr @__m\n setnz cl\n jz 1f\n mov rdx, qword ptr @__i\n" \
        "mov dword ptr [rdx], eax\n 1:\n mov byte ptr @__r, cl"); __r; })

#define __popcnt16(x) ({ unsigned short __v = (x); unsigned short __r; \
    asm("movzx eax, word ptr @__v\n popcnt eax, eax\n mov word ptr @__r, ax"); __r; })
#define __popcnt(x) ({ unsigned int __v = (x), __r; \
    asm("popcnt eax, dword ptr @__v\n mov dword ptr @__r, eax"); __r; })
#define __popcnt64(x) ({ unsigned long long __v = (x), __r; \
    asm("popcnt rax, qword ptr @__v\n mov qword ptr @__r, rax"); __r; })
#define __lzcnt16(x) ({ unsigned short __v = (x), __r; \
    asm("lzcnt ax, word ptr @__v\n mov word ptr @__r, ax"); __r; })
#define __lzcnt(x) ({ unsigned int __v = (x), __r; \
    asm("lzcnt eax, dword ptr @__v\n mov dword ptr @__r, eax"); __r; })
#define __lzcnt64(x) ({ unsigned long long __v = (x), __r; \
    asm("lzcnt rax, qword ptr @__v\n mov qword ptr @__r, rax"); __r; })
#define _tzcnt_u32(x) ({ unsigned int __v = (x), __r; \
    asm("tzcnt eax, dword ptr @__v\n mov dword ptr @__r, eax"); __r; })
#define _tzcnt_u64(x) ({ unsigned long long __v = (x), __r; \
    asm("tzcnt rax, qword ptr @__v\n mov qword ptr @__r, rax"); __r; })
#define _lzcnt_u32 __lzcnt
#define _lzcnt_u64 __lzcnt64

#define _byteswap_ushort(x) ({ unsigned short __v = (x); asm("rol word ptr @__v, 8"); __v; })
#define _byteswap_ulong(x) ({ unsigned long __v = (x); \
    asm("mov eax, dword ptr @__v\n bswap eax\n mov dword ptr @__v, eax"); __v; })
#define _byteswap_uint64(x) ({ unsigned long long __v = (x); \
    asm("mov rax, qword ptr @__v\n bswap rax\n mov qword ptr @__v, rax"); __v; })

#define __wcc_rot(type, size, op, v, s) ({ type __v = (v); unsigned char __s = (unsigned char)(s); \
    asm("mov cl, byte ptr @__s\n " op " " size " ptr @__v, cl"); __v; })
#define _rotl8(v, s) __wcc_rot(unsigned char, "byte", "rol", v, s)
#define _rotr8(v, s) __wcc_rot(unsigned char, "byte", "ror", v, s)
#define _rotl16(v, s) __wcc_rot(unsigned short, "word", "rol", v, s)
#define _rotr16(v, s) __wcc_rot(unsigned short, "word", "ror", v, s)
#define _rotl(v, s) __wcc_rot(unsigned int, "dword", "rol", v, s)
#define _rotr(v, s) __wcc_rot(unsigned int, "dword", "ror", v, s)
#define _lrotl(v, s) __wcc_rot(unsigned long, "dword", "rol", v, s)
#define _lrotr(v, s) __wcc_rot(unsigned long, "dword", "ror", v, s)
#define _rotl64(v, s) __wcc_rot(unsigned long long, "qword", "rol", v, s)
#define _rotr64(v, s) __wcc_rot(unsigned long long, "qword", "ror", v, s)

// _bittest(&bits, n): bit n of a bit array; the "and" forms change it and say what it was
#define _bittest(a, b) ((unsigned char)((((const long *)(a))[(b) >> 5] >> ((b) & 31)) & 1))
#define _bittest64(a, b) ((unsigned char)((((const long long *)(a))[(b) >> 6] >> ((b) & 63)) & 1))
#define __wcc_bitop(type, sh, mask, a, b, op) ({ type *__p = (type *)(a); long long __b = (b); \
    unsigned char __r = (unsigned char)((__p[__b >> sh] >> (__b & mask)) & 1); \
    __p[__b >> sh] op ((type)1 << (__b & mask)); __r; })
#define _bittestandset(a, b) __wcc_bitop(long, 5, 31, a, b, |=)
#define _bittestandreset(a, b) __wcc_bitop(long, 5, 31, a, b, &= ~)
#define _bittestandcomplement(a, b) __wcc_bitop(long, 5, 31, a, b, ^=)
#define _bittestandset64(a, b) __wcc_bitop(long long, 6, 63, a, b, |=)
#define _bittestandreset64(a, b) __wcc_bitop(long long, 6, 63, a, b, &= ~)
#define _bittestandcomplement64(a, b) __wcc_bitop(long long, 6, 63, a, b, ^=)

// ---- 128-bit arithmetic ----

#define _umul128(a, b, hi) ({ unsigned long long __a = (a), __b = (b), *__h = (hi), __lo; \
    asm("mov rax, qword ptr @__a\n mul qword ptr @__b\n mov rcx, qword ptr @__h\n" \
        "mov qword ptr [rcx], rdx\n mov qword ptr @__lo, rax"); __lo; })
#define _mul128(a, b, hi) ({ long long __a = (a), __b = (b), *__h = (hi), __lo; \
    asm("mov rax, qword ptr @__a\n imul qword ptr @__b\n mov rcx, qword ptr @__h\n" \
        "mov qword ptr [rcx], rdx\n mov qword ptr @__lo, rax"); __lo; })
#define __umulh(a, b) ({ unsigned long long __a = (a), __b = (b), __hi; \
    asm("mov rax, qword ptr @__a\n mul qword ptr @__b\n mov qword ptr @__hi, rdx"); __hi; })
#define __mulh(a, b) ({ long long __a = (a), __b = (b), __hi; \
    asm("mov rax, qword ptr @__a\n imul qword ptr @__b\n mov qword ptr @__hi, rdx"); __hi; })
#define _addcarry_u64(c, a, b, out) ({ unsigned char __c = (c); unsigned long long __a = (a), __b = (b), *__o = (out); \
    asm("mov al, byte ptr @__c\n add al, 255\n mov rax, qword ptr @__a\n adc rax, qword ptr @__b\n" \
        "mov rcx, qword ptr @__o\n mov qword ptr [rcx], rax\n setc byte ptr @__c"); __c; })
#define _subborrow_u64(c, a, b, out) ({ unsigned char __c = (c); unsigned long long __a = (a), __b = (b), *__o = (out); \
    asm("mov al, byte ptr @__c\n add al, 255\n mov rax, qword ptr @__a\n sbb rax, qword ptr @__b\n" \
        "mov rcx, qword ptr @__o\n mov qword ptr [rcx], rax\n setc byte ptr @__c"); __c; })
#define _addcarry_u32(c, a, b, out) ({ unsigned char __c = (c); unsigned int __a = (a), __b = (b), *__o = (out); \
    asm("mov al, byte ptr @__c\n add al, 255\n mov eax, dword ptr @__a\n adc eax, dword ptr @__b\n" \
        "mov rcx, qword ptr @__o\n mov dword ptr [rcx], eax\n setc byte ptr @__c"); __c; })
#define __shiftleft128(lo, hi, s) ({ unsigned long long __l = (lo), __h = (hi); unsigned char __s = (unsigned char)((s) & 63); \
    __s ? (__h << __s) | (__l >> (64 - __s)) : __h; })
#define __shiftright128(lo, hi, s) ({ unsigned long long __l = (lo), __h = (hi); unsigned char __s = (unsigned char)((s) & 63); \
    __s ? (__l >> __s) | (__h << (64 - __s)) : __l; })

// ---- memory by rep movs/stos ----

#define __stosb(d, v, n) ((void)({ unsigned char *__d = (d); unsigned char __v = (v); unsigned long long __n = (n); \
    asm("push rdi\n mov rdi, qword ptr @__d\n mov al, byte ptr @__v\n mov rcx, qword ptr @__n\n rep stosb\n pop rdi"); 0; }))
#define __stosd(d, v, n) ((void)({ unsigned long *__d = (d); unsigned long __v = (v); unsigned long long __n = (n); \
    asm("push rdi\n mov rdi, qword ptr @__d\n mov eax, dword ptr @__v\n mov rcx, qword ptr @__n\n rep stosd\n pop rdi"); 0; }))
#define __stosq(d, v, n) ((void)({ unsigned long long *__d = (d); unsigned long long __v = (v), __n = (n); \
    asm("push rdi\n mov rdi, qword ptr @__d\n mov rax, qword ptr @__v\n mov rcx, qword ptr @__n\n rep stosq\n pop rdi"); 0; }))
#define __movsb(d, s, n) ((void)({ unsigned char *__d = (d); const unsigned char *__s = (s); unsigned long long __n = (n); \
    asm("push rsi\n push rdi\n mov rdi, qword ptr @__d\n mov rsi, qword ptr @__s\n mov rcx, qword ptr @__n\n" \
        "rep movsb\n pop rdi\n pop rsi"); 0; }))
#define __movsq(d, s, n) ((void)({ unsigned long long *__d = (d); const unsigned long long *__s = (s); unsigned long long __n = (n); \
    asm("push rsi\n push rdi\n mov rdi, qword ptr @__d\n mov rsi, qword ptr @__s\n mov rcx, qword ptr @__n\n" \
        "rep movsq\n pop rdi\n pop rsi"); 0; }))

// ---- Interlocked: one indivisible step (the same functions windows.h has) ----

long InterlockedIncrement(long volatile *p);
long InterlockedDecrement(long volatile *p);
long InterlockedExchange(long volatile *p, long value);
long InterlockedExchangeAdd(long volatile *p, long value);
long InterlockedCompareExchange(long volatile *p, long exchange, long comparand);
long long InterlockedIncrement64(long long volatile *p);
long long InterlockedDecrement64(long long volatile *p);
long long InterlockedExchange64(long long volatile *p, long long value);
long long InterlockedExchangeAdd64(long long volatile *p, long long value);
long long InterlockedCompareExchange64(long long volatile *p, long long exchange, long long comparand);
#ifndef _InterlockedIncrement
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
#endif
// and, or, xor: a compare-exchange until nobody got in between; the old value
#define __wcc_ilop(type, p, v, op) ({ type volatile *__p = (p); type __v = (v), __o = *__p; \
    while (!__builtin_atomic_cas((type *)__p, &__o, (type)(__o op __v))); __o; })
#define _InterlockedAnd(p, v) __wcc_ilop(long, p, v, &)
#define _InterlockedOr(p, v) __wcc_ilop(long, p, v, |)
#define _InterlockedXor(p, v) __wcc_ilop(long, p, v, ^)
#define _InterlockedAnd64(p, v) __wcc_ilop(long long, p, v, &)
#define _InterlockedOr64(p, v) __wcc_ilop(long long, p, v, |)
#define _InterlockedXor64(p, v) __wcc_ilop(long long, p, v, ^)
#define _InterlockedAnd8(p, v) __wcc_ilop(char, p, v, &)
#define _InterlockedOr8(p, v) __wcc_ilop(char, p, v, |)
#define _InterlockedExchangePointer(p, v) ((void *)__builtin_atomic_exchange((void **)(p), (void *)(v)))
#define _InterlockedCompareExchangePointer(p, e, c) ({ void *__c = (void *)(c); \
    __builtin_atomic_cas((void **)(p), &__c, (void *)(e)); __c; })
#define _InterlockedCompareExchange16(p, e, c) ({ short __c = (c); __builtin_atomic_cas((short *)(p), &__c, (short)(e)); __c; })
#define _InterlockedIncrement16(p) ((short)(__builtin_atomic_fetch_add((short *)(p), 1) + 1))
#define _InterlockedDecrement16(p) ((short)(__builtin_atomic_fetch_add((short *)(p), -1) - 1))
