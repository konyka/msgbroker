#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include "../../src/pal/clock.h"
#include "../../src/pal/thread.h"

struct device_args {
    int s1;
    int s2;
};

static void device_thread (void *arg)
{
    struct device_args *a = (struct device_args *) arg;
    (void) mb_device (a->s1, a->s2);
}

static void test_device_forward (void)
{
    int left, right, c, s;
    int rc;
    char buf[64];
    struct mb_thread thr;
    struct device_args args;

    left = mb_socket (AF_MB, MB_PAIR);
    assert (left >= 0);
    right = mb_socket (AF_MB, MB_PAIR);
    assert (right >= 0);

    rc = mb_bind (left, "inproc://device_left");
    assert (rc >= 0);
    rc = mb_bind (right, "inproc://device_right");
    assert (rc >= 0);

    args.s1 = left;
    args.s2 = right;
    mb_thread_init (&thr);
    rc = mb_thread_start (&thr, device_thread, &args);
    assert (rc == 0);

    c = mb_socket (AF_MB, MB_PAIR);
    assert (c >= 0);
    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_connect (c, "inproc://device_left");
    assert (rc >= 0);
    rc = mb_connect (s, "inproc://device_right");
    assert (rc >= 0);

    usleep (50000);

    /* Idle device must keep running so a later message still forwards. */
    usleep (20000);

    rc = mb_send (c, "HELLO", 5, 0);
    assert (rc == 5);
    rc = mb_recv (s, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    rc = mb_send (s, "WORLD", 5, 0);
    assert (rc == 5);
    rc = mb_recv (c, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    mb_close (c);
    mb_close (s);
    mb_close (left);
    mb_close (right);
    mb_thread_join (&thr);
    mb_thread_term (&thr);

    printf ("  test_device_forward: PASSED\n");
}

/* Close must finish while device is stuck retrying send under EAGAIN. */
static void test_device_close_under_send_backpressure (void)
{
    int left, right, c;
    int rc;
    struct mb_thread thr;
    struct device_args args;

    left = mb_socket (AF_MB, MB_PAIR);
    assert (left >= 0);
    right = mb_socket (AF_MB, MB_PAIR);
    assert (right >= 0);

    rc = mb_bind (left, "inproc://device_bp_left");
    assert (rc >= 0);
    rc = mb_bind (right, "inproc://device_bp_right");
    assert (rc >= 0);

    args.s1 = left;
    args.s2 = right;
    mb_thread_init (&thr);
    rc = mb_thread_start (&thr, device_thread, &args);
    assert (rc == 0);

    c = mb_socket (AF_MB, MB_PAIR);
    assert (c >= 0);
    /* Only one side connected: device recv succeeds, send to right → EAGAIN. */
    rc = mb_connect (c, "inproc://device_bp_left");
    assert (rc >= 0);
    usleep (50000);

    rc = mb_send (c, "BLOCK", 5, 0);
    assert (rc == 5);
    /* Let the device thread enter the send-retry loop. */
    usleep (100000);

    alarm (3);
    mb_close (c);
    mb_close (left);
    mb_close (right);
    mb_thread_join (&thr);
    mb_thread_term (&thr);
    alarm (0);

    printf ("  test_device_close_under_send_backpressure: PASSED\n");
}

/*  T-DEVICE TDD gate: PAIR a<->b, send N from a-side, assert b receives N,
 *  then close a-side and verify the worker thread exits within 200ms. */
static void test_device_pair_n_msgs_then_close_exit (void)
{
    int a, b, ca, cb;
    int rc;
    int i;
    int n = 32;
    char buf[64];
    struct mb_thread thr;
    struct device_args args;
    uint64_t t0, elapsed_ms;

    a = mb_socket (AF_MB, MB_PAIR);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_PAIR);
    assert (b >= 0);

    rc = mb_bind (a, "inproc://td_n_left");
    assert (rc >= 0);
    rc = mb_bind (b, "inproc://td_n_right");
    assert (rc >= 0);

    args.s1 = a;
    args.s2 = b;
    mb_thread_init (&thr);
    rc = mb_thread_start (&thr, device_thread, &args);
    assert (rc == 0);

    ca = mb_socket (AF_MB, MB_PAIR);
    assert (ca >= 0);
    cb = mb_socket (AF_MB, MB_PAIR);
    assert (cb >= 0);

    rc = mb_connect (ca, "inproc://td_n_left");
    assert (rc >= 0);
    rc = mb_connect (cb, "inproc://td_n_right");
    assert (rc >= 0);

    usleep (50000);

    for (i = 0; i < n; i++) {
        rc = mb_send (ca, "X", 1, 0);
        assert (rc == 1);
    }
    for (i = 0; i < n; i++) {
        rc = mb_recv (cb, buf, sizeof (buf), 0);
        assert (rc == 1);
        assert (buf[0] == 'X');
    }

    t0 = mb_clock_ms ();
    rc = mb_close (a);
    elapsed_ms = mb_clock_ms () - t0;
    assert (rc == 0);
    assert (elapsed_ms < 200);

    mb_close (ca);
    mb_close (cb);
    mb_close (b);
    mb_thread_join (&thr);
    mb_thread_term (&thr);

    printf ("  test_device_pair_n_msgs_then_close_exit: PASSED (n=%d, exit_ms=%llu)\n",
        n, (unsigned long long) elapsed_ms);
}

int main (void)
{
    printf ("test_device:\n");
    test_device_forward ();
    test_device_close_under_send_backpressure ();
    test_device_pair_n_msgs_then_close_exit ();
    printf ("test_device: ALL PASSED\n");
    return 0;
}
