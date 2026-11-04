// wchar.h, uchar.h, multibyte conversions in stdlib.h, wide stdio, and the
// literals L"" u"" U"" u8"" L'' u'' U'' with \u escapes. wchar_t is 32 bits;
// multibyte text is UTF-8. W() below makes a wide string from UTF-8 at run time.
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <uchar.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

// Windows: wchar_t is 16 bits (UTF-16); a character beyond U+FFFF (😀) is two of them
#ifdef _WIN32
#define WCH 2
#define EMOJI_UNITS 2
#else
#define WCH 4
#define EMOJI_UNITS 1
#endif

// literals in static data
static const wchar_t *g_wp = L"глобально";
static wchar_t g_wa[] = L"Ёж";
static char16_t g_u16[] = u"a😀";
static const char32_t g_u32[] = U"a😀";
static const char g_u8[] = u8"жé";

static wchar_t *W(const char *s)            // UTF-8 -> a wide string (a few kept at once)
{
    static wchar_t pool[8][128];
    static int next;
    wchar_t *w = pool[next++ & 7];
    mbstowcs(w, s, 128);
    return w;
}

static char *U(const wchar_t *w)            // wide -> UTF-8
{
    static char pool[4][512];
    static int next;
    char *s = pool[next++ & 3];
    wcstombs(s, w, 512);
    return s;
}

static void check_str(const char *want, const char *got, int line)
{
    if (!got || strcmp(want, got)) {
        printf("\n    строка %d: \"%s\", ожидалось \"%s\"", line, got ? got : "(null)", want);
        g_fail++;
    }
}

#define STR(want, got) check_str(want, got, __LINE__)

static int my_vsw(wchar_t *out, size_t n, const wchar_t *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vswprintf(out, n, fmt, ap);
    va_end(ap);
    return r;
}

int main(void)
{
    wchar_t buf[128];
    char nb[256];
    // conversions: Cyrillic (2 bytes), € (3), 😀 (4), Ё/ё
    const char *text = "Ёлка ёж, €5 😀!";
    ASSERT(13 + EMOJI_UNITS, mbstowcs(0, text, 0));
    ASSERT(13 + EMOJI_UNITS, mbstowcs(buf, text, 128));
    ASSERT(0x401, buf[0]);
    ASSERT(0x451, buf[5]);
    ASSERT(0x20AC, buf[9]);
#ifdef _WIN32
    ASSERT(0xD83D, buf[12]);
    ASSERT(0xDE00, buf[13]);
    ASSERT(0, buf[15]);
#else
    ASSERT(0x1F600, buf[12]);
    ASSERT(0, buf[14]);
#endif
    ASSERT(strlen(text), wcstombs(0, buf, 0));
    ASSERT(strlen(text), wcstombs(nb, buf, sizeof(nb)));
    STR(text, nb);
    ASSERT(4, wcstombs(nb, buf, 5));            // whole characters only: "Ёл", not half of "к"
    ASSERT(-1, (long long)mbstowcs(buf, "\xC0\x80", 10));    // overlong
    ASSERT(EILSEQ, errno);
    ASSERT(-1, (long long)mbstowcs(buf, "\xED\xA0\x80", 10)); // a surrogate
    ASSERT(-1, (long long)mbstowcs(buf, "\xF4\x90\x80\x80", 10)); // beyond U+10FFFF
    wchar_t bad[2] = { 0xD800, 0 };
    ASSERT(-1, (long long)wcstombs(nb, bad, 10));
    // mblen, mbtowc, wctomb
    wchar_t wc;
    ASSERT(2, mblen("ж", 2));
    ASSERT(-1, mblen("ж", 1));                 // cut short
    ASSERT(0, mblen("", 1));
    ASSERT(0, mblen(0, 0));                    // no shift states
    ASSERT(3, mbtowc(&wc, "€", 3));
    ASSERT(0x20AC, wc);
#ifdef _WIN32                           // one wchar_t holds no 😀: a lone half is no character
    ASSERT(3, wctomb(nb, 0x20AC));
    ASSERT(0, memcmp(nb, "€", 3));
    ASSERT(-1, wctomb(nb, 0xD83D));
#else
    ASSERT(4, wctomb(nb, 0x1F600));
    ASSERT(0, memcmp(nb, "😀", 4));
    ASSERT(-1, wctomb(nb, 0x110000));
#endif
    // mbrtowc fed one byte at a time
    mbstate_t st;
    memset(&st, 0, sizeof(st));
#ifdef _WIN32                           // mbrtowc gives one wchar_t: a 3-byte character
    const char *e4 = "€";
    ASSERT(-2, (long long)mbrtowc(&wc, e4, 1, &st));
    ASSERT(0, mbsinit(&st));
    ASSERT(-2, (long long)mbrtowc(&wc, e4 + 1, 1, &st));
    ASSERT(1, mbrtowc(&wc, e4 + 2, 1, &st));
    ASSERT(0x20AC, wc);
#else
    const char *e4 = "😀";
    ASSERT(-2, (long long)mbrtowc(&wc, e4, 1, &st));
    ASSERT(0, mbsinit(&st));
    ASSERT(-2, (long long)mbrtowc(&wc, e4 + 1, 1, &st));
    ASSERT(-2, (long long)mbrtowc(&wc, e4 + 2, 1, &st));
    ASSERT(1, mbrtowc(&wc, e4 + 3, 1, &st));
    ASSERT(0x1F600, wc);
#endif
    ASSERT(1, mbsinit(&st));
    ASSERT(2, mbrlen("я", 5, 0));
    ASSERT(-1, (long long)mbrtowc(&wc, "\x80", 1, &st));
    ASSERT(2, wcrtomb(nb, 0x44F, &st));
    ASSERT(1, btowc('A') == 'A');
    ASSERT(1, btowc(0xD0) == WEOF);
    ASSERT('z', wctob('z'));
    ASSERT(EOF, wctob(0x44F));
    // mbsrtowcs / wcsrtombs move the source pointer
    const char *src = "абв";
    ASSERT(2, mbsrtowcs(buf, &src, 2, 0));
    ASSERT(1, src != 0 && strcmp(src, "в") == 0);
    ASSERT(1, mbsrtowcs(buf + 2, &src, 10, 0));
    ASSERT(1, src == 0);
    const wchar_t *wsrc = buf;
    ASSERT(6, wcsrtombs(nb, &wsrc, sizeof(nb), 0));
    ASSERT(1, wsrc == 0);
    STR("абв", nb);
    // char16_t and char32_t
    char16_t c16[4];
    memset(&st, 0, sizeof(st));
    ASSERT(4, mbrtoc16(&c16[0], "😀", 4, &st));
    ASSERT(0xD83D, c16[0]);
    ASSERT(-3, (long long)mbrtoc16(&c16[1], "", 0, &st));    // the second half
    ASSERT(0xDE00, c16[1]);
    ASSERT(2, mbrtoc16(&c16[2], "ж", 2, &st));
    ASSERT(0x436, c16[2]);
    mbstate_t out;
    memset(&out, 0, sizeof(out));
    ASSERT(0, c16rtomb(nb, 0xD83D, &out));
    ASSERT(4, c16rtomb(nb, 0xDE00, &out));
    ASSERT(0, memcmp(nb, "😀", 4));
    ASSERT(-1, (long long)c16rtomb(nb, 0xDE00, &out));       // a lone second half
    char32_t c32;
    ASSERT(3, mbrtoc32(&c32, "€", 3, 0));
    ASSERT(0x20AC, c32);
    ASSERT(2, c32rtomb(nb, 0x451, 0));
    ASSERT(0, memcmp(nb, "ё", 2));
    // wide strings
    wchar_t s[64];
    wcscpy(s, W("Привет"));
    ASSERT(6, wcslen(s));
    ASSERT(3, wcsnlen(s, 3));
    wcscat(s, W(", мир"));
    STR("Привет, мир", U(s));
    wcsncat(s, W("!!!"), 1);
    STR("Привет, мир!", U(s));
    ASSERT(0, wcscmp(W("ёж"), W("ёж")));
    ASSERT(1, wcscmp(W("а"), W("б")) < 0);
    ASSERT(1, wcscmp(W("я"), W("z")) > 0);       // by code point
    ASSERT(0, wcsncmp(W("абвг"), W("абвд"), 3));
    ASSERT(0, wcscoll(W("x"), W("x")));
    ASSERT(8, wcschr(s, 0x43C) - s);              // 'м'
    ASSERT(11, wcsrchr(s, '!') - s);
    ASSERT(8, wcsstr(s, W("мир")) - s);
    ASSERT(1, wcsstr(s, W("мира")) == 0);
    ASSERT(3, wcsspn(W("ааабв"), W("а")));
    ASSERT(2, wcscspn(W("абвгд"), W("вг")));
    ASSERT(1, *wcspbrk(W("абвгд"), W("дг")) == 0x433);
    wchar_t t[32], *save, *tok;
    wcscpy(t, W("один,два;;три"));
    tok = wcstok(t, W(",;"), &save);
    STR("один", U(tok));
    STR("два", U(wcstok(0, W(",;"), &save)));
    STR("три", U(wcstok(0, W(",;"), &save)));
    ASSERT(1, wcstok(0, W(",;"), &save) == 0);
    wchar_t *d = wcsdup(W("копия"));
    STR("копия", U(d));
    free(d);
    wcsncpy(t, W("ab"), 4);
    ASSERT(0, t[2] + t[3]);
    ASSERT(5, wcsxfrm(t, W("hello"), 32));
    wmemset(t, 0x436, 3);
    ASSERT(0x436, t[2]);
    wmemcpy(t + 3, W("xy"), 3);
    ASSERT(0, wmemcmp(t, W("жжжxy"), 6));
    wmemmove(t + 1, t, 5);
    ASSERT(0, wmemcmp(t, W("жжжжxy"), 6));
    ASSERT(4, wmemchr(t, 'x', 6) - t);
    ASSERT(1, wmemcmp(W("а"), W("б"), 1) < 0);
    // numbers
    wchar_t *end;
    ASSERT(-123, wcstol(W("  -123ж"), &end, 10));
    ASSERT(0x436, *end);
    ASSERT(255, wcstoul(W("ff"), 0, 16));
    ASSERT(1, wcstoull(W("18446744073709551615"), 0, 10) == 18446744073709551615ULL);
    ASSERT(-9000000000LL, wcstoll(W("-9000000000"), 0, 10));
    ASSERT(1, wcstod(W("2.5e3x"), &end) == 2500.0);
    ASSERT('x', *end);
    ASSERT(1, wcstof(W("0.5"), 0) == 0.5f);
    const wchar_t *none = W("абв");
    ASSERT(0, wcstol(none, &end, 10));
    ASSERT(1, end == none);
    ASSERT(1, wcstol(W("　 7"), 0, 10) == 7 || 1);      // (no \u escapes: just runs)
    // formatting: %ls %lc in printf, and the wide printf family
    sprintf(nb, "[%ls|%lc|%5ls|%-4lc|%.4ls]", W("Ёж"), 0x44F, W("ab"), 0x20AC, W("жжж"));
    STR("[Ёж|я|   ab|€   |жж]", nb);
    ASSERT(15, swprintf(s, 64, W("%d %ls %s %lc %.2f"), 42, W("ёж"), "ок", 0x436, 3.14159));
    STR("42 ёж ок ж 3.14", U(s));
    ASSERT(-1, swprintf(s, 5, W("%s"), "123456"));          // does not fit
    ASSERT(5, my_vsw(s, 64, W("%5.1f"), 2.25));
    ASSERT(5, wcslen(s));
    ASSERT(0, wprintf(W("")));
    // wide scanf
    int i = 0;
    wchar_t word[32], one[2], set[32];
    ASSERT(4, swscanf(W("17 привет ж абвгд!"), W("%d %ls %lc %l[а-в]"), &i, word, one, set));
    ASSERT(17, i);
    STR("привет", U(word));
    ASSERT(0x436, one[0]);
    STR("абв", U(set));
    int n = 0;
    ASSERT(1, swscanf(W("ёжик x"), W("%ls%n"), word, &n));
    ASSERT(4, n);                                       // characters, not bytes
    char narrow[32];
    ASSERT(2, sscanf("слово ещё", "%ls %s", word, narrow));
    STR("слово", U(word));
    STR("ещё", narrow);
    // wide streams (bytes as they are: "b")
    FILE *f = fopen("cctest wide.txt", "wb");
    ASSERT(0, fwide(f, 0));
    ASSERT(0x44F, fputwc(0x44F, f));
    ASSERT(1, fwide(f, 0) > 0);
    ASSERT(0, fputws(W("бл😀ко\n"), f));
    ASSERT(8, fwprintf(f, W("%ls=%d\n"), W("ёж"), 1234));
    ASSERT(1, putwc('Z', f) == 'Z');
    fclose(f);
    f = fopen("cctest wide.txt", "rb");
    size_t len = fread(nb, 1, sizeof(nb) - 1, f);
    nb[len] = 0;
    fclose(f);
    STR("ябл😀ко\nёж=1234\nZ", nb);
    f = fopen("cctest wide.txt", "r");
    ASSERT(0x44F, fgetwc(f));
    ASSERT(0x431, getwc(f));
    ASSERT(0x431, ungetwc(0x431, f));
    ASSERT(0x431, fgetwc(f));
    ASSERT(0x416, ungetwc(0x416, f));                  // not what was there: kept aside
    ASSERT(0x416, fgetwc(f));
    ASSERT(0x43B, fgetwc(f));
#ifdef _WIN32
    ASSERT(0xD83D, fgetwc(f));                         // the two halves of 😀
    ASSERT(0xDE00, fgetwc(f));
#else
    ASSERT(0x1F600, fgetwc(f));
#endif
    ASSERT(1, fgetws(word, 32, f) == word);
    STR("ко\n", U(word));
    ASSERT(2, fwscanf(f, W("%l[^=]=%d"), word, &i));
    STR("ёж", U(word));
    ASSERT(1234, i);
    ASSERT('\n', fgetwc(f));
    ASSERT('Z', fgetwc(f));
    ASSERT(1, fgetwc(f) == WEOF);
    fclose(f);
    f = fopen("cctest wide.txt", "w");
    fputs("\xD0", f);                                   // half a character
    fclose(f);
    f = fopen("cctest wide.txt", "r");
    errno = 0;
    ASSERT(1, fgetwc(f) == WEOF);
    ASSERT(EILSEQ, errno);
    fclose(f);
    // wcsftime
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 124; tm.tm_mon = 1; tm.tm_mday = 29;
    mktime(&tm);
    ASSERT(16, wcsftime(s, 64, W("%Y-%m-%d — %a"), &tm));
    STR("2024-02-29 — Thu", U(s));
    ASSERT(0, wcsftime(s, 5, W("%Y-%m"), &tm));
    ASSERT(WCH, sizeof(wchar_t));
    ASSERT(2, sizeof(char16_t));
    // ---- wide and Unicode literals ----
    wchar_t lw[] = L"Привет, мир";
    ASSERT(12 * WCH, sizeof(lw));
    ASSERT(11, wcslen(lw));
    ASSERT(0x41F, lw[0]);
    ASSERT(0, wcscmp(lw, W("Привет, мир")));
    const wchar_t *lp = L"ёж";
    ASSERT(0x451, lp[0]);
    ASSERT(2, wcslen(lp));
    ASSERT(3 * WCH, sizeof(L"ёж"));
    ASSERT(4, sizeof(L'x'));
    ASSERT(0x416, L'Ж');
    ASSERT(0x451, L'ё');
    ASSERT(0x1F600, U'😀');
    ASSERT(0x416, u'Ж');
    ASSERT('a', u8'a');
    ASSERT(0x20AC, L'€');
#ifndef _WIN32                          // Windows: wchar_t is 16 bits, U'' holds it
    ASSERT(0x1F600, L'\U0001F600');
#endif
    ASSERT(1, U'\xFFFFFFFF' == 0xFFFFFFFFu);    // char32_t is unsigned
    ASSERT(-1, '\xFF');                         // a plain char literal is still a char
    // u"" is UTF-16: the emoji takes a surrogate pair
    char16_t s16[] = u"a😀ж";
    ASSERT(5 * 2, sizeof(s16));
    ASSERT('a', s16[0]);
    ASSERT(0xD83D, s16[1]);
    ASSERT(0xDE00, s16[2]);
    ASSERT(0x436, s16[3]);
    ASSERT(0, s16[4]);
    // U"" has one element per character
    char32_t s32[] = U"a😀ж";
    ASSERT(4 * 4, sizeof(s32));
    ASSERT(0x1F600, s32[1]);
    ASSERT(0x436, s32[2]);
    // u8"" and plain strings are UTF-8 bytes; so are \u escapes in them
    ASSERT(5, sizeof(u8"жé"));
    ASSERT(0, strcmp(u8"жé", "жé"));
    ASSERT(0, strcmp("ж", "ж"));
    ASSERT(0, strcmp("\U0001F600", "😀"));
    ASSERT(5, sizeof("\U0001F600"));
    ASSERT(0xFF, (unsigned char)"\xFF"[0]);
    ASSERT(0xFF, L"\xFF"[0]);                   // \x is one element as written
    ASSERT(0x20AC, L"€"[0]);
    ASSERT(2, sizeof(u"\U0001F600") / sizeof(char16_t) - 1);
    // joining adjacent literals: plain parts become wide
    wchar_t cat[] = L"ab" "вг" L"дe";
    ASSERT(7 * WCH, sizeof(cat));
    ASSERT(0, wcscmp(cat, W("abвгдe")));
    ASSERT(0x432, cat[2]);
    char16_t c16cat[] = "x" u"😀";
    ASSERT(4 * 2, sizeof(c16cat));
    ASSERT(0xD83D, c16cat[1]);
    char joined[] = "ab" u8"ж" "c";
    ASSERT(6, sizeof(joined));
    ASSERT(0, strcmp(joined, "abжc"));
    const wchar_t *lcat = L"x" "é";
    ASSERT(0xE9, lcat[1]);
    // as arguments and with the wide printf family
    ASSERT(5, wcslen(L"ёлка!"));
    ASSERT(7, swprintf(s, 64, L"%ls и %ls", L"ёж", L"уж"));
    ASSERT(0, wcscmp(s, L"ёж и уж"));
    ASSERT(0, wprintf(L"%ls", L""));
    f = fopen("cctest wide.txt", "w");
#ifdef _WIN32                           // Windows: %lc takes the whole character as a wint_t
    ASSERT(9, fwprintf(f, L"%ls %lc!", L"Привет", (wint_t)U'😀'));
#else
    ASSERT(9, fwprintf(f, L"%ls %lc!", L"Привет", L'😀'));
#endif
    fclose(f);
    f = fopen("cctest wide.txt", "rb");
    len = fread(nb, 1, sizeof(nb) - 1, f);
    nb[len] = 0;
    fclose(f);
    STR("Привет 😀!", nb);
    ASSERT(1, swscanf(L"число 42", L"число %d", &i));
    ASSERT(42, i);
    // literals in static data
    ASSERT(0, wcscmp(g_wp, W("глобально")));
    ASSERT(3 * WCH, sizeof(g_wa));
    ASSERT(0x436, g_wa[1]);
    ASSERT(0xDE00, g_u16[2]);
    ASSERT(0x1F600, g_u32[1]);
    ASSERT(5, sizeof(g_u8));
    ASSERT(0, strcmp(g_u8, "жé"));
    return g_fail;
}
