#include "net.h"
#include "err.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/un.h>

int mb_net_parse_addr (const char *addr, char *host, size_t hostlen,
    uint16_t *port)
{
    const char *sep;
    const char *portstr;
    unsigned long p;
    const char *host_start;
    const char *host_end;

    sep = strstr (addr, "://");
    if (!sep)
        return -EINVAL;
    sep += 3;

    if (sep[0] == '[') {
        host_start = sep + 1;
        host_end = strchr (host_start, ']');
        if (!host_end)
            return -EINVAL;
        portstr = strrchr (host_end, ':');
    } else {
        host_start = sep;
        portstr = strrchr (sep, ':');
        host_end = portstr;
    }

    if (!portstr)
        return -EINVAL;

    if ((size_t) (host_end - host_start) >= hostlen)
        return -EINVAL;

    memcpy (host, host_start, (size_t) (host_end - host_start));
    host[host_end - host_start] = '\0';
    portstr++;

    p = strtoul (portstr, NULL, 10);
    if (p == 0 || p > 65535)
        return -EINVAL;
    *port = (uint16_t) p;
    return 0;
}

int mb_net_connect (const char *host, uint16_t port, int *family)
{
    return mb_net_connect_while (host, port, family, NULL, 5000);
}

/* Nonblocking connect to one sockaddr; cancellable via *running. */
static int mb_net_connect_sa (const struct sockaddr *sa, socklen_t salen,
    int family, volatile int *running, int timeout_ms)
{
    int fd;
    int rc;
    int flag = 1;
    int budget;

    if (running && !*running)
        return -ECANCELED;

    fd = socket (family, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof (flag));
    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);

    rc = connect (fd, sa, salen);
    if (rc == 0)
        return fd;
    if (errno != EINPROGRESS) {
        int err = -errno;
        close (fd);
        return err;
    }

    budget = timeout_ms;
    while (budget > 0) {
        struct pollfd pfd;
        int slice = budget > 50 ? 50 : budget;
        int soerr = 0;
        socklen_t solen = sizeof (soerr);

        if (running && !*running) {
            close (fd);
            return -ECANCELED;
        }

        pfd.fd = fd;
        pfd.events = POLLOUT;
        rc = poll (&pfd, 1, slice);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            close (fd);
            return -errno;
        }
        if (rc == 0) {
            budget -= slice;
            continue;
        }

        if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &soerr, &solen) < 0) {
            close (fd);
            return -errno;
        }
        if (soerr != 0) {
            close (fd);
            return -soerr;
        }
        return fd;
    }

    close (fd);
    return -ETIMEDOUT;
}

int mb_net_connect_while (const char *host, uint16_t port, int *family,
    volatile int *running, int timeout_ms)
{
    return mb_net_connect_cached (host, port, family, running, timeout_ms,
        NULL);
}

int mb_net_connect_cached (const char *host, uint16_t port, int *family,
    volatile int *running, int timeout_ms, struct mb_net_epaddr *cache)
{
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;
    char port_str[8];
    int fd;
    int rc;

    if (timeout_ms <= 0)
        timeout_ms = 5000;

    if (running && !*running)
        return -ECANCELED;

    /* Reconnect hot path: skip DNS entirely when we already have an addr. */
    if (cache && cache->ready) {
        fd = mb_net_connect_sa ((struct sockaddr *) &cache->addr,
            cache->addrlen, cache->family, running, timeout_ms);
        if (fd >= 0) {
            if (family)
                *family = cache->family;
            return fd;
        }
        if (fd == -ECANCELED)
            return fd;
        cache->ready = 0;
    }

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf (port_str, sizeof (port_str), "%u", port);

    /* Prefer numeric resolution (no DNS) so stop() is not blocked on
     * getaddrinfo for IP literals; fall back to full lookup for names. */
    hints.ai_flags = AI_NUMERICHOST;
    rc = getaddrinfo (host, port_str, &hints, &result);
    if (rc != 0) {
        if (running && !*running)
            return -ECANCELED;
        hints.ai_flags = 0;
        rc = getaddrinfo (host, port_str, &hints, &result);
        if (rc != 0)
            return -EADDRNOTAVAIL;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if (running && !*running) {
            freeaddrinfo (result);
            return -ECANCELED;
        }

        fd = mb_net_connect_sa (rp->ai_addr, rp->ai_addrlen, rp->ai_family,
            running, timeout_ms);
        if (fd == -ECANCELED) {
            freeaddrinfo (result);
            return fd;
        }
        if (fd < 0)
            continue;

        if (cache && rp->ai_addrlen <= sizeof (cache->addr)) {
            memcpy (&cache->addr, rp->ai_addr, rp->ai_addrlen);
            cache->addrlen = rp->ai_addrlen;
            cache->family = rp->ai_family;
            cache->ready = 1;
        }
        if (family)
            *family = rp->ai_family;
        freeaddrinfo (result);
        return fd;
    }

    freeaddrinfo (result);
    return -ECONNREFUSED;
}

int mb_net_unix_connect_while (const char *path, volatile int *running,
    int timeout_ms)
{
    struct sockaddr_un sa;
    int fd;
    int rc;
    int budget;

    if (!path || !path[0])
        return -EINVAL;
    if (timeout_ms <= 0)
        timeout_ms = 5000;

    if (running && !*running)
        return -ECANCELED;

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -errno;

    memset (&sa, 0, sizeof (sa));
    sa.sun_family = AF_UNIX;
    if (strlen (path) >= sizeof (sa.sun_path)) {
        close (fd);
        return -ENAMETOOLONG;
    }
    memcpy (sa.sun_path, path, strlen (path) + 1);

    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);

    rc = connect (fd, (struct sockaddr *) &sa, sizeof (sa));
    if (rc == 0)
        return fd;
    if (errno != EINPROGRESS) {
        int err = -errno;
        close (fd);
        return err;
    }

    budget = timeout_ms;
    while (budget > 0) {
        struct pollfd pfd;
        int slice = budget > 50 ? 50 : budget;
        int soerr = 0;
        socklen_t solen = sizeof (soerr);

        if (running && !*running) {
            close (fd);
            return -ECANCELED;
        }

        pfd.fd = fd;
        pfd.events = POLLOUT;
        rc = poll (&pfd, 1, slice);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            close (fd);
            return -errno;
        }
        if (rc == 0) {
            budget -= slice;
            continue;
        }

        if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &soerr, &solen) < 0) {
            close (fd);
            return -errno;
        }
        if (soerr != 0) {
            close (fd);
            return -soerr;
        }
        return fd;
    }

    close (fd);
    return -ETIMEDOUT;
}

int mb_net_bind (const char *host, uint16_t port, int backlog)
{
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;
    char port_str[8];
    int fd;
    int rc;
    int flag = 1;
    const char *bind_host;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    snprintf (port_str, sizeof (port_str), "%u", port);

    if (strcmp (host, "*") == 0 || strcmp (host, "0.0.0.0") == 0)
        bind_host = NULL;
    else
        bind_host = host;

    rc = getaddrinfo (bind_host, port_str, &hints, &result);
    if (rc != 0)
        return -EADDRNOTAVAIL;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;

        setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag));

#ifdef IPV6_V6ONLY
        if (rp->ai_family == AF_INET6) {
            int v6only = 0;
            setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof (v6only));
        }
#endif

        rc = bind (fd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            rc = listen (fd, backlog);
            if (rc == 0) {
                freeaddrinfo (result);
                return fd;
            }
        }

        close (fd);
    }

    freeaddrinfo (result);
    return -EADDRNOTAVAIL;
}
