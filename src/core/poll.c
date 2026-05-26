#include "../pal/sleep.h"
#include "../utils/alloc.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <errno.h>

#if !defined(_WIN32)
#include <poll.h>
#endif

static int mb_errno_value;

int mb_poll (struct mb_pollfd *fds, int nfds, int timeout)
{
    int rc;
    int i;
    int res;

    if (nfds == 0) {
        if (timeout > 0)
            mb_msleep (timeout);
        return 0;
    }

#if !defined(_WIN32)
    {
        struct pollfd *pfd;
        int *is_snd;

        pfd = (struct pollfd *) mb_alloc (sizeof (struct pollfd) * (size_t) nfds);
        if (!pfd) {
            mb_errno_value = ENOMEM;
            return -1;
        }
        is_snd = (int *) mb_alloc (sizeof (int) * (size_t) nfds);
        if (!is_snd) {
            mb_free (pfd);
            mb_errno_value = ENOMEM;
            return -1;
        }

        int nfds_set = 0;
        for (i = 0; i != nfds; ++i) {
            fds[i].revents = 0;
            pfd[i].fd = -1;
            pfd[i].events = 0;
            pfd[i].revents = 0;
            is_snd[i] = 0;

            if (fds[i].events & MB_POLLIN) {
                int fd;
                size_t sz = sizeof (fd);
                rc = mb_getsockopt (fds[i].fd, MB_SOL_SOCKET, MB_RCVFD,
                    &fd, &sz);
                if (rc < 0) {
                    mb_free (is_snd);
                    mb_free (pfd);
                    return -1;
                }
                pfd[i].fd = fd;
                pfd[i].events |= POLLIN;
                nfds_set++;
            }
            if (fds[i].events & MB_POLLOUT) {
                int fd;
                size_t sz = sizeof (fd);
                rc = mb_getsockopt (fds[i].fd, MB_SOL_SOCKET, MB_SNDFD,
                    &fd, &sz);
                if (rc < 0) {
                    mb_free (is_snd);
                    mb_free (pfd);
                    return -1;
                }
                pfd[i].fd = fd;
                pfd[i].events |= POLLIN;
                is_snd[i] = 1;
                nfds_set++;
            }
        }

        if (nfds_set > 0)
            rc = poll (pfd, (nfds_t) nfds, timeout);
        else {
            if (timeout > 0)
                mb_msleep (timeout);
            rc = 0;
        }

        if (rc < 0) {
            mb_free (is_snd);
            mb_free (pfd);
            return -1;
        }

        res = 0;
        for (i = 0; i != nfds; ++i) {
            if (pfd[i].revents & POLLIN) {
                if (is_snd[i])
                    fds[i].revents |= MB_POLLOUT;
                else
                    fds[i].revents |= MB_POLLIN;
            }
            if (fds[i].revents)
                ++res;
        }

        mb_free (is_snd);
        mb_free (pfd);
        return res;
    }
#else
    (void) rc; (void) res;
    return 0;
#endif
}
