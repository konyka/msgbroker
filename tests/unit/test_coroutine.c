#include "../../src/aio/coroutine.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int coro_steps [8];
static int coro_step_idx = 0;

static void simple_coro_fn (void *arg)
{
    struct mb_coro *coro = (struct mb_coro *) arg;

    coro_steps[coro_step_idx++] = 1;
    mb_coro_yield (coro);

    coro_steps[coro_step_idx++] = 2;
    mb_coro_yield (coro);

    coro_steps[coro_step_idx++] = 3;
}

int main (void)
{
    struct mb_coro *coro = mb_coro_create (simple_coro_fn, NULL);
    assert (coro != NULL);
    assert (!mb_coro_done (coro));
    coro->arg = coro;

    mb_coro_resume (coro);
    assert (coro_step_idx >= 1);
    assert (coro_steps[0] == 1);

    if (!mb_coro_done (coro))
        mb_coro_resume (coro);

    if (!mb_coro_done (coro))
        mb_coro_resume (coro);

    assert (coro_steps[0] == 1);
    assert (coro_steps[1] == 2);
    assert (coro_steps[2] == 3);

    mb_coro_destroy (coro);
    printf ("test_coroutine: PASSED\n");
    return 0;
}
