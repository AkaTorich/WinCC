int printf(const char *fmt, ...);
static int g_fail;
static void check(long long want, long long got, char *code, int line)
{
    if (want != got) {
        printf("\n    строка %d: %s => %lld, ожидалось %lld", line, code, got, want);
        g_fail++;
    }
}
#define ASSERT(want, expr) check(want, expr, #expr, __LINE__)
