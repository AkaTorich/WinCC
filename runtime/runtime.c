// Standard library for programs built by the DyOS compiler.
// Compiled natively into the kernel (and into the Windows test host);
// programs reach these functions by name through cc_runtime_symbols().
// The outside world (console, files, keyboard, clock) is reached only
// through the cc_* hooks declared in cc.h.
//
// WinCC compiles this same file into every Windows program (__WINCC__):
// the hooks are in winhost.c (kernel32), the names programs call (printf)
// become labels of the rt_ functions by the .alias lines at the end, and a
// long is 4 bytes there (RT_LONG_IS_INT).
#ifdef __WINCC__
#include "rtwin.h"
#define RT_LONG_IS_INT 1
// stdio for several threads: each FILE function runs under one lock (winthread.c), its body is NAME_unlocked
#define LOCKED(name) name##_unlocked
#else
#include "cc.h"
#include <emmintrin.h>      // sqrtsd, with no C library behind it
#include <intrin.h>         // the heap's lock
#define RT_LONG_IS_INT 0
#define LOCKED(name) name
#endif

// Every static below is the running program's own: each task (proc.c) has a copy.
#pragma data_seg(".pst$d")
#pragma bss_seg(".psz$e")          // the zeroed ones: a section of their own (proc.c takes both)

#define EOF (-1)

// errno values (the same numbers as the Windows C library)
#define E_PERM  1
#define E_NOENT 2
#define E_IO    5
#define E_BADF  9
#define E_NOMEM 12
#define E_ACCES 13
#define E_EXIST 17
#define E_NOTDIR 20
#define E_ISDIR 21
#define E_INVAL 22
#define E_MFILE 24
#define E_NOSPC 28
#define E_SPIPE 29
#define E_DOM   33
#define E_RANGE 34
#define E_NOSYS 40

#ifdef __WINCC__
_Thread_local int rt_errno;     // each thread has its own errno (and strtok's place, below)
#else
int rt_errno;
#endif
int rt_signgam;                 // math.h signgam: the sign of Gamma(x) at the last lgamma

typedef struct { u64 regs[10]; u8 xmm[160]; } RtJmpBuf;    // the layout jmp.asm uses
int  cc_setjmp(RtJmpBuf *buf);
__declspec(noreturn) void cc_longjmp(RtJmpBuf *buf, int value);

// ---- bits of a double ----

typedef union { double d; u64 u; } DBits;
static u64 dbits(double x) { DBits b; b.d = x; return b.u; }
static double dfrom(u64 u) { DBits b; b.u = u; return b.d; }

#define SIGN_BIT  0x8000000000000000ull
#define EXP_MASK  0x7FF0000000000000ull
#define MANT_MASK 0x000FFFFFFFFFFFFFull

static double inf_val(void) { return dfrom(EXP_MASK); }
static double nan_val(void) { return dfrom(0x7FF8000000000000ull); }
static int is_nan(double x) { return (dbits(x) & ~SIGN_BIT) > EXP_MASK; }
static int is_inf(double x) { return (dbits(x) & ~SIGN_BIT) == EXP_MASK; }
static int is_finite(double x) { return (dbits(x) & EXP_MASK) != EXP_MASK; }
static int sign_of(double x) { return (int)(dbits(x) >> 63); }

size_t rt_strlen(const char *s);
void *rt_memcpy(void *d, const void *s, size_t n);
void *rt_memset(void *d, int c, size_t n);
void *rt_malloc(size_t n);
void rt_free(void *p);
void *rt_realloc(void *p, size_t n);
void *rt_calloc(size_t n, size_t m);
int rt_isspace(int c);
double rt_strtod(const char *s, char **end);
i64 rt_strtoll(const char *s, char **end, int base);
u64 rt_strtoull(const char *s, char **end, int base);
double rt_ldexp(double x, int e);
double rt_fabs(double x);

// ---- UTF-8: the one multibyte encoding (wchar_t is 32 bits: a code point) ----

#define E_ILSEQ 42
// wchar_t: one Unicode code point in the system; UTF-16 (2 bytes) in Windows
#ifdef __WINCC__
typedef unsigned short RtWchar;
#else
typedef int RtWchar;
#endif
static const RtWchar L_NULL[] = { '(', 'n', 'u', 'l', 'l', ')', 0 };

// In Windows a character beyond U+FFFF is two wchar_t: a surrogate pair.
#ifdef __WINCC__
#define RT_WIDE16 1
#else
#define RT_WIDE16 0
#endif

static int is_hi_sur(u32 c) { return c >= 0xD800 && c < 0xDC00; }
static int is_lo_sur(u32 c) { return c >= 0xDC00 && c < 0xE000; }

// the character at s[*i] (a pair is taken whole), *i moved past it
static u32 wide_cp(const RtWchar *s, size_t *i)
{
    u32 c = s[*i];
    (*i)++;
    if (RT_WIDE16 && is_hi_sur(c) && is_lo_sur(s[*i])) {
        c = 0x10000 + ((c - 0xD800) << 10) + ((u32)s[*i] - 0xDC00);
        (*i)++;
    }
    return c;
}

// how many wchar_t a character takes, and storing it so
static int wide_units(u32 c) { return RT_WIDE16 && c >= 0x10000 ? 2 : 1; }

static void wide_store(RtWchar *d, u32 c)
{
    if (wide_units(c) == 2) {
        c -= 0x10000;
        d[0] = (RtWchar)(0xD800 + (c >> 10));
        d[1] = (RtWchar)(0xDC00 + (c & 0x3FF));
    } else {
        d[0] = (RtWchar)c;
    }
}

// bytes of the character a lead byte starts, 0 if it cannot start one
static int utf8_len_of(u8 c)
{
    return c < 0x80 ? 1 : c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
}

static int cp_valid(u32 c) { return c < 0x110000 && (c < 0xD800 || c > 0xDFFF); }

// One character from at most n bytes: its length, 0 for the NUL,
// -1 for bad bytes (overlong, surrogate, beyond U+10FFFF), -2 if cut short.
static int utf8_dec(const char *s, size_t n, u32 *out)
{
    static const u32 least[5] = { 0, 0, 0x80, 0x800, 0x10000 };
    if (!n) return -2;
    u8 c = (u8)s[0];
    if (c < 0x80) { *out = c; return c ? 1 : 0; }
    int len = utf8_len_of(c);
    if (!len) return -1;
    u32 v = c & (0x7Fu >> len);
    for (int i = 1; i < len; i++) {
        if ((size_t)i >= n) return -2;
        u8 b = (u8)s[i];
        if ((b & 0xC0) != 0x80) return -1;
        v = v << 6 | (b & 0x3F);
    }
    if (v < least[len] || !cp_valid(v)) return -1;
    *out = v;
    return len;
}

// the UTF-8 bytes of a code point: their count, -1 if it is no character
static int utf8_enc(u32 c, char *out)
{
    if (!cp_valid(c)) return -1;
    if (c < 0x80) { out[0] = (char)c; return 1; }
    if (c < 0x800) {
        out[0] = (char)(0xC0 | c >> 6);
        out[1] = (char)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        out[0] = (char)(0xE0 | c >> 12);
        out[1] = (char)(0x80 | (c >> 6 & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | c >> 18);
    out[1] = (char)(0x80 | (c >> 12 & 0x3F));
    out[2] = (char)(0x80 | (c >> 6 & 0x3F));
    out[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

// ---- console output: bytes collect here and go out as whole UTF-8 characters ----

static char g_con[520];
static int g_con_len;

static void con_flush(void)
{
    int n = g_con_len, k = n;
    // an unfinished UTF-8 character at the end waits for its other bytes
    int i = n - 1;
    while (i >= 0 && n - i <= 3 && ((u8)g_con[i] & 0xC0) == 0x80)
        i--;
    if (i >= 0 && (u8)g_con[i] >= 0xC0) {
        u8 c = (u8)g_con[i];
        int need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
        if (n - i < need)
            k = i;
    }
    if (k > 0) {
        char save = g_con[k];
        g_con[k] = 0;
        cc_print(g_con);
        g_con[k] = save;
        for (int j = k; j < n; j++)
            g_con[j - k] = g_con[j];
        g_con_len = n - k;
    }
}

static void con_put(char c)
{
    if (!c)
        return;                 // the console takes C strings
    g_con[g_con_len++] = c;
    if (g_con_len >= 512)
        con_flush();
}

// ---- FILE ----
// In a Windows program (__WINCC__) a file is read and written by pieces through
// its handle (FK_HANDLE, winfile.c): a buffer of 4 KB, the text mode turns "\n"
// into "\r\n" and back. Elsewhere (the system, cctest) and for tmpfile the file
// is held whole in memory (FK_MEM): opening reads it, fflush/fclose write it back.

enum { FK_MEM, FK_CON_IN, FK_CON_OUT, FK_HANDLE };

typedef struct RtFile RtFile;
struct RtFile {
    int kind;
    int rd, wr, app;            // may read, may write, every write goes to the end
    int eof, err;
    int dirty;                  // changed since it was last written out
    int named;                  // a file stands behind it (tmpfile has none)
    int ungot;                  // a character pushed back by ungetc, or -1
    int wungot;                 // a wide character pushed back by ungetwc, or -1
    int whi;                    // Windows: the first half of a surrogate pair fputwc waits with
    int orient;                 // fwide: 0 not yet, -1 bytes, 1 wide
    char *data;
    size_t len, cap, pos;
    size_t stored;              // 1 + the size last written to the file here, 0: none yet (ENOSPC)
    // FK_HANDLE: data[0..len) is the buffer, of the bytes at fpos in the file; pos the next one
    void *h;
    int text;                   // text mode: "\n" is "\r\n" in the file, Ctrl+Z ends the text
    int bmode;                  // the buffer holds 0 nothing, 1 bytes read, 2 bytes to write
    int vbuf;                   // setvbuf: 0 full, 1 by lines, 2 none
    int fd;                     // its descriptor (fileno), -1 none yet
    i64 fpos;
    RtFile *next;               // the list of fopen'ed files
    char name[512];
};

#define RT_OPEN_MAX 512         // files open at once (FOPEN_MAX = 64 is the least promised); then EMFILE

// cc_file_kind (cc.h) is an optional host hook: a host without it gets
// rt_no_file_kind (-1: cannot tell), and folders then stay unseen.
int rt_no_file_kind(const char *name) { (void)name; return -1; }
#pragma comment(linker, "/alternatename:cc_file_kind=rt_no_file_kind")

static int file_kind(const char *name)
{
    int k = cc_file_kind(name);
    if (k >= 0) return k;
    size_t sz;
    return cc_file_read(name, &sz) ? 1 : 0;     // a folder stays unseen
}

// Why `name` cannot be a file, for errno: EISDIR for a folder ("a/" or one
// the host knows), ENOTDIR when a folder on its path is a file; else `other`.
static int name_problem(const char *name, int other)
{
    size_t n = rt_strlen(name);
    if (n && name[n - 1] == '/') return E_ISDIR;
    if (file_kind(name) == 2) return E_ISDIR;
    char part[512];
    for (size_t i = 1; i < n && i < sizeof(part); i++)
        if (name[i] == '/') {
            rt_memcpy(part, name, i);
            part[i] = 0;
            if (file_kind(part) == 1) return E_NOTDIR;
        }
    return other;
}

static char g_stdin_buf[1024];
static RtFile g_std[3];
RtFile *rt_stdin = &g_std[0], *rt_stdout = &g_std[1], *rt_stderr = &g_std[2];
static RtFile *g_files;

static void std_reset(void)
{
    rt_memset(g_std, 0, sizeof(g_std));
    g_std[0].kind = FK_CON_IN;
    g_std[0].rd = 1;
    g_std[0].data = g_stdin_buf;
    g_std[0].cap = sizeof(g_stdin_buf);
    g_std[1].kind = g_std[2].kind = FK_CON_OUT;
    g_std[1].wr = g_std[2].wr = 1;
    for (int i = 0; i < 3; i++)
        g_std[i].ungot = g_std[i].wungot = -1;
}

static int is_std(RtFile *f) { return f >= g_std && f < g_std + 3; }

#ifdef __WINCC__
// the FILE functions under the lock (their bodies are NAME_unlocked; the wrappers are at the end)
RtFile *rt_fopen(const char *name, const char *mode);
int rt_fclose(RtFile *f);
RtFile *rt_freopen(const char *name, const char *mode, RtFile *f);
int rt_fflush(RtFile *f);
RtFile *rt_tmpfile(void);
int rt_fgetc(RtFile *f);
int rt_getchar(void);
int rt_fputc(int c, RtFile *f);
int rt_putchar(int c);
int rt_ungetc(int c, RtFile *f);
char *rt_fgets(char *buf, int n, RtFile *f);
int rt_fputs(const char *s, RtFile *f);
int rt_puts(const char *s);
size_t rt_fread(void *ptr, size_t size, size_t count, RtFile *f);
size_t rt_fwrite(const void *ptr, size_t size, size_t count, RtFile *f);
int rt_fseek(RtFile *f, i64 off, int whence);
i64 rt_ftell(RtFile *f);
int rt_vfprintf(RtFile *f, const char *fmt, va_list ap);
int rt_vfscanf(RtFile *f, const char *fmt, va_list ap);
u32 rt_fputwc(RtWchar wc, RtFile *f);
u32 rt_fgetwc(RtFile *f);
u32 rt_ungetwc(u32 wc, RtFile *f);
RtWchar *rt_fgetws(RtWchar *ws, int n, RtFile *f);
int rt_fputws(const RtWchar *ws, RtFile *f);
int rt_vfwprintf(RtFile *f, const RtWchar *wfmt, va_list ap);
int rt_vfwscanf(RtFile *f, const RtWchar *wfmt, va_list ap);
#endif

#ifdef __WINCC__
#define FH_BUF 4096
void *rt_memchr(const void *s, int c, size_t n);

static int fh_buffer(RtFile *f)
{
    if (f->data)
        return 1;
    f->data = rt_malloc(FH_BUF);
    if (!f->data) {
        f->err = 1;
        rt_errno = E_NOMEM;
        return 0;
    }
    f->cap = FH_BUF;
    return 1;
}

// the buffer goes: what waits is written, what was read and not taken is given back
// (the file's place is where the program is); EOF if the writing failed
static int fh_flush(RtFile *f)
{
    int r = 0;
    if (f->bmode == 2 && f->len) {
        i64 put = cc_fh_write(f->h, f->data, f->len);
        if (put != (i64)f->len) {
            f->err = 1;
            rt_errno = cc_fh_errno();
            r = EOF;
        }
        f->fpos += put > 0 ? put : 0;
    } else if (f->bmode == 1) {
        f->fpos += (i64)f->pos;
        cc_fh_seek(f->h, f->fpos, 0);
    }
    f->bmode = 0;
    f->len = f->pos = 0;
    return r;
}

// the next piece of the file into the buffer: 1, 0 at the end, -1 an error
static int fh_fill(RtFile *f)
{
    if (f->bmode == 2 && fh_flush(f))
        return -1;
    if (f->bmode == 1)
        f->fpos += (i64)f->len;     // the handle stands after what was read
    f->bmode = 1;
    f->len = f->pos = 0;
    if (!fh_buffer(f))
        return -1;
    i64 got = cc_fh_read(f->h, f->data, f->cap);
    if (got < 0) {
        f->err = 1;
        rt_errno = cc_fh_errno();
        return -1;
    }
    f->len = (size_t)got;
    return got > 0;
}

static int fh_raw(RtFile *f)
{
    if (f->bmode != 1 || f->pos >= f->len) {
        int r = fh_fill(f);
        if (r <= 0) {
            if (!r) f->eof = 1;
            return EOF;
        }
    }
    return (u8)f->data[f->pos++];
}

static int fh_getc(RtFile *f)
{
    int c = fh_raw(f);
    if (!f->text || c == EOF)
        return c;
    if (c == 26) {                  // Ctrl+Z: the end of a text
        f->pos--;
        f->eof = 1;
        return EOF;
    }
    if (c == '\r') {
        int d = fh_raw(f);
        if (d == '\n')
            return '\n';
        if (d != EOF)
            f->pos--;
        f->eof = 0;
    }
    return c;
}

static size_t fh_write(RtFile *f, const char *p, size_t n)
{
    if (f->bmode == 1)
        fh_flush(f);                // after reading: write at the place the program is at
    if (f->bmode == 0) {
        if (!fh_buffer(f))
            return 0;
        if (f->app) {
            i64 end = cc_fh_seek(f->h, 0, 2);
            if (end >= 0) f->fpos = end;
        }
        f->bmode = 2;
        f->len = f->pos = 0;
    }
    size_t i = 0;
    while (i < n) {
        if (f->len + 2 > f->cap && fh_flush(f) == EOF)
            return i;
        if (f->bmode == 0) {
            f->bmode = 2;
            if (f->app) {
                i64 end = cc_fh_seek(f->h, 0, 2);
                if (end >= 0) f->fpos = end;
            }
        }
        if (f->text) {
            if (p[i] == '\n')
                f->data[f->len++] = '\r';
            f->data[f->len++] = p[i++];
        } else {
            size_t k = f->cap - f->len < n - i ? f->cap - f->len : n - i;
            rt_memcpy(f->data + f->len, p + i, k);
            f->len += k;
            i += k;
        }
        f->pos = f->len;
    }
    if (f->vbuf == 2 || (f->vbuf == 1 && n && rt_memchr(p, '\n', n)))
        fh_flush(f);
    return n;
}

static i64 fh_tell(RtFile *f)
{
    i64 at = f->fpos + (f->bmode ? (i64)f->pos : 0);
    return at - (f->ungot >= 0);
}
#endif

static int file_getc(RtFile *f)
{
    if (!f || !f->rd) {
        if (f) f->err = 1;
        rt_errno = E_BADF;
        return EOF;
    }
    if (f->ungot >= 0) {
        int c = f->ungot;
        f->ungot = -1;
        return c;
    }
#ifdef __WINCC__
    if (f->kind == FK_HANDLE)
        return fh_getc(f);
#endif
    if (f->pos >= f->len) {
        if (f->kind != FK_CON_IN || f->eof) {
            f->eof = 1;
            return EOF;
        }
        con_flush();
        int n = cc_input_line(f->data, (int)f->cap);
        if (n <= 0) {
            f->eof = 1;
            return EOF;
        }
        f->len = (size_t)n;
        f->pos = 0;
    }
    return (u8)f->data[f->pos++];
}

static int file_reserve(RtFile *f, size_t need)
{
    if (need <= f->cap)
        return 1;
    size_t cap = f->cap ? f->cap * 2 : 256;
    while (cap < need)
        cap *= 2;
    char *d = rt_realloc(f->data, cap);
    if (!d) {
        f->err = 1;
        rt_errno = E_NOMEM;
        return 0;
    }
    f->data = d;
    f->cap = cap;
    return 1;
}

static size_t file_write(RtFile *f, const char *p, size_t n)
{
    if (!f || !f->wr) {
        if (f) f->err = 1;
        rt_errno = E_BADF;
        return 0;
    }
    if (f->kind == FK_CON_OUT) {
        for (size_t i = 0; i < n; i++)
            con_put(p[i]);
        return n;
    }
    f->ungot = -1;
#ifdef __WINCC__
    if (f->kind == FK_HANDLE)
        return fh_write(f, p, n);
#endif
    if (f->app)
        f->pos = f->len;
    if (!file_reserve(f, f->pos + n))
        return 0;
    if (f->pos > f->len)        // written past the end after fseek: the gap is zeros
        rt_memset(f->data + f->len, 0, f->pos - f->len);
    rt_memcpy(f->data + f->pos, p, n);
    f->pos += n;
    if (f->pos > f->len)
        f->len = f->pos;
    f->dirty = 1;
    return n;
}

static int file_putc(RtFile *f, int c)
{
    char ch = (char)c;
    return file_write(f, &ch, 1) ? (u8)ch : EOF;
}

static int file_flush(RtFile *f)
{
    if (f->kind == FK_CON_OUT) {
        con_flush();
        return 0;
    }
#ifdef __WINCC__
    if (f->kind == FK_HANDLE)
        return fh_flush(f);
#endif
    if (f->kind != FK_MEM || !f->dirty || !f->named)
        return 0;
    if (!cc_file_write(f->name, f->len ? f->data : "", f->len)) {
        f->err = 1;
        // the place took this file before, only smaller: now there is no room
        // for it (on the disk, or in memory for the files kept there)
        rt_errno = f->stored && f->len > f->stored - 1 ? E_NOSPC : E_IO;
        return EOF;
    }
    f->stored = f->len + 1;
    f->dirty = 0;
    return 0;
}

// Opens name with mode ("r", "w+", "ab", "wx" ...) into f.
static int open_into(RtFile *f, const char *name, const char *mode)
{
    int rd = 0, wr = 0, app = 0, trunc = 0, must = 0, excl = 0;
    switch (mode ? *mode : 0) {
    case 'r': rd = must = 1; break;
    case 'w': wr = trunc = 1; break;
    case 'a': wr = app = 1; break;
    default: rt_errno = E_INVAL; return 0;
    }
    int binary = 0;
    for (const char *m = mode + 1; *m; m++) {
        if (*m == '+') rd = wr = 1;
        else if (*m == 'x') excl = 1;
        else if (*m == 'b') binary = 1;
    }
    size_t nlen = name ? rt_strlen(name) : 0;
    if (!nlen || nlen >= sizeof(f->name)) {
        rt_errno = E_NOENT;
        return 0;
    }
#ifdef __WINCC__
    {
        int err = 0;
        void *h = cc_fh_open(name, rd, wr, app, trunc, must, excl, &err);
        if (!h) {
            rt_errno = err == E_NOENT ? name_problem(name, E_NOENT) : err;
            return 0;
        }
        rt_memset(f, 0, sizeof(*f));
        f->kind = FK_HANDLE;
        f->h = h;
        f->rd = rd; f->wr = wr; f->app = app;
        f->text = !binary;
        f->named = 1;
        f->ungot = f->wungot = -1;
        f->fd = -1;
        f->fpos = app ? cc_fh_seek(h, 0, 2) : 0;
        if (f->fpos < 0) f->fpos = 0;
        rt_memcpy(f->name, name, nlen + 1);
        return 1;
    }
#endif
    (void)binary;
    size_t size = 0;
    const char *old = trunc && !excl ? 0 : cc_file_read(name, &size);
    if (excl && old) { rt_errno = E_EXIST; return 0; }
    if (must && !old) { rt_errno = name_problem(name, E_NOENT); return 0; }
    if (trunc) { old = 0; size = 0; }
    char *data = 0;
    if (old && size) {
        data = rt_malloc(size);
        if (!data) { rt_errno = E_NOMEM; return 0; }
        rt_memcpy(data, old, size);
    }
    size_t stored = old ? 0 : 1;
    if (!old && wr) {           // created at once, as C expects; not over a folder
        int why = name_problem(name, 0);
        if (why || !cc_file_write(name, "", 0)) {
            rt_free(data);
            rt_errno = why ? why : E_ACCES;
            return 0;
        }
    }
    f->stored = stored;
    f->kind = FK_MEM;
    f->rd = rd; f->wr = wr; f->app = app;
    f->eof = f->err = f->dirty = 0;
    f->named = 1;
    f->ungot = f->wungot = -1;
    f->whi = 0;
    f->orient = 0;
    f->data = data;
    f->len = f->cap = data ? size : 0;
    f->pos = app ? f->len : 0;
    rt_memcpy(f->name, name, nlen + 1);
    return 1;
}

// Room for one more open file? EMFILE if RT_OPEN_MAX are open.
static int open_room(void)
{
    int n = 0;
    for (RtFile *x = g_files; x; x = x->next)
        n++;
    if (n < RT_OPEN_MAX) return 1;
    rt_errno = E_MFILE;
    return 0;
}

RtFile *LOCKED(rt_fopen)(const char *name, const char *mode)
{
    if (!open_room()) return 0;
    RtFile *f = rt_calloc(1, sizeof(RtFile));
    if (!f) { rt_errno = E_NOMEM; return 0; }
    if (!open_into(f, name, mode)) {
        rt_free(f);
        return 0;
    }
    f->next = g_files;
    g_files = f;
    return f;
}

int LOCKED(rt_fclose)(RtFile *f)
{
    if (!f)
        return EOF;
    int r = file_flush(f);
#ifdef __WINCC__
    if (f->kind == FK_HANDLE) {
        if (cc_fh_close(f->h) && !r) r = EOF;
        f->h = 0;
        rt_free(f->data);
        f->data = 0;
        if (is_std(f)) {
            f->rd = f->wr = 0;
            return r;
        }
        for (RtFile **p = &g_files; *p; p = &(*p)->next)
            if (*p == f) { *p = f->next; break; }
        rt_free(f);
        return r;
    }
#endif
    if (is_std(f)) {
        if (f->kind == FK_MEM)
            rt_free(f->data);
        f->kind = FK_MEM;
        f->data = 0;
        f->len = f->cap = f->pos = 0;
        f->rd = f->wr = 0;
        return r;
    }
    for (RtFile **p = &g_files; *p; p = &(*p)->next)
        if (*p == f) { *p = f->next; break; }
    rt_free(f->data);
    rt_free(f);
    return r;
}

RtFile *LOCKED(rt_freopen)(const char *name, const char *mode, RtFile *f)
{
    if (!f)
        return 0;
    if (!name)
        return f;               // changing only the mode: nothing to change here
    file_flush(f);
#ifdef __WINCC__
    if (f->kind == FK_HANDLE) {
        cc_fh_close(f->h);
        rt_free(f->data);
        f->h = 0;
        f->data = 0;
        f->kind = FK_MEM;
    }
#endif
    if (f->kind == FK_MEM)
        rt_free(f->data);
    f->data = 0;
    RtFile *next = f->next;
    if (!open_into(f, name, mode)) {
        f->next = next;
        rt_fclose(f);
        return 0;
    }
    f->next = next;
    return f;
}

int LOCKED(rt_fflush)(RtFile *f)
{
    if (f)
        return file_flush(f);
    int r = 0;
    for (RtFile *x = g_files; x; x = x->next)
        if (file_flush(x)) r = EOF;
    for (int i = 0; i < 3; i++)
        if (file_flush(&g_std[i])) r = EOF;
    return r;
}

RtFile *LOCKED(rt_tmpfile)(void)
{
    if (!open_room()) return 0;
    RtFile *f = rt_calloc(1, sizeof(RtFile));
    if (!f) { rt_errno = E_NOMEM; return 0; }
    f->kind = FK_MEM;
    f->rd = f->wr = 1;
    f->ungot = f->wungot = -1;
    f->next = g_files;
    g_files = f;
    return f;
}

int LOCKED(rt_fgetc)(RtFile *f) { return file_getc(f); }
int LOCKED(rt_getchar)(void) { return file_getc(rt_stdin); }

int LOCKED(rt_fputc)(int c, RtFile *f)
{
    int r = file_putc(f, c);
    if (f && f->kind == FK_CON_OUT) con_flush();
    return r;
}

int LOCKED(rt_putchar)(int c) { return rt_fputc(c, rt_stdout); }

int LOCKED(rt_ungetc)(int c, RtFile *f)
{
    if (c == EOF || !f)
        return EOF;
    if (f->ungot < 0 && f->pos > 0 && f->pos <= f->len && (u8)f->data[f->pos - 1] == (u8)c &&
        (f->kind != FK_HANDLE || (f->bmode == 1 && !f->text)))
        f->pos--;
    else if (f->ungot < 0)
        f->ungot = (u8)c;
    else
        return EOF;
    f->eof = 0;
    return (u8)c;
}

char *LOCKED(rt_fgets)(char *buf, int n, RtFile *f)
{
    if (n <= 0)
        return 0;
    int i = 0;
    while (i < n - 1) {
        int c = file_getc(f);
        if (c == EOF)
            break;
        buf[i++] = (char)c;
        if (c == '\n')
            break;
    }
    if (!i)
        return 0;
    buf[i] = 0;
    return buf;
}

int LOCKED(rt_fputs)(const char *s, RtFile *f)
{
    size_t n = rt_strlen(s);
    size_t w = file_write(f, s, n);
    if (f && f->kind == FK_CON_OUT) con_flush();
    return w == n ? 0 : EOF;
}

int LOCKED(rt_puts)(const char *s)
{
    if (rt_fputs(s, rt_stdout) == EOF)
        return EOF;
    return rt_fputc('\n', rt_stdout) == EOF ? EOF : 0;
}

size_t LOCKED(rt_fread)(void *ptr, size_t size, size_t count, RtFile *f)
{
    size_t total = size * count, got = 0;
    if (!total)
        return 0;
    u8 *p = ptr;
    while (got < total) {
#ifdef __WINCC__
        if (f && f->kind == FK_HANDLE && f->rd && !f->text && f->ungot < 0) {     // bytes as they are: by pieces
            if (f->bmode != 1 || f->pos >= f->len) {
                int r = fh_fill(f);
                if (r <= 0) {
                    if (!r) f->eof = 1;
                    break;
                }
            }
            size_t k = f->len - f->pos;
            if (k > total - got) k = total - got;
            rt_memcpy(p + got, f->data + f->pos, k);
            f->pos += k;
            got += k;
            continue;
        }
#endif
        if (f && f->kind == FK_MEM && f->rd && f->ungot < 0 && f->pos < f->len) {
            size_t k = f->len - f->pos;
            if (k > total - got) k = total - got;
            rt_memcpy(p + got, f->data + f->pos, k);
            f->pos += k;
            got += k;
            continue;
        }
        int c = file_getc(f);
        if (c == EOF)
            break;
        p[got++] = (u8)c;
    }
    return got / size;
}

size_t LOCKED(rt_fwrite)(const void *ptr, size_t size, size_t count, RtFile *f)
{
    size_t total = size * count;
    if (!total)
        return 0;
    size_t w = file_write(f, ptr, total);
    if (f && f->kind == FK_CON_OUT) con_flush();
    return w / size;
}

int LOCKED(rt_fseek)(RtFile *f, i64 off, int whence)
{
#ifdef __WINCC__
    if (f && f->kind == FK_HANDLE) {
        if (whence < 0 || whence > 2) { rt_errno = E_INVAL; return -1; }
        i64 base = whence == 1 ? fh_tell(f) : 0;
        if (fh_flush(f) == EOF) return -1;
        if (whence == 2) {
            base = cc_fh_seek(f->h, 0, 2);
            if (base < 0) { rt_errno = cc_fh_errno(); return -1; }
        }
        if (base + off < 0) { rt_errno = E_INVAL; return -1; }
        i64 at = cc_fh_seek(f->h, base + off, 0);
        if (at < 0) { rt_errno = cc_fh_errno(); return -1; }
        f->fpos = at;
        f->ungot = f->wungot = -1;
        f->eof = 0;
        return 0;
    }
#endif
    if (!f || f->kind != FK_MEM) { rt_errno = E_SPIPE; return -1; }
    i64 base = whence == 0 ? 0 : whence == 1 ? (i64)f->pos - (f->ungot >= 0) : (i64)f->len;
    if (whence < 0 || whence > 2 || base + off < 0) { rt_errno = E_INVAL; return -1; }
    f->pos = (size_t)(base + off);
    f->ungot = f->wungot = -1;
    f->eof = 0;
    return 0;
}

i64 LOCKED(rt_ftell)(RtFile *f)
{
#ifdef __WINCC__
    if (f && f->kind == FK_HANDLE)
        return fh_tell(f);
#endif
    if (!f || f->kind != FK_MEM) { rt_errno = E_SPIPE; return -1; }
    return (i64)f->pos - (f->ungot >= 0);
}

void rt_rewind(RtFile *f)
{
    if (rt_fseek(f, 0, 0) == 0)
        f->err = 0;
}

int rt_fgetpos(RtFile *f, i64 *pos)
{
    i64 p = rt_ftell(f);
    if (p < 0) return -1;
    *pos = p;
    return 0;
}

int rt_fsetpos(RtFile *f, const i64 *pos) { return rt_fseek(f, *pos, 0); }
int rt_feof(RtFile *f) { return f ? f->eof : 0; }
int rt_ferror(RtFile *f) { return f ? f->err : 0; }
void rt_clearerr(RtFile *f) { if (f) f->eof = f->err = 0; }
int rt_setvbuf(RtFile *f, char *buf, int mode, size_t size)
{
    (void)buf; (void)size;          // the buffer stays the library's own (4 KB)
    if (!f || mode < 0 || mode > 2) return -1;
    f->vbuf = mode;                 // _IOFBF 0, _IOLBF 1, _IONBF 2
    return 0;
}
void rt_setbuf(RtFile *f, char *buf) { (void)f; (void)buf; }

int rt_remove(const char *name)
{
    if (cc_file_delete(name)) return 0;
    // there but not removed: no access; else why there is no such file
    rt_errno = file_kind(name) == 1 ? E_ACCES : name_problem(name, E_NOENT);
    return -1;
}

int rt_rename(const char *from, const char *to)
{
    if (cc_file_rename(from, to)) return 0;
    rt_errno = file_kind(from) ? E_ACCES : name_problem(from, E_NOENT);
    return -1;
}

// fileno: the descriptor of a FILE (io.h); fdopen: a FILE over a descriptor
#ifdef __WINCC__
i64 rt__get_osfhandle(int fd);
int rt__open_osfhandle(i64 h, int flags);

int rt_fileno(RtFile *f)
{
    if (!f) { rt_errno = E_INVAL; return -1; }
    if (is_std(f) && f->kind != FK_HANDLE) return (int)(f - g_std);
    if (f->kind != FK_HANDLE) { rt_errno = E_BADF; return -1; }
    if (f->fd < 0)
        f->fd = rt__open_osfhandle((i64)f->h, f->text ? 0 : 0x8000);
    return f->fd;
}

RtFile *rt_fdopen(int fd, const char *mode)
{
    i64 h = rt__get_osfhandle(fd);
    if (h == -1 || !mode || (*mode != 'r' && *mode != 'w' && *mode != 'a')) { rt_errno = E_BADF; return 0; }
    if (!open_room()) return 0;
    RtFile *f = rt_calloc(1, sizeof(RtFile));
    if (!f) { rt_errno = E_NOMEM; return 0; }
    f->kind = FK_HANDLE;
    f->h = (void *)h;
    f->rd = *mode == 'r';
    f->wr = *mode != 'r';
    f->app = *mode == 'a';
    for (const char *m = mode + 1; *m; m++) {
        if (*m == '+') f->rd = f->wr = 1;
    }
    f->text = 1;
    for (const char *m = mode; *m; m++)
        if (*m == 'b') f->text = 0;
    f->ungot = f->wungot = -1;
    f->fd = fd;
    f->fpos = cc_fh_seek(f->h, 0, 1);
    if (f->fpos < 0) f->fpos = 0;
    f->next = g_files;
    g_files = f;
    return f;
}
#endif

// ---- formatting ----

typedef struct {
    char *buf;
    size_t cap, len;
    RtFile *f;                  // or straight into a stream
} Sink;

static void sink_put(Sink *s, char c)
{
    if (s->f)
        file_putc(s->f, c);
    else if (s->len + 1 < s->cap)
        s->buf[s->len] = c;
    s->len++;
}

static void sink_str(Sink *s, const char *p, int n)
{
    for (int i = 0; i < n; i++)
        sink_put(s, p[i]);
}

static void sink_rep(Sink *s, char c, int n)
{
    while (n-- > 0)
        sink_put(s, c);
}

static int fmt_uint(char *tmp, u64 v, int base, int upper)
{
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char rev[72];
    int n = 0;
    do {
        rev[n++] = d[v % (u64)base];
        v /= (u64)base;
    } while (v);
    for (int i = 0; i < n; i++)
        tmp[i] = rev[n - 1 - i];
    return n;
}

static const u32 g_pow5[14] = {
    1, 5, 25, 125, 625, 3125, 15625, 78125, 390625, 1953125, 9765625, 48828125, 244140625, 1220703125,
};

// limbs (base 10^9, lowest first) *= mul
static void dec_mul(u32 *limb, int *n, u32 mul)
{
    u64 carry = 0;
    for (int i = 0; i < *n; i++) {
        u64 t = (u64)limb[i] * mul + carry;
        limb[i] = (u32)(t % 1000000000u);
        carry = t / 1000000000u;
    }
    while (carry) {
        limb[(*n)++] = (u32)(carry % 1000000000u);
        carry /= 1000000000u;
    }
}

// The exact decimal digits of a finite v > 0: v = 0.d1d2d3... * 10^dexp.
// Returns how many digits (trailing zeros dropped). At most 767 digits.
static int dbl_digits(double v, char *dig, int *dexp)
{
    u64 u = dbits(v);
    int e = (int)((u >> 52) & 0x7FF);
    u64 m = u & MANT_MASK;
    if (e) m |= 1ull << 52; else e = 1;
    e -= 1075;                  // v = m * 2^e
    while (!(m & 1)) { m >>= 1; e++; }
    u32 limb[90];
    int n = 0, point = 0;
    while (m) {
        limb[n++] = (u32)(m % 1000000000u);
        m /= 1000000000u;
    }
    while (e > 0) {             // m * 2^e
        int k = e > 29 ? 29 : e;
        dec_mul(limb, &n, (u32)1 << k);
        e -= k;
    }
    if (e < 0) {                // m / 2^k = m * 5^k / 10^k
        point = -e;
        while (e < 0) {
            int k = -e > 13 ? 13 : -e;
            dec_mul(limb, &n, g_pow5[k]);
            e += k;
        }
    }
    int nd = 0;
    for (int i = n - 1; i >= 0; i--) {
        char t[9];
        u32 x = limb[i];
        for (int j = 8; j >= 0; j--) { t[j] = (char)('0' + x % 10); x /= 10; }
        int j = 0;
        if (i == n - 1)
            while (j < 8 && t[j] == '0') j++;
        for (; j < 9; j++)
            dig[nd++] = t[j];
    }
    *dexp = nd - point;
    while (nd > 1 && dig[nd - 1] == '0')
        nd--;
    return nd;
}

// Rounds the digits to `keep` of them, half to even. Returns the new count
// (0: the value became zero); *dexp grows when 9.99 turns into 10.
static int round_digits(char *dig, int nd, int *dexp, int keep)
{
    if (keep >= nd)
        return nd;
    if (keep < 0)
        return 0;
    int up;
    if (dig[keep] > '5') up = 1;
    else if (dig[keep] < '5') up = 0;
    else {
        int rest = 0;
        for (int i = keep + 1; i < nd; i++)
            if (dig[i] != '0') { rest = 1; break; }
        up = rest ? 1 : keep > 0 ? (dig[keep - 1] - '0') & 1 : 0;
    }
    nd = keep;
    if (up) {
        int i = keep - 1;
        while (i >= 0 && dig[i] == '9')
            i--;
        if (i < 0) {
            dig[0] = '1';
            nd = 1;
            (*dexp)++;
        } else {
            dig[i]++;
            nd = i + 1;
        }
    }
    while (nd > 0 && dig[nd - 1] == '0')
        nd--;
    return nd;
}

static char g_dp = '.';         // the decimal point: setlocale(LC_NUMERIC) may make it ','

static int strip_zeros(char *out, int n)
{
    int dot = -1;
    for (int i = 0; i < n; i++)
        if (out[i] == g_dp) dot = i;
    if (dot < 0)
        return n;
    while (n > dot + 1 && out[n - 1] == '0')
        n--;
    if (n == dot + 1)
        n = dot;
    return n;
}

// %a: hexadecimal digits of v >= 0 after "0x"
static int fmt_hex_float(char *out, double v, int prec, int upper, int alt)
{
    const char *hx = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    u64 u = dbits(v);
    int e = (int)((u >> 52) & 0x7FF);
    u64 m = u & MANT_MASK;
    int lead = 1;
    if (!e) {
        lead = 0;
        e = m ? -1022 : 0;
    } else {
        e -= 1023;
    }
    int digits = 13;
    if (prec >= 0 && prec < 13) {
        int drop = (13 - prec) * 4;
        u64 rem = m & ((1ull << drop) - 1), half = 1ull << (drop - 1);
        m >>= drop;
        if (rem > half || (rem == half && (m & 1))) {
            m++;
            if (prec == 0 || (m >> (prec * 4))) {
                m &= prec ? (1ull << (prec * 4)) - 1 : 0;
                lead++;
            }
        }
        digits = prec;
    } else if (prec < 0) {
        while (digits > 0 && !(m & 15)) { m >>= 4; digits--; }
    } else {
        digits = prec;          // more digits than there are: zeros after
    }
    int n = 0;
    out[n++] = (char)('0' + lead);
    if (digits > 0 || alt)
        out[n++] = g_dp;
    int have = prec >= 0 && prec > 13 ? 13 : digits;
    for (int i = have - 1; i >= 0; i--)
        out[n++] = hx[(m >> (i * 4)) & 15];
    for (int i = have; i < digits; i++)
        out[n++] = '0';
    out[n++] = upper ? 'P' : 'p';
    out[n++] = e < 0 ? '-' : '+';
    n += fmt_uint(out + n, (u64)(e < 0 ? -e : e), 10, 0);
    return n;
}

// %e %f %g (and upper case) of v >= 0, without the sign.
static int fmt_float(char *out, double v, int conv, int prec, int alt)
{
    int upper = conv >= 'A' && conv <= 'Z';
    int lc = conv | 32;
    if (is_nan(v) || is_inf(v)) {
        const char *t = is_nan(v) ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
        return 3;
    }
    char dig[800];
    int dexp = 1, nd = 0;
    if (v != 0)
        nd = dbl_digits(v, dig, &dexp);
    if (prec < 0) prec = 6;
    if (prec > 500) prec = 500;
    int style = lc, strip = 0;
    if (lc == 'g') {
        int P = prec ? prec : 1;
        nd = round_digits(dig, nd, &dexp, P);
        int X = nd ? dexp - 1 : 0;
        if (X < P && X >= -4) { style = 'f'; prec = P - 1 - X; }
        else { style = 'e'; prec = P - 1; }
        strip = !alt;
    }
    int n = 0;
    if (style == 'e') {
        nd = round_digits(dig, nd, &dexp, prec + 1);
        int X = nd ? dexp - 1 : 0;
        out[n++] = nd ? dig[0] : '0';
        if (prec > 0 || alt)
            out[n++] = g_dp;
        for (int i = 1; i <= prec; i++)
            out[n++] = i < nd ? dig[i] : '0';
        if (strip)
            n = strip_zeros(out, n);
        out[n++] = upper ? 'E' : 'e';
        out[n++] = X < 0 ? '-' : '+';
        int ax = X < 0 ? -X : X;
        if (ax < 10)
            out[n++] = '0';
        n += fmt_uint(out + n, (u64)ax, 10, 0);
    } else {
        nd = round_digits(dig, nd, &dexp, dexp + prec);
        if (!nd)
            dexp = 1;
        if (dexp <= 0)
            out[n++] = '0';
        else
            for (int i = 0; i < dexp; i++)
                out[n++] = i < nd ? dig[i] : '0';
        if (prec > 0 || alt)
            out[n++] = g_dp;
        for (int j = 0; j < prec; j++) {
            int idx = dexp + j;
            out[n++] = idx >= 0 && idx < nd ? dig[idx] : '0';
        }
        if (strip)
            n = strip_zeros(out, n);
    }
    return n;
}

// stores a count or a scanned number through a pointer of the given size:
// -2 char, -1 short, 0 int, 1 long / long long / size_t
static void store_int(void *p, int size, u64 v)
{
    if (!p) return;
    if (size == -2) *(char *)p = (char)v;
    else if (size == -1) *(short *)p = (short)v;
    else if (size == 0) *(int *)p = (int)v;
    else *(u64 *)p = v;
}

static void rt_vformat(Sink *s, const char *fmt, va_list ap)
{
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            sink_put(s, *f);
            continue;
        }
        f++;
        int left = 0, plus = 0, space = 0, alt = 0, zero = 0;
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '+') plus = 1;
            else if (*f == ' ') space = 1;
            else if (*f == '#') alt = 1;
            else if (*f == '0') zero = 1;
            else break;
        }
        int width = 0, prec = -1;
        if (*f == '*') {
            width = va_arg(ap, int);
            if (width < 0) { left = 1; width = -width; }
            f++;
        } else {
            while (*f >= '0' && *f <= '9')
                width = width * 10 + (*f++ - '0');
        }
        if (*f == '.') {
            f++;
            prec = 0;
            if (*f == '*') {
                prec = va_arg(ap, int);
                if (prec < 0) prec = -1;
                f++;
            } else {
                while (*f >= '0' && *f <= '9')
                    prec = prec * 10 + (*f++ - '0');
            }
        }
        int size = 0;           // -2 hh, -1 h, 0, 1 l ll z j t
        int lone_l = 0;         // just "l": a long, 4 bytes in Windows
        for (;; f++) {
            if (*f == 'h') size = size == -1 ? -2 : -1;
            else if (*f == 'l') { lone_l = size != 1; size = 1; }
            else if (*f == 'z' || *f == 'j' || *f == 't' || *f == 'q') { size = 1; lone_l = 0; }
            else if (*f == 'I') {       // Microsoft: I64 (8 bytes), I32 (4), I alone (size_t)
                lone_l = 0;
                size = f[1] == '3' && f[2] == '2' ? 0 : 1;
                if ((f[1] == '6' && f[2] == '4') || (f[1] == '3' && f[2] == '2'))
                    f += 2;
            }
            else if (*f != 'L') break;
        }
        int isize = RT_LONG_IS_INT && lone_l ? 0 : size;     // the size of an integer argument
        char conv = *f;
        if (!conv)
            return;

        char tmp[900];
        const char *body = tmp;
        int n = 0, npre = 0, zeros = 0, is_int = 0, numeric = 1;
        char pre[4];
        switch (conv) {
        case 'd':
        case 'i': {
            i64 v = isize == 1 ? va_arg(ap, i64) : (i64)va_arg(ap, int);
            if (size == -1) v = (short)v;
            else if (size == -2) v = (signed char)v;
            u64 mag = v < 0 ? (u64)(-(v + 1)) + 1 : (u64)v;
            if (v < 0) pre[npre++] = '-';
            else if (plus) pre[npre++] = '+';
            else if (space) pre[npre++] = ' ';
            if (!(prec == 0 && mag == 0))
                n = fmt_uint(tmp, mag, 10, 0);
            is_int = 1;
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o':
        case 'b': {
            u64 v = isize == 1 ? va_arg(ap, u64) : (u64)va_arg(ap, u32);
            if (size == -1) v = (unsigned short)v;
            else if (size == -2) v = (u8)v;
            int base = conv == 'u' ? 10 : conv == 'o' ? 8 : conv == 'b' ? 2 : 16;
            if (!(prec == 0 && v == 0))
                n = fmt_uint(tmp, v, base, conv == 'X');
            if (alt && conv == 'o' && !(n > 0 && tmp[0] == '0') && prec <= n)
                pre[npre++] = '0';
            if (alt && (conv == 'x' || conv == 'X') && v) {
                pre[npre++] = '0';
                pre[npre++] = conv;
            }
            is_int = 1;
            break;
        }
        case 'p': {
            u64 v = (u64)va_arg(ap, void *);
            pre[npre++] = '0';
            pre[npre++] = 'x';
            n = fmt_uint(tmp, v, 16, 0);
            break;
        }
        case 'c':
            if (size == 1) {            // %lc: a wide character, written as UTF-8
                int k = utf8_enc((u32)va_arg(ap, int), tmp);
                n = k < 0 ? 0 : k;
            } else {
                tmp[0] = (char)va_arg(ap, int);
                n = 1;
            }
            numeric = 0;
            break;
        case 's': {
            if (size == 1) {            // %ls: a wide string; precision counts bytes, whole characters only
                const RtWchar *ws = va_arg(ap, const RtWchar *);
                if (!ws) ws = L_NULL;
                int nb = 0, nc = 0;
                char t[4];
                size_t end = 0;
                while (ws[end]) {
                    size_t j = end;
                    int k = utf8_enc(wide_cp(ws, &j), t);
                    if (k < 0 || (prec >= 0 && nb + k > prec)) break;
                    nb += k;
                    nc++;
                    end = j;
                }
                int fill = width > nc ? width - nc : 0;
                if (!left) sink_rep(s, ' ', fill);
                for (size_t j = 0; j < end;)
                    sink_str(s, t, utf8_enc(wide_cp(ws, &j), t));
                if (left) sink_rep(s, ' ', fill);
                continue;
            }
            body = va_arg(ap, const char *);
            if (!body)
                body = "(null)";
            while (body[n] && (prec < 0 || n < prec))
                n++;
            numeric = 0;
            break;
        }
        case 'n':
            store_int(va_arg(ap, void *), isize, (u64)s->len);
            continue;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            double v = va_arg(ap, double);
            if (sign_of(v) && !is_nan(v)) { pre[npre++] = '-'; v = -v; }
            else if (plus) pre[npre++] = '+';
            else if (space) pre[npre++] = ' ';
            if ((conv | 32) == 'a' && is_finite(v)) {
                pre[npre++] = '0';
                pre[npre++] = conv == 'A' ? 'X' : 'x';
                n = fmt_hex_float(tmp, v, prec, conv == 'A', alt);
            } else {
                n = fmt_float(tmp, v, conv, prec, alt);
            }
            if (!is_finite(v))
                zero = 0;
            break;
        }
        case '%':
            tmp[0] = '%';
            n = 1;
            numeric = 0;
            break;
        default:
            tmp[0] = '%';
            tmp[1] = conv;
            n = 2;
            numeric = 0;
            break;
        }
        if (is_int && prec >= 0) {
            zeros = prec > n ? prec - n : 0;
            zero = 0;
        }
        // width counts characters, not UTF-8 bytes
        int chars = npre + zeros;
        for (int i = 0; i < n; i++)
            if (((u8)body[i] & 0xC0) != 0x80)
                chars++;
        int fill = width > chars ? width - chars : 0;
        if (left) {
            sink_str(s, pre, npre);
            sink_rep(s, '0', zeros);
            sink_str(s, body, n);
            sink_rep(s, ' ', fill);
        } else if (zero && numeric) {
            sink_str(s, pre, npre);
            sink_rep(s, '0', zeros + fill);
            sink_str(s, body, n);
        } else {
            sink_rep(s, ' ', fill);
            sink_str(s, pre, npre);
            sink_rep(s, '0', zeros);
            sink_str(s, body, n);
        }
    }
}

int rt_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    Sink s = { buf, cap, 0, 0 };
    rt_vformat(&s, fmt, ap);
    if (cap) {
        size_t end = s.len < cap ? s.len : cap - 1;
        if (s.len >= cap) {
            // cut short: not in the middle of a letter -- a UTF-8 sequence goes whole or not at all
            size_t j = end;
            while (j > 0 && ((u8)buf[j - 1] & 0xC0) == 0x80)
                j--;
            if (j > 0) {
                u8 lead = (u8)buf[j - 1];
                size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
                if (end - (j - 1) < need)
                    end = j - 1;
            }
        }
        buf[end] = 0;
    }
    return (int)s.len;
}

int rt_snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}

int rt_vsprintf(char *buf, const char *fmt, va_list ap)
{
    return rt_vsnprintf(buf, (size_t)1 << 30, fmt, ap);
}

int rt_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vsnprintf(buf, (size_t)1 << 30, fmt, ap);
    va_end(ap);
    return n;
}

int LOCKED(rt_vfprintf)(RtFile *f, const char *fmt, va_list ap)
{
    if (!f || !f->wr) {
        if (f) f->err = 1;
        rt_errno = E_BADF;
        return -1;
    }
    int err = f->err;
    Sink s = { 0, 0, 0, f };
    rt_vformat(&s, fmt, ap);
    if (f->kind == FK_CON_OUT)
        con_flush();
    return f->err && !err ? -1 : (int)s.len;
}

int rt_fprintf(RtFile *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int rt_vprintf(const char *fmt, va_list ap) { return rt_vfprintf(rt_stdout, fmt, ap); }

int rt_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfprintf(rt_stdout, fmt, ap);
    va_end(ap);
    return n;
}

// ---- scanf ----

typedef struct {
    RtFile *f;                  // a stream, or
    const char *s;              // a string
    int count;                  // characters taken so far (%n)
    int wide;                   // wscanf: %n counts whole characters
} Scan;

static int sc_get(Scan *in)
{
    int c;
    if (in->f) {
        c = file_getc(in->f);
    } else {
        c = (u8)*in->s;
        if (c) in->s++; else c = EOF;
    }
    if (c != EOF && !(in->wide && (c & 0xC0) == 0x80))
        in->count++;
    return c;
}

static void sc_unget(Scan *in, int c)
{
    if (c == EOF)
        return;
    if (!(in->wide && (c & 0xC0) == 0x80))
        in->count--;
    if (in->f) rt_ungetc(c, in->f);
    else in->s--;
}

static int digit_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c |= 32;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return 99;
}

static int sc_isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

// The text of an integer, at most width characters, into buf.
static int scan_int_text(Scan *in, char *buf, int cap, int width, int base)
{
    int n = 0;
    if (width <= 0 || width > cap - 1)
        width = cap - 1;
    int c = sc_get(in);
    if ((c == '+' || c == '-') && n < width) {
        buf[n++] = (char)c;
        c = sc_get(in);
    }
    if ((base == 0 || base == 16) && c == '0' && n < width) {
        buf[n++] = '0';
        c = sc_get(in);
        if ((c == 'x' || c == 'X') && n < width) {
            buf[n++] = (char)c;
            c = sc_get(in);
            base = 16;
        } else if (base == 0) {
            base = 8;
        }
    }
    if (!base)
        base = 10;
    while (n < width && c != EOF && digit_val(c) < base) {
        buf[n++] = (char)c;
        c = sc_get(in);
    }
    sc_unget(in, c);
    buf[n] = 0;
    return n;
}

static int sc_digits(Scan *in, char *buf, int *n, int width, int hex, int *c)
{
    int any = 0;
    while (*n < width && *c != EOF && digit_val(*c) < (hex ? 16 : 10)) {
        buf[(*n)++] = (char)*c;
        *c = sc_get(in);
        any = 1;
    }
    return any;
}

// The text of a floating-point number (decimal, hex, inf, nan) into buf.
static int scan_float_text(Scan *in, char *buf, int cap, int width)
{
    int n = 0;
    if (width <= 0 || width > cap - 1)
        width = cap - 1;
    int c = sc_get(in);
    if ((c == '+' || c == '-') && n < width) {
        buf[n++] = (char)c;
        c = sc_get(in);
    }
    if ((c | 32) == 'i' || (c | 32) == 'n') {      // inf, infinity, nan
        while (n < width && c != EOF && (((c | 32) >= 'a' && (c | 32) <= 'z') || c == '(' || c == ')' || c == '_')) {
            buf[n++] = (char)c;
            c = sc_get(in);
            if (buf[n - 1] == ')') break;
        }
        sc_unget(in, c);
        buf[n] = 0;
        return n;
    }
    int hex = 0;
    if (c == '0' && n < width) {
        buf[n++] = '0';
        c = sc_get(in);
        if ((c | 32) == 'x' && n < width) {
            buf[n++] = (char)c;
            c = sc_get(in);
            hex = 1;
        }
    }
    sc_digits(in, buf, &n, width, hex, &c);
    if (c == g_dp && n < width) {
        buf[n++] = g_dp;
        c = sc_get(in);
        sc_digits(in, buf, &n, width, hex, &c);
    }
    if ((c | 32) == (hex ? 'p' : 'e') && n < width) {
        int mark = n;
        buf[n++] = (char)c;
        int c1 = sc_get(in), c2 = EOF;
        if ((c1 == '+' || c1 == '-') && n < width) {
            buf[n++] = (char)c1;
            c2 = sc_get(in);
        } else {
            c2 = c1;
            c1 = EOF;
        }
        c = c2;
        if (!sc_digits(in, buf, &n, width, 0, &c)) {
            // "1e" not followed by digits: give the letters back
            sc_unget(in, c);
            if (c1 != EOF) sc_unget(in, c1);
            c = (u8)buf[mark];
            n = mark;
        }
    }
    sc_unget(in, c);
    buf[n] = 0;
    return n;
}

// one UTF-8 character of the input: its code point, EOF, or -2 for bad bytes (left unread)
static i64 sc_getcp(Scan *in)
{
    int c = sc_get(in);
    if (c == EOF || c < 0x80)
        return c;
    char t[4];
    t[0] = (char)c;
    int len = utf8_len_of((u8)c), k = 1;
    for (; k < len; k++) {
        int d = sc_get(in);
        if (d == EOF || (d & 0xC0) != 0x80) { sc_unget(in, d); break; }
        t[k] = (char)d;
    }
    u32 v;
    if (!len || k < len || utf8_dec(t, (size_t)len, &v) <= 0) {
        while (k > 0) sc_unget(in, (u8)t[--k]);
        return -2;
    }
    return v;
}

static void sc_ungetcp(Scan *in, i64 cp)
{
    char t[4];
    int k = cp >= 0 ? utf8_enc((u32)cp, t) : 0;
    while (k > 0) sc_unget(in, (u8)t[--k]);
}

// is c in the %[...] set between p and end ("a-z" ranges; UTF-8 decoded for %l[)
static int in_scanset(const char *p, const char *end, u32 c, int utf8)
{
    while (p < end) {
        u32 a = (u8)*p, v;
        int k = 1, r;
        if (utf8 && (r = utf8_dec(p, (size_t)(end - p), &v)) > 0) { a = v; k = r; }
        p += k;
        if (p + 1 < end && *p == '-') {
            u32 b = (u8)p[1];
            int kb = 1;
            if (utf8 && (r = utf8_dec(p + 1, (size_t)(end - p - 1), &v)) > 0) { b = v; kb = r; }
            p += 1 + kb;
            if (c >= a && c <= b) return 1;
            continue;
        }
        if (c == a) return 1;
    }
    return 0;
}

int rt_iswspace(u32 c);

static int rt_vscan(Scan *in, const char *fmt, va_list ap)
{
    int assigned = 0, done = 0;     // done: conversions finished (assigned or not)
    for (const char *f = fmt; *f; f++) {
        if (sc_isspace((u8)*f)) {
            int c;
            do c = sc_get(in); while (sc_isspace(c));
            sc_unget(in, c);
            continue;
        }
        if (*f != '%' || f[1] == '%') {
            if (*f == '%') {
                f++;
                int c;
                do c = sc_get(in); while (sc_isspace(c));
                sc_unget(in, c);
            }
            int c = sc_get(in);
            if (c != (u8)*f) {
                sc_unget(in, c);
                return c == EOF && !done ? EOF : assigned;
            }
            continue;
        }
        f++;
        int suppress = 0, width = 0, size = 0, lone_l = 0;
        if (*f == '*') { suppress = 1; f++; }
        while (*f >= '0' && *f <= '9')
            width = width * 10 + (*f++ - '0');
        if (*f == 'h') { size = -1; f++; if (*f == 'h') { size = -2; f++; } }
        else if (*f == 'l') { size = 1; f++; if (*f == 'l') f++; else lone_l = 1; }
        else if (*f == 'L' || *f == 'j' || *f == 'z' || *f == 't' || *f == 'q') { size = 1; f++; }
        else if (*f == 'I') {           // Microsoft: I64, I32, I
            size = f[1] == '3' && f[2] == '2' ? 0 : 1;
            f += (f[1] == '6' && f[2] == '4') || (f[1] == '3' && f[2] == '2') ? 3 : 1;
        }
        int isize = RT_LONG_IS_INT && lone_l ? 0 : size;     // where an integer goes: a long is 4 bytes in Windows
        char conv = *f;
        if (!conv)
            break;
        if (conv == 'n') {
            if (!suppress) store_int(va_arg(ap, void *), isize, (u64)(i64)in->count);
            continue;
        }
        int c;
        if (conv != 'c' && conv != '[') {
            do c = sc_get(in); while (sc_isspace(c));
            sc_unget(in, c);
            if (c == EOF)
                return done ? assigned : EOF;
        }
        switch (conv) {
        case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
            int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
            char buf[80], *e;
            scan_int_text(in, buf, sizeof(buf), width, base);
            u64 v = conv == 'd' || conv == 'i' ? (u64)rt_strtoll(buf, &e, base) : rt_strtoull(buf, &e, base);
            if (e == buf)
                return assigned;
            if (!suppress) {
                if (conv == 'p') *va_arg(ap, void **) = (void *)v;
                else store_int(va_arg(ap, void *), isize, v);
                assigned++;
            }
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            char buf[400], *e;
            scan_float_text(in, buf, sizeof(buf), width);
            double v = rt_strtod(buf, &e);
            if (e == buf)
                return assigned;
            if (!suppress) {
                if (size == 1) *va_arg(ap, double *) = v;
                else *va_arg(ap, float *) = (float)v;
                assigned++;
            }
            break;
        }
        case 's':
        case 'c':
        case '[': {
            // %s %c %[ store chars; with l (%ls %lc %l[) wide characters
            // read from UTF-8, and widths count characters
            const char *set = 0, *set_end = 0;
            int neg = 0;
            if (conv == '[') {
                f++;
                if (*f == '^') { neg = 1; f++; }
                set = f;
                if (*f == ']') f++;
                while (*f && *f != ']') f++;
                set_end = f;                    // f stays on ']'
            }
            int wide = size == 1;
            void *out = suppress ? 0 : va_arg(ap, void *);
            int n = 0, wat = 0;         // characters taken; wchar_t written (a pair is two)
            if (!width) width = conv == 'c' ? 1 : 0x7FFFFFFF;
            i64 ch = 0;
            while (n < width) {
                ch = wide ? sc_getcp(in) : sc_get(in);
                if (ch < 0) break;
                int stop = conv == 's' ? (wide ? rt_iswspace((u32)ch) : sc_isspace((int)ch))
                         : conv == '[' ? in_scanset(set, set_end, (u32)ch, wide) == neg : 0;
                if (stop) {
                    if (wide) sc_ungetcp(in, ch); else sc_unget(in, (int)ch);
                    break;
                }
                if (out) {
                    if (wide) wide_store((RtWchar *)out + wat, (u32)ch);
                    else ((char *)out)[n] = (char)ch;
                }
                n++;
                if (wide) wat += wide_units((u32)ch);
            }
            if (!n)
                return ch == EOF && !done ? EOF : assigned;
            if (out) {
                if (conv != 'c') {
                    if (wide) ((RtWchar *)out)[wat] = 0;
                    else ((char *)out)[n] = 0;
                }
                assigned++;
            }
            if (conv == '[' && !*f) return assigned;
            break;
        }
        default:
            return assigned;
        }
        done++;
    }
    return assigned;
}

int rt_vsscanf(const char *s, const char *fmt, va_list ap)
{
    Scan in = { 0, s, 0 };
    return rt_vscan(&in, fmt, ap);
}

int rt_sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vsscanf(s, fmt, ap);
    va_end(ap);
    return n;
}

int LOCKED(rt_vfscanf)(RtFile *f, const char *fmt, va_list ap)
{
    Scan in = { f, 0, 0 };
    return rt_vscan(&in, fmt, ap);
}

int rt_fscanf(RtFile *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfscanf(f, fmt, ap);
    va_end(ap);
    return n;
}

int rt_vscanf(const char *fmt, va_list ap) { return rt_vfscanf(rt_stdin, fmt, ap); }

int rt_scanf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfscanf(rt_stdin, fmt, ap);
    va_end(ap);
    return n;
}

// ---- strings and memory ----

size_t rt_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

size_t rt_strnlen(const char *s, size_t max) { size_t n = 0; while (n < max && s[n]) n++; return n; }

int rt_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}

int rt_strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i])
            return (u8)a[i] - (u8)b[i];
    }
    return 0;
}

static int lower_ascii(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int rt_strcasecmp(const char *a, const char *b)
{
    while (*a && lower_ascii((u8)*a) == lower_ascii((u8)*b)) { a++; b++; }
    return lower_ascii((u8)*a) - lower_ascii((u8)*b);
}

int rt_strncasecmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int x = lower_ascii((u8)a[i]), y = lower_ascii((u8)b[i]);
        if (x != y || !x)
            return x - y;
    }
    return 0;
}

char *rt_strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }

char *rt_stpcpy(char *d, const char *s) { while ((*d = *s++)) d++; return d; }

char *rt_strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

char *rt_strcat(char *d, const char *s) { rt_strcpy(d + rt_strlen(d), s); return d; }

char *rt_strncat(char *d, const char *s, size_t n)
{
    char *e = d + rt_strlen(d);
    size_t i = 0;
    for (; i < n && s[i]; i++) e[i] = s[i];
    e[i] = 0;
    return d;
}

char *rt_strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}

char *rt_strrchr(const char *s, int c)
{
    const char *last = 0;
    for (;; s++) {
        if (*s == (char)c) last = s;
        if (!*s) return (char *)last;
    }
}

char *rt_strstr(const char *h, const char *n)
{
    size_t m = rt_strlen(n);
    for (; *h; h++)
        if (!rt_strncmp(h, n, m))
            return (char *)h;
    return m ? 0 : (char *)h;
}

size_t rt_strspn(const char *s, const char *accept)
{
    size_t n = 0;
    while (s[n] && rt_strchr(accept, s[n])) n++;
    return n;
}

size_t rt_strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    while (s[n] && !rt_strchr(reject, s[n])) n++;
    return n;
}

char *rt_strpbrk(const char *s, const char *accept)
{
    s += rt_strcspn(s, accept);
    return *s ? (char *)s : 0;
}

char *rt_strtok_r(char *s, const char *delim, char **save)
{
    if (!s) s = *save;
    if (!s) return 0;
    s += rt_strspn(s, delim);
    if (!*s) { *save = 0; return 0; }
    char *end = s + rt_strcspn(s, delim);
    if (*end) { *end = 0; *save = end + 1; }
    else *save = 0;
    return s;
}

#ifdef __WINCC__
static _Thread_local char *g_strtok;
#else
static char *g_strtok;
#endif
char *rt_strtok(char *s, const char *delim) { return rt_strtok_r(s, delim, &g_strtok); }

char *rt_strdup(const char *s)
{
    size_t n = rt_strlen(s) + 1;
    char *d = rt_malloc(n);
    if (d) rt_memcpy(d, s, n);
    return d;
}

char *rt_strndup(const char *s, size_t max)
{
    size_t n = rt_strnlen(s, max);
    char *d = rt_malloc(n + 1);
    if (d) { rt_memcpy(d, s, n); d[n] = 0; }
    return d;
}

#ifdef __WINCC__
static const char *locale_of(int cat);
#endif

// "C": byte by byte; another locale: as Windows orders words of that language (ё after е)
int rt_strcoll(const char *a, const char *b)
{
#ifdef __WINCC__
    const char *loc = locale_of(1);
    if (loc)
        return cc_collate(loc, a, b);
#endif
    return rt_strcmp(a, b);
}

size_t rt_strxfrm(char *d, const char *s, size_t n)
{
#ifdef __WINCC__
    const char *loc = locale_of(1);
    if (loc) {
        size_t need = cc_sortkey(loc, s, d, n);
        return need ? need - 1 : 0;
    }
#endif
    size_t len = rt_strlen(s);
    if (n) rt_strncpy(d, s, n);
    return len;
}

void *rt_memcpy(void *d, const void *s, size_t n)
{
    u8 *dd = d;
    const u8 *ss = s;
    for (size_t i = 0; i < n; i++) dd[i] = ss[i];
    return d;
}

void *rt_memmove(void *d, const void *s, size_t n)
{
    u8 *dd = d;
    const u8 *ss = s;
    if (dd < ss) for (size_t i = 0; i < n; i++) dd[i] = ss[i];
    else for (size_t i = n; i > 0; i--) dd[i - 1] = ss[i - 1];
    return d;
}

void *rt_memset(void *d, int c, size_t n)
{
    u8 *dd = d;
    for (size_t i = 0; i < n; i++) dd[i] = (u8)c;
    return d;
}

int rt_memcmp(const void *a, const void *b, size_t n)
{
    const u8 *x = a, *y = b;
    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}

void *rt_memchr(const void *s, int c, size_t n)
{
    const u8 *p = s;
    for (size_t i = 0; i < n; i++)
        if (p[i] == (u8)c) return (void *)(p + i);
    return 0;
}

void *rt_memccpy(void *d, const void *s, int c, size_t n)
{
    u8 *dd = d;
    const u8 *ss = s;
    for (size_t i = 0; i < n; i++) {
        dd[i] = ss[i];
        if (ss[i] == (u8)c) return dd + i + 1;
    }
    return 0;
}

#ifdef __WINCC__
static int locale_ru(void);

static char *strerror_ru(int e)
{
    switch (e) {
    case 0: return "Нет ошибки";
    case E_PERM: return "Действие не разрешено";
    case E_NOENT: return "Нет такого файла или папки";
    case E_IO: return "Ошибка ввода-вывода";
    case E_BADF: return "Неверный дескриптор файла";
    case E_NOMEM: return "Не хватает памяти";
    case E_ACCES: return "Нет доступа";
    case E_EXIST: return "Файл уже есть";
    case E_NOTDIR: return "Это не папка";
    case E_ISDIR: return "Это папка";
    case E_INVAL: return "Неверный аргумент";
    case E_MFILE: return "Открыто слишком много файлов";
    case E_NOSPC: return "Нет места на диске";
    case E_SPIPE: return "Перемещение по этому файлу невозможно";
    case E_DOM: return "Аргумент вне области определения";
    case E_RANGE: return "Результат слишком велик";
    case E_NOSYS: return "Функция не реализована";
    case E_ILSEQ: return "Неверная последовательность байтов";
    case 41: return "Папка не пуста";
    default: return "Неизвестная ошибка";
    }
}
#endif

char *rt_strerror(int e)
{
#ifdef __WINCC__
    if (locale_ru())
        return strerror_ru(e);
#endif
    switch (e) {
    case 0: return "No error";
    case E_PERM: return "Operation not permitted";
    case E_NOENT: return "No such file or directory";
    case E_IO: return "Input/output error";
    case E_BADF: return "Bad file descriptor";
    case E_NOMEM: return "Not enough memory";
    case E_ACCES: return "Permission denied";
    case E_EXIST: return "File exists";
    case E_NOTDIR: return "Not a directory";
    case E_ISDIR: return "Is a directory";
    case E_INVAL: return "Invalid argument";
    case E_MFILE: return "Too many open files";
    case E_NOSPC: return "No space left on device";
    case E_SPIPE: return "Invalid seek";
    case E_DOM: return "Numerical argument out of domain";
    case E_RANGE: return "Result too large";
    case E_NOSYS: return "Function not implemented";
    case E_ILSEQ: return "Illegal byte sequence";
    case 41: return "Directory not empty";
    default: return "Unknown error";
    }
}

// ---- ctype (the C locale: only ASCII letters count) ----

int rt_isdigit(int c) { return c >= '0' && c <= '9'; }
int rt_isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int rt_isalnum(int c) { return rt_isalpha(c) || rt_isdigit(c); }
int rt_islower(int c) { return c >= 'a' && c <= 'z'; }
int rt_isupper(int c) { return c >= 'A' && c <= 'Z'; }
int rt_isxdigit(int c) { return rt_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int rt_isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int rt_isblank(int c) { return c == ' ' || c == '\t'; }
int rt_iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int rt_isprint(int c) { return c >= 32 && c < 127; }
int rt_isgraph(int c) { return c > 32 && c < 127; }
int rt_ispunct(int c) { return rt_isgraph(c) && !rt_isalnum(c); }
int rt_toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
int rt_tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

// ---- big integers (32-bit limbs, lowest first) for exact number conversion ----

#define BIG_MAX 136
typedef struct { int n; u32 w[BIG_MAX]; } Big;

static void big_trim(Big *a) { while (a->n && !a->w[a->n - 1]) a->n--; }

static void big_set(Big *a, u64 v)
{
    a->n = 0;
    while (v) { a->w[a->n++] = (u32)v; v >>= 32; }
}

static void big_mul_add(Big *a, u32 m, u32 add)
{
    u64 carry = add;
    for (int i = 0; i < a->n; i++) {
        u64 t = (u64)a->w[i] * m + carry;
        a->w[i] = (u32)t;
        carry = t >> 32;
    }
    if (carry && a->n < BIG_MAX)
        a->w[a->n++] = (u32)carry;
}

static u32 big_div_small(Big *a, u32 d)
{
    u64 r = 0;
    for (int i = a->n - 1; i >= 0; i--) {
        u64 cur = (r << 32) | a->w[i];
        a->w[i] = (u32)(cur / d);
        r = cur % d;
    }
    big_trim(a);
    return (u32)r;
}

static int big_cmp(const Big *a, const Big *b)
{
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->w[i] != b->w[i])
            return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

static void big_add(Big *a, const Big *b)
{
    u64 carry = 0;
    int n = a->n > b->n ? a->n : b->n;
    for (int i = 0; i < n; i++) {
        u64 t = carry + (i < a->n ? a->w[i] : 0) + (i < b->n ? b->w[i] : 0);
        a->w[i] = (u32)t;
        carry = t >> 32;
    }
    a->n = n;
    if (carry && n < BIG_MAX)
        a->w[a->n++] = (u32)carry;
}

static void big_sub(Big *a, const Big *b)       // a >= b
{
    i64 borrow = 0;
    for (int i = 0; i < a->n; i++) {
        i64 t = (i64)a->w[i] - (i < b->n ? (i64)b->w[i] : 0) - borrow;
        borrow = t < 0;
        if (t < 0) t += 4294967296LL;
        a->w[i] = (u32)t;
    }
    big_trim(a);
}

static void big_shl(Big *a, int bits)
{
    if (!a->n || bits <= 0)
        return;
    int words = bits / 32, sh = bits % 32;
    int n = a->n + words + 1;
    if (n > BIG_MAX) n = BIG_MAX;
    for (int i = n - 1; i >= 0; i--) {
        int src = i - words;
        u32 hi = src >= 0 && src < a->n ? a->w[src] : 0;
        u32 lo = src - 1 >= 0 && src - 1 < a->n ? a->w[src - 1] : 0;
        a->w[i] = sh ? (hi << sh) | (lo >> (32 - sh)) : hi;
    }
    a->n = n;
    big_trim(a);
}

static void big_shr(Big *a, int bits)
{
    int words = bits / 32, sh = bits % 32;
    if (words >= a->n) { a->n = 0; return; }
    int n = a->n - words;
    for (int i = 0; i < n; i++) {
        u32 lo = a->w[i + words];
        u32 hi = i + words + 1 < a->n ? a->w[i + words + 1] : 0;
        a->w[i] = sh ? (lo >> sh) | (hi << (32 - sh)) : lo;
    }
    a->n = n;
    big_trim(a);
}

static int big_bitlen(const Big *a)
{
    if (!a->n) return 0;
    u32 t = a->w[a->n - 1];
    int b = 0;
    while (t) { b++; t >>= 1; }
    return (a->n - 1) * 32 + b;
}

static int big_bit(const Big *a, int i)
{
    if (i < 0 || i / 32 >= a->n) return 0;
    return (int)((a->w[i / 32] >> (i % 32)) & 1);
}

// bits [lo, lo + count) as a number, count <= 64; bits below 0 count as zero
static u64 big_bits(const Big *a, int lo, int count)
{
    u64 v = 0;
    for (int i = count - 1; i >= 0; i--)
        v = (v << 1) | (u64)big_bit(a, lo + i);
    return v;
}

static int big_any_below(const Big *a, int lo)
{
    for (int i = 0; i < lo; i++)
        if (big_bit(a, i)) return 1;
    return 0;
}

static void big_mul_pow10(Big *a, int k)
{
    while (k >= 9) { big_mul_add(a, 1000000000u, 0); k -= 9; }
    static const u32 p10[9] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
    if (k) big_mul_add(a, p10[k], 0);
}

// (q + a little, if sticky) * 2^e2, rounded to the nearest double (ties to even)
static double make_double(u64 q, int sticky, int e2)
{
    if (!q)
        return 0.0;
    while (!(q >> 63)) { q <<= 1; e2--; }
    int e = e2 + 63;                // exponent of the leading bit
    if (e > 1023)
        return inf_val();
    int keep = e >= -1022 ? 53 : e + 1075;
    if (keep < 0)
        return 0.0;
    int drop = 64 - keep;
    u64 mant = drop >= 64 ? 0 : q >> drop;
    u64 rem = drop >= 64 ? q : q & ((1ull << drop) - 1);
    u64 half = 1ull << (drop - 1);
    if (rem > half || (rem == half && (sticky || (mant & 1))))
        mant++;
    if (keep == 53) {
        if (mant >> 53) {
            mant >>= 1;
            if (++e > 1023) return inf_val();
        }
        return dfrom(((u64)(e + 1023) << 52) | (mant & MANT_MASK));
    }
    return dfrom(mant);             // subnormal; 2^52 here is the smallest normal number
}

static const double g_p10[23] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
};

// digits (no leading zeros) * 10^e10, correctly rounded
static double dec_to_double(const char *dig, int nd, int e10)
{
    if (nd <= 15 && e10 >= -22 && e10 <= 22) {     // exact operands, one rounding
        double v = 0;
        for (int i = 0; i < nd; i++)
            v = v * 10 + (dig[i] - '0');
        return e10 >= 0 ? v * g_p10[e10] : v / g_p10[-e10];
    }
    if (nd + e10 > 310)
        return inf_val();
    if (nd + e10 < -324)
        return 0.0;
    Big N, D;
    big_set(&N, 0);
    for (int i = 0; i < nd;) {
        u32 chunk = 0, mul = 1;
        for (int k = 0; k < 9 && i < nd; k++, i++) {
            chunk = chunk * 10 + (u32)(dig[i] - '0');
            mul *= 10;
        }
        if (!N.n) big_set(&N, chunk);
        else big_mul_add(&N, mul, chunk);
    }
    if (e10 >= 0) {
        big_mul_pow10(&N, e10);
        int L = big_bitlen(&N);
        if (L <= 64)
            return make_double(big_bits(&N, 0, L), 0, 0);
        return make_double(big_bits(&N, L - 64, 64), big_any_below(&N, L - 64), L - 64);
    }
    big_set(&D, 1);
    big_mul_pow10(&D, -e10);
    int s = big_bitlen(&D) - big_bitlen(&N) + 63;  // so that 2^62 <= N * 2^s / D < 2^64
    if (s > 0) big_shl(&N, s);
    else if (s < 0) big_shl(&D, -s);
    Big Ds = D;
    big_shl(&Ds, 63);
    u64 q = 0;
    for (int i = 63; i >= 0; i--) {
        if (big_cmp(&N, &Ds) >= 0) {
            big_sub(&N, &Ds);
            q |= 1ull << i;
        }
        big_shr(&Ds, 1);
    }
    return make_double(q, N.n != 0, -s);
}

static int prefix_nocase(const char *s, const char *word)
{
    for (; *word; s++, word++)
        if (lower_ascii((u8)*s) != *word) return 0;
    return 1;
}

// A quiet NaN with a payload: the tag of "nan(n)" or nan("n") — a number,
// decimal, 0x hex or 0 octal, up to `stop` — goes into the low 51 bits;
// any other tag gives the plain quiet NaN.
static double nan_with(const char *tag, char stop)
{
    u64 payload = 0;
    if (tag && rt_isdigit((u8)*tag)) {
        char *e;
        int saved = rt_errno;
        u64 n = rt_strtoull(tag, &e, 0);
        rt_errno = saved;
        if (*e == stop) payload = n & 0x7FFFFFFFFFFFFull;
    }
    return dfrom(0x7FF8000000000000ull | payload);
}

// a NaN as a float: the sign and the payload's low 22 bits stay
static float nan_to_float(double d)
{
    union { float f; u32 u; } b;
    b.u = (u32)(dbits(d) >> 32 & 0x80000000u) | 0x7FC00000u | (u32)(dbits(d) & 0x3FFFFF);
    return b.f;
}

double rt_strtod(const char *s, char **end)
{
    const char *p = s;
    while (rt_isspace((u8)*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-')
        neg = *p++ == '-';
    double v;
    if (prefix_nocase(p, "inf")) {
        p += prefix_nocase(p, "infinity") ? 8 : 3;
        v = inf_val();
    } else if (prefix_nocase(p, "nan")) {
        p += 3;
        v = nan_val();
        if (*p == '(') {
            const char *q = p + 1;
            while (rt_isalpha((u8)*q) || rt_isdigit((u8)*q) || *q == '_') q++;
            if (*q == ')') {
                v = nan_with(p + 1, ')');      // "nan(5)": 5 is the payload
                p = q + 1;
            }
        }
    } else if (p[0] == '0' && (p[1] | 32) == 'x' &&
               (digit_val((u8)p[2]) < 16 || (p[2] == g_dp && digit_val((u8)p[3]) < 16))) {
        const char *q = p + 2;
        u64 m = 0;
        int e2 = 0, sticky = 0;
        for (; digit_val((u8)*q) < 16; q++) {
            int d = digit_val((u8)*q);
            if (m >> 59) { e2 += 4; if (d) sticky = 1; }
            else m = m * 16 + (u64)d;
        }
        if (*q == g_dp)
            for (q++; digit_val((u8)*q) < 16; q++) {
                int d = digit_val((u8)*q);
                if (m >> 59) { if (d) sticky = 1; }
                else { m = m * 16 + (u64)d; e2 -= 4; }
            }
        p = q;
        if ((*p | 32) == 'p') {
            const char *x = p + 1;
            int eneg = 0;
            if (*x == '+' || *x == '-') eneg = *x++ == '-';
            if (rt_isdigit((u8)*x)) {
                int ex = 0;
                for (; rt_isdigit((u8)*x); x++)
                    if (ex < 100000) ex = ex * 10 + (*x - '0');
                e2 += eneg ? -ex : ex;
                p = x;
            }
        }
        v = make_double(m, sticky, e2);
        if ((v == 0 && m) || is_inf(v)) rt_errno = E_RANGE;
    } else {
        char dig[801];
        int nd = 0, e10 = 0, any = 0, dropped = 0;
        for (; rt_isdigit((u8)*p); p++) {
            any = 1;
            if (!nd && *p == '0') continue;
            if (nd < 800) dig[nd++] = *p;
            else { e10++; if (*p != '0') dropped = 1; }
        }
        if (*p == g_dp) {
            const char *q = p + 1;
            if (any || rt_isdigit((u8)*q)) {
                p = q;
                for (; rt_isdigit((u8)*p); p++) {
                    any = 1;
                    if (!nd && *p == '0') { e10--; continue; }
                    if (nd < 800) { dig[nd++] = *p; e10--; }
                    else if (*p != '0') dropped = 1;
                }
            }
        }
        if (!any) {
            if (end) *end = (char *)s;
            return 0.0;
        }
        if ((*p | 32) == 'e') {
            const char *x = p + 1;
            int eneg = 0;
            if (*x == '+' || *x == '-') eneg = *x++ == '-';
            if (rt_isdigit((u8)*x)) {
                int ex = 0;
                for (; rt_isdigit((u8)*x); x++)
                    if (ex < 100000) ex = ex * 10 + (*x - '0');
                e10 += eneg ? -ex : ex;
                p = x;
            }
        }
        while (nd > 0 && dig[nd - 1] == '0' && !dropped) { nd--; e10++; }
        if (dropped) { dig[nd++] = '1'; e10--; }   // something below the kept digits
        v = nd ? dec_to_double(dig, nd, e10) : 0.0;
        if ((nd && v == 0) || is_inf(v)) rt_errno = E_RANGE;
    }
    if (end) *end = (char *)p;
    return neg ? -v : v;
}

// The double rounded to float. ERANGE when the float cannot hold it: beyond
// FLT_MAX (HUGE_VALF comes back) or below FLT_MIN (a denormal or zero).
// A NaN keeps its payload ("nan(5)": 5 in the float's low bits).
float rt_strtof(const char *s, char **end)
{
    double d = rt_strtod(s, end);
    if (is_nan(d))
        return nan_to_float(d);
    float f = (float)d;
    if (is_finite(d)) {
        double a = (double)f < 0 ? -(double)f : (double)f;
        if (is_inf(a) || (d != 0 && a < 1.17549435082228750797e-38)) rt_errno = E_RANGE;
    }
    return f;
}
double rt_atof(const char *s) { return rt_strtod(s, 0); }

// the magnitude, its sign, and whether it overflowed 64 bits
static u64 scan_uint(const char *s, char **end, int base, int *neg, int *ovf)
{
    const char *p = s;
    *neg = *ovf = 0;
    while (rt_isspace((u8)*p)) p++;
    if (*p == '+' || *p == '-')
        *neg = *p++ == '-';
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 32) == 'x' && digit_val((u8)p[2]) < 16) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = p[0] == '0' ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        rt_errno = E_INVAL;
        if (end) *end = (char *)s;
        return 0;
    }
    const char *start = p;
    u64 v = 0;
    for (; digit_val((u8)*p) < base; p++) {
        u64 d = (u64)digit_val((u8)*p);
        if (v > (~0ull - d) / (u64)base) *ovf = 1;
        else v = v * (u64)base + d;
    }
    if (p == start) {
        if (end) *end = (char *)s;
        return 0;
    }
    if (end) *end = (char *)p;
    return v;
}

i64 rt_strtoll(const char *s, char **end, int base)
{
    int neg, ovf;
    u64 v = scan_uint(s, end, base, &neg, &ovf);
    if (!neg && (ovf || v > 0x7FFFFFFFFFFFFFFFull)) { rt_errno = E_RANGE; return 0x7FFFFFFFFFFFFFFFll; }
    if (neg && (ovf || v > 0x8000000000000000ull)) { rt_errno = E_RANGE; return (i64)0x8000000000000000ull; }
    return neg ? (i64)(0 - v) : (i64)v;
}

u64 rt_strtoull(const char *s, char **end, int base)
{
    int neg, ovf;
    u64 v = scan_uint(s, end, base, &neg, &ovf);
    if (ovf) { rt_errno = E_RANGE; return ~0ull; }
    return neg ? 0 - v : v;
}

int rt_atoi(const char *s) { return (int)rt_strtoll(s, 0, 10); }
i64 rt_atoll(const char *s) { return rt_strtoll(s, 0, 10); }

// ---- heap: blocks in address order; the free ones also sit in bins by size ----
// Each block knows its neighbours in memory (next, prev): free merges with free neighbours at
// once. A free block's first 16 bytes link it into the bin of its size (exact bins of 16 bytes
// up to 1 KB, then one bin per power of two), so malloc and free never walk the used blocks.
// No two free blocks lie side by side (they are merged), except across pieces (a fence between).

typedef struct Block { size_t size; int used; struct Block *next, *prev; } Block;  // 32 bytes: blocks stay 16-aligned
typedef struct { Block *fnext, *fprev; } FreeLink;   // in a free block's body
#define HEAP_SMALL_BINS 64                         // 16, 32 … 1024 bytes
#define HEAP_BINS (HEAP_SMALL_BINS + 48)           // then [2^k, 2^(k+1)), k = 10…
static Block *g_heap, *g_heap_tail;
static Block *g_bins[HEAP_BINS];
static u64 g_bin_mask[2];

// Threads on several cores share the heap: one lock (cc_irq_off: no thread is
// stopped while it holds it).
u64 cc_irq_off(void);
void cc_irq_restore(u64 f);
static volatile long g_heap_lock;

static u64 heap_lock(void)
{
    u64 f = cc_irq_off();
    while (_InterlockedCompareExchange(&g_heap_lock, 1, 0)) _mm_pause();
    return f;
}

static void heap_unlock(u64 f)
{
    _InterlockedExchange(&g_heap_lock, 0);
    cc_irq_restore(f);
}

static FreeLink *flink(Block *b) { return (FreeLink *)(b + 1); }

static int bin_of(size_t size)                     // every block of bin i holds at least what bin i serves
{
    if (size <= 1024) return size < 16 ? 0 : (int)(size >> 4) - 1;
    unsigned long k;
    _BitScanReverse64(&k, (unsigned __int64)size);
    int i = HEAP_SMALL_BINS + (int)k - 10;
    return i < HEAP_BINS ? i : HEAP_BINS - 1;
}

static void bin_put(Block *b)
{
    int i = bin_of(b->size);
    FreeLink *l = flink(b);
    l->fprev = 0;
    l->fnext = g_bins[i];
    if (g_bins[i]) flink(g_bins[i])->fprev = b;
    g_bins[i] = b;
    g_bin_mask[i >> 6] |= 1ull << (i & 63);
}

static void bin_take(Block *b)
{
    int i = bin_of(b->size);
    FreeLink *l = flink(b);
    if (l->fprev) flink(l->fprev)->fnext = l->fnext; else g_bins[i] = l->fnext;
    if (l->fnext) flink(l->fnext)->fprev = l->fprev;
    if (!g_bins[i]) g_bin_mask[i >> 6] &= ~(1ull << (i & 63));
}

// A free block that holds n bytes, or 0.
static Block *heap_find(size_t n)
{
    int i = bin_of(n);
    if (i >= HEAP_SMALL_BINS) {                    // a bin of mixed sizes: first fit in it
        for (Block *b = g_bins[i]; b; b = flink(b)->fnext) if (b->size >= n) return b;
        i++;
    }
    for (int w = i >> 6; w < 2; w++) {             // any block of a later bin is big enough
        u64 m = g_bin_mask[w];
        if (w == i >> 6) m &= ~0ull << (i & 63);
        if (m) {
            unsigned long k;
            _BitScanForward64(&k, m);
            return g_bins[w * 64 + (int)k];
        }
    }
    return 0;
}

void rt_heap_reset(void *arena, size_t size)
{
    g_heap_lock = 0;
    for (int i = 0; i < HEAP_BINS; i++) g_bins[i] = 0;
    g_bin_mask[0] = g_bin_mask[1] = 0;
    g_heap = g_heap_tail = (Block *)arena;
    if (!g_heap || size < 2 * sizeof(Block)) { g_heap = g_heap_tail = 0; return; }   // empty: the first malloc asks for a piece
    g_heap->size = size - sizeof(Block);
    g_heap->used = 0;
    g_heap->next = g_heap->prev = 0;
    bin_put(g_heap);
}

// b (used, in no bin) keeps n bytes; the rest becomes a free block of its own. The block
// after b is used (free neighbours are always merged), so the rest goes straight to its bin.
static void heap_split(Block *b, size_t n)
{
    if (b->size >= n + sizeof(Block) + 16) {
        Block *rest = (Block *)((u8 *)(b + 1) + n);
        rest->size = b->size - n - sizeof(Block);
        rest->used = 0;
        rest->next = b->next;
        rest->prev = b;
        if (b->next) b->next->prev = rest; else g_heap_tail = rest;
        b->next = rest;
        b->size = n;
        bin_put(rest);
    }
}

// The heap grows: when nothing fits, the system gives another piece (program.c).
// A piece starts with an empty used block, so free neighbours are never merged
// across two pieces that do not touch.
void *cc_heap_more(size_t want, size_t *got);

static int heap_grow(size_t n)
{
    size_t got = 0;
    u8 *m = (u8 *)cc_heap_more(n + 2 * sizeof(Block), &got);   // asked without the lock: it may take a while
    if (!m || got < n + 2 * sizeof(Block)) return 0;
    Block *fence = (Block *)m, *b = fence + 1;
    fence->size = 0;
    fence->used = 1;
    fence->next = b;
    b->size = got - 2 * sizeof(Block);
    b->used = 0;
    b->next = 0;
    b->prev = fence;
    u64 f = heap_lock();
    fence->prev = g_heap_tail;
    if (!g_heap_tail) g_heap = fence; else g_heap_tail->next = fence;
    g_heap_tail = b;
    bin_put(b);
    heap_unlock(f);
    return 1;
}

void *rt_malloc(size_t n)
{
    if (n > ((size_t)1 << 40))
        return 0;
    n = n ? (n + 15) & ~(size_t)15 : 16;
    for (int round = 0; round < 2; round++) {
        u64 f = heap_lock();
        Block *b = heap_find(n);
        if (b) {
            bin_take(b);
            b->used = 1;
            heap_split(b, n);
            heap_unlock(f);
            return b + 1;
        }
        heap_unlock(f);
        if (round || !heap_grow(n)) break;
    }
    rt_errno = E_NOMEM;
    return 0;
}

void rt_free(void *p)
{
    if (!p)
        return;
    Block *b = (Block *)p - 1;
    if (b->used == 2) {                             // aligned more than 16: the block it is inside
        p = b->next;
        b = (Block *)p - 1;
    }
    u64 f = heap_lock();
    if (b->used) {                                 // freed twice: nothing to do
        b->used = 0;
        Block *nx = b->next, *pv = b->prev;
        if (nx && !nx->used) {                     // merge with the free neighbours
            bin_take(nx);
            b->size += sizeof(Block) + nx->size;
            b->next = nx->next;
            if (nx->next) nx->next->prev = b; else g_heap_tail = b;
        }
        if (pv && !pv->used) {
            bin_take(pv);
            pv->size += sizeof(Block) + b->size;
            pv->next = b->next;
            if (b->next) b->next->prev = pv; else g_heap_tail = pv;
            b = pv;
        }
        bin_put(b);
    }
    heap_unlock(f);
}

void *rt_calloc(size_t n, size_t m)
{
    if (m && n > ((size_t)1 << 40) / m)
        return 0;
    void *p = rt_malloc(n * m);
    if (p)
        rt_memset(p, 0, n * m);
    return p;
}

void *rt_realloc(void *p, size_t n)
{
    if (!p)
        return rt_malloc(n);
    if (!n) {
        rt_free(p);
        return 0;
    }
    Block *b = (Block *)p - 1;
    if (b->size >= n)
        return p;
    if (b->used == 2) {                             // aligned more: a new block (16-aligned), as C allows
        void *q = rt_malloc(n);
        if (q) {
            rt_memcpy(q, p, b->size);
            rt_free(p);
        }
        return q;
    }
    size_t want = (n + 15) & ~(size_t)15;
    u64 f = heap_lock();
    Block *nx = b->next;
    if (nx && !nx->used && b->size + sizeof(Block) + nx->size >= want) {  // grow into the free block after it
        bin_take(nx);
        b->size += sizeof(Block) + nx->size;
        b->next = nx->next;
        if (nx->next) nx->next->prev = b; else g_heap_tail = b;
        heap_split(b, want);
        heap_unlock(f);
        return p;
    }
    heap_unlock(f);
    void *q = rt_malloc(n);
    if (q) {
        rt_memcpy(q, p, b->size);
        rt_free(p);
    }
    return q;
}
// More than 16 (blocks are 16-aligned): a bigger block, the address inside it rounded up;
// before it a header of its own (used 2) says where the block is, for free and realloc
void *rt_aligned_alloc(size_t align, size_t n)
{
    if (!align || (align & (align - 1))) {
        rt_errno = E_INVAL;
        return 0;
    }
    if (align <= 16)
        return rt_malloc(n);
    u8 *raw = rt_malloc(n + align + sizeof(Block));
    if (!raw)
        return 0;
    u8 *at = (u8 *)(((u64)(raw + sizeof(Block)) + align - 1) & ~(u64)(align - 1));
    Block *h = (Block *)at - 1;
    h->used = 2;
    h->next = (Block *)raw;
    h->size = n;
    return at;
}

void *rt__aligned_malloc(size_t n, size_t align) { return rt_aligned_alloc(align, n); }
size_t rt__msize(void *p) { return p ? ((Block *)p - 1)->size : 0; }
void *rt__expand(void *p, size_t n) { return p && ((Block *)p - 1)->size >= n ? p : 0; }

// ---- stdlib: numbers, sorting, the end of the program ----

int rt_abs(int v) { return v < 0 ? -v : v; }
i64 rt_llabs(i64 v) { return v < 0 ? -v : v; }

typedef struct { int quot, rem; } RtDiv;
typedef struct { i64 quot, rem; } RtLDiv;
RtDiv rt_div(int a, int b) { RtDiv r; r.quot = a / b; r.rem = a % b; return r; }
RtLDiv rt_lldiv(i64 a, i64 b) { RtLDiv r; r.quot = a / b; r.rem = a % b; return r; }

#ifdef __WINCC__
// Windows: a long is 4 bytes. The functions of long take these, not the long long ones.
static long clamp_long(i64 v)
{
    if (v > 2147483647) { rt_errno = E_RANGE; return 2147483647; }
    if (v < -2147483647 - 1) { rt_errno = E_RANGE; return -2147483647 - 1; }
    return (long)v;
}

// strtoull gives "-5" as 2^64 - 5: a small negative stays, as for unsigned long it is 2^32 - 5
static unsigned long clamp_ulong(u64 v)
{
    if (v > 0xFFFFFFFFull && v < 0xFFFFFFFF00000001ull) { rt_errno = E_RANGE; return 0xFFFFFFFFul; }
    return (unsigned long)v;
}

typedef struct { long quot, rem; } RtLDiv32;
long rt_labs(long v) { return v < 0 ? -v : v; }
RtLDiv32 rt_ldiv(long a, long b) { RtLDiv32 r; r.quot = a / b; r.rem = a % b; return r; }
long rt_atol(const char *s) { return (long)rt_strtoll(s, 0, 10); }
long rt_strtol(const char *s, char **end, int base) { return clamp_long(rt_strtoll(s, end, base)); }
unsigned long rt_strtoul(const char *s, char **end, int base) { return clamp_ulong(rt_strtoull(s, end, base)); }
#endif

static u32 g_rand_next = 1;

int rt_rand(void)
{
    // the example generator of the C standard: RAND_MAX 32767
    g_rand_next = g_rand_next * 1103515245u + 12345u;
    return (int)((g_rand_next / 65536u) % 32768u);
}

void rt_srand(unsigned seed) { g_rand_next = seed; }

static void mem_swap(u8 *a, u8 *b, size_t n)
{
    while (n--) { u8 t = *a; *a++ = *b; *b++ = t; }
}

// Quicksort (median of three, Sedgewick's partition), insertion sort for
// short runs; recursion only into the smaller half.
void rt_qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *))
{
    u8 *b = base;
    while (n > 8) {
        u8 *lo = b, *mid = b + (n / 2) * sz, *hi = b + (n - 1) * sz;
        if (cmp(mid, lo) < 0) mem_swap(mid, lo, sz);
        if (cmp(hi, mid) < 0) {
            mem_swap(hi, mid, sz);
            if (cmp(mid, lo) < 0) mem_swap(mid, lo, sz);
        }
        mem_swap(lo, mid, sz);          // the pivot waits at b[0]
        size_t i = 0, j = n;
        for (;;) {
            do i++; while (i < n - 1 && cmp(b + i * sz, b) < 0);
            do j--; while (j > 0 && cmp(b, b + j * sz) < 0);
            if (i >= j)
                break;
            mem_swap(b + i * sz, b + j * sz, sz);
        }
        mem_swap(b, b + j * sz, sz);
        size_t left = j, right = n - j - 1;
        if (left < right) {
            rt_qsort(b, left, sz, cmp);
            b += (j + 1) * sz;
            n = right;
        } else {
            rt_qsort(b + (j + 1) * sz, right, sz, cmp);
            n = left;
        }
    }
    for (size_t i = 1; i < n; i++)
        for (size_t k = i; k > 0 && cmp(b + (k - 1) * sz, b + k * sz) > 0; k--)
            mem_swap(b + (k - 1) * sz, b + k * sz, sz);
}

void *rt_bsearch(const void *key, const void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *))
{
    const u8 *b = base;
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(key, b + mid * sz);
        if (!c) return (void *)(b + mid * sz);
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return 0;
}

char *rt_getenv(const char *name) { (void)name; return 0; }
// no command processor for programs: system(NULL) says so, a command fails with ENOSYS
int rt_system(const char *cmd)
{
    if (!cmd) return 0;
    rt_errno = E_NOSYS;
    return -1;
}

// exit() comes back to rt_run_main through this jump
static RtJmpBuf g_exit_jmp;
static int g_exit_armed, g_exit_code, g_exit_quick;
static void (*g_atexit_first[32])(void);
static void (**g_atexit)(void) = g_atexit_first;     // grows in the heap past 32
static int g_atexit_n, g_atexit_cap = 32;
static void (*g_at_quick[32])(void);
static int g_at_quick_n;
static i64 g_clock_start;
static void (*g_signal[32])(int);

static char g_locale[16];

static void lib_reset(void)
{
    rt_errno = 0;
    rt_signgam = 0;
    rt_strcpy(g_locale, "C");
    g_rand_next = 1;
    g_strtok = 0;
    g_files = 0;
    std_reset();
    g_con_len = 0;
    g_atexit_n = g_at_quick_n = 0;
    g_exit_code = g_exit_quick = 0;
    for (int i = 0; i < 32; i++)
        g_signal[i] = 0;
    g_clock_start = (i64)cc_millis();
}

int rt_atexit(void (*fn)(void))
{
    if (g_atexit_n >= g_atexit_cap) {
        void (**more)(void) = rt_malloc(sizeof(*more) * (size_t)g_atexit_cap * 2);
        if (!more) return -1;
        rt_memcpy(more, g_atexit, sizeof(*more) * (size_t)g_atexit_n);
        g_atexit = more;            // the old one is not freed: the first is static
        g_atexit_cap *= 2;
    }
    g_atexit[g_atexit_n++] = fn;
    return 0;
}

int rt_at_quick_exit(void (*fn)(void))
{
    if (g_at_quick_n >= 32) return -1;
    g_at_quick[g_at_quick_n++] = fn;
    return 0;
}

void rt_exit(int code)
{
    g_exit_code = code;
    if (g_exit_armed)
        cc_longjmp(&g_exit_jmp, 1);
    rt_fflush(0);               // not inside a program run: nowhere to go back to
#ifdef __WINCC__
    con_flush();
    wcc_exit_process(code);     // a DLL's exit(), or one after main: the process ends
#endif
}

void rt__Exit(int code)
{
    g_exit_quick = 1;
    rt_exit(code);
}

void rt_quick_exit(int code)
{
    while (g_at_quick_n > 0)
        g_at_quick[--g_at_quick_n]();
    rt__Exit(code);
}

static void abort_signal(void);

void rt_abort(void)
{
    abort_signal();             // a SIGABRT handler may longjmp away
    con_flush();
    cc_print("abort()\n");
    rt__Exit(3);
}

// Runs a program's main: exit() and abort() come back here, atexit
// functions run, files still open are written out. main may take
// (int argc, char **argv): argv[0] is the program, then its arguments.
int rt_run_main_args(int (*entry)(void), int argc, char **argv)
{
    lib_reset();
    g_exit_armed = 1;
    if (!cc_setjmp(&g_exit_jmp))
        g_exit_code = ((int (*)(int, char **))entry)(argc, argv);
    // main returned, or exit() jumped here (also from an atexit function)
    if (!g_exit_quick) {
        while (g_atexit_n > 0)
            g_atexit[--g_atexit_n]();
        rt_fflush(0);
    }
    con_flush();
    g_exit_armed = 0;
    return g_exit_code;
}

#ifdef __WINCC__
// A DLL has no main: the library is made ready when Windows loads it (start.c).
void rt_lib_init(void)
{
    lib_reset();
}
#endif

int rt_run_main(int (*entry)(void))
{
    static char *none[1] = { 0 };
    return rt_run_main_args(entry, 0, none);
}

void rt_assert_fail(const char *expr, const char *file, int line)
{
    rt_fprintf(rt_stderr, "Assertion failed: %s, file %s, line %d\n", expr, file, line);
    rt_abort();
}

void rt_perror(const char *s)
{
    if (s && *s)
        rt_fprintf(rt_stderr, "%s: %s\n", s, rt_strerror(rt_errno));
    else
        rt_fprintf(rt_stderr, "%s\n", rt_strerror(rt_errno));
}

char *rt_tmpnam(char *buf)
{
    static char own[32];
    static int counter;
    if (!buf) buf = own;
    for (int tries = 0; tries < 10000; tries++) {
        rt_snprintf(buf, 32, "tmp%05d.tmp", ++counter);
        size_t sz;
        if (!cc_file_read(buf, &sz))
            return buf;
    }
    return 0;
}

// ---- setjmp for programs ----
// The compiler keeps expression temporaries on the stack: in "r = setjmp(e)"
// the address of r was pushed before the call and is popped after it; by the
// time longjmp comes back, later calls have written over that stack. So
// setjmp (a thunk in setjmp.h) has this copy the piece between the stack
// pointer after the call (sp) and the caller's local variables, and longjmp
// puts it back. Where the locals end comes from the caller's prologue,
// "push rbp; mov rbp, rsp; sub rsp, n", found by looking back from the
// return address. env: [0, 240) registers (cc_setjmp), [240] the piece's
// size, [248] its place, [256, 4096) its bytes (jmp_buf is 4 KB).
void rt_sj_save(i64 *env, u8 *sp, u8 *fp, const u8 *ret)
{
    env[30] = 0;
    env[31] = (i64)sp;
    const u8 *p = ret - 4;
    for (int i = 0; i < (1 << 22); i++, p--) {
        if (p[0] != 0x55 || p[1] != 0x48 || p[2] != 0x8B || p[3] != 0xEC)
            continue;
        u64 size = 0;
        if (p[4] == 0x48 && p[5] == 0x81 && p[6] == 0xEC)
            size = (u64)p[7] | (u64)p[8] << 8 | (u64)p[9] << 16 | (u64)p[10] << 24;
        else if (p[4] == 0x48 && p[5] == 0x83 && p[6] == 0xEC)
            size = p[7];
        u8 *locals = fp - size;
        if (locals >= sp && locals - sp <= 4096 - 256 && !((locals - sp) & 7)) {
            rt_memcpy(env + 32, sp, (size_t)(locals - sp));
            env[30] = locals - sp;
        } else if (locals >= sp) {
            cc_print("setjmp: the expression around it is too big\n");
        }
        return;
    }
}

// ---- signals: handlers are kept, raise() calls them ----

typedef void (*SigFn)(int);
#define SIG_DFL_V ((SigFn)0)
#define SIG_IGN_V ((SigFn)1)

SigFn rt_signal(int sig, SigFn fn)
{
    if (sig <= 0 || sig >= 32) {
        rt_errno = E_INVAL;
        return (SigFn)~(u64)0;      // SIG_ERR
    }
    SigFn old = g_signal[sig];
    g_signal[sig] = fn;
    return old;
}

int rt_raise(int sig)
{
    if (sig <= 0 || sig >= 32) {
        rt_errno = E_INVAL;
        return -1;
    }
    SigFn fn = g_signal[sig];
    if (fn == SIG_IGN_V)
        return 0;
    if (fn == SIG_DFL_V) {
        con_flush();
        cc_print("signal\n");
        rt__Exit(3);
        return 0;
    }
    g_signal[sig] = SIG_DFL_V;
    fn(sig);
    return 0;
}

// CPU faults (SIGFPE, SIGILL, SIGSEGV), sent by the host: does the program
// catch this signal? Then rt_signal_call runs its handler (the host ends
// the program if the handler returns; ignoring a fault is not possible).
int rt_signal_caught(int sig)
{
    return sig > 0 && sig < 32 && g_exit_armed && g_signal[sig] != SIG_DFL_V && g_signal[sig] != SIG_IGN_V;
}

void rt_signal_call(int sig)
{
    SigFn fn = g_signal[sig];
    g_signal[sig] = SIG_DFL_V;          // a second fault in the handler ends the program
    fn(sig);
    con_flush();
}

static void abort_signal(void)
{
    SigFn fn = g_signal[22];
    if (fn != SIG_DFL_V && fn != SIG_IGN_V) {
        g_signal[22] = SIG_DFL_V;
        fn(22);
    }
}

// ---- locale: only "C" ----

typedef struct {
    char *decimal_point, *thousands_sep, *grouping, *int_curr_symbol, *currency_symbol;
    char *mon_decimal_point, *mon_thousands_sep, *mon_grouping, *positive_sign, *negative_sign;
    char int_frac_digits, frac_digits, p_cs_precedes, p_sep_by_space;
    char n_cs_precedes, n_sep_by_space, p_sign_posn, n_sign_posn;
} RtLconv;

#ifdef __WINCC__
// Each category has its locale: LC_COLLATE (strcoll), LC_CTYPE (strerror's language),
// LC_NUMERIC (the decimal point of printf, scanf, strtod), LC_TIME (strftime's names),
// LC_MONETARY (localeconv). Names: "C", "POSIX", "ru_RU.UTF-8", "ru-RU", "Russian",
// "Russian_Russia.1251", "" (the user's), ".UTF-8"; a locale of Windows behind each.
// Multibyte text is UTF-8 in all of them.
static char g_loc[6][48] = { "C", "C", "C", "C", "C", "C" };
static char g_all[300];
static char g_dp_text[8] = ".", g_ts_text[8] = "";

// the locale of a category; 0 for "C", "POSIX", "C.UTF-8" (they all behave as C)
static const char *locale_of(int cat)
{
    const char *n = g_loc[cat];
    return (n[0] == 'C' && (!n[1] || n[1] == '.')) || !rt_strcmp(n, "POSIX") ? 0 : n;
}

static int locale_ru(void)
{
    return !rt_strncmp(g_loc[2], "ru_", 3) || !rt_strncmp(g_loc[2], "ru.", 3);
}

static void locale_numbers(void)
{
    const char *loc = locale_of(4);
    int dp = loc ? cc_locale_char(loc, 0) : '.';
    int ts = loc ? cc_locale_char(loc, 1) : 0;
    g_dp = (char)(dp > 0 && dp < 128 ? dp : '.');
    g_dp_text[0] = g_dp;
    g_dp_text[1] = 0;
    g_ts_text[0] = (char)(ts > 0 && ts < 128 ? ts : 0);
    g_ts_text[1] = 0;
}

char *rt_setlocale(int cat, const char *name)
{
    if (cat < 0 || cat > 5) return 0;
    if (!name) {
        if (cat)
            return g_loc[cat];
        int same = 1;
        for (int i = 2; i <= 5; i++)
            if (rt_strcmp(g_loc[i], g_loc[1])) same = 0;
        if (same)
            return g_loc[1];
        rt_snprintf(g_all, sizeof(g_all), "LC_COLLATE=%s;LC_CTYPE=%s;LC_MONETARY=%s;LC_NUMERIC=%s;LC_TIME=%s",
                    g_loc[1], g_loc[2], g_loc[3], g_loc[4], g_loc[5]);
        return g_all;
    }
    char found[48];
    if (!rt_strcasecmp(name, "C") || !rt_strcasecmp(name, "POSIX"))
        rt_strcpy(found, rt_strcasecmp(name, "C") ? "POSIX" : "C");
    else if (!rt_strcasecmp(name, "C.UTF-8") || !rt_strcasecmp(name, "C.utf8"))
        rt_strcpy(found, "C.UTF-8");
    else if (!cc_locale_find(name, found, sizeof(found)))
        return 0;
    for (int i = 1; i <= 5; i++)
        if (!cat || cat == i)
            rt_strcpy(g_loc[i], found);
    rt_strcpy(g_loc[0], found);
    locale_numbers();
    return cat ? g_loc[cat] : rt_setlocale(0, 0);
}
#else
// Every locale behaves as "C", with UTF-8 multibyte text; these names are
// accepted, "" picks C.UTF-8. (One locale for all categories.)
static char g_locale[16] = "C";

char *rt_setlocale(int cat, const char *name)
{
    static const char *const known[] = { "C", "POSIX", "C.UTF-8", "ru_RU.UTF-8", "en_US.UTF-8", 0 };
    if (cat < 0 || cat > 5) return 0;
    if (!name) return g_locale;
    if (!*name) name = "C.UTF-8";
    for (int i = 0; known[i]; i++) {
        if (!rt_strcasecmp(name, known[i]) ||
            (rt_strlen(known[i]) > 5 && !rt_strncasecmp(name, known[i], rt_strlen(known[i]) - 5) &&
             !rt_strcasecmp(name + rt_strlen(known[i]) - 5, "UTF8"))) {     // "ru_RU.utf8" too
            rt_strcpy(g_locale, known[i]);
            return g_locale;
        }
    }
    return 0;
}

#endif

RtLconv *rt_localeconv(void)
{
    static RtLconv lc = { ".", "", "", "", "", "", "", "", "", "", 127, 127, 127, 127, 127, 127, 127, 127 };
#ifdef __WINCC__
    lc.decimal_point = g_dp_text;
    lc.thousands_sep = g_ts_text;
    lc.grouping = g_ts_text[0] ? "\3" : "";
#endif
    return &lc;
}

// ---- math ----

#define LN2_HI    6.93147180369123816490e-01     // 32 bits: k * LN2_HI is exact
#define LN2_LO    1.90821492927058770002e-10
#define INV_LN2   1.44269504088896338700e+00
#define INV_LN10  0.43429448190325182765
#define LOG10_2   0.30102999566398119521
#define SQRT2     1.41421356237309504880
#define INV_PIO2  0.63661977236758134308
#define SQRT_PI   1.77245385090551602730

static void two_sum(double a, double b, double *s, double *e)
{
    double x = a + b, bb = x - a;
    *s = x;
    *e = (a - (x - bb)) + (b - bb);
}

static void split27(double a, double *hi, double *lo)
{
    double c = 134217729.0 * a;
    *hi = c - (c - a);
    *lo = a - *hi;
}

// a * b = p + e exactly (Dekker; |a|, |b| below 2^995)
static void two_prod(double a, double b, double *p, double *e)
{
    double ah, al, bh, bl;
    *p = a * b;
    split27(a, &ah, &al);
    split27(b, &bh, &bl);
    *e = ((ah * bh - *p) + ah * bl + al * bh) + al * bl;
}

double rt_fabs(double x) { return dfrom(dbits(x) & ~SIGN_BIT); }

double rt_copysign(double x, double y) { return dfrom((dbits(x) & ~SIGN_BIT) | (dbits(y) & SIGN_BIT)); }

double rt_sqrt(double x)
{
    if (x < 0) rt_errno = E_DOM;
#ifdef __WINCC__
    asm("movsd xmm0, qword ptr [rbp+16]\n"      // x, in its home slot
        "sqrtsd xmm0, xmm0\n"
        "jmp .L.return.rt_sqrt");
    return 0;
#else
    __m128d v = _mm_set_sd(x);
    return _mm_cvtsd_f64(_mm_sqrt_sd(v, v));
#endif
}

double rt_trunc(double x)
{
    u64 u = dbits(x);
    int e = (int)((u >> 52) & 0x7FF) - 1023;
    if (e >= 52)
        return x;               // already whole, or inf / nan
    if (e < 0)
        return dfrom(u & SIGN_BIT);
    return dfrom(u & ~(MANT_MASK >> e));
}

double rt_floor(double x)
{
    double t = rt_trunc(x);
    return x < 0 && t != x ? t - 1.0 : t;
}

double rt_ceil(double x)
{
    double t = rt_trunc(x);
    return x > 0 && t != x ? t + 1.0 : t;
}

double rt_round(double x)
{
    double t = rt_trunc(x);
    if (rt_fabs(x - t) >= 0.5)
        t += x < 0 ? -1.0 : 1.0;
    return t;
}

// to the nearest whole number, ties to even
double rt_nearbyint(double x)
{
    double a = rt_fabs(x);
    if (!(a < 4503599627370496.0))
        return x;
    double r = (a + 4503599627370496.0) - 4503599627370496.0;
    return rt_copysign(r, x);
}

i64 rt_llround(double x) { return (i64)rt_round(x); }
i64 rt_llrint(double x) { return (i64)rt_nearbyint(x); }

double rt_modf(double x, double *ip)
{
    double t = rt_trunc(x);
    if (ip) *ip = t;
    if (is_inf(x)) return rt_copysign(0.0, x);
    return rt_copysign(x - t, x);
}

double rt_scalbn(double x, int n)
{
    double y = x;
    if (n > 1023) {
        y *= dfrom(0x7FE0000000000000ull);      // 2^1023
        n -= 1023;
        if (n > 1023) {
            y *= dfrom(0x7FE0000000000000ull);
            n -= 1023;
            if (n > 1023) n = 1023;
        }
    } else if (n < -1022) {
        // steps of 2^-969 keep the last rounding to one, in the subnormal range
        y *= dfrom(0x0360000000000000ull);
        n += 969;
        if (n < -1022) {
            y *= dfrom(0x0360000000000000ull);
            n += 969;
            if (n < -1022) n = -1022;
        }
    }
    return y * dfrom((u64)(0x3FF + n) << 52);
}

double rt_ldexp(double x, int e) { return rt_scalbn(x, e); }
double rt_scalbln(double x, i64 e) { return rt_scalbn(x, e > 100000 ? 100000 : e < -100000 ? -100000 : (int)e); }

double rt_frexp(double x, int *e)
{
    u64 u = dbits(x);
    int ee = (int)((u >> 52) & 0x7FF);
    if (!ee) {
        if (x != 0) {
            x = rt_frexp(x * 18446744073709551616.0, e);    // 2^64
            *e -= 64;
        } else {
            *e = 0;
        }
        return x;
    }
    if (ee == 0x7FF) {
        *e = 0;
        return x;
    }
    *e = ee - 0x3FE;
    return dfrom((u & 0x800FFFFFFFFFFFFFull) | 0x3FE0000000000000ull);
}

int rt_ilogb(double x)
{
    if (x == 0) return -2147483647 - 1;         // FP_ILOGB0
    if (is_nan(x)) return -2147483647 - 1;      // FP_ILOGBNAN
    if (is_inf(x)) return 2147483647;
    int e;
    rt_frexp(x, &e);
    return e - 1;
}

double rt_logb(double x)
{
    if (x == 0) { rt_errno = E_RANGE; return -inf_val(); }
    if (is_nan(x)) return x;
    if (is_inf(x)) return inf_val();
    return (double)rt_ilogb(x);
}

double rt_nextafter(double x, double y)
{
    if (is_nan(x) || is_nan(y)) return x + y;
    if (x == y) return y;
    if (x == 0) return rt_copysign(dfrom(1), y);
    u64 u = dbits(x);
    if ((x < y) == (x > 0)) u++;
    else u--;
    return dfrom(u);
}

double rt_fmod(double x, double y)
{
    u64 ux = dbits(x), uy = dbits(y);
    int ex = (int)((ux >> 52) & 0x7FF), ey = (int)((uy >> 52) & 0x7FF);
    u64 sx = ux & SIGN_BIT, i;
    if ((uy << 1) == 0 || is_nan(y) || ex == 0x7FF) {
        if ((uy << 1) == 0 || ex == 0x7FF) rt_errno = E_DOM;
        return nan_val();
    }
    if ((ux << 1) <= (uy << 1)) {
        if ((ux << 1) == (uy << 1)) return dfrom(sx);
        return x;
    }
    // integer mantissas with explicit leading bits
    if (!ex) { for (i = ux << 12; !(i >> 63); ex--, i <<= 1) {} ux <<= -ex + 1; }
    else { ux &= MANT_MASK; ux |= 1ull << 52; }
    if (!ey) { for (i = uy << 12; !(i >> 63); ey--, i <<= 1) {} uy <<= -ey + 1; }
    else { uy &= MANT_MASK; uy |= 1ull << 52; }
    for (; ex > ey; ex--) {
        i = ux - uy;
        if (!(i >> 63)) {
            if (!i) return dfrom(sx);
            ux = i;
        }
        ux <<= 1;
    }
    i = ux - uy;
    if (!(i >> 63)) {
        if (!i) return dfrom(sx);
        ux = i;
    }
    for (; !(ux >> 52); ux <<= 1, ex--) {}
    if (ex > 0) { ux -= 1ull << 52; ux |= (u64)ex << 52; }
    else ux >>= -ex + 1;
    return dfrom(ux | sx);
}

// x - n*y with n the whole number nearest x/y (ties to even); *quo gets
// the sign and the low three bits of n
double rt_remquo(double x, double y, int *quo)
{
    int q = 0;
    if (quo) *quo = 0;
    if (is_nan(x) || is_nan(y)) return x + y;
    if (is_inf(x) || y == 0) { rt_errno = E_DOM; return nan_val(); }
    double ay = rt_fabs(y), a = rt_fabs(x);
    if (ay < dfrom(0x7FC0000000000000ull)) {    // 8|y| fits: the low bits of n
        a = rt_fmod(a, 8 * ay);
        if (a >= 4 * ay) { a -= 4 * ay; q += 4; }
        if (a >= 2 * ay) { a -= 2 * ay; q += 2; }
    } else if (ay < dfrom(0x7FE0000000000000ull)) {
        a = rt_fmod(a, 2 * ay);
        if (a >= 2 * ay) { a -= 2 * ay; q += 2; }
    }
    // now a < 2|y|
    if (a >= ay) { a -= ay; q++; }
    if (a > 0.5 * ay || (a == 0.5 * ay && (q & 1))) { a -= ay; q++; }
    if (quo) *quo = sign_of(x) != sign_of(y) ? -(q & 7) : q & 7;
    return sign_of(x) ? -a : a;
}

double rt_remainder(double x, double y) { return rt_remquo(x, y, 0); }

double rt_fmin(double x, double y)
{
    if (is_nan(x)) return y;
    if (is_nan(y)) return x;
    if (x == y) return sign_of(x) ? x : y;
    return x < y ? x : y;
}

double rt_fmax(double x, double y)
{
    if (is_nan(x)) return y;
    if (is_nan(y)) return x;
    if (x == y) return sign_of(x) ? y : x;
    return x > y ? x : y;
}

double rt_fdim(double x, double y)
{
    if (is_nan(x) || is_nan(y)) return x + y;
    return x > y ? x - y : 0.0;
}

double rt_fma(double x, double y, double z)
{
    if (!is_finite(x) || !is_finite(y) || !is_finite(z) || rt_fabs(x) > 1e290 || rt_fabs(y) > 1e290)
        return x * y + z;
    double p, pe, s, se;
    two_prod(x, y, &p, &pe);
    two_sum(p, z, &s, &se);
    return s + (se + pe);
}

double rt_nan(const char *tag) { return nan_with(tag, 0); }       // as strtod("NAN(tag)")

int rt_isnan(double x) { return is_nan(x); }
int rt_isinf(double x) { return is_inf(x); }
int rt_isfinite(double x) { return is_finite(x); }
int rt_signbit(double x) { return sign_of(x); }

int rt_fpclassify(double x)
{
    u64 u = dbits(x) & ~SIGN_BIT;
    if (!u) return 2;                       // FP_ZERO
    if (u < 0x0010000000000000ull) return 3; // FP_SUBNORMAL
    if (u < EXP_MASK) return 4;             // FP_NORMAL
    if (u == EXP_MASK) return 1;            // FP_INFINITE
    return 0;                               // FP_NAN
}

// e^(x + xlo), |xlo| much smaller than x (fdlibm's method)
static double exp_core(double x, double xlo)
{
    static const double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03,
        P3 = 6.61375632143793436117e-05, P4 = -1.65339022054652515390e-06, P5 = 4.13813679705723846039e-08;
    if (is_nan(x))
        return x;
    if (x > 709.782712893383973096) { rt_errno = E_RANGE; return inf_val(); }
    if (x < -745.13321910194110842) { rt_errno = E_RANGE; return 0.0; }
    if (rt_fabs(x) < 3.725290298461914e-09)    // 2^-28
        return 1.0 + (x + xlo);
    int k = (int)(INV_LN2 * x + (x < 0 ? -0.5 : 0.5));
    double hi = x - k * LN2_HI;                 // exact
    double lo = k * LN2_LO - xlo;
    double r = hi - lo, t = r * r;
    double c = r - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    double y = 1.0 - ((lo - (r * c) / (2.0 - c)) - hi);
    return rt_scalbn(y, k);
}

double rt_exp(double x)
{
    if (is_inf(x)) return x > 0 ? x : 0.0;
    return exp_core(x, 0.0);
}

// x = 2^k * m with m in [sqrt(1/2), sqrt(2)); log(m) as *hi + *lo,
// the low part only when lo is given  (x > 0, finite; fdlibm's method)
static double log_m2(double x, int *kp, double *lo)
{
    static const double Lg1 = 6.666666666666735130e-01, Lg2 = 3.999999999940941908e-01,
        Lg3 = 2.857142874366239149e-01, Lg4 = 2.222219843214978396e-01, Lg5 = 1.818357216161805012e-01,
        Lg6 = 1.531383769920937332e-01, Lg7 = 1.479819860511658591e-01;
    u64 u = dbits(x);
    int k = 0;
    if (!(u >> 52)) {           // subnormal
        x *= 18014398509481984.0;   // 2^54
        k = -54;
        u = dbits(x);
    }
    k += (int)(u >> 52) - 1023;
    double m = dfrom((u & MANT_MASK) | 0x3FF0000000000000ull);
    if (m > SQRT2) { m *= 0.5; k++; }
    double f = m - 1.0;
    double s = f / (2.0 + f), z = s * s, w = z * z;
    double t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    double t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    double R = t2 + t1;
    *kp = k;
    if (!lo) {
        double hfsq = 0.5 * f * f;
        return f - (hfsq - s * (hfsq + R));
    }
    // log(m) = f - f*f/2 + s*(f*f/2 + R), the big terms kept exactly
    double p, pe, a, ae, b, be;
    two_prod(f, f, &p, &pe);
    double hfsq = 0.5 * p;
    double T = s * (hfsq + R);
    two_sum(f, -hfsq, &a, &ae);
    two_sum(a, T, &b, &be);
    double l = (ae + be) - 0.5 * pe;
    double h = b + l;
    *lo = l - (h - b);
    return h;
}

static double log_m(double x, int *kp) { return log_m2(x, kp, 0); }

// shared by log, log2, log10: 0 when x needs no more work (*r is the answer)
static int log_special(double x, double *r)
{
    if (is_nan(x)) { *r = x; return 1; }
    if (x < 0) { rt_errno = E_DOM; *r = nan_val(); return 1; }
    if (x == 0) { rt_errno = E_RANGE; *r = -inf_val(); return 1; }
    if (is_inf(x)) { *r = x; return 1; }
    return 0;
}

double rt_log(double x)
{
    double r;
    if (log_special(x, &r)) return r;
    int k;
    double L = log_m(x, &k);
    return k * LN2_HI + (L + k * LN2_LO);
}

double rt_log2(double x)
{
    double r;
    if (log_special(x, &r)) return r;
    int k;
    double L = log_m(x, &k);
    return (double)k + L * INV_LN2;
}

double rt_log10(double x)
{
    double r;
    if (log_special(x, &r)) return r;
    int k;
    double L = log_m(x, &k);
    r = k * LOG10_2 + L * INV_LN10;
    double n = rt_nearbyint(r);                 // exact powers of ten give whole numbers
    if (n >= 0 && n <= 22 && rt_fabs(r - n) < 1e-12 && x == g_p10[(int)n])
        return n;
    return r;
}

double rt_log1p(double x)
{
    if (is_nan(x)) return x;
    if (x == -1.0) { rt_errno = E_RANGE; return -inf_val(); }
    if (x < -1.0) { rt_errno = E_DOM; return nan_val(); }
    if (is_inf(x)) return x;
    double u = 1.0 + x;
    if (u == 1.0) return x;
    return rt_log(u) * x / (u - 1.0);       // corrects the rounding of 1 + x
}

double rt_expm1(double x)
{
    if (is_nan(x)) return x;
    if (x > 40) return rt_exp(x);
    if (x < -40) return -1.0;
    if (rt_fabs(x) < 1e-5) return x + x * x * 0.5 + x * x * x / 6.0;
    double u = rt_exp(x);
    if (u == 1.0) return x;
    double um1 = u - 1.0;
    if (um1 == -1.0) return -1.0;
    return um1 * x / rt_log(u);             // Kahan's correction
}

double rt_exp2(double x)
{
    if (is_nan(x)) return x;
    if (is_inf(x)) return x > 0 ? x : 0.0;
    if (x > 1024) { rt_errno = E_RANGE; return inf_val(); }
    if (x < -1080) { rt_errno = E_RANGE; return 0.0; }
    if (rt_trunc(x) == x) return rt_scalbn(1.0, (int)x);
    double p, e;
    two_prod(x, 0.69314718055994528623, &p, &e);   // ln 2 = 0.693... + 2.319e-17
    return exp_core(p, e + x * 2.3190468138462996e-17);
}

static double pow_core(double ax, double y)    // ax > 0, both finite
{
    int k;
    double Llo, L = log_m2(ax, &k, &Llo);
    double est = y * (k * (LN2_HI + LN2_LO) + L);
    if (est > 710) { rt_errno = E_RANGE; return inf_val(); }
    if (est < -746) { rt_errno = E_RANGE; return 0.0; }
    // log(ax) = k*LN2_HI (exact) + L + Llo + k*LN2_LO, as ch + cl
    double ch, cl, ph, pl;
    two_sum(k * LN2_HI, L, &ch, &cl);
    cl += Llo + k * LN2_LO;
    // y * log(ax) with the big product exact, so big exponents stay accurate
    two_prod(y, ch, &ph, &pl);
    double tl = pl + y * cl;
    double s = ph + tl;
    tl -= s - ph;
    return exp_core(s, tl);
}

double rt_pow(double x, double y)
{
    if (y == 0) return 1.0;
    if (x == 1.0) return 1.0;
    if (is_nan(x) || is_nan(y)) return x + y;
    double ax = rt_fabs(x), ay = rt_fabs(y);
    int yint = ay >= 4503599627370496.0 || rt_trunc(y) == y;
    int yodd = 0;
    if (yint && ay < 9007199254740992.0) {
        double h = y * 0.5;
        yodd = rt_trunc(h) != h;
    }
    if (is_inf(y)) {
        if (ax == 1.0) return 1.0;
        return (ax > 1.0) == (y > 0) ? inf_val() : 0.0;
    }
    if (x == 0) {
        if (y < 0) {
            rt_errno = E_RANGE;
            return yodd ? rt_copysign(inf_val(), x) : inf_val();
        }
        return yodd ? x : 0.0;
    }
    if (is_inf(x)) {
        if (x > 0) return y < 0 ? 0.0 : x;
        if (y < 0) return yodd ? -0.0 : 0.0;
        return yodd ? x : -x;
    }
    double sign = 1.0;
    if (x < 0) {
        if (!yint) { rt_errno = E_DOM; return nan_val(); }
        if (yodd) sign = -1.0;
    }
    if (y == 0.5) return rt_sqrt(ax);
    if (y == 1.0) return x;
    if (yint && ay <= 64) {     // small whole powers: exact whenever the result fits
        u64 n = (u64)ay;
        double r = 1.0, b = ax;
        while (n) {
            if (n & 1) r *= b;
            n >>= 1;
            if (n) b *= b;
        }
        if (y < 0 && is_finite(r) && r > 2.2250738585072014e-308 && r < 4.4e307)
            return sign / r;
        if (y > 0 && is_finite(r) && r > 2.2250738585072014e-308)
            return sign * r;
    }
    return sign * pow_core(ax, y);
}

double rt_cbrt(double x)
{
    if (x == 0 || !is_finite(x))
        return x;
    double a = rt_fabs(x);
    int e;
    double m = rt_frexp(a, &e);         // a = m * 2^e, m in [0.5, 1)
    int r = e % 3;
    if (r < 0) r += 3;
    m = rt_ldexp(m, r);                 // m in [0.5, 4), e - r divisible by 3
    e -= r;
    double y = 0.7 + 0.3 * m;           // then Newton
    for (int i = 0; i < 7; i++)
        y -= (y * y * y - m) / (3.0 * y * y);
    y = rt_ldexp(y, e / 3);
    return x < 0 ? -y : y;
}

double rt_hypot(double x, double y)
{
    if (is_inf(x) || is_inf(y)) return inf_val();
    if (is_nan(x) || is_nan(y)) return x + y;
    double a = rt_fabs(x), b = rt_fabs(y);
    if (a < b) { double t = a; a = b; b = t; }
    if (b == 0) return a;
    double r = b / a;
    return a * rt_sqrt(1.0 + r * r);
}

// ---- trigonometry: pi from Machin's formula, computed once to 1376 bits ----

#define PI_FRAC 1376
static Big g_pio2_fix;                  // floor(pi/2 * 2^PI_FRAC)
static double g_p1, g_p2, g_p3, g_p4;   // pi/2 in pieces of 33, 33, 33 and 53 bits
static double g_atan_hi[4], g_atan_lo[4];   // atan(0.5), atan(1), atan(1.5), pi/2
static int g_trig_ready;

static void atan_inv(Big *sum, u32 n)  // atan(1/n) * 2^PI_FRAC
{
    Big term, t;
    big_set(&term, 1);
    big_shl(&term, PI_FRAC);
    big_div_small(&term, n);
    *sum = term;
    u32 n2 = n * n;
    for (u32 k = 1; term.n; k++) {
        big_div_small(&term, n2);
        t = term;
        big_div_small(&t, 2 * k + 1);
        if (k & 1) big_sub(sum, &t);
        else big_add(sum, &t);
    }
}

static void fix_to_dd(const Big *b, double *hi, double *lo)
{
    int L = big_bitlen(b);
    double h = rt_ldexp((double)big_bits(b, L - 53, 53), L - 53 - PI_FRAC);
    double l = rt_ldexp((double)big_bits(b, L - 106, 53), L - 106 - PI_FRAC);
    double s = h + l;
    *lo = l - (s - h);
    *hi = s;
}

static double fix_piece(const Big *b, int top, int count)
{
    return rt_ldexp((double)big_bits(b, top - count, count), top - count - PI_FRAC);
}

static void trig_init(void)
{
    Big a5, a239, a2, pio4, t;
    atan_inv(&a5, 5);
    atan_inv(&a239, 239);
    atan_inv(&a2, 2);
    pio4 = a5;                          // pi/4 = 4 atan(1/5) - atan(1/239)
    big_mul_add(&pio4, 4, 0);
    big_sub(&pio4, &a239);
    g_pio2_fix = pio4;
    big_mul_add(&g_pio2_fix, 2, 0);
    int top = big_bitlen(&g_pio2_fix);
    g_p1 = fix_piece(&g_pio2_fix, top, 33);
    g_p2 = fix_piece(&g_pio2_fix, top - 33, 33);
    g_p3 = fix_piece(&g_pio2_fix, top - 66, 33);
    g_p4 = fix_piece(&g_pio2_fix, top - 99, 53);
    fix_to_dd(&a2, &g_atan_hi[0], &g_atan_lo[0]);
    fix_to_dd(&pio4, &g_atan_hi[1], &g_atan_lo[1]);
    t = pio4;                           // atan(1.5) = pi/4 + atan(1/5)
    big_add(&t, &a5);
    fix_to_dd(&t, &g_atan_hi[2], &g_atan_lo[2]);
    fix_to_dd(&g_pio2_fix, &g_atan_hi[3], &g_atan_lo[3]);
    g_trig_ready = 1;
}

// |x| from about 1.6e6 up: the exact remainder by long division with pi/2
static int rem_pio2_huge(double ax, double *y0, double *y1)
{
    u64 u = dbits(ax);
    int e = (int)(u >> 52) - 1075;
    Big X, Ds, half, t;
    big_set(&X, (u & MANT_MASK) | (1ull << 52));
    big_shl(&X, e + PI_FRAC);
    Ds = g_pio2_fix;
    int sh = big_bitlen(&X) - big_bitlen(&Ds);
    if (sh < 0) sh = 0;
    big_shl(&Ds, sh);
    u32 q = 0;                          // the low bits of the quotient
    for (int i = sh; i >= 0; i--) {
        if (big_cmp(&X, &Ds) >= 0) {
            big_sub(&X, &Ds);
            if (i < 2) q |= 1u << i;
        }
        if (i) big_shr(&Ds, 1);
    }
    half = g_pio2_fix;
    big_shr(&half, 1);
    int neg = 0;
    if (big_cmp(&X, &half) > 0) {       // nearer the next multiple
        t = g_pio2_fix;
        big_sub(&t, &X);
        X = t;
        neg = 1;
        q++;
    }
    double h = 0, l = 0;
    if (X.n) fix_to_dd(&X, &h, &l);
    *y0 = neg ? -h : h;
    *y1 = neg ? -l : l;
    return (int)(q & 3);
}

// x = n * pi/2 + (y0 + y1), |y0 + y1| <= about pi/4
static int rem_pio2(double x, double *y0, double *y1)
{
    if (!g_trig_ready)
        trig_init();
    double ax = rt_fabs(x);
    if (ax < 1.6e6) {                   // n < 2^20: n * (33-bit piece) is exact
        double fn = rt_nearbyint(x * INV_PIO2);
        double a = x - fn * g_p1;
        double s, e, s2, e2;
        two_sum(a, -(fn * g_p2), &s, &e);
        two_sum(s, -(fn * g_p3), &s2, &e2);
        double tail = (e + e2) - fn * g_p4;
        *y0 = s2 + tail;
        *y1 = tail - (*y0 - s2);
        return (int)fn;
    }
    int n = rem_pio2_huge(ax, y0, y1);
    if (x < 0) {
        *y0 = -*y0;
        *y1 = -*y1;
        n = -n;
    }
    return n;
}

// sin(x + y) and cos(x + y) for |x| <= pi/4, |y| tiny: Taylor series
static double k_sin(double x, double y)
{
    static const double S1 = -1.0 / 6, S2 = 1.0 / 120, S3 = -1.0 / 5040, S4 = 1.0 / 362880,
        S5 = -1.0 / 39916800, S6 = 1.0 / 6227020800.0, S7 = -1.0 / 1307674368000.0,
        S8 = 1.0 / 355687428096000.0, S9 = -1.0 / 121645100408832000.0;
    double z = x * x, v = z * x;
    double r = S2 + z * (S3 + z * (S4 + z * (S5 + z * (S6 + z * (S7 + z * (S8 + z * S9))))));
    return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static double k_cos(double x, double y)
{
    static const double C1 = 1.0 / 24, C2 = -1.0 / 720, C3 = 1.0 / 40320, C4 = -1.0 / 3628800,
        C5 = 1.0 / 479001600, C6 = -1.0 / 87178291200.0, C7 = 1.0 / 20922789888000.0,
        C8 = -1.0 / 6402373705728000.0;
    double z = x * x;
    double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * (C6 + z * (C7 + z * C8)))))));
    double hz = 0.5 * z, w = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}

double rt_sin(double x)
{
    if (!is_finite(x)) {
        if (is_inf(x)) rt_errno = E_DOM;
        return x - x;
    }
    if (rt_fabs(x) <= 0.78539816339744828) {
        if (rt_fabs(x) < 1.4901161193847656e-08) return x;     // 2^-26
        return k_sin(x, 0.0);
    }
    double y0, y1;
    switch (rem_pio2(x, &y0, &y1) & 3) {
    case 0: return k_sin(y0, y1);
    case 1: return k_cos(y0, y1);
    case 2: return -k_sin(y0, y1);
    default: return -k_cos(y0, y1);
    }
}

double rt_cos(double x)
{
    if (!is_finite(x)) {
        if (is_inf(x)) rt_errno = E_DOM;
        return x - x;
    }
    if (rt_fabs(x) <= 0.78539816339744828)
        return k_cos(x, 0.0);
    double y0, y1;
    switch (rem_pio2(x, &y0, &y1) & 3) {
    case 0: return k_cos(y0, y1);
    case 1: return -k_sin(y0, y1);
    case 2: return -k_cos(y0, y1);
    default: return k_sin(y0, y1);
    }
}

double rt_tan(double x)
{
    if (!is_finite(x)) {
        if (is_inf(x)) rt_errno = E_DOM;
        return x - x;
    }
    if (rt_fabs(x) < 1.4901161193847656e-08)
        return x;
    double y0 = x, y1 = 0.0;
    int n = 0;
    if (rt_fabs(x) > 0.78539816339744828)
        n = rem_pio2(x, &y0, &y1);
    double s = k_sin(y0, y1), c = k_cos(y0, y1);
    return n & 1 ? -c / s : s / c;
}

double rt_atan(double x)
{
    static const double c[22] = {
        -1.0 / 3, 1.0 / 5, -1.0 / 7, 1.0 / 9, -1.0 / 11, 1.0 / 13, -1.0 / 15, 1.0 / 17,
        -1.0 / 19, 1.0 / 21, -1.0 / 23, 1.0 / 25, -1.0 / 27, 1.0 / 29, -1.0 / 31, 1.0 / 33,
        -1.0 / 35, 1.0 / 37, -1.0 / 39, 1.0 / 41, -1.0 / 43, 1.0 / 45,
    };
    if (is_nan(x))
        return x;
    if (!g_trig_ready)
        trig_init();
    double ax = rt_fabs(x), t;
    int id;
    if (ax >= 2.4375) { id = 3; t = -1.0 / ax; }
    else if (ax >= 1.1875) { id = 2; t = (ax - 1.5) / (1.0 + 1.5 * ax); }
    else if (ax >= 0.6875) { id = 1; t = (ax - 1.0) / (ax + 1.0); }
    else if (ax >= 0.4375) { id = 0; t = (2.0 * ax - 1.0) / (2.0 + ax); }
    else {
        if (ax < 7.450580596923828e-09) return x;   // 2^-27
        id = -1;
        t = ax;
    }
    // atan(t) = t - t^3/3 + t^5/5 - ...   (|t| < 0.44)
    double z = t * t, p = c[21];
    for (int i = 20; i >= 0; i--)
        p = p * z + c[i];
    double r = t + t * z * p;
    if (id >= 0)
        r = g_atan_hi[id] + (g_atan_lo[id] + r);
    return x < 0 ? -r : r;
}

double rt_atan2(double y, double x)
{
    if (is_nan(x) || is_nan(y))
        return x + y;
    if (!g_trig_ready)
        trig_init();
    double pio2 = g_atan_hi[3], pi_hi = 2 * g_atan_hi[3], pi_lo = 2 * g_atan_lo[3];
    int sy = sign_of(y), sx = sign_of(x);
    double r;
    if (y == 0) {
        if (!sx) return y;
        return sy ? -pi_hi : pi_hi;
    }
    if (x == 0)
        return sy ? -pio2 : pio2;
    if (is_inf(x)) {
        if (is_inf(y)) r = sx ? 3 * g_atan_hi[1] : g_atan_hi[1];
        else r = sx ? pi_hi : 0.0;
        return sy ? -r : r;
    }
    if (is_inf(y))
        return sy ? -pio2 : pio2;
    double a = rt_atan(rt_fabs(y / x));
    if (sx)
        a = pi_hi - (a - pi_lo);
    return sy ? -a : a;
}

double rt_asin(double x)
{
    if (is_nan(x)) return x;
    if (rt_fabs(x) > 1.0) { rt_errno = E_DOM; return nan_val(); }
    return rt_atan2(x, rt_sqrt((1.0 - x) * (1.0 + x)));
}

double rt_acos(double x)
{
    if (is_nan(x)) return x;
    if (rt_fabs(x) > 1.0) { rt_errno = E_DOM; return nan_val(); }
    return 2.0 * rt_atan2(rt_sqrt(1.0 - x), rt_sqrt(1.0 + x));
}

double rt_sinh(double x)
{
    if (!is_finite(x) || x == 0) return x;
    double a = rt_fabs(x), r;
    if (a < 3.725290298461914e-09) return x;
    if (a < 22) {
        double t = rt_expm1(a);
        r = 0.5 * (t + t / (t + 1.0));
    } else if (a < 709.0) {
        r = 0.5 * rt_exp(a);
    } else {
        double w = rt_exp(0.5 * a);
        r = (0.5 * w) * w;
    }
    return x < 0 ? -r : r;
}

double rt_cosh(double x)
{
    if (is_nan(x)) return x;
    double a = rt_fabs(x);
    if (is_inf(a)) return a;
    if (a < 0.34657359027997264) {      // ln(2)/2
        double t = rt_expm1(a);
        return 1.0 + t * t / (2.0 * (1.0 + t));
    }
    if (a < 22) {
        double t = rt_exp(a);
        return 0.5 * t + 0.5 / t;
    }
    if (a < 709.0)
        return 0.5 * rt_exp(a);
    double w = rt_exp(0.5 * a);
    return (0.5 * w) * w;
}

double rt_tanh(double x)
{
    if (is_nan(x) || x == 0) return x;
    double a = rt_fabs(x), r;
    if (a >= 22) r = 1.0;
    else if (a < 3.725290298461914e-09) return x;
    else if (a >= 1.0) {
        double t = rt_expm1(2.0 * a);
        r = 1.0 - 2.0 / (t + 2.0);
    } else {
        double t = rt_expm1(-2.0 * a);
        r = -t / (t + 2.0);
    }
    return x < 0 ? -r : r;
}

double rt_asinh(double x)
{
    if (!is_finite(x) || x == 0) return x;
    double a = rt_fabs(x), r;
    if (a > 1e10) r = rt_log(a) + LN2_HI + LN2_LO;
    else if (a > 2) r = rt_log(2.0 * a + 1.0 / (rt_sqrt(a * a + 1.0) + a));
    else {
        double t = a * a;
        r = rt_log1p(a + t / (1.0 + rt_sqrt(1.0 + t)));
    }
    return x < 0 ? -r : r;
}

double rt_acosh(double x)
{
    if (is_nan(x)) return x;
    if (x < 1.0) { rt_errno = E_DOM; return nan_val(); }
    if (is_inf(x)) return x;
    if (x > 1e10) return rt_log(x) + LN2_HI + LN2_LO;
    if (x > 2) return rt_log(2.0 * x - 1.0 / (x + rt_sqrt(x * x - 1.0)));
    double t = x - 1.0;
    return rt_log1p(t + rt_sqrt(2.0 * t + t * t));
}

double rt_atanh(double x)
{
    if (is_nan(x)) return x;
    double a = rt_fabs(x);
    if (a > 1.0) { rt_errno = E_DOM; return nan_val(); }
    if (a == 1.0) { rt_errno = E_RANGE; return x > 0 ? inf_val() : -inf_val(); }
    if (a < 3.725290298461914e-09) return x;
    double r = 0.5 * rt_log1p(2.0 * a / (1.0 - a));
    return x < 0 ? -r : r;
}

// sin(pi * x), with x reduced exactly first
static double sin_pi(double x)
{
    double r = x - 2.0 * rt_nearbyint(0.5 * x);    // in [-1, 1], exact
    if (!g_trig_ready) trig_init();
    double pi = 2 * g_atan_hi[3];
    if (r > 0.5) r = 1.0 - r;
    else if (r < -0.5) r = -1.0 - r;
    return rt_sin(pi * r);
}

double rt_tgamma(double x)
{
    static const double g[9] = {
        0.99999999999980993, 676.5203681218851, -1259.1392167224028, 771.32342877765313,
        -176.61502916214059, 12.507343278686905, -0.13857109526572012,
        9.9843695780195716e-6, 1.5056327351493116e-7,
    };
    if (is_nan(x)) return x;
    if (is_inf(x)) {
        if (x > 0) return x;
        rt_errno = E_DOM;
        return nan_val();
    }
    if (x == 0) { rt_errno = E_RANGE; return rt_copysign(inf_val(), x); }
    if (x < 0 && rt_trunc(x) == x) { rt_errno = E_DOM; return nan_val(); }
    if (x == rt_trunc(x) && x <= 23) {     // whole numbers: an exact factorial
        double r = 1;
        for (int i = 2; i < (int)x; i++) r *= i;
        return r;
    }
    if (x < 0.5) {
        if (!g_trig_ready) trig_init();
        return 2 * g_atan_hi[3] / (sin_pi(x) * rt_tgamma(1.0 - x));
    }
    if (x > 171.7) { rt_errno = E_RANGE; return inf_val(); }
    x -= 1.0;
    double a = g[0], t = x + 7.5;
    for (int i = 1; i < 9; i++)
        a += g[i] / (x + i);
    double p = rt_pow(t, (x + 0.5) * 0.5);
    return 2.5066282746310002 * a * (p * rt_exp(-t)) * p;   // sqrt(2 pi)
}

static double lgamma_core(double x);

// log|Gamma(x)|; signgam gets the sign of Gamma(x): -1 where it is negative
// (x < 0 with an odd floor, or -0), else 1
double rt_lgamma(double x)
{
    rt_signgam = 1;
    if (x == 0 ? sign_of(x) : x < 0 && !is_inf(x) && rt_trunc(x) != x && ((i64)rt_floor(x) & 1))
        rt_signgam = -1;
    return lgamma_core(x);
}

static double lgamma_core(double x)
{
    if (is_nan(x)) return x;
    if (is_inf(x)) return inf_val();
    if (x <= 0 && rt_trunc(x) == x) { rt_errno = E_RANGE; return inf_val(); }
    if (x < 0.5) {
        if (!g_trig_ready) trig_init();
        return rt_log(2 * g_atan_hi[3] / rt_fabs(sin_pi(x))) - lgamma_core(1.0 - x);
    }
    if (x < 15)
        return rt_log(rt_fabs(rt_tgamma(x)));
    double z = 1.0 / (x * x);
    double series = (1.0 / 12 + z * (-1.0 / 360 + z * (1.0 / 1260 + z * (-1.0 / 1680)))) / x;
    return (x - 0.5) * rt_log(x) - x + 0.91893853320467274178 + series;   // log(2 pi) / 2
}

// erfc for x >= 2: the continued fraction, by Lentz's method
static double erfc_cf(double x)
{
    double f = x, C = x, D = 0;
    for (int i = 1; i < 500; i++) {
        double a = i * 0.5;
        D = x + a * D;
        if (D == 0) D = 1e-300;
        D = 1.0 / D;
        C = x + a / C;
        if (C == 0) C = 1e-300;
        double delta = C * D;
        f *= delta;
        if (rt_fabs(delta - 1.0) < 1e-16) break;
    }
    double h, l;
    two_prod(x, x, &h, &l);
    return exp_core(-h, -l) / (f * SQRT_PI);
}

double rt_erf(double x)
{
    if (is_nan(x)) return x;
    double a = rt_fabs(x), r;
    if (a >= 6) r = 1.0;
    else if (a < 2.5) {
        // 2/sqrt(pi) e^(-x^2) * sum 2^n x^(2n+1) / (1*3*...*(2n+1)): all terms positive
        double z = a * a, term = a, sum = a;
        for (int n = 1; n < 300; n++) {
            term *= 2.0 * z / (2 * n + 1);
            sum += term;
            if (term < sum * 1e-17) break;
        }
        r = 2.0 / SQRT_PI * rt_exp(-z) * sum;
    } else {
        r = 1.0 - erfc_cf(a);
    }
    return x < 0 ? -r : r;
}

double rt_erfc(double x)
{
    if (is_nan(x)) return x;
    if (x < 2.0) return 1.0 - rt_erf(x);
    if (x > 27.3) { rt_errno = E_RANGE; return 0.0; }
    return erfc_cf(x);
}

// float versions
float rt_sqrtf(float x) { return (float)rt_sqrt(x); }
float rt_fabsf(float x) { return (float)rt_fabs(x); }
float rt_floorf(float x) { return (float)rt_floor(x); }
float rt_ceilf(float x) { return (float)rt_ceil(x); }
float rt_roundf(float x) { return (float)rt_round(x); }
float rt_truncf(float x) { return (float)rt_trunc(x); }
float rt_fmodf(float x, float y) { return (float)rt_fmod(x, y); }
float rt_sinf(float x) { return (float)rt_sin(x); }
float rt_cosf(float x) { return (float)rt_cos(x); }
float rt_tanf(float x) { return (float)rt_tan(x); }
float rt_asinf(float x) { return (float)rt_asin(x); }
float rt_acosf(float x) { return (float)rt_acos(x); }
float rt_atanf(float x) { return (float)rt_atan(x); }
float rt_atan2f(float y, float x) { return (float)rt_atan2(y, x); }
float rt_sinhf(float x) { return (float)rt_sinh(x); }
float rt_coshf(float x) { return (float)rt_cosh(x); }
float rt_tanhf(float x) { return (float)rt_tanh(x); }
float rt_expf(float x) { return (float)rt_exp(x); }
float rt_exp2f(float x) { return (float)rt_exp2(x); }
float rt_logf(float x) { return (float)rt_log(x); }
float rt_log2f(float x) { return (float)rt_log2(x); }
float rt_log10f(float x) { return (float)rt_log10(x); }
float rt_powf(float x, float y) { return (float)rt_pow(x, y); }
float rt_cbrtf(float x) { return (float)rt_cbrt(x); }
float rt_hypotf(float x, float y) { return (float)rt_hypot(x, y); }
float rt_fminf(float x, float y) { return (float)rt_fmin(x, y); }
float rt_fmaxf(float x, float y) { return (float)rt_fmax(x, y); }
float rt_copysignf(float x, float y) { return (float)rt_copysign(x, y); }
float rt_ldexpf(float x, int e) { return (float)rt_ldexp(x, e); }
float rt_frexpf(float x, int *e) { return (float)rt_frexp(x, e); }
float rt_nanf(const char *tag) { return nan_to_float(nan_with(tag, 0)); }

float rt_modff(float x, float *ip)
{
    double i;
    double f = rt_modf(x, &i);
    if (ip) *ip = (float)i;
    return (float)f;
}

// ---- time ----

typedef struct {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
} RtTm;

typedef struct { i64 tv_sec, tv_nsec; } RtTimespec;

static i64 floor_div(i64 a, i64 b) { i64 q = a / b; return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q; }

// days since 1970-01-01 of a date (proleptic Gregorian)
static i64 days_from_civil(i64 y, int m, int d)
{
    y -= m <= 2;
    i64 era = floor_div(y, 400);
    i64 yoe = y - era * 400;
    i64 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    i64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(i64 z, i64 *y, int *m, int *d)
{
    z += 719468;
    i64 era = floor_div(z, 146097);
    i64 doe = z - era * 146097;
    i64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    i64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    i64 mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

static void tm_from_time(i64 t, RtTm *tm)
{
    i64 days = floor_div(t, 86400), secs = t - days * 86400, y;
    int m, d;
    civil_from_days(days, &y, &m, &d);
    tm->tm_year = (int)(y - 1900);
    tm->tm_mon = m - 1;
    tm->tm_mday = d;
    tm->tm_hour = (int)(secs / 3600);
    tm->tm_min = (int)(secs / 60 % 60);
    tm->tm_sec = (int)(secs % 60);
    tm->tm_wday = (int)(((days + 4) % 7 + 7) % 7);     // 1970-01-01 was a Thursday
    tm->tm_yday = (int)(days - days_from_civil(y, 1, 1));
    tm->tm_isdst = 0;
}

// Windows (WinCC): time_t counts real seconds since 1970 UTC; localtime adds the zone of
// Windows (summer time included). DynastyOS: the clock is local time, its seconds counted
// as if they were UTC, and local time is the same.
#ifdef __WINCC__
#define LOCAL_OFFSET(utc) cc_utc_offset(utc)
#else
#define LOCAL_OFFSET(utc) 0
#endif

i64 rt_time(i64 *out)
{
#ifdef __WINCC__
    i64 v = cc_utc_seconds();
#else
    int t[6];
    cc_now(t);
    i64 v = days_from_civil(t[0], t[1], t[2]) * 86400 + (i64)t[3] * 3600 + (i64)t[4] * 60 + t[5];
#endif
    if (out) *out = v;
    return v;
}

// local seconds (as if UTC) -> the real moment: the zone's offset at that moment
static i64 utc_of_local(i64 local)
{
    return local - LOCAL_OFFSET(local - LOCAL_OFFSET(local));
}

i64 rt_clock(void) { return (i64)cc_millis() - g_clock_start; }

double rt_difftime(i64 a, i64 b) { return (double)(a - b); }

i64 rt_mktime(RtTm *tm)
{
    i64 y = (i64)tm->tm_year + 1900, mon = tm->tm_mon;
    y += floor_div(mon, 12);
    mon -= floor_div(mon, 12) * 12;
    i64 days = days_from_civil(y, (int)mon + 1, 1) + tm->tm_mday - 1;
    i64 t = days * 86400 + (i64)tm->tm_hour * 3600 + (i64)tm->tm_min * 60 + tm->tm_sec;
    tm_from_time(t, tm);                    // the fields, normalized, stay local time
    return utc_of_local(t);
}

static RtTm g_tm;
RtTm *rt_gmtime(const i64 *t) { tm_from_time(*t, &g_tm); return &g_tm; }
RtTm *rt_gmtime_r(const i64 *t, RtTm *out) { tm_from_time(*t, out); return out; }
RtTm *rt_localtime_r(const i64 *t, RtTm *out) { tm_from_time(*t + LOCAL_OFFSET(*t), out); return out; }
RtTm *rt_localtime(const i64 *t) { return rt_localtime_r(t, &g_tm); }

static const char *const g_wday[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const g_month[12] = { "January", "February", "March", "April", "May", "June", "July",
                                         "August", "September", "October", "November", "December" };

char *rt_asctime(const RtTm *tm)
{
    static char buf[64];
    const char *wd = tm->tm_wday >= 0 && tm->tm_wday < 7 ? g_wday[tm->tm_wday] : "???";
    const char *mo = tm->tm_mon >= 0 && tm->tm_mon < 12 ? g_month[tm->tm_mon] : "???";
    rt_snprintf(buf, sizeof(buf), "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
                wd, mo, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return buf;
}

char *rt_ctime(const i64 *t) { return rt_asctime(rt_localtime(t)); }

// TIME_UTC in milliseconds: the clock's seconds read once (the first call),
// then the milliseconds counted since -- seconds and their part agree and
// never step back. The kernel's C11 deadlines (smp.c) count by it too.
#pragma data_seg()
static volatile i64 g_utc_base = -1;          // the whole system's, not a program's (outside ".pst")
#pragma data_seg(".pst$d")

i64 rt_utc_ms(void)
{
    if (g_utc_base < 0) {
        i64 m = (i64)cc_millis();
        g_utc_base = rt_time(0) * 1000 - m;
    }
    return g_utc_base + (i64)cc_millis();
}

int rt_timespec_get(RtTimespec *ts, int base)
{
    if (base != 1) return 0;
    i64 ms = rt_utc_ms();
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
    return base;
}

static int is_leap(i64 y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// ISO 8601 week number; *iso_year gets the year the week belongs to
static int iso_week(const RtTm *tm, i64 *iso_year)
{
    i64 y = (i64)tm->tm_year + 1900;
    int wd = (tm->tm_wday + 6) % 7;                 // Monday = 0
    int week = (tm->tm_yday - wd + 10) / 7;
    if (week < 1) {
        y--;
        int yday = tm->tm_yday + (is_leap(y) ? 366 : 365);
        week = (yday - wd + 10) / 7;
    } else {
        int days = is_leap(y) ? 366 : 365;
        if (tm->tm_yday - wd + 3 >= days) {         // the Thursday of this week is in next year
            y++;
            week = 1;
        }
    }
    *iso_year = y;
    return week;
}

// a name of LC_TIME's locale: 0 a month, 1 its short name, 2 a day of the week, 3 a short day
static const char *time_name(int what, int i, char *buf)
{
#ifdef __WINCC__
    const char *loc = locale_of(5);
    if (loc && cc_locale_text(loc, what, i, buf, 64))
        return buf;
#endif
    if (what == 0 || what == 2)
        return what == 0 ? g_month[i] : g_wday[i];
    rt_snprintf(buf, 64, "%.3s", what == 1 ? g_month[i] : g_wday[i]);
    return buf;
}

size_t rt_strftime(char *s, size_t max, const char *fmt, const RtTm *tm)
{
    size_t n = 0;
    char tmp[64], nm[64], nm2[64];
    for (const char *f = fmt; *f; f++) {
        const char *add = tmp;
        int len;
        if (*f != '%') {
            tmp[0] = *f;
            len = 1;
        } else {
            f++;
            if (*f == 'E' || *f == 'O') f++;
            int wday = tm->tm_wday >= 0 && tm->tm_wday < 7 ? tm->tm_wday : 0;
            int mon = tm->tm_mon >= 0 && tm->tm_mon < 12 ? tm->tm_mon : 0;
            int hour12 = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
            i64 year = (i64)tm->tm_year + 1900, iy;
            switch (*f) {
            case 'a': add = time_name(3, wday, nm); len = (int)rt_strlen(add); break;
            case 'A': add = time_name(2, wday, nm); len = (int)rt_strlen(add); break;
            case 'b': case 'h': add = time_name(1, mon, nm); len = (int)rt_strlen(add); break;
            case 'B': add = time_name(0, mon, nm); len = (int)rt_strlen(add); break;
            case 'c':
                len = rt_snprintf(tmp, sizeof(tmp), "%s %s %2d %02d:%02d:%02d %lld", time_name(3, wday, nm),
                                  time_name(1, mon, nm2), tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, year);
                break;
            case 'C': len = rt_snprintf(tmp, sizeof(tmp), "%02lld", floor_div(year, 100)); break;
            case 'd': len = rt_snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mday); break;
            case 'D': len = rt_snprintf(tmp, sizeof(tmp), "%02d/%02d/%02lld", mon + 1, tm->tm_mday, (year % 100 + 100) % 100); break;
            case 'e': len = rt_snprintf(tmp, sizeof(tmp), "%2d", tm->tm_mday); break;
            case 'F': len = rt_snprintf(tmp, sizeof(tmp), "%lld-%02d-%02d", year, mon + 1, tm->tm_mday); break;
            case 'g': iso_week(tm, &iy); len = rt_snprintf(tmp, sizeof(tmp), "%02lld", (iy % 100 + 100) % 100); break;
            case 'G': iso_week(tm, &iy); len = rt_snprintf(tmp, sizeof(tmp), "%lld", iy); break;
            case 'H': len = rt_snprintf(tmp, sizeof(tmp), "%02d", tm->tm_hour); break;
            case 'I': len = rt_snprintf(tmp, sizeof(tmp), "%02d", hour12); break;
            case 'j': len = rt_snprintf(tmp, sizeof(tmp), "%03d", tm->tm_yday + 1); break;
            case 'm': len = rt_snprintf(tmp, sizeof(tmp), "%02d", mon + 1); break;
            case 'M': len = rt_snprintf(tmp, sizeof(tmp), "%02d", tm->tm_min); break;
            case 'n': tmp[0] = '\n'; len = 1; break;
            case 'p': add = tm->tm_hour < 12 ? "AM" : "PM"; len = 2; break;
            case 'r': len = rt_snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d %s", hour12, tm->tm_min, tm->tm_sec, tm->tm_hour < 12 ? "AM" : "PM"); break;
            case 'R': len = rt_snprintf(tmp, sizeof(tmp), "%02d:%02d", tm->tm_hour, tm->tm_min); break;
            case 'S': len = rt_snprintf(tmp, sizeof(tmp), "%02d", tm->tm_sec); break;
            case 't': tmp[0] = '\t'; len = 1; break;
            case 'T': case 'X': len = rt_snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec); break;
            case 'u': len = rt_snprintf(tmp, sizeof(tmp), "%d", wday ? wday : 7); break;
            case 'U': len = rt_snprintf(tmp, sizeof(tmp), "%02d", (tm->tm_yday + 7 - wday) / 7); break;
            case 'V': len = rt_snprintf(tmp, sizeof(tmp), "%02d", iso_week(tm, &iy)); break;
            case 'w': len = rt_snprintf(tmp, sizeof(tmp), "%d", wday); break;
            case 'W': len = rt_snprintf(tmp, sizeof(tmp), "%02d", (tm->tm_yday + 7 - (wday + 6) % 7) / 7); break;
            case 'x': len = rt_snprintf(tmp, sizeof(tmp), "%02d/%02d/%02lld", mon + 1, tm->tm_mday, (year % 100 + 100) % 100); break;
            case 'y': len = rt_snprintf(tmp, sizeof(tmp), "%02lld", (year % 100 + 100) % 100); break;
            case 'Y': len = rt_snprintf(tmp, sizeof(tmp), "%lld", year); break;
            case 'z': {             // the zone of the local time in tm: +0300
                i64 local = days_from_civil(year, mon + 1, tm->tm_mday) * 86400 + (i64)tm->tm_hour * 3600 +
                            (i64)tm->tm_min * 60 + tm->tm_sec;
                int off = (int)(local - utc_of_local(local)) / 60;
                len = rt_snprintf(tmp, sizeof(tmp), "%c%02d%02d", off < 0 ? '-' : '+', (off < 0 ? -off : off) / 60,
                                  (off < 0 ? -off : off) % 60);
                break;
            }
            case 'Z':               // the name of the zone, as Windows calls it
#ifdef __WINCC__
                len = cc_zone_name(tm->tm_isdst > 0, tmp, sizeof(tmp));
#else
                len = 0;
#endif
                break;
            case '%': tmp[0] = '%'; len = 1; break;
            case 0: f--; len = 0; break;
            default: tmp[0] = '%'; tmp[1] = *f; len = 2; break;
            }
        }
        if (n + (size_t)len >= max)
            return 0;
        rt_memcpy(s + n, add, (size_t)len);
        n += (size_t)len;
    }
    if (n >= max)
        return 0;
    s[n] = 0;
    return n;
}

// ---- wide characters (wchar.h, wctype.h, uchar.h) ----
// wchar_t is int: one Unicode code point (UTF-16 in Windows, RtWchar above).
// Multibyte text is always UTF-8.

typedef struct { u32 need, value, least, pend; } RtMbstate;   // mbstate_t
#define WEOF_V 0xFFFFFFFFu

// case mapping: Latin-1, Latin Extended-A and part of B, Greek, Cyrillic,
// Armenian, Latin Extended Additional, Roman numerals, circled and fullwidth letters
u32 rt_towlower(u32 c)
{
    if (c < 0x80) return c >= 'A' && c <= 'Z' ? c + 32 : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x130) return 'i';
        if (c == 0x178) return 0xFF;
        if (c == 0x131 || c == 0x138 || c == 0x149 || c == 0x17F) return c;
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return c & 1 ? c + 1 : c;
        return c & 1 ? c : c + 1;
    }
    if (c >= 0x1CD && c <= 0x1DC) return c & 1 ? c + 1 : c;
    if ((c >= 0x1DE && c <= 0x1EF) || (c >= 0x1F8 && c <= 0x21F) || (c >= 0x222 && c <= 0x233) ||
        (c >= 0x246 && c <= 0x24F))
        return c & 1 ? c : c + 1;
    if (c >= 0x370 && c <= 0x3FF) {
        if (c == 0x370 || c == 0x372 || c == 0x376) return c + 1;
        if (c == 0x37F) return 0x3F3;
        if (c == 0x386) return 0x3AC;
        if (c >= 0x388 && c <= 0x38A) return c + 37;
        if (c == 0x38C) return 0x3CC;
        if (c == 0x38E || c == 0x38F) return c + 63;
        if (c >= 0x391 && c <= 0x3AB && c != 0x3A2) return c + 32;
        if (c == 0x3CF) return 0x3D7;
        if (c >= 0x3D8 && c <= 0x3EF) return c & 1 ? c : c + 1;
        if (c == 0x3F4) return 0x3B8;
        if (c == 0x3F7 || c == 0x3FA) return c + 1;
        if (c == 0x3F9) return 0x3F2;
        if (c >= 0x3FD) return c - 130;
        return c;
    }
    if (c >= 0x400 && c <= 0x40F) return c + 80;           // Ё -> ё, Ђ ... Џ
    if (c >= 0x410 && c <= 0x42F) return c + 32;           // А ... Я
    if ((c >= 0x460 && c <= 0x481) || (c >= 0x48A && c <= 0x4BF) || (c >= 0x4D0 && c <= 0x52F))
        return c & 1 ? c : c + 1;
    if (c == 0x4C0) return 0x4CF;
    if (c >= 0x4C1 && c <= 0x4CE) return c & 1 ? c + 1 : c;
    if (c >= 0x531 && c <= 0x556) return c + 48;
    if ((c >= 0x1E00 && c <= 0x1E95) || (c >= 0x1EA0 && c <= 0x1EFF)) return c & 1 ? c : c + 1;
    if (c == 0x1E9E) return 0xDF;
    if (c >= 0x2160 && c <= 0x216F) return c + 16;
    if (c >= 0x24B6 && c <= 0x24CF) return c + 26;
    if (c >= 0xFF21 && c <= 0xFF3A) return c + 32;
    return c;
}

u32 rt_towupper(u32 c)
{
    if (c < 0x80) return c >= 'a' && c <= 'z' ? c - 32 : c;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c == 0xFF) return 0x178;
    if (c == 0xB5) return 0x39C;
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x131) return 'I';
        if (c == 0x17F) return 'S';
        if (c == 0x130 || c == 0x138 || c == 0x149 || c == 0x178) return c;
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return c & 1 ? c : c - 1;
        return c & 1 ? c - 1 : c;
    }
    if (c >= 0x1CD && c <= 0x1DC) return c & 1 ? c : c - 1;
    if ((c >= 0x1DE && c <= 0x1EF) || (c >= 0x1F8 && c <= 0x21F) || (c >= 0x222 && c <= 0x233) ||
        (c >= 0x246 && c <= 0x24F))
        return c & 1 ? c - 1 : c;
    if (c >= 0x370 && c <= 0x3FF) {
        if (c == 0x371 || c == 0x373 || c == 0x377) return c - 1;
        if (c >= 0x37B && c <= 0x37D) return c + 130;
        if (c == 0x3AC) return 0x386;
        if (c >= 0x3AD && c <= 0x3AF) return c - 37;
        if (c == 0x3C2) return 0x3A3;                       // final sigma
        if (c >= 0x3B1 && c <= 0x3CB) return c - 32;
        if (c == 0x3CC) return 0x38C;
        if (c == 0x3CD || c == 0x3CE) return c - 63;
        if (c == 0x3D0) return 0x392;
        if (c == 0x3D1) return 0x398;
        if (c == 0x3D5) return 0x3A6;
        if (c == 0x3D6) return 0x3A0;
        if (c == 0x3D7) return 0x3CF;
        if (c >= 0x3D8 && c <= 0x3EF) return c & 1 ? c - 1 : c;
        if (c == 0x3F0) return 0x39A;
        if (c == 0x3F1) return 0x3A1;
        if (c == 0x3F2) return 0x3F9;
        if (c == 0x3F3) return 0x37F;
        if (c == 0x3F5) return 0x395;
        if (c == 0x3F8 || c == 0x3FB) return c - 1;
        return c;
    }
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c >= 0x450 && c <= 0x45F) return c - 80;
    if ((c >= 0x460 && c <= 0x481) || (c >= 0x48A && c <= 0x4BF) || (c >= 0x4D0 && c <= 0x52F))
        return c & 1 ? c - 1 : c;
    if (c == 0x4CF) return 0x4C0;
    if (c >= 0x4C1 && c <= 0x4CE) return c & 1 ? c : c - 1;
    if (c >= 0x561 && c <= 0x586) return c - 48;
    if ((c >= 0x1E00 && c <= 0x1E95) || (c >= 0x1EA0 && c <= 0x1EFF)) return c & 1 ? c - 1 : c;
    if (c >= 0x2170 && c <= 0x217F) return c - 16;
    if (c >= 0x24D0 && c <= 0x24E9) return c - 26;
    if (c >= 0xFF41 && c <= 0xFF5A) return c - 32;
    return c;
}

int rt_iswupper(u32 c) { return rt_towlower(c) != c; }

int rt_iswlower(u32 c)
{
    return rt_towupper(c) != c || c == 0xDF || c == 0x138 || c == 0x149 || c == 0x390 || c == 0x3B0 ||
           c == 0x587 || (c >= 0x250 && c <= 0x2AF);
}

int rt_iswalpha(u32 c)
{
    if (c < 0x80) return (c | 32) >= 'a' && (c | 32) <= 'z';
    if (c < 0x100) return c == 0xAA || c == 0xB5 || c == 0xBA || (c >= 0xC0 && c != 0xD7 && c != 0xF7);
    if (c <= 0x2C1) return 1;                               // Latin Extended, IPA, modifier letters
    if (c >= 0x370 && c <= 0x3FF)                           // Greek, without its signs and gaps
        return !(c == 0x374 || c == 0x375 || c == 0x378 || c == 0x379 || c == 0x37E || (c >= 0x380 && c <= 0x385) ||
                 c == 0x387 || c == 0x38B || c == 0x38D || c == 0x3A2 || c == 0x3F6);
    if ((c >= 0x400 && c <= 0x481) || (c >= 0x48A && c <= 0x52F)) return 1;   // Cyrillic
    if ((c >= 0x531 && c <= 0x556) || (c >= 0x561 && c <= 0x587)) return 1;   // Armenian
    if ((c >= 0x5D0 && c <= 0x5EA) || (c >= 0x620 && c <= 0x64A)) return 1;   // Hebrew, Arabic
    if (c >= 0x1E00 && c <= 0x1FFF) return 1;
    if ((c >= 0x3041 && c <= 0x3096) || (c >= 0x30A1 && c <= 0x30FA)) return 1;  // kana
    if ((c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7A3)) return 1;
    if ((c >= 0xFF21 && c <= 0xFF3A) || (c >= 0xFF41 && c <= 0xFF5A)) return 1;
    return rt_iswupper(c) || rt_iswlower(c);
}

int rt_iswdigit(u32 c) { return c >= '0' && c <= '9'; }
int rt_iswxdigit(u32 c) { return c < 0x80 && rt_isxdigit((int)c); }
int rt_iswalnum(u32 c) { return rt_iswalpha(c) || rt_iswdigit(c); }

int rt_iswblank(u32 c)
{
    return c == ' ' || c == '\t' || c == 0x1680 || (c >= 0x2000 && c <= 0x2006) || (c >= 0x2008 && c <= 0x200A) ||
           c == 0x205F || c == 0x3000;
}

int rt_iswspace(u32 c)
{
    return (c >= '\t' && c <= '\r') || c == 0x2028 || c == 0x2029 || (rt_iswblank(c) && c != 0xA0);
}

int rt_iswcntrl(u32 c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == 0x2028 || c == 0x2029; }
int rt_iswprint(u32 c) { return cp_valid(c) && !rt_iswcntrl(c) && c != 0xFFFE && c != 0xFFFF; }
int rt_iswgraph(u32 c) { return rt_iswprint(c) && !rt_iswspace(c); }
int rt_iswpunct(u32 c) { return rt_iswgraph(c) && !rt_iswalnum(c); }

static const char *const g_wctype_names[] = {
    "", "alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit", 0,
};

int rt_wctype(const char *name)
{
    for (int i = 1; g_wctype_names[i]; i++)
        if (!rt_strcmp(name, g_wctype_names[i])) return i;
    return 0;
}

int rt_iswctype(u32 c, int t)
{
    switch (t) {
    case 1: return rt_iswalnum(c);
    case 2: return rt_iswalpha(c);
    case 3: return rt_iswblank(c);
    case 4: return rt_iswcntrl(c);
    case 5: return rt_iswdigit(c);
    case 6: return rt_iswgraph(c);
    case 7: return rt_iswlower(c);
    case 8: return rt_iswprint(c);
    case 9: return rt_iswpunct(c);
    case 10: return rt_iswspace(c);
    case 11: return rt_iswupper(c);
    case 12: return rt_iswxdigit(c);
    default: return 0;
    }
}

int rt_wctrans(const char *name) { return !rt_strcmp(name, "tolower") ? 1 : !rt_strcmp(name, "toupper") ? 2 : 0; }
u32 rt_towctrans(u32 c, int t) { return t == 1 ? rt_towlower(c) : t == 2 ? rt_towupper(c) : c; }

// ---- multibyte <-> wide ----

static RtMbstate g_mbs_rtowc, g_mbs_rlen, g_mbs_rtomb, g_mbs_srtowcs, g_mbs_srtombs, g_mbs_c16, g_mbs_c16o, g_mbs_c32;

int rt_mbtowc(RtWchar *pwc, const char *s, size_t n)
{
    if (!s) return 0;                   // UTF-8 keeps no shift state
    u32 v;
    int r = utf8_dec(s, n, &v);
    if (r < 0 || wide_units(v) > 1) { rt_errno = E_ILSEQ; return -1; }   // beyond one wchar_t (Windows)
    if (pwc) *pwc = (RtWchar)v;
    return r;
}

int rt_mblen(const char *s, size_t n)
{
    if (!s) return 0;
    u32 v;
    int r = utf8_dec(s, n, &v);
    if (r < 0) { rt_errno = E_ILSEQ; return -1; }
    return r;
}

int rt_wctomb(char *s, RtWchar wc)
{
    if (!s) return 0;
    int r = utf8_enc((u32)wc, s);
    if (r < 0) rt_errno = E_ILSEQ;
    return r;
}

size_t rt_mbstowcs(RtWchar *dst, const char *src, size_t n)
{
    size_t k = 0;
    for (;;) {
        if (dst && k >= n) return k;
        u32 v;
        int r = utf8_dec(src, 4, &v);   // stops at the first byte that does not belong
        if (r < 0) { rt_errno = E_ILSEQ; return (size_t)-1; }
        if (!r) {
            if (dst) dst[k] = 0;
            return k;
        }
        size_t u = (size_t)wide_units(v);
        if (dst && k + u > n) return k;  // a pair is not cut in two
        if (dst) wide_store(dst + k, v);
        k += u;
        src += r;
    }
}

size_t rt_wcstombs(char *dst, const RtWchar *src, size_t n)
{
    size_t k = 0;
    for (size_t i = 0;;) {
        if (!src[i]) {
            if (dst && k < n) dst[k] = 0;
            return k;
        }
        char t[4];
        int r = utf8_enc(wide_cp(src, &i), t);
        if (r < 0) { rt_errno = E_ILSEQ; return (size_t)-1; }
        if (dst) {
            if (k + (size_t)r > n) return k;
            rt_memcpy(dst + k, t, (size_t)r);
        }
        k += (size_t)r;
    }
}

// mbrtowc: may be fed a character a few bytes at a time; ps keeps the part.
// mbr_decode gives the whole code point (char32_t, char16_t pairs take it so).
static size_t mbr_decode(u32 *pwc, const char *s, size_t n, RtMbstate *ps)
{
    if (!s) {
        int partial = ps->need != 0;
        ps->need = 0;
        if (partial) { rt_errno = E_ILSEQ; return (size_t)-1; }
        return 0;
    }
    if (!n) return (size_t)-2;
    size_t i = 0;
    u32 v = ps->value, need = ps->need, least = ps->least;
    if (!need) {
        u8 c = (u8)s[0];
        if (c < 0x80) {
            if (pwc) *pwc = c;
            return c ? 1 : 0;
        }
        int len = utf8_len_of(c);
        if (!len) goto bad;
        need = (u32)len - 1;
        v = c & (0x7Fu >> len);
        least = len == 2 ? 0x80 : len == 3 ? 0x800 : 0x10000;
        i = 1;
    }
    for (; need && i < n; i++, need--) {
        u8 b = (u8)s[i];
        if ((b & 0xC0) != 0x80) goto bad;
        v = v << 6 | (b & 0x3F);
    }
    if (need) {                         // still incomplete: keep it for the next call
        ps->need = need;
        ps->value = v;
        ps->least = least;
        return (size_t)-2;
    }
    ps->need = 0;
    if (v < least || !cp_valid(v)) goto bad;
    if (pwc) *pwc = v;
    return i;
bad:
    ps->need = 0;
    rt_errno = E_ILSEQ;
    return (size_t)-1;
}

size_t rt_mbrtowc(RtWchar *pwc, const char *s, size_t n, RtMbstate *ps)
{
    u32 v = 0;
    size_t r = mbr_decode(&v, s, n, ps ? ps : &g_mbs_rtowc);
    if (r <= 4 && s && wide_units(v) > 1) { rt_errno = E_ILSEQ; return (size_t)-1; }  // mbrtoc16 takes it in two
    if (r <= 4 && pwc && s) *pwc = (RtWchar)v;
    return r;
}

size_t rt_mbrlen(const char *s, size_t n, RtMbstate *ps)
{
    u32 v;
    return mbr_decode(&v, s, n, ps ? ps : &g_mbs_rlen);    // the bytes of any character, one wchar_t or not
}

size_t rt_wcrtomb(char *s, RtWchar wc, RtMbstate *ps)
{
    if (!ps) ps = &g_mbs_rtomb;
    char t[4];
    if (!s) s = t, wc = 0;
    if (!wc) ps->need = ps->pend = 0;   // the NUL (or s NULL) leaves the initial state
    u32 c = (u32)wc;
    if (RT_WIDE16 && is_hi_sur(c) && !ps->pend) {   // the first half of a pair: nothing yet
        ps->pend = c;
        return 0;
    }
    if (RT_WIDE16 && ps->pend) {
        if (!is_lo_sur(c)) { ps->pend = 0; rt_errno = E_ILSEQ; return (size_t)-1; }
        c = 0x10000 + ((ps->pend - 0xD800) << 10) + (c - 0xDC00);
        ps->pend = 0;
    }
    int r = utf8_enc(c, s);
    if (r < 0) { rt_errno = E_ILSEQ; return (size_t)-1; }
    return (size_t)r;
}

int rt_mbsinit(const RtMbstate *ps) { return !ps || (!ps->need && !ps->pend); }

// Character by character as mbrtowc with ps: a character begun in *ps (by
// mbrtowc fed part of it) is finished with the first bytes of *src. Only
// counting (dst NULL) leaves *ps as it was; at the end *ps is initial.
size_t rt_mbsrtowcs(RtWchar *dst, const char **src, size_t len, RtMbstate *ps)
{
    if (!ps) ps = &g_mbs_srtowcs;
    RtMbstate copy = *ps;
    RtMbstate *st = dst ? ps : &copy;
    const char *s = *src;
    size_t k = 0;
    for (;;) {
        if (dst && k >= len) break;
        u32 w = 0;
        size_t r = mbr_decode(&w, s, 4, st);   // stops at a byte that does not belong: never past the NUL
        if (r == (size_t)-1 || r == (size_t)-2) {
            if (dst) *src = s;
            st->need = 0;
            rt_errno = E_ILSEQ;
            return (size_t)-1;
        }
        if (!r) {
            if (dst) { dst[k] = 0; *src = 0; }
            return k;
        }
        size_t u = (size_t)wide_units(w);
        if (dst && k + u > len) break;      // a pair is not cut in two
        if (dst) wide_store(dst + k, w);
        k += u;
        s += r;
    }
    *src = s;
    return k;
}

// UTF-8 needs no state to write: *ps is only made initial when the
// terminating NUL is stored.
size_t rt_wcsrtombs(char *dst, const RtWchar **src, size_t len, RtMbstate *ps)
{
    if (!ps) ps = &g_mbs_srtombs;
    const RtWchar *s = *src;
    size_t k = 0;
    for (;;) {
        if (!*s) {
            if (dst) {
                if (k < len) { dst[k] = 0; *src = 0; ps->need = ps->pend = 0; }
                else *src = s;
            }
            return k;
        }
        char t[4];
        size_t at = 0;
        u32 c = wide_cp(s, &at);
        int r = utf8_enc(c, t);
        if (r < 0) {
            if (dst) *src = s;
            rt_errno = E_ILSEQ;
            return (size_t)-1;
        }
        if (dst) {
            if (k + (size_t)r > len) { *src = s; return k; }
            rt_memcpy(dst + k, t, (size_t)r);
        }
        k += (size_t)r;
        s += at;                        // a pair: both halves
    }
}

u32 rt_btowc(int c) { return c == EOF || (u32)c >= 0x80 ? WEOF_V : (u32)c; }
int rt_wctob(u32 wc) { return wc < 0x80 ? (int)wc : EOF; }

// uchar.h: char32_t is a code point; char16_t needs surrogate pairs
size_t rt_mbrtoc32(u32 *pc, const char *s, size_t n, RtMbstate *ps)
{
    u32 w = 0;
    size_t r = mbr_decode(&w, s, n, ps ? ps : &g_mbs_c32);
    if (r <= 4 && pc && s) *pc = w;
    return r;
}

size_t rt_c32rtomb(char *s, u32 c, RtMbstate *ps)
{
    if (!ps) ps = &g_mbs_c32;
    char t[4];
    if (!s) s = t, c = 0;
    if (!c) ps->need = ps->pend = 0;
    int r = utf8_enc(c, s);
    if (r < 0) { rt_errno = E_ILSEQ; return (size_t)-1; }
    return (size_t)r;
}

size_t rt_mbrtoc16(unsigned short *pc, const char *s, size_t n, RtMbstate *ps)
{
    if (!ps) ps = &g_mbs_c16;
    if (ps->pend) {                     // the second half of a pair, no input taken
        if (pc) *pc = (unsigned short)ps->pend;
        ps->pend = 0;
        return (size_t)-3;
    }
    u32 w = 0;
    size_t r = mbr_decode(&w, s, n, ps);
    if (r <= 4 && s) {
        u32 v = (u32)w;
        if (v >= 0x10000) {
            v -= 0x10000;
            if (pc) *pc = (unsigned short)(0xD800 + (v >> 10));
            ps->pend = 0xDC00 + (v & 0x3FF);
        } else if (pc) {
            *pc = (unsigned short)v;
        }
    }
    return r;
}

size_t rt_c16rtomb(char *s, unsigned short c, RtMbstate *ps)
{
    if (!ps) ps = &g_mbs_c16o;
    if (!s) { ps->pend = 0; return 1; }
    u32 v = c;
    if (c >= 0xD800 && c <= 0xDBFF) {   // first half: wait for the second
        if (ps->pend) goto bad;
        ps->pend = c;
        return 0;
    }
    if (c >= 0xDC00 && c <= 0xDFFF) {
        if (!ps->pend) goto bad;
        v = 0x10000 + ((ps->pend - 0xD800) << 10) + (c - 0xDC00u);
        ps->pend = 0;
    } else if (ps->pend) {
        goto bad;
    }
    return (size_t)utf8_enc(v, s);
bad:
    ps->pend = 0;
    rt_errno = E_ILSEQ;
    return (size_t)-1;
}

// ---- wide strings ----

size_t rt_wcslen(const RtWchar *s) { size_t n = 0; while (s[n]) n++; return n; }
size_t rt_wcsnlen(const RtWchar *s, size_t max) { size_t n = 0; while (n < max && s[n]) n++; return n; }

int rt_wcscmp(const RtWchar *a, const RtWchar *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a < *b ? -1 : *a > *b;
}

int rt_wcsncmp(const RtWchar *a, const RtWchar *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i] || !a[i]) return a[i] < b[i] ? -1 : a[i] > b[i];
    return 0;
}

RtWchar *rt_wcscpy(RtWchar *d, const RtWchar *s) { RtWchar *r = d; while ((*d++ = *s++)) {} return r; }

RtWchar *rt_wcsncpy(RtWchar *d, const RtWchar *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

RtWchar *rt_wcscat(RtWchar *d, const RtWchar *s) { rt_wcscpy(d + rt_wcslen(d), s); return d; }

RtWchar *rt_wcsncat(RtWchar *d, const RtWchar *s, size_t n)
{
    RtWchar *e = d + rt_wcslen(d);
    size_t i = 0;
    for (; i < n && s[i]; i++) e[i] = s[i];
    e[i] = 0;
    return d;
}

int rt_wcscoll(const RtWchar *a, const RtWchar *b)
{
#ifdef __WINCC__
    const char *loc = locale_of(1);
    if (loc)
        return cc_collate_w(loc, a, b);
#endif
    return rt_wcscmp(a, b);
}

size_t rt_wcsxfrm(RtWchar *d, const RtWchar *s, size_t n)
{
    size_t len = rt_wcslen(s);
    if (n) rt_wcsncpy(d, s, n);
    return len;
}

RtWchar *rt_wcschr(const RtWchar *s, RtWchar c)
{
    for (;; s++) {
        if (*s == c) return (RtWchar *)s;
        if (!*s) return 0;
    }
}

RtWchar *rt_wcsrchr(const RtWchar *s, RtWchar c)
{
    const RtWchar *last = 0;
    for (;; s++) {
        if (*s == c) last = s;
        if (!*s) return (RtWchar *)last;
    }
}

RtWchar *rt_wcsstr(const RtWchar *h, const RtWchar *n)
{
    size_t m = rt_wcslen(n);
    for (; *h; h++)
        if (!rt_wcsncmp(h, n, m)) return (RtWchar *)h;
    return m ? 0 : (RtWchar *)h;
}

size_t rt_wcsspn(const RtWchar *s, const RtWchar *acc)
{
    size_t n = 0;
    while (s[n] && rt_wcschr(acc, s[n])) n++;
    return n;
}

size_t rt_wcscspn(const RtWchar *s, const RtWchar *rej)
{
    size_t n = 0;
    while (s[n] && !rt_wcschr(rej, s[n])) n++;
    return n;
}

RtWchar *rt_wcspbrk(const RtWchar *s, const RtWchar *acc)
{
    s += rt_wcscspn(s, acc);
    return *s ? (RtWchar *)s : 0;
}

RtWchar *rt_wcstok(RtWchar *s, const RtWchar *delim, RtWchar **save)
{
    if (!s) s = *save;
    if (!s) return 0;
    s += rt_wcsspn(s, delim);
    if (!*s) { *save = 0; return 0; }
    RtWchar *end = s + rt_wcscspn(s, delim);
    if (*end) { *end = 0; *save = end + 1; }
    else *save = 0;
    return s;
}

RtWchar *rt_wcsdup(const RtWchar *s)
{
    size_t n = (rt_wcslen(s) + 1) * sizeof(RtWchar);
    RtWchar *d = rt_malloc(n);
    if (d) rt_memcpy(d, s, n);
    return d;
}

RtWchar *rt_wmemcpy(RtWchar *d, const RtWchar *s, size_t n) { return rt_memcpy(d, s, n * sizeof(RtWchar)); }
RtWchar *rt_wmemmove(RtWchar *d, const RtWchar *s, size_t n) { return rt_memmove(d, s, n * sizeof(RtWchar)); }

RtWchar *rt_wmemset(RtWchar *d, RtWchar c, size_t n)
{
    for (size_t i = 0; i < n; i++) d[i] = c;
    return d;
}

int rt_wmemcmp(const RtWchar *a, const RtWchar *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

RtWchar *rt_wmemchr(const RtWchar *s, RtWchar c, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (s[i] == c) return (RtWchar *)(s + i);
    return 0;
}

// numbers: the ASCII start of the wide string goes to the narrow parsers
static const RtWchar *wide_number(const RtWchar *s, char *buf, int cap)
{
    while (*s && rt_iswspace((u32)*s)) s++;
    int i = 0;
    for (; i < cap - 1 && s[i] > 0 && s[i] < 0x80; i++)
        buf[i] = (char)s[i];
    buf[i] = 0;
    return s;
}

#define WIDE_NUM(type, call)                                            \
    char buf[1100], *e;                                                 \
    const RtWchar *start = wide_number(s, buf, sizeof(buf));            \
    type v = call;                                                      \
    if (end) *end = (RtWchar *)(e == buf ? s : start + (e - buf));      \
    return v;

i64 rt_wcstoll(const RtWchar *s, RtWchar **end, int base) { WIDE_NUM(i64, rt_strtoll(buf, &e, base)) }
u64 rt_wcstoull(const RtWchar *s, RtWchar **end, int base) { WIDE_NUM(u64, rt_strtoull(buf, &e, base)) }
double rt_wcstod(const RtWchar *s, RtWchar **end) { WIDE_NUM(double, rt_strtod(buf, &e)) }
float rt_wcstof(const RtWchar *s, RtWchar **end) { WIDE_NUM(float, rt_strtof(buf, &e)) }

#undef WIDE_NUM

// ---- wide streams: wide characters go in and out as UTF-8 ----

int rt_fwide(RtFile *f, int mode)
{
    if (!f) return 0;
    if (mode && !f->orient) f->orient = mode > 0 ? 1 : -1;
    return f->orient;
}

u32 LOCKED(rt_fputwc)(RtWchar wc, RtFile *f)
{
    char t[4];
    u32 c = (u32)wc;
    if (RT_WIDE16 && f && is_hi_sur(c)) {
        f->whi = (int)c;
        if (!f->orient) f->orient = 1;
        return (u32)wc;
    }
    if (RT_WIDE16 && f && f->whi && is_lo_sur(c))
        c = 0x10000 + (((u32)f->whi - 0xD800) << 10) + (c - 0xDC00);
    if (f) f->whi = 0;
    int n = utf8_enc(c, t);
    if (n < 0) {
        rt_errno = E_ILSEQ;
        if (f) f->err = 1;
        return WEOF_V;
    }
    if (file_write(f, t, (size_t)n) != (size_t)n)
        return WEOF_V;
    if (f->kind == FK_CON_OUT) con_flush();
    if (!f->orient) f->orient = 1;
    return (u32)wc;
}

u32 rt_putwchar(RtWchar wc) { return rt_fputwc(wc, rt_stdout); }

u32 LOCKED(rt_fgetwc)(RtFile *f)
{
    if (!f) return WEOF_V;
    if (!f->orient) f->orient = 1;
    if (f->wungot >= 0) {
        u32 c = (u32)f->wungot;
        f->wungot = -1;
        return c;
    }
    int c = file_getc(f);
    if (c == EOF) return WEOF_V;
    if (c < 0x80) return (u32)c;
    char t[4];
    t[0] = (char)c;
    int len = utf8_len_of((u8)c);
    for (int i = 1; i < len; i++) {
        int d = file_getc(f);
        if (d == EOF || (d & 0xC0) != 0x80) {
            if (d != EOF) rt_ungetc(d, f);
            goto bad;
        }
        t[i] = (char)d;
    }
    u32 v;
    if (len && utf8_dec(t, (size_t)len, &v) > 0) {
        if (wide_units(v) > 1) {        // Windows: the first half now, the second on the next call
            v -= 0x10000;
            f->wungot = (int)(0xDC00 + (v & 0x3FF));
            return 0xD800 + (v >> 10);
        }
        return v;
    }
bad:
    rt_errno = E_ILSEQ;
    f->err = 1;
    return WEOF_V;
}

u32 rt_getwchar(void) { return rt_fgetwc(rt_stdin); }

u32 LOCKED(rt_ungetwc)(u32 wc, RtFile *f)
{
    if (wc == WEOF_V || !f) return WEOF_V;
    char t[4];
    int k = utf8_enc(wc, t);
    if (k < 0) return WEOF_V;
    int same = f->ungot < 0 && f->wungot < 0 && f->pos >= (size_t)k && f->pos <= f->len;
    for (int i = 0; same && i < k; i++)
        same = f->data[f->pos - (size_t)k + (size_t)i] == t[i];
    if (same) f->pos -= (size_t)k;      // just read: step back over its bytes
    else if (f->wungot < 0) f->wungot = (int)wc;
    else return WEOF_V;
    f->eof = 0;
    return wc;
}

RtWchar *LOCKED(rt_fgetws)(RtWchar *ws, int n, RtFile *f)
{
    if (n <= 0) return 0;
    int i = 0;
    while (i < n - 1) {
        u32 c = rt_fgetwc(f);
        if (c == WEOF_V) {
            if (f && f->err) return 0;
            break;
        }
        ws[i++] = (RtWchar)c;
        if (c == '\n') break;
    }
    if (!i) return 0;
    ws[i] = 0;
    return ws;
}

int LOCKED(rt_fputws)(const RtWchar *ws, RtFile *f)
{
    for (; *ws; ws++)
        if (rt_fputwc(*ws, f) == WEOF_V) return -1;
    return 0;
}

// a wide string as UTF-8 in the program heap, or NULL (bad characters, memory)
static char *wide_to_utf8(const RtWchar *ws)
{
    size_t n = rt_wcstombs(0, ws, 0);
    if (n == (size_t)-1) return 0;
    char *s = rt_malloc(n + 1);
    if (s) rt_wcstombs(s, ws, n + 1);
    return s;
}

static int count_chars(const char *s, size_t n)
{
    int k = 0;
    for (size_t i = 0; i < n; i++)
        if (((u8)s[i] & 0xC0) != 0x80) k++;
    return k;
}

// printf into the program heap: the text and its length in bytes
static char *format_alloc(const char *fmt, va_list ap, int *len)
{
    va_list ap2 = ap;
    char one[1];
    int n = rt_vsnprintf(one, 1, fmt, ap2);
    char *buf = rt_malloc((size_t)n + 1);
    if (buf) rt_vsnprintf(buf, (size_t)n + 1, fmt, ap);
    *len = n;
    return buf;
}

int LOCKED(rt_vfwprintf)(RtFile *f, const RtWchar *wfmt, va_list ap)
{
    char *fmt = wide_to_utf8(wfmt);
    if (!fmt) return -1;
    int len;
    char *out = format_alloc(fmt, ap, &len);
    rt_free(fmt);
    if (!out) return -1;
    size_t w = file_write(f, out, (size_t)len);
    if (f && f->kind == FK_CON_OUT) con_flush();
    if (f && !f->orient) f->orient = 1;
    int r = w == (size_t)len ? count_chars(out, (size_t)len) : -1;
    rt_free(out);
    return r;
}

int rt_fwprintf(RtFile *f, const RtWchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfwprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int rt_vwprintf(const RtWchar *fmt, va_list ap) { return rt_vfwprintf(rt_stdout, fmt, ap); }

int rt_wprintf(const RtWchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfwprintf(rt_stdout, fmt, ap);
    va_end(ap);
    return n;
}

// swprintf: -1 when n wide characters (with the NUL) are not enough
int rt_vswprintf(RtWchar *ws, size_t n, const RtWchar *wfmt, va_list ap)
{
    char *fmt = wide_to_utf8(wfmt);
    if (!fmt) return -1;
    int len;
    char *out = format_alloc(fmt, ap, &len);
    rt_free(fmt);
    if (!out) return -1;
    size_t k = 0;
    const char *p = out;
    int fits = 1;
    while (*p) {
        u32 v;
        int r = utf8_dec(p, 4, &v);
        if (r <= 0) { v = (u8)*p; r = 1; }
        if (k + (size_t)wide_units(v) >= n) { fits = 0; break; }
        wide_store(ws + k, v);
        k += (size_t)wide_units(v);
        p += r;
    }
    if (n) ws[k] = 0;
    rt_free(out);
    return fits && n ? (int)k : -1;
}

int rt_swprintf(RtWchar *ws, size_t n, const RtWchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = rt_vswprintf(ws, n, fmt, ap);
    va_end(ap);
    return r;
}

int LOCKED(rt_vfwscanf)(RtFile *f, const RtWchar *wfmt, va_list ap)
{
    char *fmt = wide_to_utf8(wfmt);
    if (!fmt) return -1;
    Scan in = { f, 0, 0, 1 };
    int r = rt_vscan(&in, fmt, ap);
    rt_free(fmt);
    return r;
}

int rt_vswscanf(const RtWchar *ws, const RtWchar *wfmt, va_list ap)
{
    char *fmt = wide_to_utf8(wfmt), *s = wide_to_utf8(ws);
    int r = -1;
    if (fmt && s) {
        Scan in = { 0, s, 0, 1 };
        r = rt_vscan(&in, fmt, ap);
    }
    rt_free(fmt);
    rt_free(s);
    return r;
}

int rt_vwscanf(const RtWchar *fmt, va_list ap) { return rt_vfwscanf(rt_stdin, fmt, ap); }

int rt_fwscanf(RtFile *f, const RtWchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfwscanf(f, fmt, ap);
    va_end(ap);
    return n;
}

int rt_swscanf(const RtWchar *s, const RtWchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vswscanf(s, fmt, ap);
    va_end(ap);
    return n;
}

int rt_wscanf(const RtWchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = rt_vfwscanf(rt_stdin, fmt, ap);
    va_end(ap);
    return n;
}

size_t rt_wcsftime(RtWchar *ws, size_t max, const RtWchar *wfmt, const RtTm *tm)
{
    char *fmt = wide_to_utf8(wfmt);
    if (!fmt || !max) { rt_free(fmt); return 0; }
    size_t cap = max * 4 + 64;
    char *out = rt_malloc(cap);
    size_t r = 0;
    if (out && (rt_strftime(out, cap, fmt, tm) || !*fmt)) {
        size_t k = rt_mbstowcs(0, out, 0);
        if (k != (size_t)-1 && k < max) {
            rt_mbstowcs(ws, out, max);
            r = k;
        }
    }
    rt_free(out);
    rt_free(fmt);
    return r;
}

// ---- stdio under the lock (Windows: several threads may print at once) ----
#ifdef __WINCC__
RtFile *rt_fopen(const char *name, const char *mode) { cc_io_lock(); RtFile * r = rt_fopen_unlocked(name, mode); cc_io_unlock(); return r; }
int rt_fclose(RtFile *f) { cc_io_lock(); int r = rt_fclose_unlocked(f); cc_io_unlock(); return r; }
RtFile *rt_freopen(const char *name, const char *mode, RtFile *f) { cc_io_lock(); RtFile * r = rt_freopen_unlocked(name, mode, f); cc_io_unlock(); return r; }
int rt_fflush(RtFile *f) { cc_io_lock(); int r = rt_fflush_unlocked(f); cc_io_unlock(); return r; }
RtFile *rt_tmpfile(void) { cc_io_lock(); RtFile * r = rt_tmpfile_unlocked(); cc_io_unlock(); return r; }
int rt_fgetc(RtFile *f) { cc_io_lock(); int r = rt_fgetc_unlocked(f); cc_io_unlock(); return r; }
int rt_getchar(void) { cc_io_lock(); int r = rt_getchar_unlocked(); cc_io_unlock(); return r; }
int rt_fputc(int c, RtFile *f) { cc_io_lock(); int r = rt_fputc_unlocked(c, f); cc_io_unlock(); return r; }
int rt_putchar(int c) { cc_io_lock(); int r = rt_putchar_unlocked(c); cc_io_unlock(); return r; }
int rt_ungetc(int c, RtFile *f) { cc_io_lock(); int r = rt_ungetc_unlocked(c, f); cc_io_unlock(); return r; }
char *rt_fgets(char *buf, int n, RtFile *f) { cc_io_lock(); char * r = rt_fgets_unlocked(buf, n, f); cc_io_unlock(); return r; }
int rt_fputs(const char *s, RtFile *f) { cc_io_lock(); int r = rt_fputs_unlocked(s, f); cc_io_unlock(); return r; }
int rt_puts(const char *s) { cc_io_lock(); int r = rt_puts_unlocked(s); cc_io_unlock(); return r; }
size_t rt_fread(void *ptr, size_t size, size_t count, RtFile *f) { cc_io_lock(); size_t r = rt_fread_unlocked(ptr, size, count, f); cc_io_unlock(); return r; }
size_t rt_fwrite(const void *ptr, size_t size, size_t count, RtFile *f) { cc_io_lock(); size_t r = rt_fwrite_unlocked(ptr, size, count, f); cc_io_unlock(); return r; }
int rt_fseek(RtFile *f, i64 off, int whence) { cc_io_lock(); int r = rt_fseek_unlocked(f, off, whence); cc_io_unlock(); return r; }
i64 rt_ftell(RtFile *f) { cc_io_lock(); i64 r = rt_ftell_unlocked(f); cc_io_unlock(); return r; }
int rt_vfprintf(RtFile *f, const char *fmt, va_list ap) { cc_io_lock(); int r = rt_vfprintf_unlocked(f, fmt, ap); cc_io_unlock(); return r; }
int rt_vfscanf(RtFile *f, const char *fmt, va_list ap) { cc_io_lock(); int r = rt_vfscanf_unlocked(f, fmt, ap); cc_io_unlock(); return r; }
u32 rt_fputwc(RtWchar wc, RtFile *f) { cc_io_lock(); u32 r = rt_fputwc_unlocked(wc, f); cc_io_unlock(); return r; }
u32 rt_fgetwc(RtFile *f) { cc_io_lock(); u32 r = rt_fgetwc_unlocked(f); cc_io_unlock(); return r; }
u32 rt_ungetwc(u32 wc, RtFile *f) { cc_io_lock(); u32 r = rt_ungetwc_unlocked(wc, f); cc_io_unlock(); return r; }
RtWchar *rt_fgetws(RtWchar *ws, int n, RtFile *f) { cc_io_lock(); RtWchar * r = rt_fgetws_unlocked(ws, n, f); cc_io_unlock(); return r; }
int rt_fputws(const RtWchar *ws, RtFile *f) { cc_io_lock(); int r = rt_fputws_unlocked(ws, f); cc_io_unlock(); return r; }
int rt_vfwprintf(RtFile *f, const RtWchar *wfmt, va_list ap) { cc_io_lock(); int r = rt_vfwprintf_unlocked(f, wfmt, ap); cc_io_unlock(); return r; }
int rt_vfwscanf(RtFile *f, const RtWchar *wfmt, va_list ap) { cc_io_lock(); int r = rt_vfwscanf_unlocked(f, wfmt, ap); cc_io_unlock(); return r; }
#endif

// ---- symbol table for the assembler ----
// In the system (and cctest) it is a table of names and addresses. In a
// Windows program (__WINCC__) every line becomes ".alias name, rt_fn": the
// name a program calls is a label of the function itself. An .alias keeps
// its first target, and a program's own function of that name wins.

#ifdef __WINCC__
// The functions of long with the Windows size (4 bytes): they come first.
long rt_wcstol32(const RtWchar *s, RtWchar **end, int base) { return clamp_long(rt_wcstoll(s, end, base)); }
unsigned long rt_wcstoul32(const RtWchar *s, RtWchar **end, int base) { return clamp_ulong(rt_wcstoull(s, end, base)); }
long rt_lround32(double x) { return (long)rt_round(x); }
long rt_lrint32(double x) { return (long)rt_nearbyint(x); }
double rt_scalbln32(double x, long e) { return rt_scalbln(x, e); }
int rt_fseek32(RtFile *f, long off, int whence) { return rt_fseek(f, off, whence); }
long rt_ftell32(RtFile *f) { return (long)rt_ftell(f); }

#define F(name, fn) ".alias " name ", " #fn "\n"
asm(
    F("labs", rt_labs) F("atol", rt_atol) F("ldiv", rt_ldiv) F("strtol", rt_strtol) F("strtoul", rt_strtoul)
    F("wcstol", rt_wcstol32) F("wcstoul", rt_wcstoul32) F("lround", rt_lround32) F("lrint", rt_lrint32)
    F("scalbln", rt_scalbln32) F("fseek", rt_fseek32) F("ftell", rt_ftell32)
    F("getenv", rt_getenv_win) F("system", rt_system_win)
    F("_fileno", rt_fileno) F("fileno", rt_fileno) F("_fdopen", rt_fdopen) F("fdopen", rt_fdopen)
    F("_fseeki64", rt_fseek) F("_ftelli64", rt_ftell) F("fseeko", rt_fseek) F("ftello", rt_ftell)
    F("_aligned_malloc", rt__aligned_malloc) F("_aligned_free", rt_free) F("_msize", rt__msize)
    F("_expand", rt__expand)
#else
#define F(name, fn) { name, (void *)fn },

static const CcSymbol g_symbols[] = {
#endif
    // stdio.h
    F("printf", rt_printf) F("sprintf", rt_sprintf) F("snprintf", rt_snprintf)
    F("vsnprintf", rt_vsnprintf) F("vsprintf", rt_vsprintf) F("vprintf", rt_vprintf)
    F("fprintf", rt_fprintf) F("vfprintf", rt_vfprintf)
    F("scanf", rt_scanf) F("vscanf", rt_vscanf) F("sscanf", rt_sscanf) F("vsscanf", rt_vsscanf)
    F("fscanf", rt_fscanf) F("vfscanf", rt_vfscanf)
    F("putchar", rt_putchar) F("puts", rt_puts) F("getchar", rt_getchar)
    F("fopen", rt_fopen) F("freopen", rt_freopen) F("fclose", rt_fclose) F("fflush", rt_fflush)
    F("fread", rt_fread) F("fwrite", rt_fwrite) F("fgetc", rt_fgetc) F("getc", rt_fgetc)
    F("fgets", rt_fgets) F("fputc", rt_fputc) F("putc", rt_fputc) F("fputs", rt_fputs)
    F("ungetc", rt_ungetc) F("fseek", rt_fseek) F("ftell", rt_ftell) F("rewind", rt_rewind)
    F("fgetpos", rt_fgetpos) F("fsetpos", rt_fsetpos)
    F("feof", rt_feof) F("ferror", rt_ferror) F("clearerr", rt_clearerr)
    F("setvbuf", rt_setvbuf) F("setbuf", rt_setbuf) F("tmpfile", rt_tmpfile) F("tmpnam", rt_tmpnam)
    F("remove", rt_remove) F("rename", rt_rename) F("perror", rt_perror)
    F("stdin", &rt_stdin) F("stdout", &rt_stdout) F("stderr", &rt_stderr)
    // string.h
    F("strlen", rt_strlen) F("strnlen", rt_strnlen) F("strcmp", rt_strcmp) F("strncmp", rt_strncmp)
    F("strcasecmp", rt_strcasecmp) F("strncasecmp", rt_strncasecmp)
    F("strcpy", rt_strcpy) F("strncpy", rt_strncpy) F("stpcpy", rt_stpcpy)
    F("strcat", rt_strcat) F("strncat", rt_strncat)
    F("strchr", rt_strchr) F("strrchr", rt_strrchr) F("strstr", rt_strstr)
    F("strspn", rt_strspn) F("strcspn", rt_strcspn) F("strpbrk", rt_strpbrk)
    F("strtok", rt_strtok) F("strtok_r", rt_strtok_r) F("strdup", rt_strdup) F("strndup", rt_strndup)
    F("strcoll", rt_strcoll) F("strxfrm", rt_strxfrm) F("strerror", rt_strerror)
    F("memcpy", rt_memcpy) F("memmove", rt_memmove) F("memset", rt_memset)
    F("memcmp", rt_memcmp) F("memchr", rt_memchr) F("memccpy", rt_memccpy)
    // ctype.h
    F("isalnum", rt_isalnum) F("isalpha", rt_isalpha) F("isblank", rt_isblank) F("iscntrl", rt_iscntrl)
    F("isdigit", rt_isdigit) F("isgraph", rt_isgraph) F("islower", rt_islower) F("isprint", rt_isprint)
    F("ispunct", rt_ispunct) F("isspace", rt_isspace) F("isupper", rt_isupper) F("isxdigit", rt_isxdigit)
    F("tolower", rt_tolower) F("toupper", rt_toupper)
    // stdlib.h
    F("malloc", rt_malloc) F("free", rt_free) F("calloc", rt_calloc) F("realloc", rt_realloc)
    F("aligned_alloc", rt_aligned_alloc)
    F("abs", rt_abs) F("labs", rt_llabs) F("llabs", rt_llabs) F("imaxabs", rt_llabs)
    F("div", rt_div) F("ldiv", rt_lldiv) F("lldiv", rt_lldiv) F("imaxdiv", rt_lldiv)
    F("atoi", rt_atoi) F("atol", rt_atoll) F("atoll", rt_atoll) F("atof", rt_atof)
    F("strtol", rt_strtoll) F("strtoll", rt_strtoll) F("strtoimax", rt_strtoll)
    F("strtoul", rt_strtoull) F("strtoull", rt_strtoull) F("strtoumax", rt_strtoull)
    F("strtod", rt_strtod) F("strtold", rt_strtod) F("strtof", rt_strtof)
    F("rand", rt_rand) F("srand", rt_srand) F("qsort", rt_qsort) F("bsearch", rt_bsearch)
    F("exit", rt_exit) F("_Exit", rt__Exit) F("_exit", rt__Exit) F("abort", rt_abort)
    F("atexit", rt_atexit) F("at_quick_exit", rt_at_quick_exit) F("quick_exit", rt_quick_exit)
    F("getenv", rt_getenv) F("system", rt_system)
    // setjmp.h
    F("__sj_save", rt_sj_save) F("__cc_setjmp", cc_setjmp) F("__cc_longjmp", cc_longjmp)
    // errno.h, assert.h, signal.h, locale.h
    F("errno", &rt_errno) F("__assert_fail", rt_assert_fail)
    F("signal", rt_signal) F("raise", rt_raise)
    F("setlocale", rt_setlocale) F("localeconv", rt_localeconv)
    // math.h
    F("sqrt", rt_sqrt) F("cbrt", rt_cbrt) F("hypot", rt_hypot) F("pow", rt_pow)
    F("exp", rt_exp) F("exp2", rt_exp2) F("expm1", rt_expm1)
    F("log", rt_log) F("log2", rt_log2) F("log10", rt_log10) F("log1p", rt_log1p)
    F("logb", rt_logb) F("ilogb", rt_ilogb)
    F("sin", rt_sin) F("cos", rt_cos) F("tan", rt_tan) F("asin", rt_asin) F("acos", rt_acos)
    F("atan", rt_atan) F("atan2", rt_atan2)
    F("sinh", rt_sinh) F("cosh", rt_cosh) F("tanh", rt_tanh)
    F("asinh", rt_asinh) F("acosh", rt_acosh) F("atanh", rt_atanh)
    F("erf", rt_erf) F("erfc", rt_erfc) F("tgamma", rt_tgamma) F("lgamma", rt_lgamma) F("signgam", &rt_signgam)
    F("floor", rt_floor) F("ceil", rt_ceil) F("round", rt_round) F("trunc", rt_trunc)
    F("rint", rt_nearbyint) F("nearbyint", rt_nearbyint)
    F("lround", rt_llround) F("llround", rt_llround) F("lrint", rt_llrint) F("llrint", rt_llrint)
    F("fmod", rt_fmod) F("remainder", rt_remainder) F("remquo", rt_remquo) F("modf", rt_modf)
    F("frexp", rt_frexp) F("ldexp", rt_ldexp) F("scalbn", rt_scalbn) F("scalbln", rt_scalbln)
    F("fabs", rt_fabs) F("fmin", rt_fmin) F("fmax", rt_fmax) F("fdim", rt_fdim) F("fma", rt_fma)
    F("copysign", rt_copysign) F("nextafter", rt_nextafter) F("nan", rt_nan)
    F("__isnan", rt_isnan) F("__isinf", rt_isinf) F("__isfinite", rt_isfinite)
    F("__signbit", rt_signbit) F("__fpclassify", rt_fpclassify)
    F("sqrtf", rt_sqrtf) F("fabsf", rt_fabsf) F("floorf", rt_floorf) F("ceilf", rt_ceilf)
    F("roundf", rt_roundf) F("truncf", rt_truncf) F("fmodf", rt_fmodf)
    F("sinf", rt_sinf) F("cosf", rt_cosf) F("tanf", rt_tanf) F("asinf", rt_asinf) F("acosf", rt_acosf)
    F("atanf", rt_atanf) F("atan2f", rt_atan2f) F("sinhf", rt_sinhf) F("coshf", rt_coshf)
    F("tanhf", rt_tanhf) F("expf", rt_expf) F("exp2f", rt_exp2f) F("logf", rt_logf)
    F("log2f", rt_log2f) F("log10f", rt_log10f) F("powf", rt_powf) F("cbrtf", rt_cbrtf)
    F("hypotf", rt_hypotf) F("fminf", rt_fminf) F("fmaxf", rt_fmaxf) F("copysignf", rt_copysignf)
    F("ldexpf", rt_ldexpf) F("frexpf", rt_frexpf) F("modff", rt_modff) F("nanf", rt_nanf)
    // long double is double here
    F("sqrtl", rt_sqrt) F("fabsl", rt_fabs) F("powl", rt_pow) F("expl", rt_exp) F("logl", rt_log)
    F("sinl", rt_sin) F("cosl", rt_cos) F("tanl", rt_tan) F("atanl", rt_atan) F("atan2l", rt_atan2)
    F("floorl", rt_floor) F("ceill", rt_ceil) F("fmodl", rt_fmod)
    // time.h
    F("time", rt_time) F("clock", rt_clock) F("difftime", rt_difftime) F("mktime", rt_mktime)
    F("gmtime", rt_gmtime) F("localtime", rt_localtime) F("gmtime_r", rt_gmtime_r)
    F("localtime_r", rt_localtime_r) F("asctime", rt_asctime) F("ctime", rt_ctime)
    F("strftime", rt_strftime) F("timespec_get", rt_timespec_get)
    // stdlib.h multibyte, wchar.h, wctype.h, uchar.h
    F("mblen", rt_mblen) F("mbtowc", rt_mbtowc) F("wctomb", rt_wctomb)
    F("mbstowcs", rt_mbstowcs) F("wcstombs", rt_wcstombs)
    F("mbrlen", rt_mbrlen) F("mbrtowc", rt_mbrtowc) F("wcrtomb", rt_wcrtomb) F("mbsinit", rt_mbsinit)
    F("mbsrtowcs", rt_mbsrtowcs) F("wcsrtombs", rt_wcsrtombs) F("btowc", rt_btowc) F("wctob", rt_wctob)
    F("mbrtoc16", rt_mbrtoc16) F("c16rtomb", rt_c16rtomb) F("mbrtoc32", rt_mbrtoc32) F("c32rtomb", rt_c32rtomb)
    F("wcslen", rt_wcslen) F("wcsnlen", rt_wcsnlen) F("wcscpy", rt_wcscpy) F("wcsncpy", rt_wcsncpy)
    F("wcscat", rt_wcscat) F("wcsncat", rt_wcsncat) F("wcscmp", rt_wcscmp) F("wcsncmp", rt_wcsncmp)
    F("wcscoll", rt_wcscoll) F("wcsxfrm", rt_wcsxfrm) F("wcschr", rt_wcschr) F("wcsrchr", rt_wcsrchr)
    F("wcsstr", rt_wcsstr) F("wcsspn", rt_wcsspn) F("wcscspn", rt_wcscspn) F("wcspbrk", rt_wcspbrk)
    F("wcstok", rt_wcstok) F("wcsdup", rt_wcsdup)
    F("wmemcpy", rt_wmemcpy) F("wmemmove", rt_wmemmove) F("wmemset", rt_wmemset)
    F("wmemcmp", rt_wmemcmp) F("wmemchr", rt_wmemchr)
    F("wcstol", rt_wcstoll) F("wcstoll", rt_wcstoll) F("wcstoimax", rt_wcstoll)
    F("wcstoul", rt_wcstoull) F("wcstoull", rt_wcstoull) F("wcstoumax", rt_wcstoull)
    F("wcstod", rt_wcstod) F("wcstold", rt_wcstod) F("wcstof", rt_wcstof)
    F("fwide", rt_fwide) F("fputwc", rt_fputwc) F("putwc", rt_fputwc) F("putwchar", rt_putwchar)
    F("fgetwc", rt_fgetwc) F("getwc", rt_fgetwc) F("getwchar", rt_getwchar) F("ungetwc", rt_ungetwc)
    F("fgetws", rt_fgetws) F("fputws", rt_fputws)
    F("wprintf", rt_wprintf) F("fwprintf", rt_fwprintf) F("swprintf", rt_swprintf)
    F("vwprintf", rt_vwprintf) F("vfwprintf", rt_vfwprintf) F("vswprintf", rt_vswprintf)
    F("wscanf", rt_wscanf) F("fwscanf", rt_fwscanf) F("swscanf", rt_swscanf)
    F("vwscanf", rt_vwscanf) F("vfwscanf", rt_vfwscanf) F("vswscanf", rt_vswscanf)
    F("wcsftime", rt_wcsftime)
    F("iswalnum", rt_iswalnum) F("iswalpha", rt_iswalpha) F("iswblank", rt_iswblank)
    F("iswcntrl", rt_iswcntrl) F("iswdigit", rt_iswdigit) F("iswgraph", rt_iswgraph)
    F("iswlower", rt_iswlower) F("iswprint", rt_iswprint) F("iswpunct", rt_iswpunct)
    F("iswspace", rt_iswspace) F("iswupper", rt_iswupper) F("iswxdigit", rt_iswxdigit)
    F("towlower", rt_towlower) F("towupper", rt_towupper) F("wctype", rt_wctype) F("iswctype", rt_iswctype)
    F("wctrans", rt_wctrans) F("towctrans", rt_towctrans)
#ifdef __WINCC__
);
#else
    { 0, 0 },
};
#endif

#undef F

#ifndef __WINCC__
const CcSymbol *cc_runtime_symbols(void)
{
    return g_symbols;
}
#endif
