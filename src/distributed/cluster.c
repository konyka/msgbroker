#include "cluster.h"
#include <string.h>
#include <errno.h>

static void mb_cluster_on_discovery (void *ctx, uint32_t node_id,
    const char *addr)
{
    struct mb_cluster *cluster = (struct mb_cluster *) ctx;
    mb_cluster_join_node (cluster, node_id, addr);
}

static void mb_cluster_on_gossip_change (void *ctx,
    struct mb_gossip_node *node, enum mb_gossip_node_state old_state)
{
    struct mb_cluster *cluster = (struct mb_cluster *) ctx;
    (void) old_state;

    if (node->state == MB_GOSSIP_NODE_DEAD)
        mb_cluster_leave_node (cluster, node->node_id);
}

void mb_cluster_init (struct mb_cluster *self, uint32_t node_id,
    const char *addr)
{
    self->state = MB_CLUSTER_STATE_INIT;
    self->local_node_id = node_id;
    strncpy (self->local_addr, addr, MB_GOSSIP_ADDR_LEN - 1);
    self->local_addr[MB_GOSSIP_ADDR_LEN - 1] = '\0';

    struct mb_gossip_config gcfg;
    gcfg.local_node_id = node_id;
    strncpy (gcfg.local_addr, addr, MB_GOSSIP_ADDR_LEN - 1);
    gcfg.local_addr[MB_GOSSIP_ADDR_LEN - 1] = '\0';
    gcfg.interval_ms = MB_GOSSIP_DEFAULT_INTERVAL_MS;
    gcfg.suspect_timeout_ms = 0;
    gcfg.dead_timeout_ms = 0;
    mb_gossip_init (&self->gossip, &gcfg);
    mb_gossip_set_callback (&self->gossip, mb_cluster_on_gossip_change, self);

    struct mb_discovery_config dcfg;
    memset (&dcfg, 0, sizeof (dcfg));
    dcfg.local_node_id = node_id;
    strncpy (dcfg.local_addr, addr, MB_DISCOVERY_MAX_ADDR - 1);
    dcfg.port = MB_DISCOVERY_DEFAULT_PORT;
    strncpy (dcfg.multicast_group, "239.255.0.1", MB_DISCOVERY_MAX_ADDR - 1);
    dcfg.interval_ms = MB_DISCOVERY_DEFAULT_INTERVAL_MS;
    mb_discovery_init (&self->discovery, &dcfg);
    self->discovery.on_node = mb_cluster_on_discovery;
    self->discovery.on_node_ctx = self;

    mb_routing_init (&self->routing);
}

void mb_cluster_term (struct mb_cluster *self)
{
    mb_cluster_stop (self);
    mb_routing_term (&self->routing);
    mb_discovery_term (&self->discovery);
    mb_gossip_term (&self->gossip);
}

int mb_cluster_start (struct mb_cluster *self)
{
    int rc;

    rc = mb_gossip_start (&self->gossip);
    if (rc != 0)
        return rc;

    rc = mb_discovery_start (&self->discovery);
    if (rc != 0) {
        mb_gossip_stop (&self->gossip);
        return rc;
    }

    self->state = MB_CLUSTER_STATE_ACTIVE;
    return 0;
}

void mb_cluster_stop (struct mb_cluster *self)
{
    if (self->state == MB_CLUSTER_STATE_ACTIVE ||
        self->state == MB_CLUSTER_STATE_JOINING) {
        self->state = MB_CLUSTER_STATE_STOPPED;
        mb_discovery_stop (&self->discovery);
        mb_gossip_stop (&self->gossip);
    }
}

int mb_cluster_join_node (struct mb_cluster *self, uint32_t node_id,
    const char *addr)
{
    int rc = mb_gossip_add_node (&self->gossip, node_id, addr);
    if (rc != 0 && rc != -EEXIST)
        return rc;

    rc = mb_routing_add_node (&self->routing, node_id, addr);
    return rc;
}

int mb_cluster_leave_node (struct mb_cluster *self, uint32_t node_id)
{
    mb_gossip_remove_node (&self->gossip, node_id);
    mb_routing_remove_node (&self->routing, node_id);
    return 0;
}

uint32_t mb_cluster_route_key (struct mb_cluster *self, const void *key,
    size_t keylen)
{
    return mb_routing_lookup (&self->routing, key, keylen);
}

int mb_cluster_node_count (struct mb_cluster *self)
{
    return mb_gossip_node_count (&self->gossip);
}
