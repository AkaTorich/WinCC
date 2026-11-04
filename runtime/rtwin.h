// What runtime.c needs inside a Windows program (WinCC compiles it with __WINCC__):
// its integer types, the host hooks (winhost.c) and the few CPU helpers (rtasm.asm)
// that stand for MSVC's intrinsics.
#pragma once

#include <stddef.h>
#include <stdarg.h>

typedef unsigned char      u8;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef long long          i64;


// ---- host hooks: winhost.c ----
void  cc_print(const char *text);                           // UTF-8 to the console (or the redirected output)
const char *cc_file_read(const char *name, size_t *size);   // the whole file or NULL
int   cc_file_write(const char *name, const char *data, size_t size);
int   cc_file_delete(const char *name);
int   cc_file_rename(const char *from, const char *to);
int   cc_file_kind(const char *name);                       // 0 nothing, 1 a file, 2 a folder
int   cc_input_line(char *buf, int cap);                    // a line ending in '\n', its length; -1 at the end
void  cc_now(int t[6]);
i64   cc_utc_seconds(void);                                 // seconds since 1970-01-01 00:00 UTC
int   cc_utc_offset(i64 utc);                               // local time minus UTC at that moment, seconds
u64   cc_millis(void);
u64   cc_irq_off(void);
void  cc_irq_restore(u64 f);
void *cc_heap_more(size_t want, size_t *got);
char *rt_getenv_win(const char *name);
int   rt_system_win(const char *cmd);
void  wcc_exit_process(int code);                           // exit() outside main (a DLL, an atexit): the process ends
unsigned short *cc_wide(const char *s, int n, int *len);    // UTF-16 of UTF-8 (n bytes, -1 to the NUL), rt_free it
int   cc_utf8(const unsigned short *w, int n, char *buf, int cap);   // UTF-8 of UTF-16 into buf, its length

// ---- winfile.c: files read and written by pieces (FILE), descriptors (io.h) ----
void *cc_fh_open(const char *name, int rd, int wr, int app, int trunc, int must, int excl, int *err);
i64   cc_fh_read(void *h, void *buf, size_t n);              // bytes, 0 at the end, -1 an error
i64   cc_fh_write(void *h, const void *buf, size_t n);
i64   cc_fh_seek(void *h, i64 off, int whence);              // the new place, -1 an error
int   cc_fh_close(void *h);
int   cc_fh_errno(void);                                     // errno of the last failed call

// ---- winthread.c: one lock for stdio (several threads may print) ----
void  cc_io_lock(void);
void  cc_io_unlock(void);

// ---- winlocale.c: what a locale of Windows says ----
int   cc_locale_find(const char *name, char *out, int cap);  // "ru_RU.UTF-8" for "Russian", "ru-RU", ""...; 0 unknown
int   cc_locale_text(const char *loc, int what, int i, char *out, int cap);   // 0 month, 1 its short name, 2 day, 3 short day
int   cc_locale_char(const char *loc, int what);             // 0 decimal point, 1 thousands separator
int   cc_collate(const char *loc, const char *a, const char *b);
int   cc_collate_w(const char *loc, const unsigned short *a, const unsigned short *b);
size_t cc_sortkey(const char *loc, const char *s, char *out, size_t cap);
int   cc_zone_name(int summer, char *out, int cap);

// ---- rtasm.asm ----
long _InterlockedCompareExchange(volatile long *p, long value, long expected);
long _InterlockedExchange(volatile long *p, long value);
void _mm_pause(void);
unsigned char _BitScanReverse64(unsigned long *index, u64 v);
unsigned char _BitScanForward64(unsigned long *index, u64 v);
