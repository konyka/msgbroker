#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_reqrep.h>
#include <msgbroker/mb_bus.h>
#include <msgbroker/mb_survey.h>

#include "../../src/pal/thread.h"
#include "../../src/pal/clock.h"

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

static void test_tcp_send_oversized (void)
{
    int s1, s2;
    int rc;
    char *buf;
    size_t n = 1024 * 1024 + 1;
    char rbuf[16];

    buf = (char *) malloc (n);
    assert (buf != NULL);
    memset (buf, 'x', n);

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18890");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "tcp://127.0.0.1:18890");
    assert (rc >= 0);
    usleep (100000);

    rc = mb_send (s2, buf, n, 0);
    assert (rc == -1);
    assert (mb_errno () == EMSGSIZE);

    rc = mb_send (s2, "ok", 2, 0);
    assert (rc == 2);
    usleep (100000);
    rc = mb_recv (s1, rbuf, sizeof (rbuf), 0);
    assert (rc == 2);
    assert (memcmp (rbuf, "ok", 2) == 0);

    free (buf);
    mb_close (s1);
    mb_close (s2);
    printf ("  test_tcp_send_oversized: PASSED\n");
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

struct tcp_poll_wake_args {
    int fd;
    int ok;
    uint64_t elapsed_ms;
};

static void tcp_poll_wake_thread (void *arg)
{
    struct tcp_poll_wake_args *a = (struct tcp_poll_wake_args *) arg;
    struct mb_pollfd fds[1];
    uint64_t t0;
    int rc;

    memset (fds, 0, sizeof (fds));
    fds[0].fd = a->fd;
    fds[0].events = MB_POLLIN;
    t0 = mb_clock_ms ();
    rc = mb_poll (fds, 1, 3000);
    a->elapsed_ms = mb_clock_ms () - t0;
    a->ok = (rc >= 1 && (fds[0].revents & MB_POLLIN));
}

/*  Blocking mb_poll must wake soon after TCP data arrives mid-wait. */
static void test_tcp_poll_wake (void)
{
    int s1, s2;
    int rc;
    char buf[64];
    struct mb_thread thr;
    struct tcp_poll_wake_args args;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18892");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "tcp://127.0.0.1:18892");
    assert (rc >= 0);
    usleep (100000);

    args.fd = s1;
    args.ok = 0;
    args.elapsed_ms = 0;
    mb_thread_init (&thr);
    rc = mb_thread_start (&thr, tcp_poll_wake_thread, &args);
    assert (rc == 0);

    /* Enter the poller's blocking wait before data is sent. */
    usleep (200000);

    rc = mb_send (s2, "WAKE", 4, 0);
    assert (rc == 4);

    mb_thread_join (&thr);
    mb_thread_term (&thr);

    assert (args.ok);
    /* Slice re-sync (~50ms) must beat the full 3000ms timeout. */
    assert (args.elapsed_ms < 800);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "WAKE", 4) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_tcp_poll_wake: PASSED\n");
}

/*  mb_poll(POLLIN) must see TCP data without a prior mb_recv. */
static void test_tcp_poll_polllin (void)
{
    int s1, s2;
    int rc;
    char buf[64];
    struct mb_pollfd fds[1];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18891");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "tcp://127.0.0.1:18891");
    assert (rc >= 0);
    usleep (100000);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = s1;
    fds[0].events = MB_POLLIN;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLIN));

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    /* Allow the segment to land in the peer kernel buffer. */
    usleep (50000);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 100);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLIN);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    fds[0].revents = 0;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLIN));

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_tcp_poll_polllin: PASSED\n");
}

/*  PAIR POLLOUT must clear under TCP backpressure and return after drain. */
static void test_pair_poll_polout_after_backpressure (void)
{
    int s1, s2;
    int rc;
    int i;
    int hit_eagain = 0;
    char buf[65536];
    char rbuf[65536];
    struct mb_pollfd fds[1];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18893");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, "tcp://127.0.0.1:18893");
    assert (rc >= 0);
    usleep (100000);

    memset (buf, 'Y', sizeof (buf));
    for (i = 0; i < 512; ++i) {
        rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 0) {
            assert (mb_errno () == EAGAIN);
            hit_eagain = 1;
            break;
        }
    }
    assert (hit_eagain);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = s2;
    fds[0].events = MB_POLLOUT;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    {
        int woke = 0;
        for (i = 0; i < 100; ++i) {
            while (mb_recv (s1, rbuf, sizeof (rbuf), MB_DONTWAIT) > 0)
                ;
            fds[0].revents = 0;
            rc = mb_poll (fds, 1, 50);
            if (rc >= 1 && (fds[0].revents & MB_POLLOUT)) {
                woke = 1;
                break;
            }
        }
        assert (woke);
    }

    rc = mb_send (s2, "OK", 2, MB_DONTWAIT);
    assert (rc == 2);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_pair_poll_polout_after_backpressure: PASSED\n");
}

/*  BUS POLLOUT must clear under TCP backpressure and return after drain. */
static void test_bus_poll_polout_after_backpressure (void)
{
    int a, b;
    int rc;
    int i;
    int hit_eagain = 0;
    char buf[65536];
    char rbuf[65536];
    struct mb_pollfd fds[1];

    a = mb_socket (AF_MB, MB_BUS);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_BUS);
    assert (b >= 0);

    rc = mb_bind (a, "tcp://127.0.0.1:18894");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (b, "tcp://127.0.0.1:18894");
    assert (rc >= 0);
    usleep (100000);

    memset (buf, 'Z', sizeof (buf));
    for (i = 0; i < 512; ++i) {
        rc = mb_send (b, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 0) {
            assert (mb_errno () == EAGAIN);
            hit_eagain = 1;
            break;
        }
    }
    assert (hit_eagain);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = b;
    fds[0].events = MB_POLLOUT;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    {
        int woke = 0;
        for (i = 0; i < 100; ++i) {
            while (mb_recv (a, rbuf, sizeof (rbuf), MB_DONTWAIT) > 0)
                ;
            fds[0].revents = 0;
            rc = mb_poll (fds, 1, 50);
            if (rc >= 1 && (fds[0].revents & MB_POLLOUT)) {
                woke = 1;
                break;
            }
        }
        assert (woke);
    }

    rc = mb_send (b, "OK", 2, MB_DONTWAIT);
    assert (rc == 2);

    rc = mb_close (a);
    assert (rc == 0);
    rc = mb_close (b);
    assert (rc == 0);

    printf ("  test_bus_poll_polout_after_backpressure: PASSED\n");
}

/*  XSURVEYOR POLLOUT must clear under TCP backpressure and return after drain. */
static void test_xsurveyor_poll_polout_after_backpressure (void)
{
    int sv, rs;
    int rc;
    int i;
    int hit_eagain = 0;
    char buf[65536];
    char rbuf[65536];
    struct mb_pollfd fds[1];

    sv = mb_socket (AF_MB, MB_XSURVEYOR);
    assert (sv >= 0);
    rs = mb_socket (AF_MB, MB_XRESPONDENT);
    assert (rs >= 0);

    rc = mb_bind (sv, "tcp://127.0.0.1:18895");
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (rs, "tcp://127.0.0.1:18895");
    assert (rc >= 0);
    usleep (100000);

    memset (buf, 'S', sizeof (buf));
    for (i = 0; i < 512; ++i) {
        rc = mb_send (sv, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 0) {
            assert (mb_errno () == EAGAIN);
            hit_eagain = 1;
            break;
        }
    }
    assert (hit_eagain);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = sv;
    fds[0].events = MB_POLLOUT;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    {
        int woke = 0;
        for (i = 0; i < 100; ++i) {
            while (mb_recv (rs, rbuf, sizeof (rbuf), MB_DONTWAIT) > 0)
                ;
            fds[0].revents = 0;
            rc = mb_poll (fds, 1, 50);
            if (rc >= 1 && (fds[0].revents & MB_POLLOUT)) {
                woke = 1;
                break;
            }
        }
        assert (woke);
    }

    rc = mb_send (sv, "OK", 2, MB_DONTWAIT);
    assert (rc == 2);

    rc = mb_close (sv);
    assert (rc == 0);
    rc = mb_close (rs);
    assert (rc == 0);

    printf ("  test_xsurveyor_poll_polout_after_backpressure: PASSED\n");
}

int main (void)
{
    printf ("test_tcp:\n");
    test_tcp_bind_connect ();
    test_tcp_large_message ();
    test_tcp_send_oversized ();
    test_tcp_connect_refused ();
    test_tcp_cross_transport ();
    test_tcp_poll_polllin ();
    test_tcp_poll_wake ();
    test_pair_poll_polout_after_backpressure ();
    test_bus_poll_polout_after_backpressure ();
    test_xsurveyor_poll_polout_after_backpressure ();
    printf ("test_tcp: ALL PASSED\n");
    return 0;
}
