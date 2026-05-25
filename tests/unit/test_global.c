#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

int main (void)
{
    int s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);
    assert (s < MB_MAX_SOCKETS);

    int val = 256 * 1024;
    int rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SNDBUF, &val, sizeof (val));
    assert (rc == 0);

    int got = 0;
    size_t gotlen = sizeof (got);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_SNDBUF, &got, &gotlen);
    assert (rc == 0);
    assert (got == 256 * 1024);

    got = 0;
    gotlen = sizeof (got);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_PROTOCOL, &got, &gotlen);
    assert (rc == 0);
    assert (got == MB_PAIR);

    got = 0;
    gotlen = sizeof (got);
    rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_DOMAIN, &got, &gotlen);
    assert (rc == 0);
    assert (got == AF_MB);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("test_global: PASSED\n");
    return 0;
}
