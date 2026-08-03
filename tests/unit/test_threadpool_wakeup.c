/*  test_threadpool_wakeup.c — TDD gate for threadpool submit wakeup (T-TP1).
 *
 *  Pre-fix: a worker whose local queue was empty slept for 1 ms
 *  (nanosleep) before checking again. Submitting a task during the
 *  sleep could therefore wait up to 1 ms before the worker noticed.
 *  The TDD gate below submits a single task to an idle worker and
 *  asserts the task function ran in well under 50 ms.
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "../../src/aio/threadpool.h"
#include "../../src/pal/clock.h"

static void task_fn (void *arg)
{
    uint64_t *done = (uint64_t *) arg;
    *done = mb_clock_ms ();
}

int main (void)
{
    struct mb_threadpool pool;
    struct mb_threadpool_task task;
    uint64_t t0, done;
    int rc;

    rc = mb_threadpool_init (&pool, 2);
    assert (rc == 0);

    /* Sleep a little so the worker is firmly in nanosleep when we
     * submit. This maximises the chance of catching the bug. */
    {
        struct timespec ts = {0, 100 * 1000 * 1000};  /* 100 ms */
        nanosleep (&ts, NULL);
    }

    done = 0;
    task.fn = task_fn;
    task.arg = &done;
    t0 = mb_clock_ms ();
    mb_threadpool_submit (&pool, &task);
    mb_threadpool_wait (&pool);

    assert (done > 0);
    uint64_t elapsed = done - t0;
    assert (elapsed < 50);

    mb_threadpool_term (&pool);

    printf ("test_threadpool_wakeup: PASSED (elapsed=%llu ms)\n",
        (unsigned long long) elapsed);
    return 0;
}
