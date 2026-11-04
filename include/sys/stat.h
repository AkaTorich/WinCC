// sys/stat.h: what a file is -- its size, times, a folder or a file.
// Here struct stat has the 64-bit size and times (struct _stat64 of Microsoft).
//
//   struct stat st;
//   if (stat("a.txt", &st) == 0) printf("%lld байт\n", st.st_size);
#pragma once
#include <sys/types.h>

struct stat {
    _dev_t st_dev;
    _ino_t st_ino;
    unsigned short st_mode;         // _S_IFDIR or _S_IFREG, and _S_IREAD, _S_IWRITE, _S_IEXEC
    short st_nlink;
    short st_uid;
    short st_gid;
    _dev_t st_rdev;
    long long st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};
#define _stat64 stat
#define _stati64 stat
#define _stat64i32 stat

#define _S_IFMT     0xF000
#define _S_IFDIR    0x4000
#define _S_IFCHR    0x2000
#define _S_IFIFO    0x1000
#define _S_IFREG    0x8000
#define _S_IREAD    0x0100
#define _S_IWRITE   0x0080
#define _S_IEXEC    0x0040
#define S_IFMT  _S_IFMT
#define S_IFDIR _S_IFDIR
#define S_IFCHR _S_IFCHR
#define S_IFIFO _S_IFIFO
#define S_IFREG _S_IFREG
#define S_IREAD _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC _S_IEXEC
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IXUSR _S_IEXEC
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#define S_ISCHR(m) (((m) & _S_IFMT) == _S_IFCHR)
#define S_ISFIFO(m) (((m) & _S_IFMT) == _S_IFIFO)

int stat(const char *name, struct stat *st);
int fstat(int fd, struct stat *st);
int _stat(const char *name, struct stat *st);
int _fstat(int fd, struct stat *st);
int _fstat64(int fd, struct stat *st);
int _fstati64(int fd, struct stat *st);
int _mkdir(const char *name);
int _umask(int mask);
int umask(int mask);
