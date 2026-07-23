#include "../../src/pal/efd.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

static void test_efd_basic (void)
{
    struct mb_efd efd;

    mb_efd_init (&efd);
    assert (mb_efd_getfd (&efd) >= 0);

    mb_efd_signal (&efd);

    int rc = mb_efd_wait (&efd, 100);
    assert (rc == 0);
    (void) rc;

    mb_efd_unsignal (&efd);

    rc = mb_efd_wait (&efd, 50);
    assert (rc != 0);
    (void) rc;

    mb_efd_term (&efd);
    printf ("  efd_basic: OK\n");
}

struct efd_race_arg {
    struct mb_efd *efd;
    volatile int stop;
};

static void *efd_race_signaller (void *arg)
{
    struct efd_race_arg *a = (struct efd_race_arg *) arg;
    while (!a->stop)
        mb_efd_signal (a->efd);
    return NULL;
}

static void *efd_race_unsignaller (void *arg)
{
    struct efd_race_arg *a = (struct efd_race_arg *) arg;
    while (!a->stop)
        mb_efd_unsignal (a->efd);
    return NULL;
}

/* Concurrent signal/unsignal must not leave a permanent lost wakeup when
   the last operation intended to leave the efd signaled. */
static void test_efd_signal_unsignal_race (void)
{
    struct mb_efd efd;
    struct efd_race_arg arg;
    pthread_t t1, t2;
    int i;
    int rc;

    mb_efd_init (&efd);
    arg.efd = &efd;
    arg.stop = 0;

    rc = pthread_create (&t1, NULL, efd_race_signaller, &arg);
    assert (rc == 0);
    rc = pthread_create (&t2, NULL, efd_race_unsignaller, &arg);
    assert (rc == 0);

    /* Let them fight, then stop and leave a definitive signal. */
    for (i = 0; i < 50; ++i)
        mb_efd_wait (&efd, 1);

    arg.stop = 1;
    pthread_join (t1, NULL);
    pthread_join (t2, NULL);

    mb_efd_unsignal (&efd);
    mb_efd_signal (&efd);
    rc = mb_efd_wait (&efd, 100);
    assert (rc == 0);

    mb_efd_term (&efd);
    printf ("  efd_signal_unsignal_race: OK\n");
}

int main (void)
{
    printf ("test_efd:\n");
    test_efd_basic ();
    test_efd_signal_unsignal_race ();
    printf ("test_efd: PASSED\n");
    return 0;
}
