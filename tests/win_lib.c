// The library of a Windows program: threads.h, stdatomic.h, complex.h, fenv.h, tgmath.h, io.h,
// direct.h, sys/stat.h, process.h, intrin.h, malloc.h, getenv, atexit, clock, strftime %Z.
#ifndef __WINCC__
int main(void) { return 0; }
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <threads.h>
#include <stdatomic.h>
#include <complex.h>
#include <fenv.h>
#include <tgmath.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <process.h>
#include <intrin.h>
#include <malloc.h>
#include <conio.h>
#include <windows.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("ПРОВАЛ строка %d: %s\n", __LINE__, #c); fails++; } } while (0)

static atomic_int counter;
static mtx_t mtx;
static int plain;
static tss_t key;
static int dtor_runs;
static void dtor(void *p) { (void)p; dtor_runs++; }

static int worker(void *arg)
{
    for (int i = 0; i < 10000; i++) {
        atomic_fetch_add(&counter, 1);
        mtx_lock(&mtx);
        plain++;
        mtx_unlock(&mtx);
    }
    tss_set(key, arg);
    printf("поток %d готов\n", (int)(intptr_t)arg);
    return (int)(intptr_t)arg;
}

static once_flag once = ONCE_FLAG_INIT;
static int once_runs;
static void init_once(void) { once_runs++; }

static unsigned __stdcall ex_worker(void *p) { *(int *)p = 77; return 5; }

static int exits;
static void on_exit_fn(void) { exits++; }

int main(void)
{
    // threads.h, stdatomic.h
    mtx_init(&mtx, mtx_plain);
    tss_create(&key, dtor);
    thrd_t t[4];
    for (int i = 0; i < 4; i++)
        CHECK(thrd_create(&t[i], worker, (void *)(intptr_t)(i + 1)) == thrd_success);
    int sum = 0;
    for (int i = 0; i < 4; i++) {
        int r = 0;
        thrd_join(t[i], &r);
        sum += r;
    }
    CHECK(sum == 10 && atomic_load(&counter) == 40000 && plain == 40000 && dtor_runs == 4);
    call_once(&once, init_once);
    call_once(&once, init_once);
    CHECK(once_runs == 1);
    atomic_flag fl = ATOMIC_FLAG_INIT;
    CHECK(!atomic_flag_test_and_set(&fl) && atomic_flag_test_and_set(&fl));
    atomic_int ai = 6;
    CHECK(atomic_fetch_or(&ai, 9) == 6 && ai == 15);
    int exp = 15;
    CHECK(atomic_compare_exchange_strong(&ai, &exp, 1) && ai == 1);

    // complex.h
    _Dcomplex z = _Cbuild(3, 4);
    CHECK(cabs(z) == 5);
    _Dcomplex r = csqrt(_Cbuild(-4, 0));
    CHECK(fabs(creal(r)) < 1e-12 && fabs(cimag(r) - 2) < 1e-12);
    _Dcomplex e = cexp(_Cbuild(0, 3.14159265358979323846));
    CHECK(fabs(creal(e) + 1) < 1e-12);

    // fenv.h
    CHECK(fegetround() == FE_TONEAREST);
    fesetround(FE_UPWARD);
    volatile double third = 1.0 / 3.0;
    fesetround(FE_DOWNWARD);
    volatile double third2 = 1.0 / 3.0;
    fesetround(FE_TONEAREST);
    CHECK(third > third2);
    feclearexcept(FE_ALL_EXCEPT);
    volatile double zero = 0;
    volatile double inf = 1.0 / zero;
    (void)inf;
    CHECK(fetestexcept(FE_DIVBYZERO) != 0);

    // tgmath.h
    float f2 = 2.0f;
    CHECK(sizeof(sqrt(f2)) == 4 && sizeof(sqrt(2.0)) == 8);

    // io.h, direct.h, sys/stat.h
    _mkdir("пробная папка");
    int fd = _open("пробная папка/a.bin", _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY);
    CHECK(fd > 2);
    CHECK(_write(fd, "hello", 5) == 5);
    _close(fd);
    struct stat st;
    CHECK(stat("пробная папка/a.bin", &st) == 0 && st.st_size == 5 && S_ISREG(st.st_mode));
    CHECK(stat("пробная папка", &st) == 0 && S_ISDIR(st.st_mode));
    struct _finddata_t fdata;
    intptr_t fh = _findfirst("пробная папка/*.bin", &fdata);
    CHECK(fh != -1 && strcmp(fdata.name, "a.bin") == 0 && fdata.size == 5);
    _findclose(fh);
    fd = _open("пробная папка/a.bin", _O_RDONLY | _O_BINARY);
    char rb[16] = { 0 };
    CHECK(_read(fd, rb, 16) == 5 && strcmp(rb, "hello") == 0);
    CHECK(_lseek(fd, 1, SEEK_SET) == 1 && _read(fd, rb, 2) == 2 && rb[0] == 'e');
    _close(fd);
    CHECK(_access("пробная папка/a.bin", 0) == 0 && _access("нет такого", 0) == -1);
    char *cwd = _getcwd(0, 0);
    CHECK(cwd && strlen(cwd) > 2);
    free(cwd);
    CHECK(_unlink("пробная папка/a.bin") == 0 && _rmdir("пробная папка") == 0);

    // process.h
    int out = 0;
    HANDLE th = (HANDLE)_beginthreadex(0, 0, ex_worker, &out, 0, 0);
    WaitForSingleObject(th, INFINITE);
    DWORD code = 0;
    GetExitCodeThread(th, &code);
    CloseHandle(th);
    CHECK(out == 77 && code == 5 && _getpid() == (int)GetCurrentProcessId());
    CHECK(_spawnlp(_P_WAIT, "cmd", "cmd", "/c", "exit 7", NULL) == 7);
    CHECK(system("exit 3") == 3);

    // intrin.h
    int info[4];
    __cpuid(info, 0);
    CHECK(info[0] > 0);
    CHECK(__rdtsc() != 0);
    unsigned long idx = 0;
    CHECK(_BitScanForward(&idx, 0x50) && idx == 4);
    CHECK(_BitScanReverse64(&idx, 1ULL << 40) && idx == 40);
    CHECK(__popcnt(0xFF00FF) == 16 && __popcnt64(~0ULL) == 64);
    CHECK(_byteswap_ulong(0x11223344) == 0x44332211 && _byteswap_ushort(0x1122) == 0x2211);
    CHECK(_rotl(0x80000001u, 1) == 3 && _rotr64(1ULL, 1) == 0x8000000000000000ULL);
    unsigned long long hi = 0;
    CHECK(_umul128(~0ULL, 2, &hi) == ~0ULL - 1 && hi == 1);
    long bits = 0;
    CHECK(_bittestandset(&bits, 3) == 0 && bits == 8 && _bittest(&bits, 3) == 1);
    CHECK(NtCurrentTeb() != 0);
    volatile long vl = 5;
    CHECK(_InterlockedIncrement(&vl) == 6 && _InterlockedOr(&vl, 8) == 6 && vl == 14);

    // malloc.h, aligned memory
    char *a = _alloca(64);
    a[63] = 1;
    void *al = _aligned_malloc(100, 256);
    CHECK(((uintptr_t)al & 255) == 0);
    _aligned_free(al);
    void *al2 = aligned_alloc(64, 128);
    CHECK(((uintptr_t)al2 & 63) == 0);
    al2 = realloc(al2, 4096);
    free(al2);

    // stdlib: getenv keeps each name's text, atexit past 32, clock by milliseconds
    char *p1 = getenv("PATH"), *p2 = getenv("WINDIR");
    CHECK(p1 && p2 && p1 != p2 && strlen(p1) > 5);
    for (int i = 0; i < 40; i++)
        CHECK(atexit(on_exit_fn) == 0);
    clock_t c0 = clock();
    while (clock() == c0) {}
    CHECK(clock() - c0 < 5);
    char zbuf[64];
    time_t now = time(0);
    strftime(zbuf, sizeof(zbuf), "%Z", localtime(&now));
    CHECK(strlen(zbuf) > 0);

    printf(fails ? "провалов: %d\n" : "всё верно\n", fails);
    return fails;
}
#endif
