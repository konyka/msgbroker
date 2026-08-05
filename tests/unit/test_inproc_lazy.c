/*
 * T-INPROC-LAZY: per-process name registry for inproc://
 *
 * Currently mb_connect(inproc://name) without a prior mb_bind succeeds
 * but yields no pipe — the connecting endpoint silently never sees a peer.
 *
 * The lazy registry changes that: the first declaration of an address
 * (either mb_bind or mb_connect) creates the virtual pipe, and any
 * subsequent declaration of the same name auto-attaches to that pipe
 * rather than returning EADDRINUSE or stranding the connection.
 *
 * The first to declare wins. Bind always claims the address slot.
 * Among multiple connectors, the first connect creates the pipe,
 * and later connects attach to it (lazy / on-demand).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include "../../src/transport/inproc/ins.h"
#include "../../src/core/ep.h"

/* Two PAIR sockets, both call connect (no bind). The first connect
 * creates the lazy pipe; the second connect auto-attaches. After both
 * have connected, PAIR a sends and PAIR b must receive. */
static void test_inproc_lazy_pair_double_connect (void)
{
    int a, b;
    int rc;
    char buf[64];

    a = mb_socket (AF_MB, MB_PAIR);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_PAIR);
    assert (b >= 0);

    /* No bind anywhere: both ends lazily attach to inproc://lazy_pair. */
    rc = mb_connect (a, "inproc://lazy_pair");
    assert (rc >= 0);

    rc = mb_connect (b, "inproc://lazy_pair");
    assert (rc >= 0);

    rc = mb_send (a, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (b, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    /* PAIR is bidirectional. */
    rc = mb_send (b, "WORLD", 5, 0);
    assert (rc == 5);

    rc = mb_recv (a, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "WORLD", 5) == 0);

    rc = mb_close (a);
    assert (rc == 0);
    rc = mb_close (b);
    assert (rc == 0);

    printf ("  test_inproc_lazy_pair_double_connect: PASSED\n");
}

/* Bind first, then a later connect must auto-attach to that bind and
 * not fail with EADDRINUSE. */
static void test_inproc_lazy_bind_then_connect (void)
{
    int a, b;
    int rc;
    char buf[64];

    a = mb_socket (AF_MB, MB_PAIR);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_PAIR);
    assert (b >= 0);

    rc = mb_bind (a, "inproc://lazy_bind_connect");
    assert (rc >= 0);

    rc = mb_connect (b, "inproc://lazy_bind_connect");
    assert (rc >= 0);

    rc = mb_send (a, "BIND_FIRST", 10, 0);
    assert (rc == 10);

    rc = mb_recv (b, buf, sizeof (buf), 0);
    assert (rc == 10);
    assert (memcmp (buf, "BIND_FIRST", 10) == 0);

    rc = mb_close (a);
    assert (rc == 0);
    rc = mb_close (b);
    assert (rc == 0);

    printf ("  test_inproc_lazy_bind_then_connect: PASSED\n");
}

/* Three connectors against the same lazy address must all auto-attach.
 * PAIR only allows one peer, so extra connectors must fail with
 * EISCONN — but they must NOT crash and they must NOT attach to a
 * phantom pipe. */
static void test_inproc_lazy_triple_connect (void)
{
    int a, b, c;
    int rc;

    a = mb_socket (AF_MB, MB_PAIR);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_PAIR);
    assert (b >= 0);
    c = mb_socket (AF_MB, MB_PAIR);
    assert (c >= 0);

    rc = mb_connect (a, "inproc://lazy_triple");
    assert (rc >= 0);
    rc = mb_connect (b, "inproc://lazy_triple");
    assert (rc >= 0);

    /* PAIR refuses a third peer. */
    rc = mb_connect (c, "inproc://lazy_triple");
    assert (rc < 0);
    assert (mb_errno () == EISCONN);

    /* Failed handshake must not leave phantom stats on c. */
    assert (mb_get_statistic (c, MB_STAT_CURRENT_CONNECTIONS) == 0);
    assert (mb_get_statistic (c, MB_STAT_ESTABLISHED_CONNECTIONS) == 0);
    assert (mb_get_statistic (c, MB_STAT_BROKEN_CONNECTIONS) == 0);

    /* a↔b pipe still works. */
    rc = mb_send (a, "OK", 2, 0);
    assert (rc == 2);
    {
        char buf[8];
        rc = mb_recv (b, buf, sizeof (buf), 0);
        assert (rc == 2);
        assert (memcmp (buf, "OK", 2) == 0);
    }

    mb_close (c);
    mb_close (b);
    mb_close (a);

    printf ("  test_inproc_lazy_triple_connect: PASSED\n");
}

/* Closing one end of a lazy pipe must release the address so a
 * subsequent declaration rebinds cleanly. */
static void test_inproc_lazy_close_releases (void)
{
    int a, b, c;
    int rc;
    char buf[32];

    a = mb_socket (AF_MB, MB_PAIR);
    assert (a >= 0);
    b = mb_socket (AF_MB, MB_PAIR);
    assert (b >= 0);

    rc = mb_connect (a, "inproc://lazy_release");
    assert (rc >= 0);
    rc = mb_connect (b, "inproc://lazy_release");
    assert (rc >= 0);

    rc = mb_close (a);
    assert (rc == 0);
    rc = mb_close (b);
    assert (rc == 0);

    /* The address must now be reusable for a fresh bind+connect. */
    c = mb_socket (AF_MB, MB_PAIR);
    assert (c >= 0);
    rc = mb_connect (c, "inproc://lazy_release");
    assert (rc >= 0);

    /* No peer yet — send with DONTWAIT so the test returns EAGAIN
     * rather than blocking forever. The endpoint is live (registered
     * in the registry) and stays that way until close. */
    rc = mb_send (c, "FRESH", 5, MB_DONTWAIT);
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);

    rc = mb_recv (c, buf, sizeof (buf), MB_DONTWAIT);
    /* No peer, no message — DONTWAIT recv returns EAGAIN either way. */
    assert (rc == -1);
    assert (mb_errno () == EAGAIN);
    (void) buf;

    rc = mb_close (c);
    assert (rc == 0);

    printf ("  test_inproc_lazy_close_releases: PASSED\n");
}

int main (void)
{
    printf ("test_inproc_lazy:\n");
    test_inproc_lazy_pair_double_connect ();
    test_inproc_lazy_bind_then_connect ();
    test_inproc_lazy_triple_connect ();
    test_inproc_lazy_close_releases ();
    printf ("test_inproc_lazy: ALL PASSED\n");
    return 0;
}
