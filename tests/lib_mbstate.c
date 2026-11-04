// mbstate_t between calls: a UTF-8 character begun by mbrtowc is finished by
// mbsrtowcs (and mbrtowc, mbrlen, mbrtoc16/32); counting leaves the state
#include "test.h"
#include <string.h>
#include <wchar.h>
#include <uchar.h>
#include <errno.h>

int main(void)
{
    const char *text = "жук€";                  // D0 B6, D1 83, D0 BA, E2 82 AC
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    wchar_t wc = 0, w[16];
    // the first byte of "ж" alone
    ASSERT(-2, (long long)mbrtowc(&wc, text, 1, &st));
    ASSERT(0, mbsinit(&st));
    // counting only (dst NULL): the state is left for the real run
    const char *src = text + 1;
    ASSERT(4, (long long)mbsrtowcs(NULL, &src, 0, &st));
    ASSERT(1, src == text + 1);
    ASSERT(0, mbsinit(&st));
    // the rest: "ж" finished from the state, then "ук€"
    size_t n = mbsrtowcs(w, &src, 16, &st);
    ASSERT(4, (long long)n);
    ASSERT(1, src == 0);
    ASSERT(1, mbsinit(&st));
    ASSERT(0x436, w[0]);
    ASSERT(0x443, w[1]);
    ASSERT(0x43A, w[2]);
    ASSERT(0x20AC, w[3]);
    ASSERT(0, w[4]);
    // "€" cut after two bytes, finished in a second call with a small dst
    memset(&st, 0, sizeof(st));
    ASSERT(-2, (long long)mbrtowc(&wc, "\xE2\x82", 2, &st));
    src = "\xAC!x";
    ASSERT(2, (long long)mbsrtowcs(w, &src, 2, &st));
    ASSERT(0x20AC, w[0]);
    ASSERT('!', w[1]);
    ASSERT(0, strcmp(src, "x"));                 // stopped at len, between characters
    ASSERT(1, mbsinit(&st));
    // a begun character that the next bytes do not finish
    memset(&st, 0, sizeof(st));
    mbrtowc(&wc, "\xD0", 1, &st);
    src = "ab";
    errno = 0;
    ASSERT(-1, (long long)mbsrtowcs(w, &src, 16, &st));
    ASSERT(EILSEQ, errno);
    ASSERT(1, mbsinit(&st));
    // without a state of its own (ps NULL) the call keeps one inside
    src = "ok";
    ASSERT(2, (long long)mbsrtowcs(w, &src, 16, NULL));
    // mbrlen and mbrtoc32 carry their state the same way
    memset(&st, 0, sizeof(st));
    ASSERT(-2, (long long)mbrlen("\xF0\x9F", 2, &st));
    ASSERT(2, (long long)mbrlen("\x98\x80", 2, &st));    // 😀 U+1F600
    char32_t c32 = 0;
    memset(&st, 0, sizeof(st));
    ASSERT(-2, (long long)mbrtoc32(&c32, "\xF0\x9F\x98", 3, &st));
    ASSERT(1, (long long)mbrtoc32(&c32, "\x80", 1, &st));
    ASSERT(0x1F600, c32);
    char16_t c16 = 0;
    memset(&st, 0, sizeof(st));
    ASSERT(-2, (long long)mbrtoc16(&c16, "\xF0", 1, &st));
    ASSERT(3, (long long)mbrtoc16(&c16, "\x9F\x98\x80", 3, &st));
    ASSERT(0xD83D, c16);
    ASSERT(-3, (long long)mbrtoc16(&c16, "", 0, &st));
    ASSERT(0xDE00, c16);
    // wcsrtombs: the NUL written leaves the initial state; a small dst stops between characters
    const wchar_t *ws = L"Ёж";
    char out[8];
    memset(&st, 0, sizeof(st));
    ASSERT(2, (long long)wcsrtombs(out, &ws, 3, &st));
    ASSERT(1, ws != 0 && *ws == 0x436);
    ASSERT(2, (long long)wcsrtombs(out + 2, &ws, 6, &st));
    ASSERT(1, ws == 0);
    ASSERT(0, strcmp(out, "Ёж"));
    ASSERT(1, mbsinit(&st));
    // wcrtomb with s NULL makes a state initial again
    memset(&st, 0, sizeof(st));
    mbrtowc(&wc, "\xD0", 1, &st);
    ASSERT(1, (long long)wcrtomb(NULL, L'x', &st));
    ASSERT(1, mbsinit(&st));
    return g_fail;
}
