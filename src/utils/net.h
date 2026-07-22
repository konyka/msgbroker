#ifndef MB_NET_H
#define MB_NET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>

/* Cached peer address so reconnect skips blocking getaddrinfo. */
struct mb_net_epaddr {
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int family;
    int ready;
};

int mb_net_parse_addr (const char *addr, char *host, size_t hostlen,
    uint16_t *port);
int mb_net_connect (const char *host, uint16_t port, int *family);
/* Nonblocking connect; returns early if *running becomes 0. timeout_ms
 * is the per-address connect budget (0 = 5s default). */
int mb_net_connect_while (const char *host, uint16_t port, int *family,
    volatile int *running, int timeout_ms);
/* Like connect_while, but reuse *cache when ready and refresh it on DNS. */
int mb_net_connect_cached (const char *host, uint16_t port, int *family,
    volatile int *running, int timeout_ms, struct mb_net_epaddr *cache);
/* Nonblocking AF_UNIX connect; abort when *running clears. */
int mb_net_unix_connect_while (const char *path, volatile int *running,
    int timeout_ms);
int mb_net_bind (const char *host, uint16_t port, int backlog);
/* Best-effort: apply MB_SNDBUF/MB_RCVBUF to a stream fd (values <=0 skipped). */
void mb_net_apply_bufs (int fd, int sndbuf, int rcvbuf);

#endif
