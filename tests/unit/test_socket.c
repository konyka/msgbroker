#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

int main (void)
{
    int s1 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);

    int s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s2 >= 0);
    assert (s1 != s2);

    int rc = mb_close (s1);
    assert (rc == 0);

    rc = mb_close (s2);
    assert (rc == 0);

    rc = mb_close (9999);
    assert (rc == -1);

    printf ("test_socket: PASSED\n");
    return 0;
}
