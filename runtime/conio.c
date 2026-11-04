// conio.h of a Windows program (WinCC): a key at once, without Enter (_getch),
// is a key waiting (_kbhit), and text straight to the console (_putch, _cputs,
// _cprintf). Text is UTF-8: a letter beyond ASCII comes from _getch byte by byte;
// an arrow or F1 comes as two: 0 or 0xE0, then the key's scan code.
#include "rtwin.h"

typedef void *HANDLE;
typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef struct {
    BOOL bKeyDown;
    unsigned short wRepeatCount, wVirtualKeyCode, wVirtualScanCode;
    WCHAR UnicodeChar;
    DWORD dwControlKeyState;
} KEY_EVENT;
typedef struct {
    unsigned short EventType;
    union { KEY_EVENT Key; u8 other[16]; } Event;
} INPUT_RECORD;

HANDLE GetStdHandle(DWORD which);
BOOL ReadConsoleInputW(HANDLE h, INPUT_RECORD *rec, DWORD n, DWORD *got);
BOOL PeekConsoleInputW(HANDLE h, INPUT_RECORD *rec, DWORD n, DWORD *got);
BOOL GetConsoleMode(HANDLE h, DWORD *mode);
BOOL SetConsoleMode(HANDLE h, DWORD mode);
int rt_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
void *rt_malloc(size_t n);
void rt_free(void *p);
int rt_fflush(void *f);

static u8 g_pending[8];             // what _getch gives next: the rest of a letter, a scan code
static int g_npending;
static int g_ungot = -1;

static HANDLE con_in(void) { return GetStdHandle((DWORD)-10); }

// the scan codes of the keys that have no character (the second of two, after 0 or 0xE0)
static int extended(KEY_EVENT *k)
{
    int vk = k->wVirtualKeyCode;
    if ((vk >= 0x21 && vk <= 0x2E) || (vk >= 0x5B && vk <= 0x5D))    // PgUp..Del, the Win keys
        return 0xE0;
    if (vk >= 0x70 && vk <= 0x87)                                    // F1..F24
        return 0;
    return -1;
}

// the next key pressed (UTF-16 in *ch, or a key without a character: *ch 0 and *scan)
static int next_key(WCHAR *ch, int *lead, int *scan)
{
    HANDLE h = con_in();
    for (;;) {
        INPUT_RECORD r;
        DWORD got = 0;
        if (!ReadConsoleInputW(h, &r, 1, &got) || !got)
            return 0;
        if (r.EventType != 1 || !r.Event.Key.bKeyDown)
            continue;
        KEY_EVENT *k = &r.Event.Key;
        if (k->UnicodeChar) {
            *ch = k->UnicodeChar;
            return 1;
        }
        int e = extended(k);
        if (e >= 0) {
            *ch = 0;
            *lead = e;
            *scan = k->wVirtualScanCode;
            return 1;
        }
    }
}

static WCHAR g_high;                // the first half of a pair (a character beyond U+FFFF)

int rt__getch(void)
{
    if (g_ungot >= 0) {
        int c = g_ungot;
        g_ungot = -1;
        return c;
    }
    if (g_npending) {
        int c = g_pending[0];
        for (int i = 1; i < g_npending; i++) g_pending[i - 1] = g_pending[i];
        g_npending--;
        return c;
    }
    rt_fflush(0);
    WCHAR ch;
    int lead = 0, scan = 0;
    for (;;) {
        if (!next_key(&ch, &lead, &scan))
            return -1;
        if (!ch) {
            g_pending[0] = (u8)scan;
            g_npending = 1;
            return lead;
        }
        if (ch >= 0xD800 && ch < 0xDC00) {
            g_high = ch;
            continue;
        }
        break;
    }
    WCHAR pair[3] = { ch, 0, 0 };
    if (ch >= 0xDC00 && ch < 0xE000 && g_high) {
        pair[0] = g_high;
        pair[1] = ch;
        g_high = 0;
    }
    char utf[8];
    int n = cc_utf8(pair, -1, utf, sizeof(utf));
    if (n <= 0)
        return '?';
    for (int i = 1; i < n; i++)
        g_pending[g_npending++] = (u8)utf[i];
    return (u8)utf[0];
}

// one UTF-16 character, without echo (the wide _getwch)
unsigned short rt__getwch(void)
{
    WCHAR ch;
    int lead = 0, scan = 0;
    rt_fflush(0);
    if (!next_key(&ch, &lead, &scan))
        return 0xFFFF;
    if (!ch) {
        g_pending[0] = (u8)scan;
        g_npending = 1;
        return (unsigned short)lead;
    }
    return ch;
}

void rt__putch_bytes(const char *s, int n);

int rt__getche(void)
{
    int c = rt__getch();
    if (c > 0 && c != 0xE0) {
        char b = (char)c;
        rt__putch_bytes(&b, 1);
    }
    return c;
}

int rt__ungetch(int c)
{
    if (g_ungot >= 0) return -1;
    g_ungot = c & 0xFF;
    return g_ungot;
}

int rt__kbhit(void)
{
    if (g_npending || g_ungot >= 0)
        return 1;
    HANDLE h = con_in();
    for (;;) {
        INPUT_RECORD r;
        DWORD got = 0;
        if (!PeekConsoleInputW(h, &r, 1, &got) || !got)
            return 0;
        if (r.EventType == 1 && r.Event.Key.bKeyDown &&
            (r.Event.Key.UnicodeChar || extended(&r.Event.Key) >= 0))
            return 1;
        ReadConsoleInputW(h, &r, 1, &got);      // not a key someone waits for: away with it
    }
}

// ---- output: UTF-8 bytes, a letter may come byte by byte ----

static char g_out[16];
static int g_nout;

void rt__putch_bytes(const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        g_out[g_nout++] = s[i];
        u8 first = (u8)g_out[0];
        int need = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
        if (g_nout >= need || g_nout >= 15) {
            g_out[g_nout] = 0;
            cc_print(g_out);
            g_nout = 0;
        }
    }
}

int rt__putch(int c)
{
    char b = (char)c;
    rt_fflush(0);
    rt__putch_bytes(&b, 1);
    return c & 0xFF;
}

unsigned short rt__putwch(unsigned short c)
{
    WCHAR w[2] = { c, 0 };
    char utf[8];
    rt_fflush(0);
    if (cc_utf8(w, -1, utf, sizeof(utf)) > 0)
        cc_print(utf);
    return c;
}

int rt__cputs(const char *s)
{
    rt_fflush(0);
    cc_print(s);
    return 0;
}

int rt__cprintf(const char *fmt, ...)
{
    char small[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vsnprintf(small, sizeof(small), fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    rt_fflush(0);
    if (n < (int)sizeof(small)) {
        cc_print(small);
        return n;
    }
    char *big = rt_malloc((size_t)n + 1);
    if (!big) return -1;
    va_start(ap, fmt);
    rt_vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    cc_print(big);
    rt_free(big);
    return n;
}

#define F(name, fn) ".alias " name ", " #fn "\n"
asm(
    F("_getch", rt__getch) F("getch", rt__getch) F("_getche", rt__getche) F("getche", rt__getche)
    F("_getwch", rt__getwch) F("_ungetch", rt__ungetch) F("ungetch", rt__ungetch)
    F("_kbhit", rt__kbhit) F("kbhit", rt__kbhit) F("_putch", rt__putch) F("putch", rt__putch)
    F("_putwch", rt__putwch) F("_cputs", rt__cputs) F("cputs", rt__cputs)
    F("_cprintf", rt__cprintf) F("cprintf", rt__cprintf)
);
#undef F
