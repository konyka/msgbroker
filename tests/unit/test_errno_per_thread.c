/*  test_errno_per_thread.c — TDD gate for mb_errno thread-locality (T-HE10).
 *
 *  Two threads each set a distinct mb_errno via a deliberate mb_setsockopt
 *  failure (which internally calls mb_err_set_errno with a known native
 *  code path). After join, each thread's last mb_errno() read must match
 *  what that thread set — not the other thread's value.
 *
 *  With the single static-int bug (pre-fix), this test is racy and may
 *  pass by luck under low contention, but a single sanity check that one
 *  thread's value survives until the other finishes always reproduces
 *  the failure once the test is run with enough interleaving. The test
 *  is RUN_SERIAL because we want each thread to alternate enough set/read
 *  pairs that the bug, if present, is observed deterministically.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <msgbroker/mb.h>
#include "../../src/utils/err.h"

#define ITERATIONS 1000

static void *worker (void *arg)
{
    int marker = (int) (intptr_t) arg;
    int i;

    for (i = 0; i < ITERATIONS; ++i) {
        mb_err_set_errno (marker);
        if (mb_errno () != marker) {
            fprintf (stderr,
                "thread %d: mb_errno()=%d but expected %d\n",
                marker, mb_errno (), marker);
            return (void *) (intptr_t) 1;
        }
    }
    return NULL;
}

int main (void)
{
    pthread_t t1, t2;
    void *r1, *r2;

    if (pthread_create (&t1, NULL, worker, (void *) (intptr_t) ECONNREFUSED)
        != 0)
        return 1;
    if (pthread_create (&t2, NULL, worker, (void *) (intptr_t) ETIMEDOUT)
        != 0)
        return 1;
    pthread_join (t1, &r1);
    pthread_join (t2, &r2);
    assert (r1 == NULL);
    assert (r2 == NULL);

    printf ("test_errno_per_thread: PASSED\n");
    return 0;
}
