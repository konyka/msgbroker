#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

int main (void)
{
    int s1, s2, rc;
    struct mb_pollfd fds[2];
    char buf[64];

    s1 = mb_socket (AF_MB, MB_PAIR);
    s2 = mb_socket (AF_MB, MB_PAIR);
    assert (s1 >= 0);
    assert (s2 >= 0);

    rc = mb_bind (s1, "inproc://poll");
    assert (rc >= 0);
    rc = mb_connect (s2, "inproc://poll");
    assert (rc >= 0);

    rc = mb_poll (NULL, 0, 0);
    assert (rc == 0);

    memset (fds, 0, sizeof (fds));
    fds[0].fd = s1;
    fds[0].events = MB_POLLIN;
    fds[1].fd = s2;
    fds[1].events = MB_POLLIN;

    rc = mb_poll (fds, 2, 0);
    assert (rc == 0);

    rc = mb_send (s2, "HELLO", 5, 0);
    assert (rc == 5);

    rc = mb_poll (fds, 2, 100);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLIN);
    assert (!(fds[1].revents & MB_POLLIN));

    rc = mb_recv (s1, buf, sizeof (buf), 0);
    assert (rc == 5);
    assert (memcmp (buf, "HELLO", 5) == 0);

    fds[0].revents = 0;
    fds[0].events = MB_POLLOUT;
    fds[1].revents = 0;
    fds[1].events = MB_POLLIN;

    rc = mb_poll (fds, 2, 100);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLOUT);

    mb_close (s2);
    mb_close (s1);

    printf ("test_poll: PASSED\n");
    return 0;
}
