// The SSE intrinsics hcrypt.c uses (AES-NI, PCLMULQDQ, SSSE3, RDRAND/RDSEED),
// each a few lines of asm over a 16-byte value in memory: WinCC keeps no value
// in xmm registers between statements, so every one loads and stores it.
// The count of a shift and the selector of pclmulqdq must be written as numbers.
#pragma once

typedef struct { unsigned long long __q[2]; } __m128i;

#define HC_SSE2(op, a, b) ({ __m128i __x = (a), __y = (b); \
    asm("movdqu xmm0, xmmword ptr @__x\n movdqu xmm1, xmmword ptr @__y\n" op " xmm0, xmm1\n" \
        "movdqu xmmword ptr @__x, xmm0"); __x; })
#define HC_SSE1(op, a, n) ({ __m128i __x = (a); \
    asm("movdqu xmm0, xmmword ptr @__x\n" op " xmm0, " #n "\n movdqu xmmword ptr @__x, xmm0"); __x; })

#define _mm_loadu_si128(p) (*(const __m128i *)(p))
#define _mm_storeu_si128(p, v) ((void)(*(__m128i *)(p) = (v)))
#define _mm_setzero_si128() ((__m128i){ { 0, 0 } })
// bytes from the highest (e15) to the lowest (e0), as Microsoft C takes them
#define _mm_set_epi8(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0) \
    ((__m128i){ { (unsigned long long)(unsigned char)(e0) | (unsigned long long)(unsigned char)(e1) << 8 | \
        (unsigned long long)(unsigned char)(e2) << 16 | (unsigned long long)(unsigned char)(e3) << 24 | \
        (unsigned long long)(unsigned char)(e4) << 32 | (unsigned long long)(unsigned char)(e5) << 40 | \
        (unsigned long long)(unsigned char)(e6) << 48 | (unsigned long long)(unsigned char)(e7) << 56, \
        (unsigned long long)(unsigned char)(e8) | (unsigned long long)(unsigned char)(e9) << 8 | \
        (unsigned long long)(unsigned char)(e10) << 16 | (unsigned long long)(unsigned char)(e11) << 24 | \
        (unsigned long long)(unsigned char)(e12) << 32 | (unsigned long long)(unsigned char)(e13) << 40 | \
        (unsigned long long)(unsigned char)(e14) << 48 | (unsigned long long)(unsigned char)(e15) << 56 } })

#define _mm_xor_si128(a, b) HC_SSE2("pxor", a, b)
#define _mm_or_si128(a, b) HC_SSE2("por", a, b)
#define _mm_shuffle_epi8(a, b) HC_SSE2("pshufb", a, b)
#define _mm_aesenc_si128(a, b) HC_SSE2("aesenc", a, b)
#define _mm_aesenclast_si128(a, b) HC_SSE2("aesenclast", a, b)
#define _mm_aesdec_si128(a, b) HC_SSE2("aesdec", a, b)
#define _mm_aesdeclast_si128(a, b) HC_SSE2("aesdeclast", a, b)
#define _mm_aesimc_si128(a) HC_SSE2("aesimc", a, a)
#define _mm_slli_si128(a, n) HC_SSE1("pslldq", a, n)
#define _mm_srli_si128(a, n) HC_SSE1("psrldq", a, n)
#define _mm_slli_epi32(a, n) HC_SSE1("pslld", a, n)
#define _mm_srli_epi32(a, n) HC_SSE1("psrld", a, n)
#define _mm_clmulepi64_si128(a, b, n) ({ __m128i __x = (a), __y = (b); \
    asm("movdqu xmm0, xmmword ptr @__x\n movdqu xmm1, xmmword ptr @__y\n pclmulqdq xmm0, xmm1, " #n "\n" \
        "movdqu xmmword ptr @__x, xmm0"); __x; })

// 1 and the number into *p when the processor gave one, 0 when it had none ready
#define _rdrand64_step(p) ({ unsigned long long *__p = (p); int __ok; \
    asm("xor ecx, ecx\n rdrand rax\n setc cl\n mov rdx, qword ptr @__p\n mov qword ptr [rdx], rax\n" \
        "mov dword ptr @__ok, ecx"); __ok; })
#define _rdseed64_step(p) ({ unsigned long long *__p = (p); int __ok; \
    asm("xor ecx, ecx\n rdseed rax\n setc cl\n mov rdx, qword ptr @__p\n mov qword ptr [rdx], rax\n" \
        "mov dword ptr @__ok, ecx"); __ok; })
