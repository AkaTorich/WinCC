// __declspec(...) as in Microsoft C: dllexport marks a function for the export
// table of a .dll/.exe (the table itself: tools\cctest\exports.ps1), the rest is skipped.
#include "test.h"

__declspec(dllexport) int exported_add(int a, int b);
__declspec(dllexport) int exported_add(int a, int b) { return a + b; }
__declspec(dllimport) int imported_like(int a);
int imported_like(int a) { return a * 2; }
__declspec(noreturn) void never_called(void);
static __declspec(dllexport) int static_marked(void) { return 7; }
typedef struct { __declspec(align(16)) int x; } Aligned;

int main(void)
{
    ASSERT(5, exported_add(2, 3));
    ASSERT(8, imported_like(4));
    ASSERT(7, static_marked());
    Aligned a;
    a.x = 9;
    ASSERT(9, a.x);
    return g_fail;
}
