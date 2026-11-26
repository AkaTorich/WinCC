// Growable arrays and strings (UTF-8 <-> UTF-16) for the C implementation.
#include "el_internal.h"

void *vgrow(void *a, int *cap, int need, size_t size)
{
    int c = *cap > 0 ? *cap : 8;
    while (c < need) c *= 2;
    if (c != *cap) {
        void *p = realloc(a, (size_t)c * size);
        if (!p) abort();
        if (c > *cap) memset((char *)p + (size_t)*cap * size, 0, (size_t)(c - *cap) * size);
        a = p;
        *cap = c;
    }
    return a;
}

char *sdup(const char *s)
{
    size_t n;
    char *d;
    if (!s || !*s) return NULL;
    n = strlen(s);
    d = (char *)malloc(n + 1);
    if (!d) abort();
    memcpy(d, s, n + 1);
    return d;
}

void sset(char **dst, const char *s)
{
    char *n = sdup(s);
    free(*dst);
    *dst = n;
}

int seq(const char *a, const char *b)
{
    return strcmp(S(a), S(b)) == 0;
}

WCHAR *wdup(const char *s)
{
    int n;
    WCHAR *w;
    if (!s) s = "";
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) n = 1;
    w = (WCHAR *)malloc((size_t)n * sizeof(WCHAR));
    if (!w) abort();
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n)) w[0] = 0;
    return w;
}

char *udup(const WCHAR *w)
{
    int n;
    char *s;
    if (!w) w = L"";
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) n = 1;
    s = (char *)malloc((size_t)n);
    if (!s) abort();
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL)) s[0] = 0;
    return s;
}

int wlen(const WCHAR *w)
{
    return w ? (int)wcslen(w) : 0;
}
