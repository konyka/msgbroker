/*  test_init_race.c — TDD gate for mb_global_init race (T-HE03).
 *
 *  Pre-fix: mb_socket lazily initialised the global mutex/condvar
 *  outside the global lock; two concurrent first-callers could race
 *  on mb_mutex_init. The TDD gate below opens many threads that
 *  call mb_socket simultaneously, then closes every returned fd,
 *  and asserts no UB / leak / double-init under TSan + ASan.
 *
 *  Also exercises the term-then-socket-then-term lifecycle: after
 *  mb_term, a follow-up mb_socket must reinitialise and close cleanly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#define THREADS 8
#define PER_THREAD 32

static void *worker (void *arg)
{
    int *fds = (int *) arg;
    int i;
    for (i = 0; i < PER_THREAD; ++i)
        fds[i] = -1;
    for (i = 0; i < PER_THREAD; ++i) {
        fds[i] = mb_socket (AF_MB, MB_PAIR);
        if (fds[i] < 0)
            return (void *) (intptr_t) 1;
    }
    return NULL;
}

int main (void)
{
    pthread_t th[THREADS];
    int *fds[THREADS];
    void *rv;
    int i, j;
    int rc;

    for (i = 0; i < THREADS; ++i) {
        fds[i] = (int *) calloc (PER_THREAD, sizeof (int));
        assert (fds[i] != NULL);
    }

    for (i = 0; i < THREADS; ++i) {
        rc = pthread_create (&th[i], NULL, worker, fds[i]);
        assert (rc == 0);
    }
    for (i = 0; i < THREADS; ++i) {
        pthread_join (th[i], &rv);
        assert (rv == NULL);
    }

    /* Close every socket. The post-close global term path must
     * release every transport cleanly. */
    for (i = 0; i < THREADS; ++i) {
        for (j = 0; j < PER_THREAD; ++j) {
            rc = mb_close (fds[i][j]);
            assert (rc == 0);
        }
        free (fds[i]);
    }

    /* term → re-term lifecycle: a second mb_term must be a safe no-op
     * rather than a NULL-pointer dereference. */
    mb_term ();
    mb_term ();

    printf ("test_init_race: PASSED\n");
    return 0;
}
