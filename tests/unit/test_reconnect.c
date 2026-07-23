#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include "../../src/pal/sleep.h"

static void test_reconnect_auto (void)
{
    int s1, s2;
    int rc;
    char buf[64];
    int ivl = 100;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    mb_setsockopt (s2, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl, sizeof (ivl));

    rc = mb_connect (s2, "tcp://127.0.0.1:18895");
    assert (rc >= 0);

    usleep (200000);

    rc = mb_bind (s1, "tcp://127.0.0.1:18895");
    assert (rc >= 0);

    usleep (300000);

    rc = mb_send (s1, "RECONNECT", 9, 0);
    assert (rc == 9);

    usleep (200000);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 9);
    assert (memcmp (buf, "RECONNECT", 9) == 0);

    mb_close (s1);
    mb_close (s2);

    printf ("  test_reconnect_auto: PASSED\n");
}

static void test_reconnect_disabled (void)
{
    int s;
    int rc;
    int ivl = 0;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl, sizeof (ivl));

    rc = mb_connect (s, "tcp://127.0.0.1:18896");
    assert (rc < 0);

    mb_close (s);

    printf ("  test_reconnect_disabled: PASSED\n");
}

static void test_reconnect_backoff_option (void)
{
    int s;
    int ivl = 50;
    int ivl_max = 1000;
    int val;
    size_t vallen;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl, sizeof (ivl));
    mb_setsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL_MAX, &ivl_max,
        sizeof (ivl_max));

    vallen = sizeof (val);
    mb_getsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL, &val, &vallen);
    assert (val == 50);

    vallen = sizeof (val);
    mb_getsockopt (s, MB_SOL_SOCKET, MB_RECONNECT_IVL_MAX, &val, &vallen);
    assert (val == 1000);

    mb_close (s);

    printf ("  test_reconnect_backoff_option: PASSED\n");
}

/* Doubling near INT_MAX must clamp, never overflow to a negative sleep. */
static void test_reconnect_backoff_no_overflow (void)
{
    int ivl;
    int i;

    assert (mb_reconnect_next_ivl (100, 1000) == 200);
    assert (mb_reconnect_next_ivl (800, 1000) == 1000);
    assert (mb_reconnect_next_ivl (1000, 1000) == 1000);
    assert (mb_reconnect_next_ivl (50, 0) == 50);

    ivl = INT_MAX / 2 + 1;
    assert (mb_reconnect_next_ivl (ivl, INT_MAX) == INT_MAX);

    ivl = 1000;
    for (i = 0; i < 40; ++i)
        ivl = mb_reconnect_next_ivl (ivl, INT_MAX);
    assert (ivl == INT_MAX);
    assert (ivl > 0);

    printf ("  test_reconnect_backoff_no_overflow: PASSED\n");
}

int main (void)
{
    printf ("Reconnect Tests:\n");

    test_reconnect_disabled ();
    test_reconnect_backoff_option ();
    test_reconnect_backoff_no_overflow ();
    test_reconnect_auto ();

    printf ("\nAll reconnect tests PASSED\n");
    return 0;
}
