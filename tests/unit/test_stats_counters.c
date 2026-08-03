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
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

/*  T-STATS: per-socket traffic counters exposed via mb_get_statistic.

    Validates:
      - MB_STAT_BYTES_SENT and MB_STAT_BYTES_RECEIVED advance by the
        number of payload bytes the user successfully transferred
        (assert counter reads >= N after sending N bytes).
      - MB_STAT_MSGS_SENT and MB_STAT_MSGS_RECEIVED advance by one per
        successful send/recv.
      - MB_STAT_QUEUE_FULL is incremented exactly when a send returns
        -EAGAIN because the connected HWM is saturated; it parallels
        MB_STAT_DROPPED in that scenario and starts at 0 on a fresh
        socket. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static uint64_t get_stat_via_get (int s, int stat)
{
    return mb_get_statistic (s, stat);
}

static void test_counters_initial_zero (void)
{
    int s;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    assert (get_stat_via_get (s, MB_STAT_MSGS_SENT) == 0);
    assert (get_stat_via_get (s, MB_STAT_MSGS_RECEIVED) == 0);
    assert (get_stat_via_get (s, MB_STAT_BYTES_SENT) == 0);
    assert (get_stat_via_get (s, MB_STAT_BYTES_RECEIVED) == 0);
    assert (get_stat_via_get (s, MB_STAT_QUEUE_FULL) == 0);

    mb_close (s);

    printf ("  test_counters_initial_zero: PASSED\n");
}

static void test_bytes_counters_match_sent_bytes (void)
{
    int s1, s2;
    int rc;
    const size_t N = 1024;
    char buf[1024];
    char recv_buf[1024];
    uint64_t sent_before;
    uint64_t sent_after;
    uint64_t rcvd_before;
    uint64_t rcvd_after;
    uint64_t msgs_s_before;
    uint64_t msgs_s_after;
    uint64_t msgs_r_before;
    uint64_t msgs_r_after;
    int i;

    memset (buf, 'a', sizeof (buf));

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://stats_counters_bytes");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://stats_counters_bytes");
    assert (rc >= 0);
    usleep (50000);

    sent_before = get_stat_via_get (s1, MB_STAT_BYTES_SENT);
    rcvd_before = get_stat_via_get (s2, MB_STAT_BYTES_RECEIVED);
    msgs_s_before = get_stat_via_get (s1, MB_STAT_MSGS_SENT);
    msgs_r_before = get_stat_via_get (s2, MB_STAT_MSGS_RECEIVED);

    /* Send three messages totalling exactly 3*N bytes. */
    for (i = 0; i < 3; i++) {
        rc = mb_send (s1, buf, N, 0);
        assert (rc == (int) N);
    }

    for (i = 0; i < 3; i++) {
        rc = mb_recv (s2, recv_buf, sizeof (recv_buf), 0);
        assert (rc == (int) N);
    }

    sent_after = get_stat_via_get (s1, MB_STAT_BYTES_SENT);
    rcvd_after = get_stat_via_get (s2, MB_STAT_BYTES_RECEIVED);
    msgs_s_after = get_stat_via_get (s1, MB_STAT_MSGS_SENT);
    msgs_r_after = get_stat_via_get (s2, MB_STAT_MSGS_RECEIVED);

    /* Per the spec: counter reads >= N after sending N bytes. With three
     * sends the deltas total 3*N and 3 messages per direction. */
    assert (sent_after - sent_before >= 3 * (uint64_t) N);
    assert (rcvd_after - rcvd_before >= 3 * (uint64_t) N);
    assert (msgs_s_after - msgs_s_before >= 3);
    assert (msgs_r_after - msgs_r_before >= 3);

    mb_close (s2);
    mb_close (s1);

    printf ("  test_bytes_counters_match_sent_bytes: PASSED\n");
}

static void test_stat_via_getsockopt_matches (void)
{
    int s1, s2;
    int rc;
    char buf[256];
    uint64_t v_opt;
    uint64_t v_get;
    size_t sz;

    memset (buf, 'b', sizeof (buf));

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://stats_counters_opt");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://stats_counters_opt");
    assert (rc >= 0);
    usleep (50000);

    rc = mb_send (s1, buf, sizeof (buf), 0);
    assert (rc == (int) sizeof (buf));

    v_get = mb_get_statistic (s1, MB_STAT_MSGS_SENT);
    assert (v_get >= 1);

    v_opt = 0;
    sz = sizeof (v_opt);
    rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_STAT_MSGS_SENT, &v_opt, &sz);
    assert (rc == 0);
    assert (sz == sizeof (uint64_t));
    assert (v_opt == v_get);

    mb_close (s2);
    mb_close (s1);

    printf ("  test_stat_via_getsockopt_matches: PASSED\n");
}

static void test_queue_full_increments_on_hwm_reject (void)
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
    uint64_t qf_before;
    uint64_t qf_after;
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
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_LINGER, &linger,
        sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVTIMEO, &rcvtimeo,
        sizeof (rcvtimeo));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &linger,
        sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_HWM, &hwm, sizeof (hwm));
    assert (rc == 0);

    rc = mb_bind (s1, "ipc:///tmp/mb_test_stats_queue_full");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "ipc:///tmp/mb_test_stats_queue_full");
    assert (rc >= 0);
    usleep (100000);

    qf_before = get_stat_via_get (s2, MB_STAT_QUEUE_FULL);
    dropped_before = get_stat_via_get (s2, MB_STAT_DROPPED);

    /* Flood DONTWAIT sends. The 1st fits; subsequent ones hit the
     * in-flight HWM and must produce EAGAIN. */
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

    qf_after = get_stat_via_get (s2, MB_STAT_QUEUE_FULL);
    dropped_after = get_stat_via_get (s2, MB_STAT_DROPPED);
    assert (qf_after - qf_before == (uint64_t) n_eagain);
    assert (dropped_after - dropped_before == (uint64_t) n_eagain);

    /* Drain the peer and let the wire settle so the test cleans up. */
    for (i = 0; i < 100; i++) {
        rc = mb_recv (s1, drain, sizeof (drain), 0);
        if (rc > 0)
            drained++;
        else
            break;
    }
    (void) drained;
    usleep (100000);

    mb_close (s1);
    mb_close (s2);
    unlink ("/tmp/mb_test_stats_queue_full");

    printf ("  test_queue_full_increments_on_hwm_reject: PASSED\n");
}

int main (void)
{
    printf ("test_stats_counters:\n");

    test_counters_initial_zero ();
    test_bytes_counters_match_sent_bytes ();
    test_stat_via_getsockopt_matches ();
    test_queue_full_increments_on_hwm_reject ();

    printf ("test_stats_counters: all tests passed\n");
    return 0;
}
