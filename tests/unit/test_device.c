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

    mb_bind (s1, "inproc://dev1");
    mb_bind (s2, "inproc://dev2");

    int c1 = mb_socket (AF_MB, MB_PAIR);
    assert (c1 >= 0);
    int c2 = mb_socket (AF_MB, MB_PAIR);
    assert (c2 >= 0);

    mb_connect (c1, "inproc://dev1");
    mb_connect (c2, "inproc://dev2");

    int rc = mb_send (c1, "A", 1, 0);
    assert (rc == 0);

    char buf[16];
    rc = mb_recv (c2, buf, sizeof (buf), 0);
    assert (rc == 1);
    assert (buf[0] == 'A');

    rc = mb_send (c2, "B", 1, 0);
    assert (rc == 0);

    rc = mb_recv (c1, buf, sizeof (buf), 0);
    assert (rc == 1);
    assert (buf[0] == 'B');

    mb_close (c2);
    mb_close (c1);
    mb_close (s2);
    mb_close (s1);

    printf ("test_device: PASSED\n");
    return 0;
}
