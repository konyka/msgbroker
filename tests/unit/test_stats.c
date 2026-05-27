#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_broken_connections_initial (void)
{
    int s1, s2;
    int rc;
    uint64_t val;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:19998");
    assert (rc >= 0);
    rc = mb_connect (s2, "tcp://127.0.0.1:19998");
    assert (rc >= 0);

    usleep (50000);

    val = mb_get_statistic (s1, MB_STAT_BROKEN_CONNECTIONS);
    assert (val == 0);

    mb_close (s2);
    mb_close (s1);

    printf ("  test_broken_connections_initial: PASSED\n");
}

static void test_messages_sent_received_stat (void)
{
    int s1, s2;
    int rc;
    uint64_t val;
    size_t vallen;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:19997");
    assert (rc >= 0);
    rc = mb_connect (s2, "tcp://127.0.0.1:19997");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_send (s1, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_recv (s2, buf, sizeof (buf), 0);
    assert (rc == 5);

    val = mb_get_statistic (s1, MB_STAT_MESSAGES_SENT);
    assert (val >= 1);

    val = mb_get_statistic (s2, MB_STAT_MESSAGES_RECEIVED);
    assert (val >= 1);

    printf ("  test_messages_sent_received_stat: PASSED\n");

    mb_close (s2);
    mb_close (s1);
}

int main (void)
{
    printf ("Statistics tests:\n");

    test_messages_sent_received_stat ();
    test_broken_connections_initial ();

    printf ("\nAll statistics tests PASSED\n");
    return 0;
}
