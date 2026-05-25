#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>

/*  Test PUSH->PULL via inproc: basic send/recv. */
static void test_pipeline_inproc (void)
{
    int push, pull;
    int rc;
    char buf[64];

    push = mb_socket (AF_MB, MB_PUSH);
    assert (push >= 0);
    pull = mb_socket (AF_MB, MB_PULL);
    assert (pull >= 0);

    rc = mb_bind (pull, "inproc://pipeline1");
    assert (rc >= 0);

    rc = mb_connect (push, "inproc://pipeline1");
    assert (rc >= 0);

    /*  PUSH can send, PULL can recv. */
    rc = mb_send (push, "DATA", 4, 0);
    assert (rc == 4);

    rc = mb_recv (pull, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "DATA", 4) == 0);

    rc = mb_close (push);
    assert (rc == 0);
    rc = mb_close (pull);
    assert (rc == 0);

    printf ("  test_pipeline_inproc: PASSED\n");
}

/*  Test that PUSH cannot recv (NORECV flag). */
static void test_pipeline_push_norecv (void)
{
    int push;
    char buf[64];
    int rc;

    push = mb_socket (AF_MB, MB_PUSH);
    assert (push >= 0);

    rc = mb_recv (push, buf, sizeof (buf), 0);
    assert (rc < 0);
    assert (mb_errno () == ENOTSUP);

    rc = mb_close (push);
    assert (rc == 0);

    printf ("  test_pipeline_push_norecv: PASSED\n");
}

/*  Test that PULL cannot send (NOSEND flag). */
static void test_pipeline_pull_nosend (void)
{
    int pull;
    int rc;

    pull = mb_socket (AF_MB, MB_PULL);
    assert (pull >= 0);

    rc = mb_send (pull, "X", 1, 0);
    assert (rc < 0);
    assert (mb_errno () == ENOTSUP);

    rc = mb_close (pull);
    assert (rc == 0);

    printf ("  test_pipeline_pull_nosend: PASSED\n");
}

/*  Test PUSH->PULL via TCP. */
static void test_pipeline_tcp (void)
{
    int push, pull;
    int rc;
    char buf[64];

    push = mb_socket (AF_MB, MB_PUSH);
    assert (push >= 0);
    pull = mb_socket (AF_MB, MB_PULL);
    assert (pull >= 0);

    rc = mb_bind (pull, "tcp://127.0.0.1:19876");
    assert (rc >= 0);

    rc = mb_connect (push, "tcp://127.0.0.1:19876");
    assert (rc >= 0);

    usleep (200000);

    rc = mb_send (push, "TCP", 3, 0);
    assert (rc == 3);

    rc = mb_recv (pull, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "TCP", 3) == 0);

    rc = mb_close (push);
    assert (rc == 0);
    rc = mb_close (pull);
    assert (rc == 0);

    printf ("  test_pipeline_tcp: PASSED\n");
}

/*  Test PUSH->multiple PULL round-robin. */
static void test_pipeline_fanout (void)
{
    int push, p1, p2;
    int rc;
    char buf[64];

    push = mb_socket (AF_MB, MB_PUSH);
    assert (push >= 0);
    p1 = mb_socket (AF_MB, MB_PULL);
    assert (p1 >= 0);
    p2 = mb_socket (AF_MB, MB_PULL);
    assert (p2 >= 0);

    rc = mb_bind (p1, "inproc://fanout1");
    assert (rc >= 0);
    rc = mb_bind (p2, "inproc://fanout2");
    assert (rc >= 0);

    rc = mb_connect (push, "inproc://fanout1");
    assert (rc >= 0);
    rc = mb_connect (push, "inproc://fanout2");
    assert (rc >= 0);

    /*  Send two messages. */
    rc = mb_send (push, "A", 1, 0);
    assert (rc == 1);
    rc = mb_send (push, "B", 1, 0);
    assert (rc == 1);

    /*  Collect from both pull sockets. */
    int got1 = 0, got2 = 0;
    rc = mb_recv (p1, buf, sizeof (buf), 0);
    if (rc == 1) got1 = 1;
    rc = mb_recv (p2, buf, sizeof (buf), 0);
    if (rc == 1) got2 = 1;

    assert (got1 + got2 == 2);

    rc = mb_close (push);
    assert (rc == 0);
    rc = mb_close (p1);
    assert (rc == 0);
    rc = mb_close (p2);
    assert (rc == 0);

    printf ("  test_pipeline_fanout: PASSED\n");
}

int main (void)
{
    printf ("Pipeline protocol tests:\n");
    test_pipeline_inproc ();
    test_pipeline_push_norecv ();
    test_pipeline_pull_nosend ();
    test_pipeline_tcp ();
    test_pipeline_fanout ();
    printf ("All pipeline tests passed.\n");
    return 0;
}
