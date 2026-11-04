// __FILE__ and __LINE__ inside macros of a header (assert among them) are
// the place of use: this file and the line of the call
#include "test.h"
#include <string.h>
#include "pp_file_line.h"

static const char *g_file;
static int g_line;

static void note(const char *file, int line)
{
    g_file = file;
    g_line = line;
}

// assert's own report, caught here instead of the library's
void __assert_fail(const char *expr, const char *file, int line)
{
    note(file, line);
}

#include <assert.h>

static int ends_with(const char *s, const char *tail)
{
    int n = (int)strlen(s), m = (int)strlen(tail);
    return n >= m && !strcmp(s + n - m, tail);
}

int main(void)
{
    ASSERT(1, ends_with(__FILE__, "pp_file_line.c"));
    ASSERT(1, ends_with(HERE_FILE, "pp_file_line.c"));
    ASSERT(0, strstr(HERE_FILE, ".h") != 0);
    int here = __LINE__; int got = HERE_LINE;
    ASSERT(here, got);
    HERE_CALL(note); here = __LINE__;
    ASSERT(1, ends_with(g_file, "pp_file_line.c"));
    ASSERT(here, g_line);
    HERE_DEEP(); here = __LINE__;           // a macro calling a macro: still here
    ASSERT(1, ends_with(g_file, "pp_file_line.c"));
    ASSERT(here, g_line);
    g_file = 0;
    assert(1 == 2); here = __LINE__;
    ASSERT(1, g_file && ends_with(g_file, "pp_file_line.c"));
    ASSERT(here, g_line);
    ASSERT(1, ends_with(header_own_file(), "pp_file_line.h"));   // a header's function keeps its file
    // "tests\pp_file_line.c" (the test host's path) keeps its backslash
    const char *slash = strrchr(__FILE__, '\\');
    ASSERT(1, !slash || !strcmp(slash, "\\pp_file_line.c"));
    return g_fail;
}
