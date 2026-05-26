#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#define TEST_PORT 18888

static void test_tcp_bind_connect (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18888");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://127.0.0.1:18888");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    usleep (100000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    usleep (100000);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_tcp_bind_connect: PASSED\n");
}

static void test_tcp_large_message (void)
{
    int s1, s2;
    int rc;
    char sendbuf[8192];
    char recvbuf[8192];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18889");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://127.0.0.1:18889");
    assert (rc >= 0);

    usleep (100000);

    for (int i = 0; i < (int) sizeof (sendbuf); i++)
        sendbuf[i] = (char) (i & 0xFF);

    rc = mb_send (s2, sendbuf, sizeof (sendbuf), 0);
    assert (rc == (int) sizeof (sendbuf));

    usleep (200000);

    rc = mb_recv (s1, recvbuf, sizeof (recvbuf), 0);
    assert (rc == (int) sizeof (sendbuf));
    assert (memcmp (sendbuf, recvbuf, sizeof (sendbuf)) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_tcp_large_message: PASSED\n");
}

static void test_tcp_connect_refused (void)
{
    int s;
    int rc;
    int ivl = 0;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl, sizeof (ivl));

    rc = mb_connect (s, "tcp://127.0.0.1:19999");
    assert (rc < 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  test_tcp_connect_refused: PASSED\n");
}

static void test_tcp_cross_transport (void)
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

    rc = mb_bind (s1, "tcp://127.0.0.1:18890");
    assert (rc >= 0);
    rc = mb_bind (s3, "inproc://test_cross");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://127.0.0.1:18890");
    assert (rc >= 0);
    rc = mb_connect (s4, "inproc://test_cross");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_send (s2, "TCP", 3, 0);
    assert (rc == 3);
    rc = mb_send (s4, "INP", 3, 0);
    assert (rc == 3);

    usleep (100000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "TCP", 3) == 0);

    rc = mb_recv (s3, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "INP", 3) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    rc = mb_close (s3);
    assert (rc == 0);
    rc = mb_close (s4);
    assert (rc == 0);

    printf ("  test_tcp_cross_transport: PASSED\n");
}

int main (void)
{
    printf ("test_tcp:\n");
    test_tcp_bind_connect ();
    test_tcp_large_message ();
    test_tcp_connect_refused ();
    test_tcp_cross_transport ();
    printf ("test_tcp: ALL PASSED\n");
    return 0;
}
