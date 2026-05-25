#ifndef MB_DISTRIBUTED_CLUSTER_H_INCLUDED
#define MB_DISTRIBUTED_CLUSTER_H_INCLUDED

#include "gossip.h"
#include "discovery.h"
#include "routing.h"

#include <stdint.h>

enum mb_cluster_state {
    MB_CLUSTER_STATE_INIT = 0,
    MB_CLUSTER_STATE_JOINING = 1,
    MB_CLUSTER_STATE_ACTIVE = 2,
    MB_CLUSTER_STATE_LEAVING = 3,
    MB_CLUSTER_STATE_STOPPED = 4,
};

struct mb_cluster {
    enum mb_cluster_state state;
    uint32_t local_node_id;
    char local_addr[MB_GOSSIP_ADDR_LEN];

    struct mb_gossip gossip;
    struct mb_discovery discovery;
    struct mb_routing routing;
};

void mb_cluster_init (struct mb_cluster *self, uint32_t node_id,
    const char *addr);
void mb_cluster_term (struct mb_cluster *self);
int mb_cluster_start (struct mb_cluster *self);
void mb_cluster_stop (struct mb_cluster *self);

int mb_cluster_join_node (struct mb_cluster *self, uint32_t node_id,
    const char *addr);
int mb_cluster_leave_node (struct mb_cluster *self, uint32_t node_id);
uint32_t mb_cluster_route_key (struct mb_cluster *self, const void *key,
    size_t keylen);
int mb_cluster_node_count (struct mb_cluster *self);

#endif
