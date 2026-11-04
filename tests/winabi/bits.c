// Bit fields as MSVC lays them out: the size and alignment of each structure and the
// bytes each field takes (all its bits set in an empty structure), the value read back.
// Built by MSVC and by WinCC, the two lists must be the same (abi.ps1).
#include <stdio.h>
#include <string.h>
#include <stddef.h>

struct S1 { int a : 3; int b : 5; int c : 24; };
struct S2 { char a : 3; int b : 5; };
struct S3 { int a : 3; char b : 5; };
struct S4 { char a; int b : 4; char c; };
struct S5 { int a : 30; int b : 4; };
struct S6 { short a : 4; unsigned short b : 12; short c : 1; };
struct S7 { int a : 4; int : 0; int b : 4; };
struct S8 { char c; int : 0; char d; };
struct S9 { char c; int : 4; };
struct S10 { long long a : 40; int b : 10; };
struct S11 { unsigned a : 1; unsigned b : 31; unsigned c : 1; };
struct S12 { _Bool a : 1; _Bool b : 1; char c; };
struct S13 { int a : 4; long b : 4; };
struct S14 { int a : 4; unsigned int b : 4; };
union U15 { int a : 4; char b : 2; long long c : 33; };
enum E16 { E_ONE = 1 };
struct S16 { enum E16 e : 3; int x : 2; };
#pragma pack(push, 1)
struct S17 { char c; int a : 4; int b : 30; };
#pragma pack(pop)
struct S18 { char a : 4; char : 0; char b : 4; };
struct S19 { int a : 4; char : 0; int b : 4; };
struct S20 { char a : 1; short : 0; char b : 1; };
struct S21 { int : 0; int a : 3; };
struct S22 { char c; long long : 0; char d; };
struct S23 { char a : 2; char : 3; char b : 3; char d : 1; };
struct S24 { unsigned long long a : 64; unsigned b : 32; };
struct S25 { int a : 4; struct { int x : 3; int y : 2; } in; int b : 4; };
struct S26 { char a : 4; short b : 4; char c : 4; };
struct S27 { short a; int b : 4; };

static void dump(const char *name, const void *p, int n, long long v)
{
    printf("%-10s", name);
    for (int i = 0; i < n; i++)
        printf(" %02x", ((const unsigned char *)p)[i]);
    printf("  = %lld\n", v);
}

#define SZ(T) printf("%-10s size %d align %d\n", #T, (int)sizeof(T), (int)offsetof(struct { char c; T t; }, t))
#define F(T, f) do { T s; memset(&s, 0, sizeof s); s.f = -1; dump(#T "." #f, &s, (int)sizeof s, (long long)s.f); \
                     memset(&s, 0, sizeof s); s.f = 2; dump(#T "." #f, &s, (int)sizeof s, (long long)s.f); } while (0)

int main(void)
{
    SZ(struct S1); F(struct S1, a); F(struct S1, b); F(struct S1, c);
    SZ(struct S2); F(struct S2, a); F(struct S2, b);
    SZ(struct S3); F(struct S3, a); F(struct S3, b);
    SZ(struct S4); F(struct S4, b); F(struct S4, c);
    SZ(struct S5); F(struct S5, a); F(struct S5, b);
    SZ(struct S6); F(struct S6, a); F(struct S6, b); F(struct S6, c);
    SZ(struct S7); F(struct S7, a); F(struct S7, b);
    SZ(struct S8); F(struct S8, d);
    SZ(struct S9);
    SZ(struct S10); F(struct S10, a); F(struct S10, b);
    SZ(struct S11); F(struct S11, a); F(struct S11, b); F(struct S11, c);
    SZ(struct S12); F(struct S12, a); F(struct S12, b); F(struct S12, c);
    SZ(struct S13); F(struct S13, a); F(struct S13, b);
    SZ(struct S14); F(struct S14, a); F(struct S14, b);
    SZ(union U15); F(union U15, a); F(union U15, b); F(union U15, c);
    SZ(struct S16); F(struct S16, e); F(struct S16, x);
    SZ(struct S17); F(struct S17, a); F(struct S17, b);
    SZ(struct S18); F(struct S18, a); F(struct S18, b);
    SZ(struct S19); F(struct S19, a); F(struct S19, b);
    SZ(struct S20); F(struct S20, a); F(struct S20, b);
    SZ(struct S21); F(struct S21, a);
    SZ(struct S22); F(struct S22, d);
    SZ(struct S23); F(struct S23, a); F(struct S23, b); F(struct S23, d);
    SZ(struct S24); F(struct S24, a); F(struct S24, b);
    SZ(struct S25); F(struct S25, a); F(struct S25, in.x); F(struct S25, in.y); F(struct S25, b);
    SZ(struct S26); F(struct S26, a); F(struct S26, b); F(struct S26, c);
    SZ(struct S27); F(struct S27, b);
    return 0;
}
