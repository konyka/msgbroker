#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>

static void test_sub_subscribe_unsubscribe (void)
{
    int sub;
    int rc;

    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "sports", 6);
    assert (rc == 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "news", 4);
    assert (rc == 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_UNSUBSCRIBE, "sports", 6);
    assert (rc == 0);

    mb_close (sub);

    printf ("  test_sub_subscribe_unsubscribe: PASSED\n");
}

static void test_sub_subscribe_recv (void)
{
    int pub, sub;
    int rc;
    char buf[64];

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "sport", 5);
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://subfilter");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://subfilter");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_send (pub, "news:breaking", 13, 0);
    assert (rc == 13);

    rc = mb_send (pub, "sport:football", 14, 0);
    assert (rc == 14);

    usleep (50000);

    rc = mb_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 14);
    assert (memcmp (buf, "sport:football", 14) == 0);

    mb_close (sub);
    mb_close (pub);

    printf ("  test_sub_subscribe_recv: PASSED\n");
}

/*  DONTWAIT must skip queued non-matching topics in one call. */
static void test_sub_filter_dontwait_skip (void)
{
    int pub, sub;
    int rc;
    char buf[64];

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "sport", 5);
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://subfilter_dw");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://subfilter_dw");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_send (pub, "news:breaking", 13, 0);
    assert (rc == 13);
    rc = mb_send (pub, "weather:rain", 12, 0);
    assert (rc == 12);
    rc = mb_send (pub, "sport:football", 14, 0);
    assert (rc == 14);

    usleep (50000);

    rc = mb_recv (sub, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc == 14);
    assert (memcmp (buf, "sport:football", 14) == 0);

    mb_close (sub);
    mb_close (pub);

    printf ("  test_sub_filter_dontwait_skip: PASSED\n");
}

/*  With no subscriptions, SUB must drop all messages (not receive-all). */
static void test_sub_no_subscribe_drops (void)
{
    int pub, sub;
    int rc;
    char buf[64];

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_bind (pub, "inproc://sub_nosub");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://sub_nosub");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_send (pub, "ANY", 3, 0);
    assert (rc == 3);

    usleep (50000);

    rc = mb_recv (sub, buf, sizeof (buf), MB_DONTWAIT);
    assert (rc < 0);
    assert (mb_errno () == EAGAIN);

    mb_close (sub);
    mb_close (pub);

    printf ("  test_sub_no_subscribe_drops: PASSED\n");
}

/*  Empty subscription string receives all topics. */
static void test_sub_empty_subscribe_all (void)
{
    int pub, sub;
    int rc;
    char buf[64];

    pub = mb_socket (AF_MB, MB_PUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_SUB);
    assert (sub >= 0);

    rc = mb_setsockopt (sub, MB_SUB_PROTO, MB_SUB_SUBSCRIBE, "", 0);
    assert (rc == 0);

    rc = mb_bind (pub, "inproc://sub_empty");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://sub_empty");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_send (pub, "ANY", 3, 0);
    assert (rc == 3);

    rc = mb_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 3);
    assert (memcmp (buf, "ANY", 3) == 0);

    mb_close (sub);
    mb_close (pub);

    printf ("  test_sub_empty_subscribe_all: PASSED\n");
}

/*  XSUB must be able to send subscription traffic upstream (not permanent EAGAIN). */
static void test_xsub_send (void)
{
    int pub, sub;
    int rc;
    char submsg[8];

    pub = mb_socket (AF_MB, MB_XPUB);
    assert (pub >= 0);
    sub = mb_socket (AF_MB, MB_XSUB);
    assert (sub >= 0);

    rc = mb_bind (pub, "inproc://xsub_send");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://xsub_send");
    assert (rc >= 0);

    usleep (50000);

    submsg[0] = 1;
    memcpy (submsg + 1, "sport", 5);
    rc = mb_send (sub, submsg, 6, MB_DONTWAIT);
    assert (rc == 6);

    mb_close (sub);
    mb_close (pub);

    printf ("  test_xsub_send: PASSED\n");
}

int main (void)
{
    printf ("PUB/SUB subscription tests:\n");

    test_sub_subscribe_unsubscribe ();
    test_sub_subscribe_recv ();
    test_sub_filter_dontwait_skip ();
    test_sub_no_subscribe_drops ();
    test_sub_empty_subscribe_all ();
    test_xsub_send ();

    printf ("\nAll pubsub filter tests PASSED\n");
    return 0;
}
