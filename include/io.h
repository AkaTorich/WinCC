// io.h: files by their descriptors (numbers), as in Microsoft C -- _open,
// _read, _write, _lseek, _close; and the files of a folder (_findfirst).
// 0, 1, 2 are the standard input, output and errors. Names are UTF-8.
//
//   int fd = _open("data.bin", _O_RDONLY | _O_BINARY);
//   int n = _read(fd, buf, sizeof buf);
//   _close(fd);
#pragma once
#include <stddef.h>
#include <fcntl.h>
#include <stdint.h>

#define _A_NORMAL   0x00
#define _A_RDONLY   0x01
#define _A_HIDDEN   0x02
#define _A_SYSTEM   0x04
#define _A_SUBDIR   0x10
#define _A_ARCH     0x20

typedef unsigned long _fsize_t;
struct _finddata_t {
    unsigned attrib;
    long long time_create, time_access, time_write;     // seconds since 1970 (time_t)
    _fsize_t size;
    char name[260];
};
#define _finddata64i32_t _finddata_t

int _open(const char *name, int flags, ...);
int _creat(const char *name, int pmode);
int _close(int fd);
int _read(int fd, void *buf, unsigned n);
int _write(int fd, const void *buf, unsigned n);
long _lseek(int fd, long off, int whence);
long long _lseeki64(int fd, long long off, int whence);
long _tell(int fd);
long long _telli64(int fd);
int _eof(int fd);
int _isatty(int fd);
int _dup(int fd);
int _dup2(int fd, int to);
int _setmode(int fd, int mode);         // _O_TEXT or _O_BINARY; the mode it had
long _filelength(int fd);
long long _filelengthi64(int fd);
int _chsize(int fd, long size);
int _chsize_s(int fd, long long size);
int _commit(int fd);
intptr_t _get_osfhandle(int fd);        // the HANDLE of Windows
int _open_osfhandle(intptr_t handle, int flags);
int _access(const char *name, int mode);        // 0 being, 2 writing, 4 reading, 6 both: 0 if so
int _unlink(const char *name);
int _umask(int mask);
intptr_t _findfirst(const char *pattern, struct _finddata_t *data);   // -1 when nothing
int _findnext(intptr_t handle, struct _finddata_t *data);              // -1 at the end
int _findclose(intptr_t handle);

// the POSIX names
int open(const char *name, int flags, ...);
int creat(const char *name, int pmode);
int close(int fd);
int read(int fd, void *buf, unsigned n);
int write(int fd, const void *buf, unsigned n);
long lseek(int fd, long off, int whence);
long tell(int fd);
int eof(int fd);
int isatty(int fd);
int dup(int fd);
int dup2(int fd, int to);
int setmode(int fd, int mode);
long filelength(int fd);
int chsize(int fd, long size);
int access(const char *name, int mode);
int unlink(const char *name);
int umask(int mask);

#define F_OK 0
#define W_OK 2
#define R_OK 4
