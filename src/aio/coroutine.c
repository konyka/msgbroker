#include "coroutine.h"
#include "../utils/alloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ucontext.h>

static struct mb_coro *g_current_coro = NULL;

struct mb_coro *mb_coro_create (void (*fn) (void *arg), void *arg)
{
    struct mb_coro *coro = (struct mb_coro *) mb_alloc (sizeof (*coro));
    if (!coro)
        return NULL;
    memset (coro, 0, sizeof (*coro));
    coro->fn = fn;
    coro->arg = arg;
    coro->state = MB_CORO_READY;
    coro->stack_size = MB_CORO_STACK_SIZE;
    coro->stack = mb_alloc (coro->stack_size);
    if (!coro->stack) {
        mb_free (coro);
        return NULL;
    }
    return coro;
}

void mb_coro_destroy (struct mb_coro *coro)
{
    if (!coro)
        return;
    if (coro->stack)
        mb_free (coro->stack);
    mb_free (coro);
}

static void mb_coro_entry (void *arg)
{
    struct mb_coro *coro = (struct mb_coro *) arg;
    coro->fn (coro->arg);
    coro->state = MB_CORO_DONE;
}

int mb_coro_resume (struct mb_coro *coro)
{
    if (coro->state == MB_CORO_DONE)
        return -1;

    if (coro->state == MB_CORO_READY) {
        getcontext (&coro->uctx);
        coro->uctx.uc_stack.ss_sp = coro->stack;
        coro->uctx.uc_stack.ss_size = coro->stack_size;
        coro->uctx.uc_link = &coro->caller_uctx;
        makecontext (&coro->uctx, (void (*)(void)) mb_coro_entry, 1, coro);

        g_current_coro = coro;
        coro->state = MB_CORO_RUNNING;
        swapcontext (&coro->caller_uctx, &coro->uctx);
        return coro->state == MB_CORO_DONE ? 1 : 0;
    }

    g_current_coro = coro;
    coro->state = MB_CORO_RUNNING;
    swapcontext (&coro->caller_uctx, &coro->uctx);
    return coro->state == MB_CORO_DONE ? 1 : 0;
}

void mb_coro_yield (struct mb_coro *coro)
{
    if (!coro)
        coro = g_current_coro;
    if (!coro || coro->state != MB_CORO_RUNNING)
        return;
    coro->state = MB_CORO_YIELDED;
    swapcontext (&coro->uctx, &coro->caller_uctx);
}

int mb_coro_done (struct mb_coro *coro)
{
    return coro->state == MB_CORO_DONE;
}
