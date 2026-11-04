// wctype.h: letters and case of Cyrillic (Ё/ё too), Latin-1, Latin
// Extended-A, Greek; classes; wctype/wctrans
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wctype.h>
#include <wchar.h>

static int bad;

static void pair(wint_t up, wint_t low, int line)
{
    if (towlower(up) != low || towupper(low) != up || !iswupper(up) || !iswlower(low) ||
        iswlower(up) || iswupper(low) || !iswalpha(up) || !iswalpha(low)) {
        printf("\n    строка %d: пара U+%04X / U+%04X", line, up, low);
        bad++;
    }
}

int main(void)
{
    // Cyrillic: А-Я / а-я, Ё/ё and the rest of 0400-045F
    for (wint_t c = 0x410; c <= 0x42F; c++) pair(c, c + 32, __LINE__);
    for (wint_t c = 0x400; c <= 0x40F; c++) pair(c, c + 80, __LINE__);
    pair(0x401, 0x451, __LINE__);                    // Ё ё
    pair(0x462, 0x463, __LINE__);                    // Ѣ ѣ
    pair(0x490, 0x491, __LINE__);                    // Ґ ґ
    pair(0x4D8, 0x4D9, __LINE__);                    // Ә ә
    pair(0x4C1, 0x4C2, __LINE__);                    // Ӂ ӂ
    pair(0x4C0, 0x4CF, __LINE__);                    // Ӏ ӏ
    // Latin-1 and Latin Extended-A
    for (wint_t c = 0xC0; c <= 0xDE; c++)
        if (c != 0xD7) pair(c, c + 32, __LINE__);
    pair(0x178, 0xFF, __LINE__);                     // Ÿ ÿ
    pair(0x100, 0x101, __LINE__);                    // Ā ā
    pair(0x141, 0x142, __LINE__);                    // Ł ł
    pair(0x17D, 0x17E, __LINE__);                    // Ž ž
    pair(0x152, 0x153, __LINE__);                    // Œ œ
    ASSERT(1, iswlower(0xDF) && towupper(0xDF) == 0xDF);    // ß has no single capital
    ASSERT('i', towlower(0x130));                    // İ
    ASSERT('I', towupper(0x131));                    // ı
    ASSERT(0x39C, towupper(0xB5));                   // µ
    ASSERT(0, iswalpha(0xD7));                       // ×
    ASSERT(0, iswalpha(0xF7));                       // ÷
    ASSERT(1, iswalpha(0xAA) && !iswupper(0xAA) && !iswlower(0xAA));    // ª
    // Greek
    for (wint_t c = 0x391; c <= 0x3A9; c++)
        if (c != 0x3A2) pair(c, c + 32, __LINE__);
    pair(0x386, 0x3AC, __LINE__);                    // Ά ά
    pair(0x388, 0x3AD, __LINE__);
    pair(0x38C, 0x3CC, __LINE__);                    // Ό ό
    pair(0x38F, 0x3CE, __LINE__);                    // Ώ ώ
    ASSERT(0x3A3, towupper(0x3C2));                  // final ς -> Σ
    ASSERT(1, iswlower(0x3C2));
    ASSERT(0, iswalpha(0x37E));                      // Greek question mark
    ASSERT(1, iswpunct(0x37E));
    // round trips over a sentence
    const char *ru = "Съешь же ещё этих мягких французских булок, да выпей чаю. ЁЛКА!";
    wchar_t w[128], up[128], low[128];
    int n = (int)mbstowcs(w, ru, 128);
    for (int i = 0; i <= n; i++) {
        up[i] = (wchar_t)towupper((wint_t)w[i]);
        low[i] = (wchar_t)towlower((wint_t)w[i]);
    }
    char out[256];
    wcstombs(out, up, sizeof(out));
    ASSERT(0, strcmp(out, "СЪЕШЬ ЖЕ ЕЩЁ ЭТИХ МЯГКИХ ФРАНЦУЗСКИХ БУЛОК, ДА ВЫПЕЙ ЧАЮ. ЁЛКА!"));
    wcstombs(out, low, sizeof(out));
    ASSERT(0, strcmp(out, "съешь же ещё этих мягких французских булок, да выпей чаю. ёлка!"));
    int letters = 0, spaces = 0, punct = 0;
    for (int i = 0; i < n; i++) {
        letters += iswalpha((wint_t)w[i]) != 0;
        spaces += iswspace((wint_t)w[i]) != 0;
        punct += iswpunct((wint_t)w[i]) != 0;
    }
    ASSERT(50, letters);
    ASSERT(10, spaces);
    ASSERT(3, punct);
    mbstowcs(w, "Ὀδυσσεύς Ἀθῆναι ΑΒΓ", 128);
    ASSERT(1, iswalpha((wint_t)w[0]) && iswalpha((wint_t)w[9]));    // Greek extended
    // ASCII agrees with ctype
    int diff = 0;
    for (wint_t c = 0; c < 128; c++) {
        diff += !!iswalpha(c) != ((c | 32) >= 'a' && (c | 32) <= 'z');
        diff += !!iswdigit(c) != (c >= '0' && c <= '9');
        diff += !!iswspace(c) != (c == ' ' || (c >= 9 && c <= 13));
        diff += !!iswprint(c) != (c >= 32 && c < 127);
        diff += !!iswcntrl(c) != (c < 32 || c == 127);
        diff += !!iswblank(c) != (c == ' ' || c == '\t');
        diff += !!iswxdigit(c) != ((c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'f'));
    }
    ASSERT(0, diff);
    // more classes
    ASSERT(0, iswdigit(0x663));                      // Arabic-Indic digits are not "digit" in C
    ASSERT(1, iswspace(0x3000) && iswblank(0x3000));
    ASSERT(1, iswspace(0x2028) && !iswblank(0x2028));
    ASSERT(0, iswspace(0xA0));                       // no-break space
    ASSERT(1, iswprint(0xA0) && iswgraph(0xA0));
    ASSERT(1, iswcntrl(0x85));
    ASSERT(0, iswprint(0xD800));
    ASSERT(1, iswpunct(0x20AC));                     // €
    ASSERT(1, iswalpha(0x4E2D));                     // 中
    ASSERT(1, iswalnum('7') && iswalnum(0x436));
    ASSERT(0, iswalpha(WEOF));
    ASSERT(1, towlower(WEOF) == WEOF);
    // wctype, wctrans
    ASSERT(1, iswctype(0x436, wctype("alpha")));
    ASSERT(1, iswctype(0x416, wctype("upper")));
    ASSERT(0, iswctype(0x436, wctype("upper")));
    ASSERT(1, iswctype('5', wctype("digit")));
    ASSERT(0, wctype("nonsense"));
    ASSERT(0, iswctype('a', 0));
    ASSERT(0x416, towctrans(0x436, wctrans("toupper")));
    ASSERT(0x451, towctrans(0x401, wctrans("tolower")));
    ASSERT(0, wctrans("toupside"));
    ASSERT(0, bad);
    return g_fail;
}
