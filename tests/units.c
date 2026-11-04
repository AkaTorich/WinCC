// with: units_b.part units_c.part
// Three sources of one program, each its own translation unit: macros,
// typedefs, struct tags and enums of one are not seen by the next; a header
// is read anew by each (its include guard and #pragma once too), and its
// statics are each source's own. External names are shared.
#include <stdio.h>
#include "units_guard.h"
#include "units_once.h"

#define ONLY_IN_A 1
#define limit 100                   // units_b.part has a function limit()
#define scale(x) ((x) * 1000)       // units_c.part has a function scale()
#undef __DYNASTYOS__                // predefined again in the next source
typedef int Num;                    // units_b.part: Num is double
struct Box { int a; int b; };       // units_b.part: struct Box { char c[4]; }
enum { RED = 1 };                   // units_b.part: RED = 9
struct Box g_box = { 3, 4 };
int shared_total = 5;               // one object for all sources

int b_sees_a(void);
int b_limit(void);
int b_guard(void);
int b_once(void);
double b_num(void);
int b_red(void);
int b_builtin(void);
int b_box(void);
int b_total(void);
int c_guard(void);
int c_scale(void);
int c_once(void);
const char *c_file(void);

static int ends_with(const char *s, const char *tail)
{
    int n = 0, m = 0;
    while (s[n]) n++;
    while (tail[m]) m++;
    if (n < m) return 0;
    for (int i = 0; i < m; i++)
        if (s[n - m + i] != tail[i]) return 0;
    return 1;
}

int main(void)
{
    int bad = 0;
    if (b_sees_a()) { printf("macro of a in b\n"); bad++; }
    if (b_limit() != 5 || limit != 100) { printf("limit\n"); bad++; }
    if (c_scale() != 2 || scale(2) != 2000) { printf("scale\n"); bad++; }
    if (!b_builtin()) { printf("predefined macro lost\n"); bad++; }
    if (HDR_VERSION != 3) { printf("HDR_VERSION\n"); bad++; }
    // a header's statics: each source its own
    if (hdr_bump() != 1 || hdr_bump() != 2) { printf("hdr a\n"); bad++; }
    if (b_guard() != 1) { printf("hdr b\n"); bad++; }
    if (c_guard() != 21) { printf("hdr c\n"); bad++; }
    if (hdr_bump() != 3) { printf("hdr a again\n"); bad++; }
    if (once_id() != 1 || once_id() != 2 || b_once() != 1 || c_once() != 1 || b_once() != 2) {
        printf("pragma once\n");
        bad++;
    }
    // typedefs, tags, enums: each source its own
    if (b_num() != 2.5) { printf("typedef\n"); bad++; }
    if (b_red() != 9 || RED != 1) { printf("enum\n"); bad++; }
    struct Box x = { 1, 2 }, y;
    y = x;
    if (y.a * 10 + y.b != 12 || sizeof(struct Box) != 8) { printf("struct a\n"); bad++; }
    if (g_box.a != 3 || g_box.b != 4) { printf("g_box\n"); bad++; }
    if (b_box() != 7) { printf("struct b\n"); bad++; }
    // external names: one object
    if (b_total() != 6 || shared_total != 6) { printf("shared_total\n"); bad++; }
    if (!ends_with(c_file(), "units_c.part")) { printf("__FILE__ %s\n", c_file()); bad++; }
    return bad;
}
