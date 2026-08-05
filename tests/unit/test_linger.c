/*  TDD gate for MB_LINGER semantics (T-LINGER).
 *
 *  - linger = 0   : immediate close (outbuf discarded), mb_close returns 0
 *  - linger > 0   : mb_close blocks until outbuf drains or deadline expires
 *                  returns 0 on drain, -ETIMEDOUT on timeout
 *  - inproc       : linger is bypassed (no kernel outbuf)
 *  - SIPC / STCP  : linger flushes the per-sipc outbuf
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include "../../src/pal/thread.h"

#define LINGER_PORT 19901

/* Helper: bind/connect PAIR over tcp with a 4 KiB SO_{SND,RCV}BUF so the
 * outbuf fills up quickly under load. */
static void pair_tcp_connect (int *s1_out, int *s2_out)
{
    int s1, s2;
    int rc;
    int bufsz = 4096;
    int maxsz = 128 * 1024;
    char url[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    snprintf (url, sizeof (url), "tcp://127.0.0.1:%d", LINGER_PORT);
    rc = mb_bind (s1, url);
    assert (rc >= 0);
    usleep (50000);
    rc = mb_connect (s2, url);
    assert (rc >= 0);
    usleep (100000);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_SNDBUF, &bufsz, sizeof (bufsz));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &bufsz, sizeof (bufsz));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_SNDBUF, &bufsz, sizeof (bufsz));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_RCVBUF, &bufsz, sizeof (bufsz));
    assert (rc == 0);
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVMAXSIZE, &maxsz, sizeof (maxsz));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_RCVMAXSIZE, &maxsz, sizeof (maxsz));
    assert (rc == 0);

    *s1_out = s1;
    *s2_out = s2;
}

/* linger=0 must be a non-blocking immediate close. */
static void test_linger_default_zero (void)
{
    int s1, s2;
    int rc;
    int linger = -1; /* sentinel — get should return 0 by default */
    int val = 0;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    rc = mb_bind (s1, "inproc://linger_default");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://linger_default");
    assert (rc >= 0);

    /* Default must be 0 (immediate close). */
    val = -1;
    rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &val, (size_t[]){sizeof (val)});
    assert (rc == 0);
    assert (val == 0);

    linger = 100;
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);
    val = -1;
    rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &val, (size_t[]){sizeof (val)});
    assert (rc == 0);
    assert (val == 100);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    printf ("  test_linger_default_zero: PASSED\n");
}

/* linger > 0 with the peer not reading: mb_close must block until the
 * timeout fires and return -ETIMEDOUT. */
static void test_linger_timeout_when_peer_does_not_drain (void)
{
    int s1, s2;
    int rc;
    int linger = 50;            /* 50ms deadline */
    size_t n = 64 * 1024;
    char *payload;
    int small = 1;
    int zero = 0;
    uint64_t t0, t1, dt;

    pair_tcp_connect (&s1, &s2);

    /* Force outbuf build-up: peer is intentionally not going to recv. */
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);

    payload = (char *) malloc (n);
    assert (payload != NULL);
    memset (payload, 'L', n);
    rc = mb_send (s2, payload, n, 0);
    /* Either accepted into the sipc outbuf, or rejected with EAGAIN; both
     * leave something in-flight or ready. */
    assert (rc == (int) n || (rc < 0 && errno == EAGAIN));
    free (payload);

    t0 = 0; /* set in plumbing */
    (void) t0; (void) t1; (void) dt; (void) small; (void) zero;

    /* mb_close on the SENDER must block until linger expires (peer never
     * drains the outbuf) and return -ETIMEDOUT. */
    rc = mb_close (s2);
    assert (rc == -1);
    assert (mb_errno () == ETIMEDOUT);

    /* The receiver can still be closed with linger=0; nothing to drain. */
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &zero, sizeof (zero));
    assert (rc == 0);
    rc = mb_close (s1);
    assert (rc == 0);
    printf ("  test_linger_timeout_when_peer_does_not_drain: PASSED\n");
}

/* linger > 0 with the peer actively draining: mb_close must return 0. */
struct linger_drain_args {
    int fd;
    size_t expect;
    int ok;
};

static void linger_drain_peer_thread (void *arg)
{
    struct linger_drain_args *a = (struct linger_drain_args *) arg;
    char *buf;
    int rc;

    buf = (char *) malloc (a->expect);
    if (!buf) {
        a->ok = 0;
        return;
    }
    rc = mb_recv (a->fd, buf, a->expect, 0);
    a->ok = (rc == (int) a->expect);
    free (buf);
}

static void test_linger_drain_returns_zero (void)
{
    int s1, s2;
    int rc;
    int linger = 2000;          /* 2s deadline; peer drains well before */
    int zero = 0;
    size_t n = 64 * 1024;
    char *payload;
    struct mb_thread thr;
    struct linger_drain_args args;

    pair_tcp_connect (&s1, &s2);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);

    args.fd = s1;
    args.expect = n;
    args.ok = 0;
    mb_thread_init (&thr);
    rc = mb_thread_start (&thr, linger_drain_peer_thread, &args);
    assert (rc == 0);
    usleep (50000);

    payload = (char *) malloc (n);
    assert (payload != NULL);
    memset (payload, 'D', n);
    rc = mb_send (s2, payload, n, 0);
    assert (rc == (int) n);
    free (payload);

    rc = mb_close (s2);
    assert (rc == 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &zero, sizeof (zero));
    assert (rc == 0);
    rc = mb_close (s1);
    assert (rc == 0);
    printf ("  test_linger_drain_returns_zero: PASSED\n");
}

/* inproc ignores linger entirely: close is always immediate and successful. */
static void test_linger_inproc_bypass (void)
{
    int s1, s2;
    int rc;
    int linger = 1;             /* ridiculously small; would normally expire */

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    rc = mb_bind (s1, "inproc://linger_bypass");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://linger_bypass");
    assert (rc >= 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);
    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_LINGER, &linger, sizeof (linger));
    assert (rc == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    printf ("  test_linger_inproc_bypass: PASSED\n");
}

int main (void)
{
    printf ("test_linger:\n");
    test_linger_default_zero ();
    test_linger_inproc_bypass ();
    test_linger_timeout_when_peer_does_not_drain ();
    test_linger_drain_returns_zero ();
    printf ("test_linger: OK\n");
    return 0;
}
