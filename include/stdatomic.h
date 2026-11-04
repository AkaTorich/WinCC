// stdatomic.h (C11): variables changed by several threads in one indivisible
// step. "_Atomic int n; n += 5;" is already such a step (lock cmpxchg); here are
// the functions: atomic_load, atomic_store, atomic_exchange, atomic_fetch_add,
// atomic_compare_exchange_strong... The memory_order of the _explicit forms is
// taken and kept by x86 as it is: every one works as memory_order_seq_cst.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    memory_order_relaxed, memory_order_consume, memory_order_acquire,
    memory_order_release, memory_order_acq_rel, memory_order_seq_cst
} memory_order;

#define ATOMIC_BOOL_LOCK_FREE 2
#define ATOMIC_CHAR_LOCK_FREE 2
#define ATOMIC_CHAR16_T_LOCK_FREE 2
#define ATOMIC_CHAR32_T_LOCK_FREE 2
#define ATOMIC_WCHAR_T_LOCK_FREE 2
#define ATOMIC_SHORT_LOCK_FREE 2
#define ATOMIC_INT_LOCK_FREE 2
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#define ATOMIC_POINTER_LOCK_FREE 2

typedef _Atomic(_Bool) atomic_bool;
typedef _Atomic(char) atomic_char;
typedef _Atomic(signed char) atomic_schar;
typedef _Atomic(unsigned char) atomic_uchar;
typedef _Atomic(short) atomic_short;
typedef _Atomic(unsigned short) atomic_ushort;
typedef _Atomic(int) atomic_int;
typedef _Atomic(unsigned int) atomic_uint;
typedef _Atomic(long) atomic_long;
typedef _Atomic(unsigned long) atomic_ulong;
typedef _Atomic(long long) atomic_llong;
typedef _Atomic(unsigned long long) atomic_ullong;
typedef _Atomic(unsigned short) atomic_char16_t;
typedef _Atomic(unsigned int) atomic_char32_t;
typedef _Atomic(unsigned short) atomic_wchar_t;
typedef _Atomic(int8_t) atomic_int8_t;
typedef _Atomic(uint8_t) atomic_uint8_t;
typedef _Atomic(int16_t) atomic_int16_t;
typedef _Atomic(uint16_t) atomic_uint16_t;
typedef _Atomic(int32_t) atomic_int32_t;
typedef _Atomic(uint32_t) atomic_uint32_t;
typedef _Atomic(int64_t) atomic_int64_t;
typedef _Atomic(uint64_t) atomic_uint64_t;
typedef _Atomic(intptr_t) atomic_intptr_t;
typedef _Atomic(uintptr_t) atomic_uintptr_t;
typedef _Atomic(size_t) atomic_size_t;
typedef _Atomic(ptrdiff_t) atomic_ptrdiff_t;
typedef _Atomic(long long) atomic_intmax_t;
typedef _Atomic(unsigned long long) atomic_uintmax_t;

#define ATOMIC_VAR_INIT(v) (v)
#define atomic_init(p, v) ((void)(*(p) = (v)))
#define kill_dependency(y) (y)
#define atomic_is_lock_free(p) ((void)(p), 1)

// a full fence: mfence; for one thread and its signal handler the compiler's order is enough
#define atomic_thread_fence(order) ((void)({ (void)(order); asm("mfence"); 0; }))
#define atomic_signal_fence(order) ((void)(order))

// an aligned load of up to 8 bytes is one step on x86; a store goes by xchg (a full fence)
#define atomic_load(p) (*(p))
#define atomic_store(p, v) ((void)__builtin_atomic_exchange((p), (v)))
#define atomic_exchange(p, v) __builtin_atomic_exchange((p), (v))
// 1 and *p = desired when *p was *expected; else 0 and *expected = *p
#define atomic_compare_exchange_strong(p, expected, desired) __builtin_atomic_cas((p), (expected), (desired))
#define atomic_compare_exchange_weak(p, expected, desired) __builtin_atomic_cas((p), (expected), (desired))
#define atomic_fetch_add(p, v) __builtin_atomic_fetch_add((p), (v))
#define atomic_fetch_sub(p, v) __builtin_atomic_fetch_add((p), -(v))
#define __wcc_atomic_op(p, v, op) ({ __typeof__(p) __p = (p); __typeof__(*__p) __v = (v), __o = *__p; \
    while (!__builtin_atomic_cas(__p, &__o, (__typeof__(*__p))(__o op __v))); __o; })
#define atomic_fetch_or(p, v) __wcc_atomic_op(p, v, |)
#define atomic_fetch_xor(p, v) __wcc_atomic_op(p, v, ^)
#define atomic_fetch_and(p, v) __wcc_atomic_op(p, v, &)

#define atomic_load_explicit(p, o) ((void)(o), atomic_load(p))
#define atomic_store_explicit(p, v, o) ((void)(o), atomic_store(p, v))
#define atomic_exchange_explicit(p, v, o) ((void)(o), atomic_exchange(p, v))
#define atomic_compare_exchange_strong_explicit(p, e, d, s, f) ((void)(s), (void)(f), atomic_compare_exchange_strong(p, e, d))
#define atomic_compare_exchange_weak_explicit(p, e, d, s, f) ((void)(s), (void)(f), atomic_compare_exchange_weak(p, e, d))
#define atomic_fetch_add_explicit(p, v, o) ((void)(o), atomic_fetch_add(p, v))
#define atomic_fetch_sub_explicit(p, v, o) ((void)(o), atomic_fetch_sub(p, v))
#define atomic_fetch_or_explicit(p, v, o) ((void)(o), atomic_fetch_or(p, v))
#define atomic_fetch_xor_explicit(p, v, o) ((void)(o), atomic_fetch_xor(p, v))
#define atomic_fetch_and_explicit(p, v, o) ((void)(o), atomic_fetch_and(p, v))

// atomic_flag: a flag set in one step (a spin lock)
typedef struct { _Atomic(unsigned char) __v; } atomic_flag;
#define ATOMIC_FLAG_INIT { 0 }
#define atomic_flag_test_and_set(f) ((_Bool)__builtin_atomic_exchange(&(f)->__v, (unsigned char)1))
#define atomic_flag_test_and_set_explicit(f, o) ((void)(o), atomic_flag_test_and_set(f))
#define atomic_flag_clear(f) ((void)__builtin_atomic_exchange(&(f)->__v, (unsigned char)0))
#define atomic_flag_clear_explicit(f, o) ((void)(o), atomic_flag_clear(f))
