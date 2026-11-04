// Locales of a Windows program (WinCC): runtime.c's setlocale asks here what a
// locale is called and what it says -- the names of months and days, the decimal
// point, the order of words (strcoll), the name of the time zone. All from Windows.
// A locale is named "ru_RU.UTF-8" in the library and "ru-RU" for Windows.
#include "rtwin.h"

typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef struct { unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct {
    long Bias;
    WCHAR StandardName[32];
    SYSTEMTIME StandardDate;
    long StandardBias;
    WCHAR DaylightName[32];
    SYSTEMTIME DaylightDate;
    long DaylightBias;
} TIME_ZONE_INFORMATION;

int GetUserDefaultLocaleName(WCHAR *name, int cap);
int GetLocaleInfoEx(const WCHAR *loc, DWORD type, WCHAR *out, int cap);
BOOL IsValidLocaleName(const WCHAR *name);
BOOL EnumSystemLocalesEx(BOOL (*fn)(WCHAR *name, DWORD flags, i64 param), DWORD flags, i64 param, void *reserved);
int CompareStringEx(const WCHAR *loc, DWORD flags, const WCHAR *a, int na, const WCHAR *b, int nb,
                    void *version, void *reserved, i64 param);
int LCMapStringEx(const WCHAR *loc, DWORD flags, const WCHAR *src, int n, WCHAR *out, int cap,
                  void *version, void *reserved, i64 param);
DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION *tz);

void rt_free(void *p);

#define LOCALE_SENGLISHLANGUAGENAME 0x1001
#define LOCALE_SENGLISHCOUNTRYNAME 0x1002
#define LOCALE_SDECIMAL 0x0E
#define LOCALE_STHOUSAND 0x0F

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

// "ru-RU" (UTF-16) -> "ru_RU.UTF-8"
static int name_out(const WCHAR *w, char *out, int cap)
{
    int n = 0;
    for (; w[n] && n < cap - 7; n++)
        out[n] = w[n] == '-' ? '_' : (char)w[n];
    const char *tail = ".UTF-8";
    for (int i = 0; tail[i] && n < cap - 1; i++)
        out[n++] = tail[i];
    out[n] = 0;
    return n;
}

// "ru_RU.UTF-8" -> "ru-RU" (UTF-16)
static void name_in(const char *loc, WCHAR *w, int cap)
{
    int n = 0;
    for (; loc[n] && loc[n] != '.' && n < cap - 1; n++)
        w[n] = loc[n] == '_' ? '-' : (WCHAR)(u8)loc[n];
    w[n] = 0;
}

// "Russian_Russia.1251": the language and the country in English, as Windows has them
typedef struct { const char *lang, *country; int ln, cn; WCHAR found[85]; int how; } Search;

static int same(const WCHAR *w, const char *s, int n)
{
    int i = 0;
    for (; i < n && w[i]; i++)
        if (lower(w[i]) != lower((u8)s[i])) return 0;
    return i == n && !w[i];
}

static BOOL by_english(WCHAR *name, DWORD flags, i64 param)
{
    (void)flags;
    Search *s = (Search *)param;
    WCHAR lang[128], country[128];
    if (!GetLocaleInfoEx(name, LOCALE_SENGLISHLANGUAGENAME, lang, 128) || !same(lang, s->lang, s->ln))
        return 1;
    int with_country = 0;
    for (int i = 0; name[i]; i++)
        if (name[i] == '-') with_country = 1;
    int how = 1;
    if (s->cn) {
        if (!GetLocaleInfoEx(name, LOCALE_SENGLISHCOUNTRYNAME, country, 128) || !same(country, s->country, s->cn))
            return 1;
        how = 3;
    } else if (with_country) {
        how = 2;                    // "Russian" alone: a locale with its country is better than none
        // the language's own country first: ru-RU for Russian, de-DE for German
        int k = 0;
        while (name[k] && name[k] != '-') k++;
        if (name[k] == '-' && lower(name[k + 1]) == lower(name[0]) && lower(name[k + 2]) == lower(name[1]))
            how = 4;
    }
    if (how > s->how) {
        s->how = how;
        int i = 0;
        for (; name[i] && i < 84; i++) s->found[i] = name[i];
        s->found[i] = 0;
    }
    return s->how < 3;              // an exact one: stop
}

// Is it a locale Windows has? (IsValidLocaleName says yes to any well-formed tag: "xx-QQ" too)
typedef struct { const WCHAR *want; int found; } Known;

static BOOL by_name(WCHAR *name, DWORD flags, i64 param)
{
    (void)flags;
    Known *k = (Known *)param;
    int i = 0;
    while (name[i] && lower(name[i]) == lower(k->want[i])) i++;
    if (!name[i] && !k->want[i]) {
        k->found = 1;
        return 0;
    }
    return 1;
}

static int known(const WCHAR *w)
{
    Known k;
    k.want = w;
    k.found = 0;
    EnumSystemLocalesEx(by_name, 0, (i64)&k, 0);
    return k.found;
}

int cc_locale_find(const char *name, char *out, int cap)
{
    WCHAR w[85];
    if (!*name || name[0] == '.') {         // "": the user's locale; ".UTF-8" the same, in UTF-8
        if (!GetUserDefaultLocaleName(w, 85)) return 0;
        return name_out(w, out, cap);
    }
    name_in(name, w, 85);
    if (w[0] && IsValidLocaleName(w) && known(w))       // ru_RU.UTF-8, ru-RU, ru
        return name_out(w, out, cap);
    Search s;
    s.lang = name;
    s.ln = 0;
    while (name[s.ln] && name[s.ln] != '_' && name[s.ln] != '.') s.ln++;
    s.country = name + s.ln + (name[s.ln] == '_');
    s.cn = 0;
    if (name[s.ln] == '_')
        while (s.country[s.cn] && s.country[s.cn] != '.') s.cn++;
    s.how = 0;
    s.found[0] = 0;
    EnumSystemLocalesEx(by_english, 0, (i64)&s, 0);
    if (!s.how) return 0;
    return name_out(s.found, out, cap);
}

// a month (0), a short month (1), a day of the week (2, 0 = Sunday), a short day (3)
int cc_locale_text(const char *loc, int what, int i, char *out, int cap)
{
    WCHAR w[85], text[80];
    name_in(loc, w, 85);
    DWORD type;
    if (what == 0) type = 0x38 + (DWORD)i;                  // LOCALE_SMONTHNAME1
    else if (what == 1) type = 0x44 + (DWORD)i;             // LOCALE_SABBREVMONTHNAME1
    else if (what == 2) type = i == 0 ? 0x30 : 0x2A + (DWORD)i - 1;     // LOCALE_SDAYNAME1 is Monday
    else type = i == 0 ? 0x37 : 0x31 + (DWORD)i - 1;        // LOCALE_SABBREVDAYNAME1
    int n = GetLocaleInfoEx(w, type, text, 80);
    if (n <= 1) return 0;
    return cc_utf8(text, -1, out, cap);
}

// the decimal point (0) or the thousands separator (1): its first character (a space for U+00A0)
int cc_locale_char(const char *loc, int what)
{
    WCHAR w[85], text[8];
    name_in(loc, w, 85);
    if (GetLocaleInfoEx(w, what ? LOCALE_STHOUSAND : LOCALE_SDECIMAL, text, 8) <= 1)
        return what ? 0 : '.';
    if (text[0] == 0xA0 || text[0] == 0x202F) return ' ';
    return text[0] < 128 ? text[0] : (what ? ' ' : '.');
}

int cc_collate(const char *loc, const char *a, const char *b)
{
    WCHAR w[85];
    name_in(loc, w, 85);
    WCHAR *wa = cc_wide(a, -1, 0), *wb = cc_wide(b, -1, 0);
    int r = wa && wb ? CompareStringEx(w, 0, wa, -1, wb, -1, 0, 0, 0) : 0;
    rt_free(wa);
    rt_free(wb);
    if (!r) {                                       // Windows could not: byte by byte
        while (*a && *a == *b) { a++; b++; }
        return (u8)*a - (u8)*b;
    }
    return r - 2;                                   // CSTR_LESS_THAN 1, EQUAL 2, GREATER 3
}

int cc_collate_w(const char *loc, const unsigned short *a, const unsigned short *b)
{
    WCHAR w[85];
    name_in(loc, w, 85);
    int r = CompareStringEx(w, 0, a, -1, b, -1, 0, 0, 0);
    if (!r) {
        while (*a && *a == *b) { a++; b++; }
        return (int)*a - (int)*b;
    }
    return r - 2;
}

// strxfrm: the sort key of Windows (bytes; strcmp of two keys is strcoll of the texts); its size with the NUL
size_t cc_sortkey(const char *loc, const char *s, char *out, size_t cap)
{
    WCHAR w[85];
    name_in(loc, w, 85);
    WCHAR *ws = cc_wide(s, -1, 0);
    if (!ws) return 0;
    int need = LCMapStringEx(w, 0x400, ws, -1, 0, 0, 0, 0, 0);     // LCMAP_SORTKEY: the size in bytes
    if (need > 0 && (size_t)need <= cap && out)
        LCMapStringEx(w, 0x400, ws, -1, (WCHAR *)out, need, 0, 0, 0);
    rt_free(ws);
    return need > 0 ? (size_t)need : 0;
}

// strftime's %Z: the name Windows gives the zone (in winter, or in summer)
int cc_zone_name(int summer, char *out, int cap)
{
    TIME_ZONE_INFORMATION tz;
    if (GetTimeZoneInformation(&tz) == 0xFFFFFFFFu) {
        out[0] = 0;
        return 0;
    }
    return cc_utf8(summer ? tz.DaylightName : tz.StandardName, -1, out, cap);
}
