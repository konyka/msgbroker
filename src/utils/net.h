#ifndef MB_NET_H
#define MB_NET_H

#include <stdint.h>
#include <stddef.h>

int mb_net_parse_addr (const char *addr, char *host, size_t hostlen,
    uint16_t *port);
int mb_net_connect (const char *host, uint16_t port, int *family);
/* Nonblocking connect; returns early if *running becomes 0. timeout_ms
 * is the per-address connect budget (0 = 5s default). */
int mb_net_connect_while (const char *host, uint16_t port, int *family,
    volatile int *running, int timeout_ms);
/* Nonblocking AF_UNIX connect; abort when *running clears. */
int mb_net_unix_connect_while (const char *path, volatile int *running,
    int timeout_ms);
int mb_net_bind (const char *host, uint16_t port, int backlog);

#endif
