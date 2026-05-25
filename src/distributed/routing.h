#ifndef MB_DISTRIBUTED_ROUTING_H_INCLUDED
#define MB_DISTRIBUTED_ROUTING_H_INCLUDED

#include "ring.h"
#include "../pal/mutex.h"

#include <stdint.h>
#include <stddef.h>

struct mb_routing {
    struct mb_ring ring;
    struct mb_mutex sync;
};

void mb_routing_init (struct mb_routing *self);
void mb_routing_term (struct mb_routing *self);

int mb_routing_add_node (struct mb_routing *self, uint32_t node_id,
    const char *addr);
int mb_routing_remove_node (struct mb_routing *self, uint32_t node_id);
uint32_t mb_routing_lookup (struct mb_routing *self, const void *key,
    size_t keylen);

#endif
