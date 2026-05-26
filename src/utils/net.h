#ifndef MB_NET_H
#define MB_NET_H

#include <stdint.h>
#include <stddef.h>

int mb_net_parse_addr (const char *addr, char *host, size_t hostlen,
    uint16_t *port);
int mb_net_connect (const char *host, uint16_t port, int *family);
int mb_net_bind (const char *host, uint16_t port, int backlog);

#endif
