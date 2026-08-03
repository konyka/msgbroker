/*  T-TIMEOUT: public MB_SNDTIMEO and MB_RCVTIMEO socket options.
 *
 *  Validates that the SNDTIMEO/RCVTIMEO round-trip through setsockopt/getsockopt
 *  and that blocking mb_send/mb_recv honour the values: when the call would block
 *  longer than the configured timeout, the call returns -1 with errno=ETIMEDOUT.
 *
 *  Refs: T-TIMEOUT
 */
#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static long elapsed_ms (struct timespec t0, struct timespec t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000L +
        (t1.tv_nsec - t0.tv_nsec) / 1000000L;
}

/* Sender-only socket, RCVTIMEO=50ms, no receiver: mb_recv must time out. */
static void test_sender_only_rcvtimeo (void)
{
    int s;
    int val;
    size_t len;
    int rc;
    char buf[16];
    struct timespec t0, t1;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = 50;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, sizeof (val));
    assert (rc == 0);

    val = 0;
    len = sizeof (val);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, &len);
    assert (rc == 0);
    assert (len == sizeof (val));
    assert (val == 50);

    clock_gettime (CLOCK_MONOTONIC, &t0);
    rc = mb_recv (s, buf, sizeof (buf), 0);
    clock_gettime (CLOCK_MONOTONIC, &t1);
    assert (rc == -1);
    assert (mb_errno () == ETIMEDOUT);
    /* Polling sleeps 1ms per tick, so 50ms timeout returns well under 500ms. */
    assert (elapsed_ms (t0, t1) < 500);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  sender_only_rcvtimeo: PASSED\n");
}

/* Sender-only socket, SNDTIMEO=50ms, no receiver: mb_send must time out. */
static void test_sender_only_sndtimeo (void)
{
    int s;
    int val;
    size_t len;
    int rc;
    struct timespec t0, t1;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = 50;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &val, sizeof (val));
    assert (rc == 0);

    val = 0;
    len = sizeof (val);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &val, &len);
    assert (rc == 0);
    assert (val == 50);

    clock_gettime (CLOCK_MONOTONIC, &t0);
    rc = mb_send (s, "X", 1, 0);
    clock_gettime (CLOCK_MONOTONIC, &t1);
    assert (rc == -1);
    assert (mb_errno () == ETIMEDOUT);
    assert (elapsed_ms (t0, t1) < 500);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  sender_only_sndtimeo: PASSED\n");
}

/* Default sentinel (-1) means forever. */
static void test_default_forever (void)
{
    int s;
    int rc;
    int val;
    size_t len;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = -42;
    len = sizeof (val);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, &len);
    assert (rc == 0);
    assert (val == -1);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  default_forever: PASSED\n");
}

/* Negative values accepted: "forever" semantics, no EINVAL. */
static void test_negative_is_forever (void)
{
    int s;
    int val;
    int rc;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    val = -1;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDTIMEO, &val, sizeof (val));
    assert (rc == 0);

    val = -100;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVTIMEO, &val, sizeof (val));
    assert (rc == 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  negative_is_forever: PASSED\n");
}

int main (void)
{
    printf ("test_send_recv_timeout:\n");
    test_sender_only_rcvtimeo ();
    test_sender_only_sndtimeo ();
    test_default_forever ();
    test_negative_is_forever ();
    printf ("test_send_recv_timeout: PASSED\n");
    return 0;
}