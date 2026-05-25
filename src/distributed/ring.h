#ifndef MB_DISTRIBUTED_RING_H_INCLUDED
#define MB_DISTRIBUTED_RING_H_INCLUDED

#include "../utils/list.h"

#include <stdint.h>
#include <stddef.h>

#define MB_RING_VIRTUAL_NODES 150

struct mb_ring_vnode {
    uint32_t hash;
    uint32_t node_id;
    struct mb_list_item item;
};

struct mb_ring {
    struct mb_list nodes;
    struct mb_list vnodes;
    int virtual_count;
    uint32_t _padding;
};

struct mb_ring_node {
    uint32_t node_id;
    char addr[64];
    struct mb_list_item item;
};

void mb_ring_init (struct mb_ring *self, int virtual_count);
void mb_ring_term (struct mb_ring *self);

int mb_ring_add (struct mb_ring *self, uint32_t node_id, const char *addr);
int mb_ring_remove (struct mb_ring *self, uint32_t node_id);
uint32_t mb_ring_lookup (struct mb_ring *self, const void *key,
    size_t keylen);
uint32_t mb_ring_lookup_n (struct mb_ring *self, const void *key,
    size_t keylen, uint32_t *node_ids, int max_count);

int mb_ring_count (struct mb_ring *self);

#endif
