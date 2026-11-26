// Files of a Windows program (WinCC), below runtime.c's FILE: a file is read and
// written by pieces through its handle (cc_fh_*), so its size does not matter.
// Also the descriptors of io.h (_open, _read, _write...), the folders of
// direct.h (_mkdir, _getcwd...) and stat of sys/stat.h. Names are UTF-8.
#include "rtwin.h"

typedef void *HANDLE;
typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef struct { DWORD lo, hi; } FILETIME;
typedef struct {
    DWORD attrib;
    FILETIME created, accessed, written;
    DWORD size_hi, size_lo;
} FILE_ATTRS;                                   // WIN32_FILE_ATTRIBUTE_DATA
typedef struct {
    DWORD attrib;
    FILETIME created, accessed, written;
    DWORD size_hi, size_lo;
    DWORD reserved0, reserved1;
    WCHAR name[260];
    WCHAR alt[14];
    DWORD obsolete[3];
} FIND_DATA;                                    // WIN32_FIND_DATAW

#define INVALID_HANDLE ((HANDLE)(i64)-1)
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define SHARE_ALL 7u
#define ATTR_NORMAL 0x80
#define ATTR_READONLY 0x1
#define ATTR_DIRECTORY 0x10
#define NO_ATTRIBUTES 0xFFFFFFFFu

HANDLE CreateFileW(const WCHAR *name, DWORD access, DWORD share, void *sa, DWORD disposition, DWORD flags, HANDLE tmpl);
BOOL ReadFile(HANDLE h, void *buf, DWORD size, DWORD *read, void *overlapped);
BOOL WriteFile(HANDLE h, const void *data, DWORD size, DWORD *written, void *overlapped);
BOOL SetFilePointerEx(HANDLE h, i64 off, i64 *now, DWORD how);
BOOL SetEndOfFile(HANDLE h);
BOOL FlushFileBuffers(HANDLE h);
BOOL GetFileSizeEx(HANDLE h, i64 *size);
BOOL CloseHandle(HANDLE h);
DWORD GetLastError(void);
DWORD GetFileType(HANDLE h);
HANDLE GetStdHandle(DWORD which);
HANDLE GetCurrentProcess(void);
BOOL DuplicateHandle(HANDLE sp, HANDLE h, HANDLE tp, HANDLE *out, DWORD access, BOOL inherit, DWORD options);
BOOL GetConsoleMode(HANDLE h, DWORD *mode);
BOOL WriteConsoleW(HANDLE h, const void *text, DWORD count, DWORD *written, void *reserved);
DWORD GetFileAttributesW(const WCHAR *name);
BOOL GetFileAttributesExW(const WCHAR *name, int level, FILE_ATTRS *out);
BOOL DeleteFileW(const WCHAR *name);
BOOL CreateDirectoryW(const WCHAR *name, void *sa);
BOOL RemoveDirectoryW(const WCHAR *name);
BOOL SetCurrentDirectoryW(const WCHAR *name);
DWORD GetCurrentDirectoryW(DWORD cap, WCHAR *buf);
HANDLE FindFirstFileW(const WCHAR *spec, FIND_DATA *d);
BOOL FindNextFileW(HANDLE h, FIND_DATA *d);
BOOL FindClose(HANDLE h);
BOOL GetFileInformationByHandle(HANDLE h, void *info);

void *rt_malloc(size_t n);
void rt_free(void *p);
extern _Thread_local int rt_errno;

static _Thread_local int g_err;

// errno for the last error of Windows
static int errno_of(DWORD e)
{
    switch (e) {
    case 2: case 3: case 15: case 161: return 2;            // ENOENT
    case 5: case 32: case 33: case 19: return 13;           // EACCES
    case 80: case 183: return 17;                           // EEXIST
    case 39: case 112: return 28;                           // ENOSPC
    case 6: return 9;                                       // EBADF
    case 8: case 14: return 12;                             // ENOMEM
    case 87: case 123: return 22;                           // EINVAL
    case 267: return 20;                                    // ENOTDIR
    case 145: return 41;                                    // ENOTEMPTY
    case 4: return 24;                                      // EMFILE
    default: return 5;                                      // EIO
    }
}

static int fail(void)
{
    g_err = errno_of(GetLastError());
    rt_errno = g_err;
    return -1;
}

int cc_fh_errno(void) { return g_err; }

static int is_folder(const WCHAR *w)
{
    DWORD a = GetFileAttributesW(w);
    return a != NO_ATTRIBUTES && (a & ATTR_DIRECTORY);
}

// ---- FILE below runtime.c ----

void *cc_fh_open(const char *name, int rd, int wr, int app, int trunc, int must, int excl, int *err)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) { *err = 12; return 0; }
    DWORD access = (rd ? GENERIC_READ : 0) | (wr ? GENERIC_WRITE : 0);
    DWORD how = excl ? 1 : trunc ? 2 : must ? 3 : 4;    // CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING, OPEN_ALWAYS
    HANDLE h = CreateFileW(w, access, SHARE_ALL, 0, how, ATTR_NORMAL, 0);
    if (h == INVALID_HANDLE) {
        *err = is_folder(w) ? 21 : errno_of(GetLastError());      // EISDIR
        rt_free(w);
        return 0;
    }
    rt_free(w);
    (void)app;
    return h;
}

i64 cc_fh_read(void *h, void *buf, size_t n)
{
    size_t have = 0;
    while (have < n) {
        DWORD got = 0;
        DWORD want = n - have > 0x40000000 ? 0x40000000 : (DWORD)(n - have);
        if (!ReadFile(h, (u8 *)buf + have, want, &got, 0)) {
            DWORD e = GetLastError();
            if (e == 109)           // ERROR_BROKEN_PIPE: the writer is gone, the end
                break;
            if (have) break;
            g_err = errno_of(e);
            return -1;
        }
        if (!got)
            break;
        have += got;
        if (GetFileType(h) != 1)    // a pipe or a device gives what it has: no waiting for more
            break;
    }
    return (i64)have;
}

i64 cc_fh_write(void *h, const void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        DWORD put = 0;
        DWORD want = n - done > 0x40000000 ? 0x40000000 : (DWORD)(n - done);
        if (!WriteFile(h, (const u8 *)buf + done, want, &put, 0) || !put) {
            g_err = errno_of(GetLastError());
            return done ? (i64)done : -1;
        }
        done += put;
    }
    return (i64)done;
}

i64 cc_fh_seek(void *h, i64 off, int whence)
{
    i64 now = 0;
    if (!SetFilePointerEx(h, off, &now, (DWORD)whence)) {
        g_err = errno_of(GetLastError());
        return -1;
    }
    return now;
}

int cc_fh_close(void *h)
{
    return CloseHandle(h) ? 0 : -1;
}

// ---- io.h: descriptors ----
// 0, 1, 2 are the standard handles; the others come from _open (or _open_osfhandle, _dup)

#define FD_MAX 512
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_APPEND 0x0008
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_EXCL 0x0400
#define O_TEXT 0x4000
#define O_BINARY 0x8000

typedef struct { HANDLE h; int used, text, app, eof; } Fd;
static Fd g_fd[FD_MAX];

static Fd *fd_of(int fd)
{
    if (fd >= 0 && fd < 3 && !g_fd[fd].used) {
        HANDLE h = GetStdHandle((DWORD)(-10 - fd));
        if (h && h != INVALID_HANDLE) {
            g_fd[fd].h = h;
            g_fd[fd].used = 1;
            g_fd[fd].text = 1;
        }
    }
    if (fd < 0 || fd >= FD_MAX || !g_fd[fd].used) {
        rt_errno = 9;           // EBADF
        return 0;
    }
    return &g_fd[fd];
}

static int fd_new(HANDLE h, int flags)
{
    for (int i = 3; i < FD_MAX; i++)
        if (!g_fd[i].used) {
            g_fd[i].h = h;
            g_fd[i].used = 1;
            g_fd[i].text = !(flags & O_BINARY);
            g_fd[i].app = (flags & O_APPEND) != 0;
            g_fd[i].eof = 0;
            return i;
        }
    rt_errno = 24;              // EMFILE
    return -1;
}

int rt__open(const char *name, int flags, ...)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) { rt_errno = 12; return -1; }
    int acc = flags & 3;
    DWORD access = acc == O_WRONLY ? GENERIC_WRITE : acc == O_RDWR ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
    DWORD how = (flags & O_CREAT) ? ((flags & O_EXCL) ? 1 : (flags & O_TRUNC) ? 2 : 4) : ((flags & O_TRUNC) ? 5 : 3);
    HANDLE h = CreateFileW(w, access, SHARE_ALL, 0, how, ATTR_NORMAL, 0);
    if (h == INVALID_HANDLE) {
        int folder = is_folder(w);
        rt_free(w);
        fail();
        if (folder) rt_errno = 21;
        return -1;
    }
    rt_free(w);
    int fd = fd_new(h, flags);
    if (fd < 0)
        CloseHandle(h);
    return fd;
}

int rt__creat(const char *name, int pmode)
{
    (void)pmode;
    return rt__open(name, O_WRONLY | O_CREAT | O_TRUNC);
}

int rt__close(int fd)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    BOOL ok = fd < 3 ? 1 : CloseHandle(f->h);
    f->used = 0;
    return ok ? 0 : fail();
}

int rt__read(int fd, void *buf, unsigned n)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    if (!n) return 0;
    i64 got = cc_fh_read(f->h, buf, n);
    if (got < 0) { rt_errno = g_err; return -1; }
    if (!got) { f->eof = 1; return 0; }
    if (f->text) {                              // "\r\n" -> "\n", Ctrl+Z ends the text
        u8 *p = buf;
        int k = 0;
        for (int i = 0; i < (int)got; i++) {
            if (p[i] == 26) { f->eof = 1; break; }
            if (p[i] == '\r' && i + 1 < (int)got && p[i + 1] == '\n')
                continue;
            p[k++] = p[i];
        }
        return k;
    }
    return (int)got;
}

static int is_console(HANDLE h)
{
    DWORD mode;
    return GetConsoleMode(h, &mode) != 0;
}

int rt__write(int fd, const void *buf, unsigned n)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    if (!n) return 0;
    const u8 *p = buf;
    if (is_console(f->h)) {                     // the console takes UTF-16
        int len;
        WCHAR *w = cc_wide((const char *)p, (int)n, &len);
        if (!w) return -1;
        DWORD done;
        WriteConsoleW(f->h, w, (DWORD)len, &done, 0);
        rt_free(w);
        return (int)n;
    }
    if (f->app)
        cc_fh_seek(f->h, 0, 2);
    if (!f->text) {
        i64 put = cc_fh_write(f->h, p, n);
        if (put < 0) { rt_errno = g_err; return -1; }
        return (int)put;
    }
    u8 out[1024];                               // text: "\n" -> "\r\n"
    int k = 0;
    for (unsigned i = 0; i < n; i++) {
        if (p[i] == '\n')
            out[k++] = '\r';
        out[k++] = p[i];
        if (k >= (int)sizeof(out) - 2 || i + 1 == n) {
            if (cc_fh_write(f->h, out, (size_t)k) < 0) { rt_errno = g_err; return -1; }
            k = 0;
        }
    }
    return (int)n;
}

i64 rt__lseeki64(int fd, i64 off, int whence)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    i64 r = cc_fh_seek(f->h, off, whence);
    if (r < 0) rt_errno = g_err;
    else f->eof = 0;
    return r;
}

long rt__lseek(int fd, long off, int whence) { return (long)rt__lseeki64(fd, off, whence); }
i64 rt__telli64(int fd) { return rt__lseeki64(fd, 0, 1); }
long rt__tell(int fd) { return (long)rt__lseeki64(fd, 0, 1); }

int rt__eof(int fd)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    i64 size = 0, at = cc_fh_seek(f->h, 0, 1);
    if (at < 0 || !GetFileSizeEx(f->h, &size)) return f->eof;
    return at >= size;
}

int rt__isatty(int fd)
{
    Fd *f = fd_of(fd);
    return f && GetFileType(f->h) == 2;       // FILE_TYPE_CHAR: the console, a device
}

int rt__dup(int fd)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    HANDLE h = 0;
    if (!DuplicateHandle(GetCurrentProcess(), f->h, GetCurrentProcess(), &h, 0, 0, 2))
        return fail();
    int n = fd_new(h, f->text ? 0 : O_BINARY);
    if (n < 0) CloseHandle(h);
    return n;
}

int rt__dup2(int fd, int to)
{
    Fd *f = fd_of(fd);
    if (!f || to < 0 || to >= FD_MAX) { rt_errno = 9; return -1; }
    HANDLE h = 0;
    if (!DuplicateHandle(GetCurrentProcess(), f->h, GetCurrentProcess(), &h, 0, 0, 2))
        return fail();
    if (g_fd[to].used && to >= 3)
        CloseHandle(g_fd[to].h);
    g_fd[to].h = h;
    g_fd[to].used = 1;
    g_fd[to].text = f->text;
    g_fd[to].app = f->app;
    g_fd[to].eof = 0;
    return 0;
}

int rt__setmode(int fd, int mode)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    int old = f->text ? O_TEXT : O_BINARY;
    f->text = !(mode & O_BINARY);
    return old;
}

i64 rt__filelengthi64(int fd)
{
    Fd *f = fd_of(fd);
    i64 size = 0;
    if (!f) return -1;
    if (!GetFileSizeEx(f->h, &size)) return fail();
    return size;
}

long rt__filelength(int fd) { return (long)rt__filelengthi64(fd); }

int rt__chsize_s(int fd, i64 size)
{
    Fd *f = fd_of(fd);
    if (!f) return 9;
    i64 at = cc_fh_seek(f->h, 0, 1);
    if (cc_fh_seek(f->h, size, 0) < 0 || !SetEndOfFile(f->h)) {
        fail();
        return rt_errno;
    }
    cc_fh_seek(f->h, at, 0);
    return 0;
}

int rt__chsize(int fd, long size) { return rt__chsize_s(fd, size) ? -1 : 0; }

int rt__commit(int fd)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    return FlushFileBuffers(f->h) ? 0 : fail();
}

i64 rt__get_osfhandle(int fd)
{
    Fd *f = fd_of(fd);
    return f ? (i64)f->h : -1;
}

int rt__open_osfhandle(i64 h, int flags)
{
    return fd_new((HANDLE)h, flags);
}

// 0 there and may be used so (0 being, 2 writing, 4 reading, 6 both); -1 and errno otherwise
int rt__access(const char *name, int mode)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) return -1;
    DWORD a = GetFileAttributesW(w);
    rt_free(w);
    if (a == NO_ATTRIBUTES) { rt_errno = 2; return -1; }
    if ((mode & 2) && (a & ATTR_READONLY) && !(a & ATTR_DIRECTORY)) { rt_errno = 13; return -1; }
    return 0;
}

int rt__unlink(const char *name)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) return -1;
    BOOL ok = DeleteFileW(w);
    rt_free(w);
    return ok ? 0 : fail();
}

int rt__umask(int mask) { (void)mask; return 0; }

// ---- _findfirst: the files of a folder by a pattern ("*.txt", "dir\\*") ----

typedef struct {
    unsigned attrib;
    i64 time_create, time_access, time_write;
    unsigned long size;
    char name[260];
} RtFindData;                                   // struct _finddata_t (the 64-bit times)

#define FT_TO_UNIX(t) (i64)(((((u64)(t).hi << 32) | (t).lo) / 10000000ull) - 11644473600ull)

static void find_fill(const FIND_DATA *d, RtFindData *out)
{
    out->attrib = d->attrib;
    out->time_create = FT_TO_UNIX(d->created);
    out->time_access = FT_TO_UNIX(d->accessed);
    out->time_write = FT_TO_UNIX(d->written);
    out->size = d->size_lo;
    cc_utf8(d->name, -1, out->name, sizeof(out->name));
}

i64 rt__findfirst(const char *spec, RtFindData *out)
{
    WCHAR *w = cc_wide(spec, -1, 0);
    if (!w) return -1;
    FIND_DATA d;
    HANDLE h = FindFirstFileW(w, &d);
    rt_free(w);
    if (h == INVALID_HANDLE) { fail(); return -1; }
    find_fill(&d, out);
    return (i64)h;
}

int rt__findnext(i64 h, RtFindData *out)
{
    FIND_DATA d;
    if (!FindNextFileW((HANDLE)h, &d)) {
        rt_errno = 2;
        return -1;
    }
    find_fill(&d, out);
    return 0;
}

int rt__findclose(i64 h) { return FindClose((HANDLE)h) ? 0 : -1; }

// ---- direct.h: folders ----

int rt__mkdir(const char *name)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) return -1;
    BOOL ok = CreateDirectoryW(w, 0);
    rt_free(w);
    return ok ? 0 : fail();
}

int rt__rmdir(const char *name)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) return -1;
    BOOL ok = RemoveDirectoryW(w);
    rt_free(w);
    return ok ? 0 : fail();
}

int rt__chdir(const char *name)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) return -1;
    BOOL ok = SetCurrentDirectoryW(w);
    rt_free(w);
    return ok ? 0 : fail();
}

// the current folder (UTF-8) into buf, or a new buffer when buf is 0
char *rt__getcwd(char *buf, int cap)
{
    static WCHAR w[32768];
    DWORD n = GetCurrentDirectoryW(32768, w);
    if (!n || n >= 32768) { fail(); return 0; }
    char tmp[32768 * 3];
    int len = cc_utf8(w, (int)n, tmp, sizeof(tmp));
    if (!buf) {
        int size = cap > len ? cap : len + 1;
        buf = rt_malloc((size_t)size);
        if (!buf) { rt_errno = 12; return 0; }
        cap = size;
    }
    if (len + 1 > cap) { rt_errno = 34; return 0; }    // ERANGE
    for (int i = 0; i <= len; i++)
        buf[i] = tmp[i];
    return buf;
}

// the current drive: 1 = A, 3 = C; 0 when the folder is not on a lettered drive
int rt__getdrive(void)
{
    static WCHAR w[32768];
    DWORD n = GetCurrentDirectoryW(32768, w);
    if (n < 2 || w[1] != ':') return 0;
    int c = w[0] | 32;
    return c >= 'a' && c <= 'z' ? c - 'a' + 1 : 0;
}

int rt__chdrive(int drive)
{
    WCHAR w[4] = { (WCHAR)('A' + drive - 1), ':', 0, 0 };
    if (drive < 1 || drive > 26) { rt_errno = 13; return -1; }
    return SetCurrentDirectoryW(w) ? 0 : fail();
}

// ---- sys/stat.h ----

typedef struct {
    unsigned st_dev;
    unsigned short st_ino, st_mode;
    short st_nlink, st_uid, st_gid;
    unsigned st_rdev;
    i64 st_size;
    i64 st_atime, st_mtime, st_ctime;
} RtStat;                                       // struct stat, struct _stat64 (sizes and times 64-bit)

static void stat_fill(RtStat *st, DWORD attrib, FILETIME c, FILETIME a, FILETIME w, i64 size, int drive)
{
    u8 *z = (u8 *)st;
    for (size_t i = 0; i < sizeof(*st); i++) z[i] = 0;
    st->st_mode = (unsigned short)((attrib & ATTR_DIRECTORY ? 0x4000 | 0x40 : 0x8000) | 0x100 |
                                   (attrib & ATTR_READONLY ? 0 : 0x80));
    st->st_mode |= (unsigned short)((st->st_mode & 0x1C0) >> 3 | (st->st_mode & 0x1C0) >> 6);   // group, others: the same
    st->st_nlink = 1;
    st->st_size = attrib & ATTR_DIRECTORY ? 0 : size;
    st->st_ctime = FT_TO_UNIX(c);
    st->st_atime = FT_TO_UNIX(a);
    st->st_mtime = FT_TO_UNIX(w);
    st->st_dev = st->st_rdev = (unsigned)drive;
}

int rt__stat64(const char *name, RtStat *st)
{
    WCHAR *w = cc_wide(name, -1, 0);
    if (!w) return -1;
    FILE_ATTRS a;
    BOOL ok = GetFileAttributesExW(w, 0, &a);
    rt_free(w);
    if (!ok) return fail();
    int drive = name[0] && name[1] == ':' ? ((name[0] | 32) - 'a') : rt__getdrive() - 1;
    stat_fill(st, a.attrib, a.created, a.accessed, a.written, (i64)(((u64)a.size_hi << 32) | a.size_lo), drive);
    if (st->st_mode & 0x8000) {                 // .exe .com .bat .cmd may be run
        int n = 0;
        while (name[n]) n++;
        if (n > 4 && name[n - 4] == '.') {
            char e[4] = { (char)(name[n - 3] | 32), (char)(name[n - 2] | 32), (char)(name[n - 1] | 32), 0 };
            if ((e[0] == 'e' && e[1] == 'x' && e[2] == 'e') || (e[0] == 'c' && e[1] == 'o' && e[2] == 'm') ||
                (e[0] == 'b' && e[1] == 'a' && e[2] == 't') || (e[0] == 'c' && e[1] == 'm' && e[2] == 'd'))
                st->st_mode |= 0x40 | 0x8 | 0x1;
        }
    }
    return 0;
}

typedef struct {
    DWORD attrib;
    FILETIME created, accessed, written;
    DWORD serial, size_hi, size_lo, links, index_hi, index_lo;
} HANDLE_INFO;                                  // BY_HANDLE_FILE_INFORMATION

int rt__fstat64(int fd, RtStat *st)
{
    Fd *f = fd_of(fd);
    if (!f) return -1;
    HANDLE_INFO in;
    DWORD type = GetFileType(f->h);
    if (type != 1 || !GetFileInformationByHandle(f->h, &in)) {     // the console, a pipe
        FILETIME none = { 0, 0 };
        stat_fill(st, 0, none, none, none, 0, 0);
        st->st_mode = (unsigned short)(type == 2 ? 0x2000 | 0x1B6 : 0x1000 | 0x1B6);   // _S_IFCHR, _S_IFIFO
        return 0;
    }
    stat_fill(st, in.attrib, in.created, in.accessed, in.written, (i64)(((u64)in.size_hi << 32) | in.size_lo), 0);
    st->st_nlink = (short)in.links;
    return 0;
}

// stdio.h of Microsoft with UTF-16 names: _wfopen and the like -- the name in UTF-8, then the usual function
int WideCharToMultiByte(unsigned cp, DWORD flags, const WCHAR *w, int wn, char *s, int sn, const char *def, BOOL *used);
void *rt_fopen(const char *name, const char *mode);
void *rt_freopen(const char *name, const char *mode, void *f);
int rt_remove(const char *name);
int rt_rename(const char *from, const char *to);

static char *utf8_name(const WCHAR *w)
{
    if (!w) return 0;
    int n = WideCharToMultiByte(65001, 0, w, -1, 0, 0, 0, 0);
    char *s = rt_malloc(n > 0 ? (size_t)n : 1);
    if (!s) return 0;
    if (n <= 0 || !WideCharToMultiByte(65001, 0, w, -1, s, n, 0, 0)) s[0] = 0;
    return s;
}

void *rt__wfopen(const WCHAR *name, const WCHAR *mode)
{
    char *n = utf8_name(name), *m = utf8_name(mode);
    void *f = n && m ? rt_fopen(n, m) : 0;
    rt_free(n);
    rt_free(m);
    return f;
}

void *rt__wfreopen(const WCHAR *name, const WCHAR *mode, void *file)
{
    char *n = utf8_name(name), *m = utf8_name(mode);
    void *f = m ? rt_freopen(n, m, file) : 0;
    rt_free(n);
    rt_free(m);
    return f;
}

int rt__wremove(const WCHAR *name)
{
    char *n = utf8_name(name);
    int r = n ? rt_remove(n) : -1;
    rt_free(n);
    return r;
}

int rt__wrename(const WCHAR *from, const WCHAR *to)
{
    char *a = utf8_name(from), *b = utf8_name(to);
    int r = a && b ? rt_rename(a, b) : -1;
    rt_free(a);
    rt_free(b);
    return r;
}

// The names programs call (io.h, direct.h, sys/stat.h): the Microsoft ones and the POSIX ones
#define F(name, fn) ".alias " name ", " #fn "\n"
asm(
    F("_open", rt__open) F("open", rt__open) F("_creat", rt__creat) F("creat", rt__creat)
    F("_close", rt__close) F("close", rt__close) F("_read", rt__read) F("read", rt__read)
    F("_write", rt__write) F("write", rt__write) F("_lseek", rt__lseek) F("lseek", rt__lseek)
    F("_lseeki64", rt__lseeki64) F("_tell", rt__tell) F("tell", rt__tell) F("_telli64", rt__telli64)
    F("_eof", rt__eof) F("eof", rt__eof) F("_isatty", rt__isatty) F("isatty", rt__isatty)
    F("_dup", rt__dup) F("dup", rt__dup) F("_dup2", rt__dup2) F("dup2", rt__dup2)
    F("_setmode", rt__setmode) F("setmode", rt__setmode)
    F("_filelength", rt__filelength) F("filelength", rt__filelength) F("_filelengthi64", rt__filelengthi64)
    F("_chsize", rt__chsize) F("chsize", rt__chsize) F("_chsize_s", rt__chsize_s) F("_commit", rt__commit)
    F("_get_osfhandle", rt__get_osfhandle) F("_open_osfhandle", rt__open_osfhandle)
    F("_access", rt__access) F("access", rt__access) F("_unlink", rt__unlink) F("unlink", rt__unlink)
    F("_umask", rt__umask) F("umask", rt__umask)
    F("_findfirst", rt__findfirst) F("_findfirst64", rt__findfirst) F("_findnext", rt__findnext)
    F("_findnext64", rt__findnext) F("_findclose", rt__findclose)
    F("_mkdir", rt__mkdir) F("mkdir", rt__mkdir) F("_rmdir", rt__rmdir) F("rmdir", rt__rmdir)
    F("_chdir", rt__chdir) F("chdir", rt__chdir) F("_getcwd", rt__getcwd) F("getcwd", rt__getcwd)
    F("_getdrive", rt__getdrive) F("_chdrive", rt__chdrive)
    F("_stat64", rt__stat64) F("_stat", rt__stat64) F("stat", rt__stat64) F("_stati64", rt__stat64)
    F("_fstat64", rt__fstat64) F("_fstat", rt__fstat64) F("fstat", rt__fstat64) F("_fstati64", rt__fstat64)
    F("_wfopen", rt__wfopen) F("_wfreopen", rt__wfreopen) F("_wremove", rt__wremove) F("_wrename", rt__wrename)
);
#undef F
