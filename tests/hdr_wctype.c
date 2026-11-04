// <wctype.h> alone (no wchar.h, stddef.h, stdlib.h) declares wchar_t, wint_t, WEOF
#include "test.h"
#include <wctype.h>

int main(void)
{
    wchar_t s[] = L"Жук";
    wint_t up = towupper((wint_t)s[1]);
    ASSERT(0x423, up);                  // у -> У
#ifdef _WIN32
    ASSERT(1, sizeof(wchar_t) == 2 && sizeof(wint_t) == 4);
#else
    ASSERT(1, sizeof(wchar_t) == 4 && sizeof(wint_t) == 4);
#endif
    ASSERT(1, (wint_t)-1 == WEOF);
    ASSERT(0, iswalpha(WEOF));
    wctype_t t = wctype("upper");
    ASSERT(1, iswctype((wint_t)s[0], t));
    return g_fail;
}
