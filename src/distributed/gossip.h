#ifndef MB_DISTRIBUTED_GOSSIP_H_INCLUDED
#define MB_DISTRIBUTED_GOSSIP_H_INCLUDED

#include "../pal/mutex.h"
#include "../pal/thread.h"
#include "../pal/atomic.h"
#include "../utils/list.h"

#include <stdint.h>
#include <stddef.h>

#define MB_GOSSIP_MAX_NODES 256
#define MB_GOSSIP_ADDR_LEN 64
#define MB_GOSSIP_DEFAULT_INTERVAL_MS 1000

enum mb_gossip_node_state {
    MB_GOSSIP_NODE_UNKNOWN = 0,
    MB_GOSSIP_NODE_ALIVE = 1,
    MB_GOSSIP_NODE_SUSPECT = 2,
    MB_GOSSIP_NODE_DEAD = 3,
};

struct mb_gossip_node {
    uint32_t node_id;
    char addr[MB_GOSSIP_ADDR_LEN];
    uint64_t incarnation;
    enum mb_gossip_node_state state;
    uint64_t last_seen_ms;
    struct mb_list_item item;
};

struct mb_gossip_config {
    uint32_t local_node_id;
    char local_addr[MB_GOSSIP_ADDR_LEN];
    int interval_ms;
    int suspect_timeout_ms;
    int dead_timeout_ms;
};

typedef void (*mb_gossip_on_change)(void *ctx, struct mb_gossip_node *node,
    enum mb_gossip_node_state old_state);

struct mb_gossip {
    struct mb_gossip_config config;
    struct mb_list nodes;
    struct mb_mutex sync;
    struct mb_thread thread;
    mb_atomic_int running;
    mb_gossip_on_change on_change;
    void *on_change_ctx;
};

void mb_gossip_init (struct mb_gossip *self,
    const struct mb_gossip_config *config);
void mb_gossip_term (struct mb_gossip *self);
int mb_gossip_start (struct mb_gossip *self);
void mb_gossip_stop (struct mb_gossip *self);

int mb_gossip_add_node (struct mb_gossip *self, uint32_t node_id,
    const char *addr);
int mb_gossip_remove_node (struct mb_gossip *self, uint32_t node_id);
struct mb_gossip_node *mb_gossip_find_node (struct mb_gossip *self,
    uint32_t node_id);

int mb_gossip_node_count (struct mb_gossip *self);
void mb_gossip_set_callback (struct mb_gossip *self,
    mb_gossip_on_change cb, void *ctx);

void mb_gossip_tick (struct mb_gossip *self);

#endif
