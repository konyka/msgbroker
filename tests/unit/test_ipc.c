#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_ipc_bind_connect (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ipc:///tmp/mb_test_ipc_1");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "ipc:///tmp/mb_test_ipc_1");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    usleep (50000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    usleep (50000);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    unlink ("/tmp/mb_test_ipc_1");
    printf ("  test_ipc_bind_connect: PASSED\n");
}

static void test_ipc_large_message (void)
{
    int s1, s2;
    int rc;
    char sendbuf[4096];
    char recvbuf[4096];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ipc:///tmp/mb_test_ipc_large");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "ipc:///tmp/mb_test_ipc_large");
    assert (rc >= 0);

    usleep (100000);

    for (int i = 0; i < (int) sizeof (sendbuf); i++)
        sendbuf[i] = (char) (i & 0xFF);

    rc = mb_send (s2, sendbuf, sizeof (sendbuf), 0);
    assert (rc == (int) sizeof (sendbuf));

    usleep (100000);

    rc = mb_recv (s1, recvbuf, sizeof (recvbuf), 0);
    assert (rc == (int) sizeof (sendbuf));
    assert (memcmp (sendbuf, recvbuf, sizeof (sendbuf)) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    unlink ("/tmp/mb_test_ipc_large");
    printf ("  test_ipc_large_message: PASSED\n");
}

static void test_ipc_connect_refused (void)
{
    int s;
    int rc;
    int ivl = 0;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl, sizeof (ivl));

    rc = mb_connect (s, "ipc:///tmp/mb_test_ipc_nonexist");
    assert (rc < 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  test_ipc_connect_refused: PASSED\n");
}

static void test_ipc_multiple_messages (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ipc:///tmp/mb_test_ipc_multi");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "ipc:///tmp/mb_test_ipc_multi");
    assert (rc >= 0);

    usleep (100000);

    for (int i = 0; i < 10; i++) {
        char msg[8];
        msg[0] = 'A' + i;
        msg[1] = '\0';
        rc = mb_send (s2, msg, 1, 0);
        assert (rc == 1);
    }

    usleep (200000);

    for (int i = 0; i < 10; i++) {
        rc = mb_recv (s1, buf, sizeof (buf), 0);
        assert (rc == 1);
        assert (buf[0] == 'A' + i);
    }

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    unlink ("/tmp/mb_test_ipc_multi");
    printf ("  test_ipc_multiple_messages: PASSED\n");
}

int main (void)
{
    printf ("test_ipc:\n");
    test_ipc_bind_connect ();
    test_ipc_large_message ();
    test_ipc_connect_refused ();
    test_ipc_multiple_messages ();
    printf ("test_ipc: ALL PASSED\n");
    return 0;
}
