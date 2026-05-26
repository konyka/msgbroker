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
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;
    char port_str[8];
    int fd;
    int rc;
    int flag = 1;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf (port_str, sizeof (port_str), "%u", port);

    rc = getaddrinfo (host, port_str, &hints, &result);
    if (rc != 0)
        return -EADDRNOTAVAIL;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;

        setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof (flag));

        rc = connect (fd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            if (family)
                *family = rp->ai_family;
            freeaddrinfo (result);
            return fd;
        }

        close (fd);
    }

    freeaddrinfo (result);
    return -ECONNREFUSED;
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
