// What every part of the net library uses: the error of each thread in words, the
// start of the sockets, UTF-8 <-> UTF-16, addresses written with %XX, a growing buffer.
#include "net_int.h"

// ---- what each thread keeps ----
typedef struct {
    char error[512];
    NetProgress progress;
    void *progress_user;
} NetThread;

static SRWLOCK g_lock = SRWLOCK_INIT;
static DWORD g_tls = TLS_OUT_OF_INDEXES;
static int g_wsa;                                   // 1 = WSAStartup done, -1 = failed
static int g_timeout = 30000;

static NetThread *thread_state(void)
{
    if (g_tls == TLS_OUT_OF_INDEXES) {
        AcquireSRWLockExclusive(&g_lock);
        if (g_tls == TLS_OUT_OF_INDEXES)
            g_tls = TlsAlloc();
        ReleaseSRWLockExclusive(&g_lock);
    }
    NetThread *t = TlsGetValue(g_tls);
    if (!t) {
        t = calloc(1, sizeof *t);
        if (!t) {
            static NetThread spare;             // no memory: one shared place is better than none
            return &spare;
        }
        TlsSetValue(g_tls, t);
    }
    return t;
}

const char *net_error(void)
{
    return thread_state()->error;
}

void net_set_error(const char *text)
{
    NetThread *t = thread_state();
    snprintf(t->error, sizeof t->error, "%s", text);
}

void net_set_errorf(const char *format, ...)
{
    NetThread *t = thread_state();
    va_list ap;
    va_start(ap, format);
    vsnprintf(t->error, sizeof t->error, format, ap);
    va_end(ap);
}

// the words Windows has for a code: sockets and the system, or WinHTTP (12000...)
void net_set_system_error(const char *what, DWORD code)
{
    wchar_t text[400];
    DWORD n = 0;
    DWORD flags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK;
    if (code >= WINHTTP_ERROR_BASE && code < WINHTTP_ERROR_BASE + 1000) {
        HMODULE m = GetModuleHandleW(L"winhttp.dll");
        if (m)
            n = FormatMessageW(flags | FORMAT_MESSAGE_FROM_HMODULE, m, code, 0, text, 400, 0);
    }
    if (!n)
        n = FormatMessageW(flags | FORMAT_MESSAGE_FROM_SYSTEM, 0, code, 0, text, 400, 0);
    while (n && (text[n - 1] == ' ' || text[n - 1] == '.' || text[n - 1] == '\r' || text[n - 1] == '\n'))
        n--;
    char *words = n ? net_utf8(text, (int)n) : 0;
    if (words)
        net_set_errorf("%s: %s (%lu)", what, words, code);
    else
        net_set_errorf("%s: ошибка %lu", what, code);
    free(words);
}

// ---- settings ----
void net_set_timeout(int ms)
{
    g_timeout = ms > 0 ? ms : 0;
}

int net_timeout_ms(void)
{
    return g_timeout;
}

void net_on_progress(NetProgress fn, void *user)
{
    NetThread *t = thread_state();
    t->progress = fn;
    t->progress_user = user;
}

NetProgress net_progress_fn(void **user)
{
    NetThread *t = thread_state();
    *user = t->progress_user;
    return t->progress;
}

int net_startup(void)
{
    if (g_wsa == 0) {
        AcquireSRWLockExclusive(&g_lock);
        if (g_wsa == 0) {
            WSADATA wsa;
            int e = WSAStartup(MAKEWORD(2, 2), &wsa);
            g_wsa = e ? -1 : 1;
            if (e)
                net_set_system_error("Сеть не запускается", (DWORD)e);
        }
        ReleaseSRWLockExclusive(&g_lock);
    }
    return g_wsa == 1;
}

// ---- text ----
wchar_t *net_wide(const char *utf8)
{
    if (!utf8)
        return 0;
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    wchar_t *w = malloc(sizeof(wchar_t) * (size_t)(n > 0 ? n : 1));
    if (!w)
        return 0;
    if (n <= 0)
        w[0] = 0;
    else
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n);
    return w;
}

char *net_utf8(const wchar_t *wide, int length)
{
    if (!wide)
        return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, wide, length, 0, 0, 0, 0);
    char *s = malloc((size_t)(n > 0 ? n : 0) + 1);
    if (!s)
        return 0;
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, wide, length, s, n, 0, 0);
    s[n > 0 ? n : 0] = 0;                           // with length -1 the 0 is already counted in n
    return s;
}

char *net_strndup(const char *s, size_t n)
{
    char *d = malloc(n + 1);
    if (d) {
        memcpy(d, s, n);
        d[n] = 0;
    }
    return d;
}

char *net_strdup(const char *s)
{
    return s ? net_strndup(s, strlen(s)) : 0;
}

int net_same(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y)
            return 0;
        if (!x)
            return 1;
    }
}

char *net_find_header(const char *headers, const char *name)
{
    size_t n = strlen(name);
    for (const char *line = headers; line && *line;) {
        const char *end = strstr(line, "\r\n");
        if (!end)
            end = line + strlen(line);
        const char *colon = memchr(line, ':', (size_t)(end - line));
        if (colon && (size_t)(colon - line) == n) {
            size_t i = 0;
            while (i < n && tolower((unsigned char)line[i]) == tolower((unsigned char)name[i]))
                i++;
            if (i == n) {
                const char *v = colon + 1;
                while (v < end && (*v == ' ' || *v == '\t'))
                    v++;
                const char *ve = end;
                while (ve > v && (ve[-1] == ' ' || ve[-1] == '\t'))
                    ve--;
                return net_strndup(v, (size_t)(ve - v));
            }
        }
        line = *end ? end + 2 : end;
    }
    return 0;
}

// ---- %XX ----
char *net_url_encode(const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(text);
    char *out = malloc(n * 3 + 1), *o = out;
    if (!out)
        return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
            *p == '-' || *p == '_' || *p == '.' || *p == '~')
            *o++ = (char)*p;
        else if (*p == ' ')
            *o++ = '+';
        else {
            *o++ = '%';
            *o++ = hex[*p >> 4];
            *o++ = hex[*p & 15];
        }
    }
    *o = 0;
    return out;
}

static int hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char *net_url_decode(const char *text)
{
    char *out = malloc(strlen(text) + 1), *o = out;
    if (!out)
        return 0;
    for (const char *p = text; *p; p++) {
        if (*p == '+')
            *o++ = ' ';
        else if (*p == '%' && hex_digit(p[1]) >= 0 && hex_digit(p[2]) >= 0) {
            *o++ = (char)(hex_digit(p[1]) * 16 + hex_digit(p[2]));
            p += 2;
        } else
            *o++ = *p;
    }
    *o = 0;
    return out;
}

// ---- a growing buffer ----
int net_buf_add(NetBuf *b, const void *data, size_t size)
{
    if (b->size + size + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (cap < b->size + size + 1)
            cap *= 2;
        char *d = realloc(b->data, cap);
        if (!d)
            return 0;
        b->data = d;
        b->cap = cap;
    }
    if (size)
        memcpy(b->data + b->size, data, size);
    b->size += size;
    b->data[b->size] = 0;
    return 1;
}

int net_buf_printf(NetBuf *b, const char *format, ...)
{
    char small[512];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(small, sizeof small, format, ap);
    va_end(ap);
    if (n < 0)
        return 0;
    if ((size_t)n < sizeof small)
        return net_buf_add(b, small, (size_t)n);
    char *big = malloc((size_t)n + 1);
    if (!big)
        return 0;
    va_start(ap, format);
    vsnprintf(big, (size_t)n + 1, format, ap);
    va_end(ap);
    int ok = net_buf_add(b, big, (size_t)n);
    free(big);
    return ok;
}

// ---- names ----
char *net_resolve(const char *host)
{
    if (!net_startup())
        return 0;
    struct addrinfo hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int e = getaddrinfo(host, 0, &hints, &res);
    if (e || !res) {
        net_set_system_error(host, (DWORD)e);
        return 0;
    }
    char text[INET6_ADDRSTRLEN] = "";
    getnameinfo(res->ai_addr, (socklen_t)res->ai_addrlen, text, sizeof text, 0, 0, NI_NUMERICHOST);
    freeaddrinfo(res);
    net_set_error("");
    return net_strdup(text);
}

char *net_hostname(void)
{
    if (!net_startup())
        return 0;
    wchar_t name[256];
    if (GetHostNameW(name, 256)) {
        net_set_system_error("Имя компьютера", (DWORD)WSAGetLastError());
        return 0;
    }
    return net_utf8(name, -1);
}
