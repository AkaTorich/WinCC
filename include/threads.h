// threads.h (C11): threads, mutexes, condition variables, thread-specific
// storage and call_once, on the threads of Windows.
//
//   int work(void *arg) { ... return 0; }
//   thrd_t t; thrd_create(&t, work, &data); thrd_join(t, &result);
#pragma once
#include <time.h>

enum { thrd_success = 0, thrd_nomem = 1, thrd_timedout = 2, thrd_busy = 3, thrd_error = 4 };
enum { mtx_plain = 1, mtx_timed = 2, mtx_recursive = 0x100 };

typedef struct { void *__handle; unsigned long __id; } thrd_t;
typedef int (*thrd_start_t)(void *);
typedef struct { unsigned long long __cs[5]; int __type, __ready; } mtx_t;    // a critical section
typedef struct { void *__cv; } cnd_t;                                        // a condition variable
typedef int tss_t;
typedef void (*tss_dtor_t)(void *);
typedef struct { volatile long __state; } once_flag;
#define ONCE_FLAG_INIT { 0 }
#define TSS_DTOR_ITERATIONS 4
#define thread_local _Thread_local

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int thrd_join(thrd_t thr, int *res);
int thrd_detach(thrd_t thr);
thrd_t thrd_current(void);
int thrd_equal(thrd_t a, thrd_t b);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);
_Noreturn void thrd_exit(int res);

int mtx_init(mtx_t *mtx, int type);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *mtx, const struct timespec *until);     // until: TIME_UTC, as timespec_get gives
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);
void mtx_destroy(mtx_t *mtx);

int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_broadcast(cnd_t *cond);
int cnd_wait(cnd_t *cond, mtx_t *mtx);
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *until);
void cnd_destroy(cnd_t *cond);

int tss_create(tss_t *key, tss_dtor_t dtor);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);
void tss_delete(tss_t key);

void call_once(once_flag *flag, void (*func)(void));
