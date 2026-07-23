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

static void test_connection_stats (void)
{
    int s1, s2;
    int rc;
    uint64_t val;
    uint64_t via_opt;
    size_t vallen;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://stat_conns");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://stat_conns");
    assert (rc >= 0);

    usleep (50000);

    val = mb_get_statistic (s1, MB_STAT_ACCEPTED_CONNECTIONS);
    assert (val == 1);
    val = mb_get_statistic (s1, MB_STAT_CURRENT_CONNECTIONS);
    assert (val == 1);
    val = mb_get_statistic (s1, MB_STAT_ESTABLISHED_CONNECTIONS);
    assert (val == 0);

    val = mb_get_statistic (s2, MB_STAT_ESTABLISHED_CONNECTIONS);
    assert (val == 1);
    val = mb_get_statistic (s2, MB_STAT_CURRENT_CONNECTIONS);
    assert (val == 1);
    val = mb_get_statistic (s2, MB_STAT_ACCEPTED_CONNECTIONS);
    assert (val == 0);

    /* getsockopt must expose the same counters as mb_get_statistic. */
    via_opt = 0;
    vallen = sizeof (via_opt);
    rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_STAT_ACCEPTED_CONNECTIONS,
        &via_opt, &vallen);
    assert (rc == 0);
    assert (vallen == sizeof (uint64_t));
    assert (via_opt == 1);

    via_opt = 0;
    vallen = sizeof (via_opt);
    rc = mb_getsockopt (s2, MB_SOL_SOCKET, MB_STAT_ESTABLISHED_CONNECTIONS,
        &via_opt, &vallen);
    assert (rc == 0);
    assert (via_opt == 1);

    mb_close (s2);
    usleep (50000);

    val = mb_get_statistic (s1, MB_STAT_CURRENT_CONNECTIONS);
    assert (val == 0);
    val = mb_get_statistic (s1, MB_STAT_BROKEN_CONNECTIONS);
    assert (val >= 1);

    via_opt = 0;
    vallen = sizeof (via_opt);
    rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_STAT_BROKEN_CONNECTIONS,
        &via_opt, &vallen);
    assert (rc == 0);
    assert (via_opt == val);

    mb_close (s1);
    printf ("  test_connection_stats: PASSED\n");
}

int main (void)
{
    printf ("Statistics tests:\n");

    test_messages_sent_received_stat ();
    test_broken_connections_initial ();
    test_connection_stats ();

    printf ("\nAll statistics tests PASSED\n");
    return 0;
}
