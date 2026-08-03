#ifndef MB_FAST_H_INCLUDED
#define MB_FAST_H_INCLUDED

#if defined(__GNUC__) || defined(__clang__)
#define mb_likely(x)   __builtin_expect(!!(x), 1)
#define mb_unlikely(x) __builtin_expect(!!(x), 0)
#else
#define mb_likely(x)   (x)
#define mb_unlikely(x) (x)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define mb_forceinline __attribute__((always_inline)) static inline
#elif defined(_MSC_VER)
#define mb_forceinline __forceinline static __inline
#else
#define mb_forceinline static inline
#endif

#if defined(__GNUC__) || defined(__clang__)
#define mb_noinline __attribute__((noinline))
#elif defined(_MSC_VER)
#define mb_noinline __declspec(noinline)
#else
#define mb_noinline
#endif

#if defined(__GNUC__) || defined(__clang__)
#define mb_cacheline_align __attribute__((aligned(64)))
#elif defined(_MSC_VER)
#define mb_cacheline_align __declspec(align(64))
#else
#define mb_cacheline_align
#endif

/*  Per-thread storage keyword. Used for the mb_err_ctx slot so that
 *  mb_errno() returns the value that the calling thread last observed
 *  rather than whatever some other thread most recently wrote. The
 *  doxygen for mb_errno() in mb.h documents thread-safe semantics;
 *  the implementation now matches. */
#if defined(__GNUC__) || defined(__clang__)
#define mb_thread_local __thread
#elif defined(_MSC_VER)
#define mb_thread_local __declspec(thread)
#else
#define mb_thread_local
#endif

#endif
