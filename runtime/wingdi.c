// Text drawing of GDI for UTF-8 programs (WinCC).
//
// The manifest makes UTF-8 the code page of a WinCC program, and user32
// (titles, menus, buttons, edits) takes the UTF-8 of the ...A functions.
// GDI does not: it reads A-text in the system's code page. windows.h sends
// TextOutA, DrawTextA and the rest here; the text becomes UTF-16 and goes to
// the W function. A program that does not draw text leaves all this out.
#include "rtwin.h"

typedef void *HDC, *HFONT;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned short WCHAR;
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int cx, cy; } SIZE;

int  MultiByteToWideChar(unsigned cp, unsigned flags, const char *s, int n, WCHAR *out, int cap);
BOOL TextOutW(HDC dc, int x, int y, const WCHAR *s, int n);
BOOL ExtTextOutW(HDC dc, int x, int y, UINT opts, const RECT *r, const WCHAR *s, UINT n, const int *dx);
int  DrawTextW(HDC dc, const WCHAR *s, int n, RECT *r, UINT format);
int  DrawTextExW(HDC dc, WCHAR *s, int n, RECT *r, UINT format, void *params);
BOOL GetTextExtentPoint32W(HDC dc, const WCHAR *s, int n, SIZE *size);
long TabbedTextOutW(HDC dc, int x, int y, const WCHAR *s, int n, int ntabs, const int *tabs, int origin);
HFONT CreateFontW(int h, int w, int esc, int orient, int weight, unsigned long italic, unsigned long underline,
                  unsigned long strike, unsigned long charset, unsigned long out_prec, unsigned long clip_prec,
                  unsigned long quality, unsigned long pitch, const WCHAR *face);

HFONT CreateFontIndirectW(const void *lf);
int  GetTextFaceW(HDC dc, int cap, WCHAR *face);
BOOL GetTextExtentExPointW(HDC dc, const WCHAR *s, int n, int max, int *fit, int *dx, SIZE *size);
int  WideCharToMultiByte(unsigned cp, unsigned flags, const WCHAR *s, int n, char *out, int cap, const char *def, int *used);

typedef struct {
    long height, width, escapement, orientation, weight;
    u8 italic, underline, strike, charset, out_prec, clip_prec, quality, pitch;
    char face[32];
} LOGFONTA_;
typedef struct {
    long height, width, escapement, orientation, weight;
    u8 italic, underline, strike, charset, out_prec, clip_prec, quality, pitch;
    WCHAR face[32];
} LOGFONTW_;

void *rt_malloc(size_t n);
void rt_free(void *p);

// UTF-16 of n bytes (n < 0: up to the NUL) into small, or the heap when it is long; its length in *len
static WCHAR *utf16(const char *s, int n, WCHAR *small, int cap, int *len)
{
    if (!s) { *len = 0; small[0] = 0; return small; }
    if (n < 0) { n = 0; while (s[n]) n++; }
    int k = n ? MultiByteToWideChar(65001, 0, s, n, 0, 0) : 0;
    WCHAR *w = k < cap ? small : rt_malloc(((size_t)k + 1) * sizeof(WCHAR));
    if (!w) { *len = 0; small[0] = 0; return small; }
    if (k) MultiByteToWideChar(65001, 0, s, n, w, k);
    w[k] = 0;
    *len = k;
    return w;
}

static void done(WCHAR *w, WCHAR *small) { if (w != small) rt_free(w); }

BOOL wcc_TextOutA(HDC dc, int x, int y, const char *s, int n)
{
    WCHAR small[256]; int len;
    WCHAR *w = utf16(s, n, small, 256, &len);
    BOOL r = TextOutW(dc, x, y, w, len);
    done(w, small);
    return r;
}

BOOL wcc_ExtTextOutA(HDC dc, int x, int y, UINT opts, const RECT *rc, const char *s, UINT n, const int *dx)
{
    WCHAR small[256]; int len;
    WCHAR *w = utf16(s, (int)n, small, 256, &len);
    BOOL r = ExtTextOutW(dc, x, y, opts, rc, w, (UINT)len, dx);     // dx counts characters: right for Cyrillic
    done(w, small);
    return r;
}

int wcc_DrawTextA(HDC dc, const char *s, int n, RECT *rc, UINT format)
{
    WCHAR small[512]; int len;
    WCHAR *w = utf16(s, n, small, 512, &len);
    int r = DrawTextW(dc, w, len, rc, format);
    done(w, small);
    return r;
}

int wcc_DrawTextExA(HDC dc, char *s, int n, RECT *rc, UINT format, void *params)
{
    WCHAR small[512]; int len;
    WCHAR *w = utf16(s, n, small, 512, &len);
    int r = DrawTextExW(dc, w, len, rc, format & ~0x10000u, params);   // not DT_MODIFYSTRING: the text stays as it was
    done(w, small);
    return r;
}

BOOL wcc_GetTextExtentPoint32A(HDC dc, const char *s, int n, SIZE *size)
{
    WCHAR small[256]; int len;
    WCHAR *w = utf16(s, n, small, 256, &len);
    BOOL r = GetTextExtentPoint32W(dc, w, len, size);
    done(w, small);
    return r;
}

long wcc_TabbedTextOutA(HDC dc, int x, int y, const char *s, int n, int ntabs, const int *tabs, int origin)
{
    WCHAR small[256]; int len;
    WCHAR *w = utf16(s, n, small, 256, &len);
    long r = TabbedTextOutW(dc, x, y, w, len, ntabs, tabs, origin);
    done(w, small);
    return r;
}

HFONT wcc_CreateFontA(int h, int wd, int esc, int orient, int weight, unsigned long italic, unsigned long underline,
                      unsigned long strike, unsigned long charset, unsigned long out_prec, unsigned long clip_prec,
                      unsigned long quality, unsigned long pitch, const char *face)
{
    WCHAR small[64]; int len;
    WCHAR *w = utf16(face, -1, small, 64, &len);
    HFONT f = CreateFontW(h, wd, esc, orient, weight, italic, underline, strike, charset, out_prec, clip_prec,
                          quality, pitch, face ? w : 0);
    done(w, small);
    return f;
}

HFONT wcc_CreateFontIndirectA(const LOGFONTA_ *lf)
{
    LOGFONTW_ w;
    const u8 *from = (const u8 *)lf;
    u8 *to = (u8 *)&w;
    for (int i = 0; i < 28; i++)                    // the numbers and the bytes are the same
        to[i] = from[i];
    int k = MultiByteToWideChar(65001, 0, lf->face, -1, w.face, 32);
    if (k <= 0) w.face[0] = 0;
    w.face[31] = 0;
    return CreateFontIndirectW(&w);
}

int wcc_GetTextFaceA(HDC dc, int cap, char *face)
{
    WCHAR w[64];
    int n = GetTextFaceW(dc, 64, w);
    if (!n) return 0;
    if (!face) return WideCharToMultiByte(65001, 0, w, -1, 0, 0, 0, 0);
    int k = WideCharToMultiByte(65001, 0, w, -1, face, cap, 0, 0);
    if (k <= 0 && cap > 0) face[0] = 0;
    return k > 0 ? k - 1 : 0;
}

// fit and dx count characters of the UTF-16 text (a Cyrillic letter is one, as it should be)
BOOL wcc_GetTextExtentExPointA(HDC dc, const char *s, int n, int max, int *fit, int *dx, SIZE *size)
{
    WCHAR small[256]; int len;
    WCHAR *w = utf16(s, n, small, 256, &len);
    BOOL r = GetTextExtentExPointW(dc, w, len, max, fit, dx, size);
    done(w, small);
    return r;
}

BOOL wcc_GetTextExtentPointA(HDC dc, const char *s, int n, SIZE *size)
{
    return wcc_GetTextExtentPoint32A(dc, s, n, size);
}
