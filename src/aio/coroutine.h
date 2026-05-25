#ifndef MB_COROUTINE_H_INCLUDED
#define MB_COROUTINE_H_INCLUDED

#include <stddef.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <ucontext.h>
#endif

#define MB_CORO_STACK_SIZE (64 * 1024)

enum mb_coro_state {
    MB_CORO_READY,
    MB_CORO_RUNNING,
    MB_CORO_YIELDED,
    MB_CORO_DONE
};

struct mb_coro {
    void (*fn) (void *arg);
    void *arg;
    enum mb_coro_state state;
    void *stack;
    size_t stack_size;
#if defined(_WIN32)
    void *fiber;
    void *caller_fiber;
#else
    ucontext_t uctx;
    ucontext_t caller_uctx;
#endif
};

struct mb_coro *mb_coro_create (void (*fn) (void *arg), void *arg);
void mb_coro_destroy (struct mb_coro *coro);
int mb_coro_resume (struct mb_coro *coro);
void mb_coro_yield (struct mb_coro *coro);
int mb_coro_done (struct mb_coro *coro);

#endif
