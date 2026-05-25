#include "routing.h"

void mb_routing_init (struct mb_routing *self)
{
    mb_ring_init (&self->ring, MB_RING_VIRTUAL_NODES);
    mb_mutex_init (&self->sync);
}

void mb_routing_term (struct mb_routing *self)
{
    mb_mutex_term (&self->sync);
    mb_ring_term (&self->ring);
}

int mb_routing_add_node (struct mb_routing *self, uint32_t node_id,
    const char *addr)
{
    mb_mutex_lock (&self->sync);
    int rc = mb_ring_add (&self->ring, node_id, addr);
    mb_mutex_unlock (&self->sync);
    return rc;
}

int mb_routing_remove_node (struct mb_routing *self, uint32_t node_id)
{
    mb_mutex_lock (&self->sync);
    int rc = mb_ring_remove (&self->ring, node_id);
    mb_mutex_unlock (&self->sync);
    return rc;
}

uint32_t mb_routing_lookup (struct mb_routing *self, const void *key,
    size_t keylen)
{
    mb_mutex_lock (&self->sync);
    uint32_t node_id = mb_ring_lookup (&self->ring, key, keylen);
    mb_mutex_unlock (&self->sync);
    return node_id;
}
