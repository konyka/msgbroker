#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

static void test_endpoint_basic (void)
{
    int s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    int rc = mb_bind (s, "tcp://127.0.0.1:19999");
    assert (rc >= 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("  endpoint_basic: OK\n");
}

/* Oversized addresses must return EINVAL, not overflow or abort. */
static void test_endpoint_addr_too_long (void)
{
    int s;
    int rc;
    char addr[MB_SOCKADDR_MAX + 64];
    size_t i;

    memcpy (addr, "tcp://127.0.0.1:", 15);
    for (i = 15; i < sizeof (addr) - 1; ++i)
        addr[i] = '9';
    addr[sizeof (addr) - 1] = '\0';
    assert (strlen (addr) > MB_SOCKADDR_MAX);

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);
    rc = mb_bind (s, addr);
    assert (rc < 0);
    assert (mb_errno () == EINVAL);
    mb_close (s);

    s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);
    rc = mb_connect (s, addr);
    assert (rc < 0);
    assert (mb_errno () == EINVAL);
    mb_close (s);

    printf ("  endpoint_addr_too_long: OK\n");
}

int main (void)
{
    printf ("test_endpoint:\n");
    test_endpoint_basic ();
    test_endpoint_addr_too_long ();
    printf ("test_endpoint: PASSED\n");
    return 0;
}
