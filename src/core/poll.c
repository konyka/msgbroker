#include "../pal/sleep.h"
#include "../pal/clock.h"
#include "../utils/alloc.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <errno.h>

#if !defined(_WIN32)
#include <poll.h>
#endif

/* Stream transports only arm rcvfd/sndfd when sync runs (has_msg probes the
 * wire fd). Re-sync each slice so a blocking mb_poll wakes shortly after
 * kernel data arrives, instead of sleeping the full timeout on a silent efd. */
#define MB_POLL_SLICE_MS 50

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
        int *map_idx;
        int *map_snd;
        int np;
        int maxp;
        uint64_t start_ms;

        /* Each socket may contribute separate rcvfd and sndfd entries. */
        maxp = nfds * 2;
        pfd = (struct pollfd *) mb_alloc (
            sizeof (struct pollfd) * (size_t) maxp);
        if (!pfd) {
            mb_errno_value = ENOMEM;
            return -1;
        }
        map_idx = (int *) mb_alloc (sizeof (int) * (size_t) maxp);
        if (!map_idx) {
            mb_free (pfd);
            mb_errno_value = ENOMEM;
            return -1;
        }
        map_snd = (int *) mb_alloc (sizeof (int) * (size_t) maxp);
        if (!map_snd) {
            mb_free (map_idx);
            mb_free (pfd);
            mb_errno_value = ENOMEM;
            return -1;
        }

        start_ms = mb_clock_ms ();

        for (;;) {
            int slice;

            np = 0;
            for (i = 0; i != nfds; ++i) {
                fds[i].revents = 0;

                if (fds[i].events & MB_POLLIN) {
                    int fd;
                    size_t sz = sizeof (fd);

                    rc = mb_getsockopt (fds[i].fd, MB_SOL_SOCKET, MB_RCVFD,
                        &fd, &sz);
                    if (rc < 0) {
                        mb_free (map_snd);
                        mb_free (map_idx);
                        mb_free (pfd);
                        return -1;
                    }
                    pfd[np].fd = fd;
                    pfd[np].events = POLLIN;
                    pfd[np].revents = 0;
                    map_idx[np] = i;
                    map_snd[np] = 0;
                    np++;
                }
                if (fds[i].events & MB_POLLOUT) {
                    int fd;
                    size_t sz = sizeof (fd);

                    rc = mb_getsockopt (fds[i].fd, MB_SOL_SOCKET, MB_SNDFD,
                        &fd, &sz);
                    if (rc < 0) {
                        mb_free (map_snd);
                        mb_free (map_idx);
                        mb_free (pfd);
                        return -1;
                    }
                    pfd[np].fd = fd;
                    pfd[np].events = POLLIN;
                    pfd[np].revents = 0;
                    map_idx[np] = i;
                    map_snd[np] = 1;
                    np++;
                }
            }

            if (timeout < 0)
                slice = MB_POLL_SLICE_MS;
            else if (timeout == 0)
                slice = 0;
            else {
                uint64_t elapsed = mb_clock_ms () - start_ms;
                if (elapsed >= (uint64_t) timeout) {
                    mb_free (map_snd);
                    mb_free (map_idx);
                    mb_free (pfd);
                    return 0;
                }
                {
                    uint64_t remain = (uint64_t) timeout - elapsed;
                    slice = remain < (uint64_t) MB_POLL_SLICE_MS
                        ? (int) remain : MB_POLL_SLICE_MS;
                }
            }

            if (np > 0)
                rc = poll (pfd, (nfds_t) np, slice);
            else {
                if (slice > 0)
                    mb_msleep (slice);
                rc = 0;
            }

            if (rc < 0) {
                mb_free (map_snd);
                mb_free (map_idx);
                mb_free (pfd);
                return -1;
            }

            if (rc > 0) {
                res = 0;
                for (i = 0; i != np; ++i) {
                    if (!(pfd[i].revents & POLLIN))
                        continue;
                    if (map_snd[i])
                        fds[map_idx[i]].revents |= MB_POLLOUT;
                    else
                        fds[map_idx[i]].revents |= MB_POLLIN;
                }
                for (i = 0; i != nfds; ++i) {
                    if (fds[i].revents)
                        ++res;
                }
                mb_free (map_snd);
                mb_free (map_idx);
                mb_free (pfd);
                return res;
            }

            if (timeout == 0) {
                mb_free (map_snd);
                mb_free (map_idx);
                mb_free (pfd);
                return 0;
            }
            if (timeout > 0 &&
                mb_clock_ms () - start_ms >= (uint64_t) timeout) {
                mb_free (map_snd);
                mb_free (map_idx);
                mb_free (pfd);
                return 0;
            }
        }
    }
#else
    (void) rc; (void) res;
    return 0;
#endif
}
