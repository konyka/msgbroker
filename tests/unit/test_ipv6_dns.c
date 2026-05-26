#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_tcp_ipv6_loopback (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://[::1]:18890");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://[::1]:18890");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_send (s2, "IPV6OK", 6, 0);
    assert (rc == 6);

    usleep (100000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 6);
    assert (memcmp (buf, "IPV6OK", 6) == 0);

    mb_close (s1);
    mb_close (s2);

    printf ("  test_tcp_ipv6_loopback: PASSED\n");
}

static void test_tcp_ipv6_wildcard (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://[::]:18891");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://127.0.0.1:18891");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_send (s2, "DUAL", 4, 0);
    assert (rc == 4);

    usleep (100000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 4);
    assert (memcmp (buf, "DUAL", 4) == 0);

    mb_close (s1);
    mb_close (s2);

    printf ("  test_tcp_ipv6_wildcard_dual_stack: PASSED\n");
}

static void test_tcp_localhost_dns (void)
{
    int s1, s2;
    int rc;
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://127.0.0.1:18892");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://localhost:18892");
    assert (rc >= 0);

    usleep (100000);

    rc = mb_send (s2, "DNS_OK", 6, 0);
    assert (rc == 6);

    usleep (100000);

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 6);
    assert (memcmp (buf, "DNS_OK", 6) == 0);

    mb_close (s1);
    mb_close (s2);

    printf ("  test_tcp_localhost_dns: PASSED\n");
}

static void test_tcp_ipv4_wildcard (void)
{
    int s1, s2;
    int rc;

    s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);

    rc = mb_bind (s1, "tcp://*:18893");
    assert (rc >= 0);

    usleep (50000);

    rc = mb_connect (s2, "tcp://127.0.0.1:18893");
    assert (rc >= 0);

    usleep (100000);

    mb_close (s1);
    mb_close (s2);

    printf ("  test_tcp_ipv4_wildcard: PASSED\n");
}

int main (void)
{
    printf ("IPv6 + DNS Tests:\n");

    test_tcp_ipv4_wildcard ();
    test_tcp_localhost_dns ();
    test_tcp_ipv6_loopback ();
    test_tcp_ipv6_wildcard ();

    printf ("\nAll IPv6 + DNS tests PASSED\n");
    return 0;
}
