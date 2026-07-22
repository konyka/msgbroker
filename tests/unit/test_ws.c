#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <unistd.h>

static void test_ws_basic (void)
{
    int s1, s2, rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ws://*:9100");
    assert (rc >= 0);

    rc = mb_connect (s2, "ws://127.0.0.1:9100");
    assert (rc >= 0);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s1, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    mb_close (s2);
    mb_close (s1);

    printf ("  ws_basic: OK\n");
}

static void test_ws_bidir (void)
{
    int s1, s2, rc;
    char buf[64];
    int i;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ws://*:9101");
    assert (rc >= 0);
    rc = mb_connect (s2, "ws://127.0.0.1:9101");
    assert (rc >= 0);

    for (i = 0; i < 100; i++) {
        char send_buf[32];
        int len = snprintf (send_buf, sizeof (send_buf), "msg_%d", i);
        rc = mb_send (s2, send_buf, (size_t) len, 0);
        assert (rc == len);
        rc = mb_recv (s1, buf, sizeof (buf), 0);
        assert (rc == len);
        assert (memcmp (buf, send_buf, (size_t) len) == 0);
    }

    mb_close (s2);
    mb_close (s1);

    printf ("  ws_bidir: OK\n");
}

static void test_ws_large_msg (void)
{
    int s1, s2, rc;
    char send_buf[256];
    char recv_buf[256];
    size_t i;

    for (i = 0; i < sizeof (send_buf); i++)
        send_buf[i] = (char) ('A' + (i % 26));

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ws://*:9102");
    assert (rc >= 0);
    rc = mb_connect (s2, "ws://127.0.0.1:9102");
    assert (rc >= 0);

    rc = mb_send (s2, send_buf, sizeof (send_buf), 0);
    assert (rc == (int) sizeof (send_buf));
    rc = mb_recv (s1, recv_buf, sizeof (recv_buf), 0);
    assert (rc == (int) sizeof (send_buf));
    assert (memcmp (recv_buf, send_buf, sizeof (send_buf)) == 0);

    mb_close (s2);
    mb_close (s1);
    printf ("  ws_large_msg: OK\n");
}

int main (void)
{
    printf ("test_ws:\n");
    test_ws_basic ();
    test_ws_bidir ();
    test_ws_large_msg ();
    printf ("test_ws: PASSED\n");
    return 0;
}
