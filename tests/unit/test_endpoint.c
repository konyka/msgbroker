#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

int main (void)
{
    int s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    int rc = mb_bind (s, "tcp://127.0.0.1:19999");
    assert (rc >= 0);

    rc = mb_close (s);
    assert (rc == 0);

    printf ("test_endpoint: PASSED\n");
    return 0;
}
