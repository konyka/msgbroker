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

    rc = mb_bind (pub, "inproc://subfilter");
    assert (rc >= 0);
    rc = mb_connect (sub, "inproc://subfilter");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_send (pub, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (sub, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    mb_close (sub);
    mb_close (pub);

    printf ("  test_sub_subscribe_recv: PASSED\n");
}

int main (void)
{
    printf ("PUB/SUB subscription tests:\n");

    test_sub_subscribe_unsubscribe ();
    test_sub_subscribe_recv ();

    printf ("\nAll pubsub filter tests PASSED\n");
    return 0;
}
