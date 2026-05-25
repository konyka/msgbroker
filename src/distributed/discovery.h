#ifndef MB_DISTRIBUTED_DISCOVERY_H_INCLUDED
#define MB_DISTRIBUTED_DISCOVERY_H_INCLUDED

#include "../pal/thread.h"
#include "../pal/atomic.h"

#include <stdint.h>
#include <stddef.h>

#define MB_DISCOVERY_MAX_ADDR 64
#define MB_DISCOVERY_DEFAULT_PORT 9700
#define MB_DISCOVERY_DEFAULT_INTERVAL_MS 2000

struct mb_discovery_config {
    uint32_t local_node_id;
    char local_addr[MB_DISCOVERY_MAX_ADDR];
    int port;
    char multicast_group[MB_DISCOVERY_MAX_ADDR];
    int interval_ms;
};

typedef void (*mb_discovery_on_node)(void *ctx, uint32_t node_id,
    const char *addr);

struct mb_discovery {
    struct mb_discovery_config config;
    struct mb_thread thread;
    mb_atomic_int running;
    int sock_fd;
    mb_discovery_on_node on_node;
    void *on_node_ctx;
};

void mb_discovery_init (struct mb_discovery *self,
    const struct mb_discovery_config *config);
void mb_discovery_term (struct mb_discovery *self);
int mb_discovery_start (struct mb_discovery *self);
void mb_discovery_stop (struct mb_discovery *self);

#endif
