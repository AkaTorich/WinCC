// process.h: threads (_beginthreadex) and other programs (_spawnv, _execv) as
// in Microsoft C.
//
//   unsigned __stdcall work(void *arg) { ... return 0; }
//   HANDLE t = (HANDLE)_beginthreadex(0, 0, work, &data, 0, 0);
//   intptr_t code = _spawnlp(_P_WAIT, "notepad", "notepad", "a.txt", NULL);
#pragma once
#include <stddef.h>
#include <stdint.h>

#define _P_WAIT     0       // wait for it; the result is its exit code
#define _P_NOWAIT   1       // go on; the result is its handle (_cwait waits)
#define _P_OVERLAY  2       // this program ends with the other one's code
#define _P_NOWAITO  3
#define _P_DETACH   4       // on its own, without a console
#define _WAIT_CHILD 0
#define P_WAIT _P_WAIT
#define P_NOWAIT _P_NOWAIT
#define P_OVERLAY _P_OVERLAY

uintptr_t _beginthreadex(void *security, unsigned stack, unsigned (*start)(void *), void *arg,
                         unsigned flags, unsigned *thread_id);
uintptr_t _beginthread(void (*start)(void *), unsigned stack, void *arg);
void _endthreadex(unsigned code);
void _endthread(void);
int _getpid(void);
int getpid(void);

intptr_t _spawnv(int mode, const char *path, const char *const *argv);
intptr_t _spawnvp(int mode, const char *path, const char *const *argv);   // searched in PATH
intptr_t _spawnl(int mode, const char *path, const char *arg0, ...);     // the list ends with NULL
intptr_t _spawnlp(int mode, const char *path, const char *arg0, ...);
intptr_t _execv(const char *path, const char *const *argv);
intptr_t _execvp(const char *path, const char *const *argv);
intptr_t _execl(const char *path, const char *arg0, ...);
intptr_t _execlp(const char *path, const char *arg0, ...);
intptr_t _cwait(int *status, intptr_t handle, int action);
#define spawnv _spawnv
#define spawnvp _spawnvp
#define spawnl _spawnl
#define spawnlp _spawnlp
#define execv _execv
#define execvp _execvp
#define execl _execl
#define execlp _execlp
#define cwait _cwait

int system(const char *cmd);
void exit(int code);
void _exit(int code);
void abort(void);
