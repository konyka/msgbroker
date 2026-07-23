#include "../memory/msg.h"
#include "../utils/err.h"

#include "global.h"
#include "sock.h"

#include <msgbroker/mb.h>

#include <errno.h>

/* Bounded wait so STOPPING is rechecked while blocked on peer readiness. */
#define MB_DEVICE_POLL_MS 50

/* Forward one message. Returns 1 on progress, -EAGAIN if idle, else hard error. */
static int mb_device_forward (struct mb_sock *from, struct mb_sock *to,
    int to_fd)
{
    struct mb_msg msg;
    int rc;

    mb_msg_init (&msg, 0);
    rc = mb_sock_recv (from, &msg);
    if (rc < 0) {
        mb_msg_term (&msg);
        return rc;
    }

    for (;;) {
        /* Recv already holds a message; must not miss STOPPING here or
         * mb_close waits forever on device socket holds. */
        if ((__atomic_load_n (&from->flags, __ATOMIC_ACQUIRE) &
                MB_SOCK_FLAG_STOPPING) ||
            (__atomic_load_n (&to->flags, __ATOMIC_ACQUIRE) &
                MB_SOCK_FLAG_STOPPING)) {
            mb_msg_term (&msg);
            return -EAGAIN;
        }
        rc = mb_sock_send (to, &msg);
        if (rc == 0) {
            mb_msg_term (&msg);
            return 1;
        }
        if (rc != -EAGAIN) {
            mb_msg_term (&msg);
            return rc;
        }
        {
            struct mb_pollfd pfd;

            pfd.fd = to_fd;
            pfd.events = MB_POLLOUT;
            pfd.revents = 0;
            (void) mb_poll (&pfd, 1, MB_DEVICE_POLL_MS);
        }
    }
}

int mb_device (int s1, int s2)
{
    struct mb_sock *sock1;
    struct mb_sock *sock2;
    int rc;
    int ret = 0;

    rc = mb_global_hold_socket (&sock1, s1);
    if (rc < 0) {
        mb_err_set_errno (-rc);
        return -1;
    }

    rc = mb_global_hold_socket (&sock2, s2);
    if (rc < 0) {
        mb_global_rele_socket (sock1);
        mb_err_set_errno (-rc);
        return -1;
    }

    for (;;) {
        int a;
        int b;

        /* mb_close sets STOPPING then waits for holds; exit so close can finish. */
        if ((__atomic_load_n (&sock1->flags, __ATOMIC_ACQUIRE) &
                MB_SOCK_FLAG_STOPPING) ||
            (__atomic_load_n (&sock2->flags, __ATOMIC_ACQUIRE) &
                MB_SOCK_FLAG_STOPPING))
            break;

        a = mb_device_forward (sock1, sock2, s2);
        if (a < 0 && a != -EAGAIN) {
            mb_err_set_errno (-a);
            ret = -1;
            break;
        }

        b = mb_device_forward (sock2, sock1, s1);
        if (b < 0 && b != -EAGAIN) {
            mb_err_set_errno (-b);
            ret = -1;
            break;
        }

        if (a == -EAGAIN && b == -EAGAIN) {
            struct mb_pollfd pfds[2];

            pfds[0].fd = s1;
            pfds[0].events = MB_POLLIN | MB_POLLOUT;
            pfds[0].revents = 0;
            pfds[1].fd = s2;
            pfds[1].events = MB_POLLIN | MB_POLLOUT;
            pfds[1].revents = 0;
            (void) mb_poll (pfds, 2, MB_DEVICE_POLL_MS);
        }
    }

    mb_global_rele_socket (sock1);
    mb_global_rele_socket (sock2);
    return ret;
}
