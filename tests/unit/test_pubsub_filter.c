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

int main (void)
{
    printf ("PUB/SUB subscription tests:\n");

    test_sub_subscribe_unsubscribe ();
    test_sub_subscribe_recv ();
    test_sub_filter_dontwait_skip ();

    printf ("\nAll pubsub filter tests PASSED\n");
    return 0;
}
