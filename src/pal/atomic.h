#ifndef MB_ATOMIC_H_INCLUDED
#define MB_ATOMIC_H_INCLUDED

#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>

typedef _Atomic int mb_atomic_int;
typedef _Atomic uint32_t mb_atomic_uint32;
typedef _Atomic uint64_t mb_atomic_uint64;
typedef _Atomic void *mb_atomic_ptr;

#define mb_atomic_load(ptr)            atomic_load(ptr)
#define mb_atomic_store(ptr, val)      atomic_store(ptr, val)
#define mb_atomic_fetch_add(ptr, val)  atomic_fetch_add(ptr, val)
#define mb_atomic_fetch_sub(ptr, val)  atomic_fetch_sub(ptr, val)
#define mb_atomic_cas(ptr, expected, desired) \
    atomic_compare_exchange_strong(ptr, expected, desired)

#elif defined(__GNUC__) || defined(__clang__)

typedef volatile int mb_atomic_int;
typedef volatile uint32_t mb_atomic_uint32;
typedef volatile uint64_t mb_atomic_uint64;
typedef volatile void *mb_atomic_ptr;

static inline int mb_atomic_load (volatile int *ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}
static inline void mb_atomic_store (volatile int *ptr, int val)
{
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
}
static inline int mb_atomic_fetch_add (volatile int *ptr, int val)
{
    return __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST);
}
static inline int mb_atomic_fetch_sub (volatile int *ptr, int val)
{
    return __atomic_fetch_sub(ptr, val, __ATOMIC_SEQ_CST);
}
static inline int mb_atomic_cas (volatile int *ptr, int *expected, int desired)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

#elif defined(_MSC_VER)

#include <intrin.h>

typedef volatile long mb_atomic_int;
typedef volatile unsigned __int32 mb_atomic_uint32;
typedef volatile unsigned __int64 mb_atomic_uint64;
typedef volatile void *mb_atomic_ptr;

static inline int mb_atomic_load (volatile long *ptr)
{
    return *ptr;
}
static inline void mb_atomic_store (volatile long *ptr, long val)
{
    *ptr = val;
}
static inline int mb_atomic_fetch_add (volatile long *ptr, long val)
{
    return _InterlockedExchangeAdd(ptr, val);
}
static inline int mb_atomic_fetch_sub (volatile long *ptr, long val)
{
    return _InterlockedExchangeAdd(ptr, -val);
}
static inline int mb_atomic_cas (volatile long *ptr, long *expected, long desired)
{
    long prev = _InterlockedCompareExchange(ptr, desired, *expected);
    if (prev == *expected) return 1;
    *expected = prev;
    return 0;
}

#else
#error "No atomic operations available for this compiler"
#endif

#endif
