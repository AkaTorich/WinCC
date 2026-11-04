// The language of C11 and Microsoft C: typeof, _Generic, VLA, _alloca, over-aligned locals, K&R,
// designators into unnamed members, _Atomic, _Thread_local, selectany, @locals in asm, numeric labels,
// new instructions, __try / __except / __finally.
#ifndef __WINCC__                       // DynastyOS (cctest): the Windows ones are not there
int main(void) { return 0; }
#else
#include <stdio.h>
#include <string.h>
#include <windows.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("ПРОВАЛ строка %d: %s\n", __LINE__, #c); fails++; } } while (0)

#define TYPE_NAME(x) _Generic((x), int: "int", double: "double", char *: "char*", default: "другое")

int knr(a, b, c) int a; char *b; double c; { return a + (int)strlen(b) + (int)c; }

int sum2d(int n, int m, int a[n][m])
{
    int s = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            s += a[i][j];
    return s + (int)(sizeof(a[0]) / sizeof(int)) * 1000;
}

struct P { int kind; union { int i; float f; }; struct { int x, y; }; };

_Thread_local int tl_counter = 5;
__declspec(thread) static int tl_other;
static DWORD WINAPI worker(LPVOID p)
{
    tl_counter += (int)(INT_PTR)p;
    tl_other = 7;
    return (DWORD)tl_counter;
}

__declspec(selectany) int shared_value = 42;

static int finally_runs;
static int with_finally(int k)
{
    __try {
        if (k == 1)
            return 10;
        if (k == 2)
            __leave;
        finally_runs += 100;
    } __finally {
        finally_runs++;
    }
    return 20;
}

int main(void)
{
    // typeof, _Generic
    int x = 3;
    typeof(x) y = 4;
    __typeof__(x + 1.0) z = 2.5;
    CHECK(y == 4 && z == 2.5);
    CHECK(strcmp(TYPE_NAME(x), "int") == 0);
    CHECK(strcmp(TYPE_NAME(z), "double") == 0);
    CHECK(strcmp(TYPE_NAME("s"), "char*") == 0);
    CHECK(strcmp(TYPE_NAME(1L << 40), "другое") == 0);

    // VLA
    int n = 5;
    int v[n];
    for (int i = 0; i < n; i++) v[i] = i * i;
    CHECK(sizeof(v) == 20 && v[4] == 16);
    int m = 3;
    int a2[n][m];
    for (int i = 0; i < n; i++) for (int j = 0; j < m; j++) a2[i][j] = i + j;
    CHECK(sizeof(a2) == 60 && sizeof(a2[1]) == 12);
    CHECK(sum2d(n, m, a2) == 3000 + 45);
    long long before = 0, after = 0;
    for (int r = 0; r < 100000; r++) {       // the stack does not grow round after round
        char buf[n * 100];
        buf[0] = (char)r;
        if (r == 0) before = (long long)(INT_PTR)buf;
        if (r == 99999) after = (long long)(INT_PTR)buf;
    }
    CHECK(before == after);
    CHECK(sizeof(int[n]) == 20);

    // _alloca, over-aligned local
    char *p = _alloca(100);
    memset(p, 'a', 100);
    CHECK(p[99] == 'a' && ((INT_PTR)p & 15) == 0);
    char *q = _alloca(200000);           // more than a page: the guard page is passed page by page
    q[0] = 1; q[199999] = 2;
    CHECK(q[199999] == 2);
    _Alignas(64) char big[100];
    __declspec(align(32)) double d4[4] = { 1, 2, 3, 4 };
    CHECK(((INT_PTR)big & 63) == 0 && ((INT_PTR)d4 & 31) == 0 && d4[3] == 4);

    // K&R
    CHECK(knr(1, "abc", 2.0) == 6);

    // designators into unnamed members
    struct P pt = { .kind = 1, .f = 1.5f, .y = 9 };
    CHECK(pt.kind == 1 && pt.f == 1.5f && pt.x == 0 && pt.y == 9);

    // atomics
    _Atomic int at = 10;
    at += 5;
    at++;
    CHECK(at == 16);
    int exp = 16;
    CHECK(__builtin_atomic_cas(&at, &exp, 20) == 1 && at == 20);
    exp = 1;
    CHECK(__builtin_atomic_cas(&at, &exp, 30) == 0 && exp == 20);
    CHECK(__builtin_atomic_exchange(&at, 7) == 20 && at == 7);
    CHECK(__builtin_atomic_fetch_add(&at, 3) == 7 && at == 10);

    // thread-local
    HANDLE h = CreateThread(0, 0, worker, (LPVOID)100, 0, 0);
    WaitForSingleObject(h, INFINITE);
    DWORD code = 0;
    GetExitCodeThread(h, &code);
    CloseHandle(h);
    CHECK(code == 105 && tl_counter == 5 && tl_other == 0);
    tl_counter = 9;
    CHECK(tl_counter == 9);
    CHECK(shared_value == 42);

    // asm with locals, numeric labels, new instructions
    long long cnt = 0, val = 0xF0F0, sw = 0x1122334455667788LL;
    asm("mov rax, qword ptr @val\n"
        "popcnt rax, rax\n"
        "mov qword ptr @cnt, rax\n"
        "mov rax, qword ptr @sw\n"
        "bswap rax\n"
        "mov qword ptr @sw, rax\n");
    CHECK(cnt == 8 && sw == (long long)0x8877665544332211LL);
    int loops = 0;
    asm("mov ecx, 5\n"
        "xor eax, eax\n"
        "1:\n"
        "inc eax\n"
        "dec ecx\n"
        "jnz 1b\n"
        "jmp 2f\n"
        "mov eax, 99\n"
        "2:\n"
        "mov dword ptr @loops, eax\n");
    CHECK(loops == 5);

    // structured exceptions
    int caught = 0;
    unsigned ecode = 0;
    __try {
        volatile int *np = 0;
        *np = 1;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        caught = 1;
        ecode = GetExceptionCode();
    }
    CHECK(caught == 1 && ecode == EXCEPTION_ACCESS_VIOLATION);
    caught = 0;
    __try {
        volatile int zero = 0;
        volatile int r = 10 / zero;
        (void)r;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        caught = GetExceptionCode() == EXCEPTION_INT_DIVIDE_BY_ZERO;
    }
    CHECK(caught == 1);
    CHECK(with_finally(0) == 20 && finally_runs == 101);
    CHECK(with_finally(1) == 10 && finally_runs == 102);
    CHECK(with_finally(2) == 20 && finally_runs == 103);
    int fin_in_loop = 0;
    for (int i = 0; i < 3; i++) {
        __try {
            if (i == 1) break;
        } __finally {
            fin_in_loop++;
        }
    }
    CHECK(fin_in_loop == 2);
    // an exception from deeper down passes through a __finally on its way
    int unwound = 0;
    __try {
        __try {
            RaiseException(0xE0000001, 0, 0, 0);
        } __finally {
            unwound = AbnormalTermination() ? 2 : 1;
        }
    } __except (GetExceptionCode() == 0xE0000001) {
        caught = 7;
    }
    CHECK(unwound == 2 && caught == 7);

    printf(fails ? "провалов: %d\n" : "всё верно\n", fails);
    return fails;
}
#endif
