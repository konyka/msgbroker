#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

struct rcvfd_race {
    int s_send;
    volatile int done;
};

static void *poll_rcvfd_sender (void *arg)
{
    struct rcvfd_race *race = (struct rcvfd_race *) arg;

    while (!race->done) {
        int src = mb_send (race->s_send, "Z", 1, MB_DONTWAIT);
        if (src < 0 && mb_errno () == EAGAIN) {
            usleep (100);
            continue;
        }
        if (src < 0)
            break;
    }
    return NULL;
}

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

    /* After draining, rcvfd must not stay sticky-ready. */
    fds[0].revents = 0;
    fds[0].events = MB_POLLIN;
    fds[1].revents = 0;
    fds[1].events = MB_POLLIN;
    rc = mb_poll (fds, 2, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLIN));

    fds[0].revents = 0;
    fds[0].events = MB_POLLOUT;
    fds[1].revents = 0;
    fds[1].events = MB_POLLIN;

    rc = mb_poll (fds, 2, 100);
    assert (rc >= 1);
    assert (fds[0].revents & MB_POLLOUT);

    /* Same socket requesting IN|OUT must watch both efd fds. */
    rc = mb_send (s1, "PING", 4, 0);
    assert (rc == 4);
    fds[0].revents = 0;
    fds[0].events = MB_POLLIN | MB_POLLOUT;
    fds[1].revents = 0;
    fds[1].events = MB_POLLIN | MB_POLLOUT;
    rc = mb_poll (fds, 2, 100);
    assert (rc >= 1);
    assert (fds[1].revents & MB_POLLIN);
    assert (fds[0].revents & MB_POLLOUT);

    /* Peer gone: sndfd must not stay sticky-ready. */
    mb_close (s2);
    fds[0].revents = 0;
    fds[0].events = MB_POLLOUT;
    rc = mb_poll (fds, 1, 0);
    assert (rc == 0);
    assert (!(fds[0].revents & MB_POLLOUT));

    mb_close (s1);

    /* Dirty mb_errno, then force poll alloc failure → ENOMEM via mb_errno(). */
    {
        int s = mb_socket (AF_MB, MB_PAIR);
        assert (s >= 0);
        assert (mb_send (s, "x", 1, MB_DONTWAIT) == -1);
        assert (mb_errno () == EAGAIN);
        mb_close (s);

        rc = mb_poll (NULL, INT_MAX / 2 + 1, 0);
        assert (rc == -1);
        assert (mb_errno () == ENOMEM);
        printf ("  poll_oom_errno: OK\n");
    }

    /* Stress: inproc send racing sync_rcvfd must not lose MB_RCVFD POLLIN. */
    {
        struct rcvfd_race race;
        pthread_t thr;
        int s1, s2;
        int rcvfd = -1;
        size_t sz = sizeof (rcvfd);
        int i;
        char buf[8];
        struct pollfd pfd;

        s1 = mb_socket (AF_MB, MB_PAIR);
        s2 = mb_socket (AF_MB, MB_PAIR);
        assert (s1 >= 0 && s2 >= 0);
        rc = mb_bind (s1, "inproc://poll_rcvfd_race");
        assert (rc >= 0);
        rc = mb_connect (s2, "inproc://poll_rcvfd_race");
        assert (rc >= 0);

        race.s_send = s2;
        race.done = 0;
        rc = pthread_create (&thr, NULL, poll_rcvfd_sender, &race);
        assert (rc == 0);

        rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_RCVFD, &rcvfd, &sz);
        assert (rc == 0);
        assert (rcvfd >= 0);

        for (i = 0; i < 200; ++i) {
            /* Keep poking sync while the sender enqueues. */
            rc = mb_getsockopt (s1, MB_SOL_SOCKET, MB_RCVFD, &rcvfd, &sz);
            assert (rc == 0);

            pfd.fd = rcvfd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            rc = poll (&pfd, 1, 2000);
            assert (rc == 1);
            assert (pfd.revents & POLLIN);

            rc = mb_recv (s1, buf, sizeof (buf), 0);
            assert (rc == 1);
        }

        race.done = 1;
        pthread_join (thr, NULL);
        mb_close (s2);
        mb_close (s1);
        printf ("  poll_rcvfd_inproc_race: OK\n");
    }

    printf ("test_poll: PASSED\n");
    return 0;
}
