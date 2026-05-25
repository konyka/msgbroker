#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

/*  Test basic inproc bind/connect and message passing between PAIR sockets. */
static void test_inproc_bind_connect (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    /*  Create two PAIR sockets. */
    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    /*  Bind s1 to inproc address. */
    rc = mb_bind (s1, "inproc://test_bind_connect");
    assert (rc >= 0);

    /*  Connect s2 to the same inproc address. */
    rc = mb_connect (s2, "inproc://test_bind_connect");
    assert (rc >= 0);

    /*  Send from s1, recv on s2. */
    rc = mb_send (s1, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    /*  Send from s2, recv on s1 (PAIR is bidirectional). */
    rc = mb_send (s2, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    /*  Close sockets. */
    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_bind_connect: PASSED\n");
}

/*  Test that binding to the same address twice fails. */
static void test_inproc_duplicate_bind (void)
{
    int s1, s2;
    int rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://test_dup");
    assert (rc >= 0);

    /*  Second bind to the same address should fail. */
    rc = mb_bind (s2, "inproc://test_dup");
    assert (rc < 0);
    assert (mb_errno () == EADDRINUSE);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_duplicate_bind: PASSED\n");
}

/*  Test connecting to a non-existent address (no bind yet). */
static void test_inproc_connect_no_bind (void)
{
    int s;
    int rc;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    /*  Connect to address that nobody has bound to — should succeed
        (the connection is just deferred/lost, not an error). */
    rc = mb_connect (s, "inproc://test_no_bind");
    assert (rc >= 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  test_inproc_connect_no_bind: PASSED\n");
}

/*  Test that close/unbind releases the address for reuse. */
static void test_inproc_address_reuse (void)
{
    int s1, s2;
    int rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);

    rc = mb_bind (s1, "inproc://test_reuse");
    assert (rc >= 0);

    rc = mb_close (s1);
    assert (rc == 0);

    /*  After close, the address should be available again. */
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s2, "inproc://test_reuse");
    assert (rc >= 0);

    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_address_reuse: PASSED\n");
}

/*  Test multiple inproc connections (different addresses). */
static void test_inproc_multiple_addresses (void)
{
    int s1, s2, s3, s4;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    s3 = mb_socket (AF_MB, MB_PAIR);
    assert (s3 >= 0);
    s4 = mb_socket (AF_MB, MB_PAIR);
    assert (s4 >= 0);

    rc = mb_bind (s1, "inproc://test_multi_1");
    assert (rc >= 0);
    rc = mb_bind (s3, "inproc://test_multi_2");
    assert (rc >= 0);

    rc = mb_connect (s2, "inproc://test_multi_1");
    assert (rc >= 0);
    rc = mb_connect (s4, "inproc://test_multi_2");
    assert (rc >= 0);

    /*  Send on first pair. */
    rc = mb_send (s1, "AAA", 3, 0);
    assert (rc == 3);
    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "AAA", 3) == 0);

    /*  Send on second pair. */
    rc = mb_send (s3, "BBB", 3, 0);
    assert (rc == 3);
    rc = mb_recv (s4, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "BBB", 3) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    rc = mb_close (s3);
    assert (rc == 0);
    rc = mb_close (s4);
    assert (rc == 0);

    printf ("  test_inproc_multiple_addresses: PASSED\n");
}

/*  Test sending a larger message through inproc. */
static void test_inproc_large_message (void)
{
    int s1, s2;
    int rc;
    char sendbuf[4096];
    char recvbuf[4096];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://test_large");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_large");
    assert (rc >= 0);

    /*  Fill with pattern. */
    for (int i = 0; i < (int) sizeof (sendbuf); i++)
        sendbuf[i] = (char) (i & 0xFF);

    rc = mb_send (s1, sendbuf, sizeof (sendbuf), 0);
    assert (rc == (int) sizeof (sendbuf));

    rc = mb_recv (s2, recvbuf, sizeof (recvbuf), 0);
    assert (rc == (int) sizeof (recvbuf));
    assert (memcmp (sendbuf, recvbuf, sizeof (sendbuf)) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_large_message: PASSED\n");
}

int main (void)
{
    printf ("test_inproc:\n");
    test_inproc_bind_connect ();
    test_inproc_duplicate_bind ();
    test_inproc_connect_no_bind ();
    test_inproc_address_reuse ();
    test_inproc_multiple_addresses ();
    test_inproc_large_message ();
    printf ("test_inproc: ALL PASSED\n");
    return 0;
}
