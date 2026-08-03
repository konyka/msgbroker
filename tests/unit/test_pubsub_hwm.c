/*
    msgbroker -- High-performance messaging library in pure C.

    Copyright 2024 msgbroker contributors. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

/*  T-BACKP1: HWM (high-water mark) enforcement and MB_STAT_DROPPED.

    Validates:
      - default hwm == 0 keeps today's unbounded behaviour.
      - setsockopt/getsockopt for MB_HWM at MB_SOL_SOCKET level; negative
        hwm is rejected.
      - MB_STAT_DROPPED counter is exposed, starts at 0 and remains 0 when
        no drop occurs.
      - hwm > 0 on a saturated pipe produces -EAGAIN DONTWAIT sends and
        increments MB_STAT_DROPPED by exactly the number of rejected sends.
      - draining the pipe and re-trying lets the next DONTWAIT send succeed
        (the in_flight count decrements as the wire flushes). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static uint64_t get_stat (int s, int stat)
{
    uint64_t v = 0;
    size_t sz = sizeof (v);
    int rc = mb_getsockopt (s, MB_SOL_SOCKET, stat, &v, &sz);
    assert (rc == 0);
    return v;
}

static void test_hwm_default_unbounded (void)
{
    int s1, s2;
    int rc;
    int i;
    int hwm = -1;
    size_t hwm_sz = sizeof (hwm);
    int sent = 0;
    int small = 1024;
    int timeout = 50;
    char buf[800];

    memset (buf, 'x', sizeof (buf));

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "ipc:///tmp/mb_test_hwm_default");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "ipc:///tmp/mb_test_hwm_default");
    assert (rc >= 0);
    usleep (100000);

    rc = mb_getsockopt (s2, MB_SOL_SOCKET, MB_HWM, &hwm, &hwm_sz);
    assert (rc == 0);
    assert (hwm == 0);

    /* Peer never reads: with hwm == 0 the legacy unbounded behaviour holds
     * and blocking sends keep succeeding until sndtimeo elapses, not
     * drop. Use a small sndbuf + sndtimeo so the loop terminates in
     * bounded time. */
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_SNDBUF, &small, sizeof (small));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_SNDTIMEO, &timeout,
        sizeof (timeout));
    assert (rc == 0);
    {
        int linger = 0;
        rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_LINGER, &linger,
            sizeof (linger));
        assert (rc == 0);
    }

    for (i = 0; i < 32; i++) {
        rc = mb_send (s2, buf, sizeof (buf), 0);
        if (rc == (int) sizeof (buf))
            sent++;
        else
            break;
    }
    assert (sent >= 1);

    /* Without HWM, no drop has been recorded. */
    assert (get_stat (s2, MB_STAT_DROPPED) == 0);

    mb_close (s1);
    mb_close (s2);
    unlink ("/tmp/mb_test_hwm_default");

    printf ("  test_hwm_default_unbounded: PASSED\n");
}

/* Wait for the peer to drain and the wire to free a slot. The pipe lets
 * the next DONTWAIT send succeed only once the previous outbuf fully
 * lands in the kernel. Bounded retry keeps the unit test deterministic. */
static int wait_pipe_ready (int s, int attempts)
{
    int i;
    int rc;
    char buf[16];

    for (i = 0; i < attempts; i++) {
        rc = mb_send (s, "x", 1, MB_DONTWAIT);
        if (rc == 1)
            return 1;
        usleep (20000);
    }
    (void) buf;
    return 0;
}

static void test_hwm_rejects_and_counts (void)
{
    int s1, s2;
    int rc;
    int hwm = 1;
    int small = 1024;
    int timeout = 100;
    int rcvtimeo = 200;
    int linger = 0;
    char buf[800];
    int n_eagain = 0;
    int n_accepted = 0;
    int i;
    char drain[2048];
    int drained = 0;
    uint64_t dropped_before;
    uint64_t dropped_after;

    memset (buf, 'x', sizeof (buf));

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_SNDBUF, &small, sizeof (small));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_SNDTIMEO, &timeout,
        sizeof (timeout));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVTIMEO, &rcvtimeo,
        sizeof (rcvtimeo));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_HWM, &hwm, sizeof (hwm));
    assert (rc == 0);

    rc = mb_bind (s1, "ipc:///tmp/mb_test_hwm_enforce");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "ipc:///tmp/mb_test_hwm_enforce");
    assert (rc >= 0);
    usleep (100000);

    dropped_before = get_stat (s2, MB_STAT_DROPPED);

    /* Flood DONTWAIT sends. The 1st fits, subsequent ones hit the wire
     * limit and must be rejected. */
    for (i = 0; i < 64; i++) {
        rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
        if (rc == (int) sizeof (buf))
            n_accepted++;
        else if (rc == -1 && errno == EAGAIN)
            n_eagain++;
        else
            break;
    }
    assert (n_accepted >= 1);
    assert (n_eagain >= 1);

    dropped_after = get_stat (s2, MB_STAT_DROPPED);
    assert (dropped_after - dropped_before == (uint64_t) n_eagain);

    /* Drain the peer (bounded by rcvtimeo) and let the wire settle. */
    for (i = 0; i < 100; i++) {
        rc = mb_recv (s1, drain, sizeof (drain), 0);
        if (rc > 0)
            drained++;
        else
            break;
    }
    assert (drained >= 1);
    usleep (100000);

    /* The next DONTWAIT send must eventually succeed once the previous
     * outbuf lands in the kernel. */
    int recovered = wait_pipe_ready (s2, 25);
    assert (recovered == 1);

    mb_close (s1);
    mb_close (s2);
    unlink ("/tmp/mb_test_hwm_enforce");

    printf ("  test_hwm_rejects_and_counts: PASSED\n");
}

static void test_hwm_getsockopt_roundtrip (void)
{
    int s;
    int hwm_in = 7;
    int hwm_out = -1;
    size_t sz = sizeof (hwm_out);
    int rc;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_HWM, &hwm_in, sizeof (hwm_in));
    assert (rc == 0);

    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_HWM, &hwm_out, &sz);
    assert (rc == 0);
    assert (sz == sizeof (hwm_out));
    assert (hwm_out == hwm_in);

    hwm_in = -1;
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_HWM, &hwm_in, sizeof (hwm_in));
    assert (rc < 0 && rc != -EAGAIN);

    mb_close (s);

    printf ("  test_hwm_getsockopt_roundtrip: PASSED\n");
}

static void test_stat_dropped_initial_zero (void)
{
    int s;
    uint64_t v;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);
    v = get_stat (s, MB_STAT_DROPPED);
    assert (v == 0);
    mb_close (s);

    printf ("  test_stat_dropped_initial_zero: PASSED\n");
}

int main (void)
{
    printf ("test_pubsub_hwm:\n");
    test_hwm_default_unbounded ();
    test_hwm_rejects_and_counts ();
    test_hwm_getsockopt_roundtrip ();
    test_stat_dropped_initial_zero ();
    printf ("test_pubsub_hwm: all tests passed\n");
    return 0;
}
