#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

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

    {
        char name[64];
        char too_long[65];
        const char *want = "my-sock";

        rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SOCKET_NAME, want,
            strlen (want));
        assert (rc == 0);

        gotlen = sizeof (name);
        rc = mb_getsockopt (s, MB_SOL_SOCKET, MB_SOCKET_NAME, name, &gotlen);
        assert (rc == 0);
        assert (strcmp (name, want) == 0);
        assert (gotlen == strlen (want) + 1);

        memset (too_long, 'a', sizeof (too_long));
        rc = mb_setsockopt (s, MB_SOL_SOCKET, MB_SOCKET_NAME, too_long,
            sizeof (too_long));
        assert (rc == -1);
        assert (mb_errno () == EINVAL);
    }

    rc = mb_close (s);
    assert (rc == 0);

    printf ("test_global: PASSED\n");
    return 0;
}
