#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_pubsub.h>

#include "../../src/transport/inproc/ins.h"
#include "../../src/core/ep.h"

/*  Test basic inproc bind/connect and message passing between PAIR sockets. */
static void test_inproc_bind_connect (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    /*  Create two PAIR sockets. */
    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    /*  Bind s1 to inproc address. */
    rc = mb_bind (s1, "inproc://test_bind_connect");
    assert (rc >= 0);

    /*  Connect s2 to the same inproc address. */
    rc = mb_connect (s2, "inproc://test_bind_connect");
    assert (rc >= 0);

    /*  Send from s1, recv on s2. */
    rc = mb_send (s1, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    /*  Send from s2, recv on s1 (PAIR is bidirectional). */
    rc = mb_send (s2, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    /*  Close sockets. */
    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_bind_connect: PASSED\n");
}

/*  Test that binding to the same address twice fails. */
static void test_inproc_duplicate_bind (void)
{
    int s1, s2;
    int rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://test_dup");
    assert (rc >= 0);

    /*  Second bind to the same address should fail. */
    rc = mb_bind (s2, "inproc://test_dup");
    assert (rc < 0);
    assert (mb_errno () == EADDRINUSE);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_duplicate_bind: PASSED\n");
}

/*  Test connecting to a non-existent address (no bind yet). */
static void test_inproc_connect_no_bind (void)
{
    int s;
    int rc;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    /*  Connect to address that nobody has bound to — should succeed
        (the connection is just deferred/lost, not an error). */
    rc = mb_connect (s, "inproc://test_no_bind");
    assert (rc >= 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  test_inproc_connect_no_bind: PASSED\n");
}

/*  Test that close/unbind releases the address for reuse. */
static void test_inproc_address_reuse (void)
{
    int s1, s2;
    int rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);

    rc = mb_bind (s1, "inproc://test_reuse");
    assert (rc >= 0);

    rc = mb_close (s1);
    assert (rc == 0);

    /*  After close, the address should be available again. */
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s2, "inproc://test_reuse");
    assert (rc >= 0);

    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_address_reuse: PASSED\n");
}

/*  Test multiple inproc connections (different addresses). */
static void test_inproc_multiple_addresses (void)
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

    rc = mb_bind (s1, "inproc://test_multi_1");
    assert (rc >= 0);
    rc = mb_bind (s3, "inproc://test_multi_2");
    assert (rc >= 0);

    rc = mb_connect (s2, "inproc://test_multi_1");
    assert (rc >= 0);
    rc = mb_connect (s4, "inproc://test_multi_2");
    assert (rc >= 0);

    /*  Send on first pair. */
    rc = mb_send (s1, "AAA", 3, 0);
    assert (rc == 3);
    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "AAA", 3) == 0);

    /*  Send on second pair. */
    rc = mb_send (s3, "BBB", 3, 0);
    assert (rc == 3);
    rc = mb_recv (s4, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "BBB", 3) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    rc = mb_close (s3);
    assert (rc == 0);
    rc = mb_close (s4);
    assert (rc == 0);

    printf ("  test_inproc_multiple_addresses: PASSED\n");
}

/*  Test sending a larger message through inproc. */
static void test_inproc_large_message (void)
{
    int s1, s2;
    int rc;
    char sendbuf[4096];
    char recvbuf[4096];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://test_large");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_large");
    assert (rc >= 0);

    /*  Fill with pattern. */
    for (int i = 0; i < (int) sizeof (sendbuf); i++)
        sendbuf[i] = (char) (i & 0xFF);

    rc = mb_send (s1, sendbuf, sizeof (sendbuf), 0);
    assert (rc == (int) sizeof (sendbuf));

    rc = mb_recv (s2, recvbuf, sizeof (recvbuf), 0);
    assert (rc == (int) sizeof (recvbuf));
    assert (memcmp (sendbuf, recvbuf, sizeof (sendbuf)) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_large_message: PASSED\n");
}

static int ins_fail_cb (struct mb_ins_item *self, struct mb_ins_item *peer)
{
    (void) self;
    (void) peer;
    return -ENOMEM;
}

/* PAIR allows only one peer; second connect must fail with EISCONN. */
static void test_inproc_pair_second_connect (void)
{
    int a, b, c;
    int rc;

    a = mb_socket (AF_MB, MB_PAIR);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_PAIR);
    assert (b >= 0);
    c = mb_socket (AF_MB, MB_PAIR);
    assert (c >= 0);

    rc = mb_bind (a, "inproc://pair_second");
    assert (rc >= 0);
    rc = mb_connect (b, "inproc://pair_second");
    assert (rc >= 0);

    rc = mb_connect (c, "inproc://pair_second");
    assert (rc < 0);
    assert (mb_errno () == EISCONN);

    /* Failed handshake must not leave phantom ESTABLISHED/BROKEN stats. */
    assert (mb_get_statistic (c, MB_STAT_CURRENT_CONNECTIONS) == 0);
    assert (mb_get_statistic (c, MB_STAT_ESTABLISHED_CONNECTIONS) == 0);
    assert (mb_get_statistic (c, MB_STAT_BROKEN_CONNECTIONS) == 0);
    assert (mb_get_statistic (a, MB_STAT_CURRENT_CONNECTIONS) == 1);

    rc = mb_send (c, "x", 1, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    rc = mb_send (b, "ok", 2, 0);
    assert (rc == 2);

    mb_close (c);
    mb_close (b);
    mb_close (a);
    printf ("  test_inproc_pair_second_connect: PASSED\n");
}

/* Incompatible socktypes must not create an inproc pipe. */
static void test_inproc_proto_mismatch (void)
{
    int pub, pair;
    int rc;

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    pair = mb_socket (AF_MB, MB_PAIR);
    assert (pair >= 0);

    rc = mb_bind (pub, "inproc://proto_mismatch");
    assert (rc >= 0);

    rc = mb_connect (pair, "inproc://proto_mismatch");
    assert (rc < 0);
    assert (mb_errno () == EPROTONOSUPPORT);

    mb_close (pair);
    mb_close (pub);
    printf ("  test_inproc_proto_mismatch: PASSED\n");
}

/* Cooked↔raw peers must connect (Phase 143 bidirectional ispeer). */
static void test_inproc_cooked_raw_peers (void)
{
    int cooked, raw;
    int rc;
    char buf[32];

    cooked = mb_socket (AF_MB, MB_SUB);
    assert (cooked >= 0);
    raw = mb_socket (AF_MB, MB_XPUB);
    assert (raw >= 0);

    rc = mb_setsockopt (cooked, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    assert (rc == 0);

    rc = mb_bind (cooked, "inproc://cooked_raw_sub_xpub");
    assert (rc >= 0);
    rc = mb_connect (raw, "inproc://cooked_raw_sub_xpub");
    assert (rc >= 0);

    rc = mb_send (raw, "hi", 2, 0);
    assert (rc == 2);
    rc = mb_recv (cooked, buf, sizeof (buf), 0);
    assert (rc == 2);
    assert (memcmp (buf, "hi", 2) == 0);

    mb_close (raw);
    mb_close (cooked);

    cooked = mb_socket (AF_MB, MB_PUB);
    assert (cooked >= 0);
    raw = mb_socket (AF_MB, MB_XSUB);
    assert (raw >= 0);

    rc = mb_bind (cooked, "inproc://cooked_raw_pub_xsub");
    assert (rc >= 0);
    rc = mb_connect (raw, "inproc://cooked_raw_pub_xsub");
    assert (rc >= 0);

    rc = mb_send (cooked, "ok", 2, 0);
    assert (rc == 2);

    mb_close (raw);
    mb_close (cooked);

    printf ("  test_inproc_cooked_raw_peers: PASSED\n");
}

/* Compatible PUB/SUB still connects. */
static void test_inproc_pubsub_ok (void)
{
    int pub, sub;
    int rc;
    char buf[32];

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://pubsub_ok");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://pubsub_ok");
    assert (rc >= 0);

    rc = mb_send (pub, "hi", 2, 0);
    assert (rc == 2);
    rc = mb_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 2);
    assert (memcmp (buf, "hi", 2) == 0);

    mb_close (sub);
    mb_close (pub);
    printf ("  test_inproc_pubsub_ok: PASSED\n");
}

/* Peer bound + connect callback OOM must surface as -ENOMEM. */
static void test_inproc_ins_connect_oom (void)
{
    struct mb_ep bound_ep;
    struct mb_ep conn_ep;
    struct mb_ins_item bound;
    struct mb_ins_item conn;
    int rc;

    memset (&bound_ep, 0, sizeof (bound_ep));
    memset (&conn_ep, 0, sizeof (conn_ep));
    strcpy (bound_ep.addr, "inproc://test_ins_oom");
    strcpy (conn_ep.addr, "inproc://test_ins_oom");

    mb_ins_init ();
    mb_ins_item_init (&bound, &bound_ep);
    mb_ins_item_init (&conn, &conn_ep);

    rc = mb_ins_bind (&bound, NULL);
    assert (rc == 0);

    rc = mb_ins_connect (&conn, ins_fail_cb);
    assert (rc == -ENOMEM);

    mb_ins_unbind (&bound);
    mb_ins_item_term (&conn);
    mb_ins_item_term (&bound);

    printf ("  test_inproc_ins_connect_oom: PASSED\n");
}

/*  MB_RCVBUF must bound the inproc receive queue (backpressure). */
static void test_inproc_rcvbuf_backpressure (void)
{
    int s1, s2;
    int rc;
    int i;
    int hit_eagain = 0;
    int rcvbuf = 64;
    char buf[32];
    char rbuf[32];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf, sizeof (rcvbuf));
    assert (rc == 0);

    rc = mb_bind (s1, "inproc://test_rcvbuf");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_rcvbuf");
    assert (rc >= 0);

    memset (buf, 'B', sizeof (buf));
    for (i = 0; i < 16; ++i) {
        rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 0) {
            assert (mb_errno () == EAGAIN);
            hit_eagain = 1;
            break;
        }
        assert (rc == (int) sizeof (buf));
    }
    assert (hit_eagain);
    assert (i >= 1);

    rc = mb_recv (s1, rbuf, sizeof (rbuf), 0);
    assert (rc == (int) sizeof (rbuf));

    rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc == (int) sizeof (buf));

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_rcvbuf_backpressure: PASSED\n");
}

/*  MB_RCVBUF after connect must still apply to the inproc queue. */
static void test_inproc_rcvbuf_after_connect (void)
{
    int s1, s2;
    int rc;
    int i;
    int hit_eagain = 0;
    int rcvbuf = 64;
    char buf[32];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://test_rcvbuf_late");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_rcvbuf_late");
    assert (rc >= 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf, sizeof (rcvbuf));
    assert (rc == 0);

    memset (buf, 'L', sizeof (buf));
    for (i = 0; i < 16; ++i) {
        rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 0) {
            assert (mb_errno () == EAGAIN);
            hit_eagain = 1;
            break;
        }
        assert (rc == (int) sizeof (buf));
    }
    assert (hit_eagain);
    assert (i >= 1);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_rcvbuf_after_connect: PASSED\n");
}

/* Enlarging MB_RCVBUF must wake a peer blocked on a cached SNDFD. */
static void test_inproc_rcvbuf_enlarge_wakes_sndfd (void)
{
    int s1, s2;
    int rc;
    int i;
    int rcvbuf = 64;
    int raised = 4096;
    int sndfd = -1;
    size_t sz;
    char buf[32];
    struct pollfd pfd;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf, sizeof (rcvbuf));
    assert (rc == 0);

    rc = mb_bind (s1, "inproc://test_rcvbuf_wake");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_rcvbuf_wake");
    assert (rc >= 0);

    sz = sizeof (sndfd);
    rc = mb_getsockopt (s2, MB_SOL_SOCKET, MB_SNDFD, &sndfd, &sz);
    assert (rc == 0);
    assert (sndfd >= 0);

    memset (buf, 'W', sizeof (buf));
    for (i = 0; i < 16; ++i) {
        rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
        if (rc < 0) {
            assert (mb_errno () == EAGAIN);
            break;
        }
    }
    assert (i >= 1);
    assert (rc < 0);

    /* Receiver raises RCVBUF without reading — must signal peer sndfd. */
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &raised, sizeof (raised));
    assert (rc == 0);

    pfd.fd = sndfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    rc = poll (&pfd, 1, 500);
    assert (rc > 0);
    assert (pfd.revents & POLLIN);

    rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc == (int) sizeof (buf));

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    printf ("  test_inproc_rcvbuf_enlarge_wakes_sndfd: PASSED\n");
}

/* Shrinking MB_RCVBUF to a full queue must clear peer sticky SNDFD. */
static void test_inproc_rcvbuf_shrink_clears_sndfd (void)
{
    int s1, s2;
    int rc;
    int rcvbuf = 4096;
    int shrunk = 32;
    int sndfd = -1;
    size_t sz;
    char buf[32];
    struct pollfd pfd;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &rcvbuf, sizeof (rcvbuf));
    assert (rc == 0);

    rc = mb_bind (s1, "inproc://test_rcvbuf_shrink");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_rcvbuf_shrink");
    assert (rc >= 0);

    sz = sizeof (sndfd);
    rc = mb_getsockopt (s2, MB_SOL_SOCKET, MB_SNDFD, &sndfd, &sz);
    assert (rc == 0);
    assert (sndfd >= 0);

    memset (buf, 'S', sizeof (buf));
    rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc == (int) sizeof (buf));

    /* Queue holds 32 bytes — shrink RCVBUF to 32 so it is full. */
    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVBUF, &shrunk, sizeof (shrunk));
    assert (rc == 0);

    pfd.fd = sndfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    rc = poll (&pfd, 1, 0);
    assert (rc == 0);

    rc = mb_send (s2, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EAGAIN);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);
    printf ("  test_inproc_rcvbuf_shrink_clears_sndfd: PASSED\n");
}

/*  MB_RCVBUF/MB_SNDBUF <= 0 must be rejected (would disable inproc caps). */
static void test_inproc_buf_rejects_nonpositive (void)
{
    int s;
    int rc;
    int zero = 0;
    int neg = -1;
    int unlimited = -1;
    int got;
    size_t gotlen;

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVBUF, &zero, sizeof (zero));
    assert (rc == -1);
    assert (mb_errno () == EINVAL);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVBUF, &neg, sizeof (neg));
    assert (rc == -1);
    assert (mb_errno () == EINVAL);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDBUF, &zero, sizeof (zero));
    assert (rc == -1);
    assert (mb_errno () == EINVAL);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDBUF, &neg, sizeof (neg));
    assert (rc == -1);
    assert (mb_errno () == EINVAL);

    /* MB_RCVMAXSIZE: 0 is invalid; -1 means unlimited. */
    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVMAXSIZE, &zero, sizeof (zero));
    assert (rc == -1);
    assert (mb_errno () == EINVAL);

    rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_RCVMAXSIZE, &unlimited,
        sizeof (unlimited));
    assert (rc == 0);
    gotlen = sizeof (got);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_RCVMAXSIZE, &got, &gotlen);
    assert (rc == 0);
    assert (got == -1);

    rc = mb_close (s);
    assert (rc == 0);
    printf ("  test_inproc_buf_rejects_nonpositive: PASSED\n");
}

/*  MB_RCVMAXSIZE must reject oversized inproc sends (sender or receiver). */
static void test_inproc_rcvmaxsize (void)
{
    int s1, s2;
    int rc;
    int max = 1024;
    char small[512];
    char *big;
    char rbuf[512];

    /* Sender-side limit. */
    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s2, MB_SOL_SOCKET, MB_RCVMAXSIZE, &max, sizeof (max));
    assert (rc == 0);

    rc = mb_bind (s1, "inproc://test_rcvmax");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_rcvmax");
    assert (rc >= 0);

    big = (char *) malloc (2048);
    assert (big != NULL);
    memset (big, 'Z', 2048);
    rc = mb_send (s2, big, 2048, 0);
    assert (rc == -1);
    assert (mb_errno () == EMSGSIZE);

    memset (small, 's', sizeof (small));
    rc = mb_send (s2, small, sizeof (small), 0);
    assert (rc == (int) sizeof (small));
    rc = mb_recv (s1, rbuf, sizeof (rbuf), 0);
    assert (rc == (int) sizeof (small));
    assert (memcmp (rbuf, small, sizeof (small)) == 0);

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    /* Receiver-side limit (peer queue has no wire decode gate). */
    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_setsockopt (s1, MB_SOL_SOCKET, MB_RCVMAXSIZE, &max, sizeof (max));
    assert (rc == 0);

    rc = mb_bind (s1, "inproc://test_rcvmax_peer");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://test_rcvmax_peer");
    assert (rc >= 0);

    rc = mb_send (s2, big, 2048, 0);
    assert (rc == -1);
    assert (mb_errno () == EMSGSIZE);
    free (big);

    rc = mb_send (s2, small, sizeof (small), 0);
    assert (rc == (int) sizeof (small));
    rc = mb_recv (s1, rbuf, sizeof (rbuf), 0);
    assert (rc == (int) sizeof (small));

    rc = mb_close (s1);
    assert (rc == 0);
    rc = mb_close (s2);
    assert (rc == 0);

    printf ("  test_inproc_rcvmaxsize: PASSED\n");
}

int main (void)
{
    printf ("test_inproc:\n");
    test_inproc_bind_connect ();
    test_inproc_duplicate_bind ();
    test_inproc_connect_no_bind ();
    test_inproc_address_reuse ();
    test_inproc_multiple_addresses ();
    test_inproc_large_message ();
    test_inproc_pair_second_connect ();
    test_inproc_proto_mismatch ();
    test_inproc_cooked_raw_peers ();
    test_inproc_pubsub_ok ();
    test_inproc_ins_connect_oom ();
    test_inproc_rcvbuf_backpressure ();
    test_inproc_rcvbuf_after_connect ();
    test_inproc_rcvbuf_enlarge_wakes_sndfd ();
    test_inproc_rcvbuf_shrink_clears_sndfd ();
    test_inproc_buf_rejects_nonpositive ();
    test_inproc_rcvmaxsize ();
    printf ("test_inproc: ALL PASSED\n");
    return 0;
}
