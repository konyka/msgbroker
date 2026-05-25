#include "ring.h"
#include "../utils/alloc.h"
#include "../utils/list.h"
#include "../utils/cont.h"

#include <string.h>
#include <stdio.h>

static uint32_t mb_ring_hash (const void *data, size_t len)
{
    const uint8_t *buf = (const uint8_t *) data;
    uint32_t h = 0x4D424452;
    uint64_t k;
    size_t i;

    for (i = 0; i + 4 <= len; i += 4) {
        k = (uint64_t) buf[i] | ((uint64_t) buf[i+1] << 8) |
            ((uint64_t) buf[i+2] << 16) | ((uint64_t) buf[i+3] << 24);
        h ^= (uint32_t) k;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    k = 0;
    for (; i < len; i++)
        k |= (uint64_t) buf[i] << ((i % 4) * 8);
    h ^= (uint32_t) k;
    h *= 0x5bd1e995;
    h ^= h >> 15;

    return h;
}

void mb_ring_init (struct mb_ring *self, int virtual_count)
{
    mb_list_init (&self->nodes);
    mb_list_init (&self->vnodes);
    self->virtual_count = virtual_count > 0 ? virtual_count :
        MB_RING_VIRTUAL_NODES;
}

void mb_ring_term (struct mb_ring *self)
{
    struct mb_list_item *it;
    struct mb_list_item *next;

    it = mb_list_begin (&self->vnodes);
    while (it != mb_list_end (&self->vnodes)) {
        struct mb_ring_vnode *vn = mb_cont (it, struct mb_ring_vnode, item);
        next = mb_list_next (&self->vnodes, it);
        mb_list_erase (&self->vnodes, &vn->item);
        mb_free (vn);
        it = next;
    }

    it = mb_list_begin (&self->nodes);
    while (it != mb_list_end (&self->nodes)) {
        struct mb_ring_node *node = mb_cont (it, struct mb_ring_node, item);
        next = mb_list_next (&self->nodes, it);
        mb_list_erase (&self->nodes, &node->item);
        mb_free (node);
        it = next;
    }

    mb_list_term (&self->vnodes);
    mb_list_term (&self->nodes);
}

static void mb_ring_insert_vnode_sorted (struct mb_ring *self,
    struct mb_ring_vnode *vn)
{
    struct mb_list_item *it;
    for (it = mb_list_begin (&self->vnodes);
         it != mb_list_end (&self->vnodes);
         it = mb_list_next (&self->vnodes, it)) {
        struct mb_ring_vnode *existing = mb_cont (it, struct mb_ring_vnode, item);
        if (vn->hash <= existing->hash) {
            mb_list_insert (&self->vnodes, &vn->item, it);
            return;
        }
    }
    mb_list_insert (&self->vnodes, &vn->item, mb_list_end (&self->vnodes));
}

int mb_ring_add (struct mb_ring *self, uint32_t node_id, const char *addr)
{
    struct mb_ring_node *node = (struct mb_ring_node *)
        mb_alloc (sizeof (struct mb_ring_node));
    if (!node)
        return -1;

    node->node_id = node_id;
    strncpy (node->addr, addr, sizeof (node->addr) - 1);
    node->addr[sizeof (node->addr) - 1] = '\0';
    mb_list_item_init (&node->item);
    mb_list_insert (&self->nodes, &node->item, mb_list_end (&self->nodes));

    for (int i = 0; i < self->virtual_count; i++) {
        struct mb_ring_vnode *vn = (struct mb_ring_vnode *)
            mb_alloc (sizeof (struct mb_ring_vnode));
        if (!vn) {
            for (int j = 0; j < i; j++) {
                struct mb_list_item *vi = mb_list_begin (&self->vnodes);
                while (vi != mb_list_end (&self->vnodes)) {
                    struct mb_ring_vnode *v = mb_cont (vi, struct mb_ring_vnode, item);
                    vi = mb_list_next (&self->vnodes, vi);
                    if (v->node_id == node_id) {
                        mb_list_erase (&self->vnodes, &v->item);
                        mb_list_item_term (&v->item);
                        mb_free (v);
                    }
                }
            }
            mb_list_erase (&self->nodes, &node->item);
            mb_list_item_term (&node->item);
            mb_free (node);
            return -1;
        }

        char buf[128];
        int slen = snprintf (buf, sizeof (buf), "%u:%d", node_id, i);
        vn->hash = mb_ring_hash (buf, (size_t) slen);
        vn->node_id = node_id;
        mb_list_item_init (&vn->item);
        mb_ring_insert_vnode_sorted (self, vn);
    }

    return 0;
}

int mb_ring_remove (struct mb_ring *self, uint32_t node_id)
{
    struct mb_list_item *it;
    for (it = mb_list_begin (&self->nodes);
         it != mb_list_end (&self->nodes);
         it = mb_list_next (&self->nodes, it)) {
        struct mb_ring_node *node = mb_cont (it, struct mb_ring_node, item);
        if (node->node_id == node_id) {
            mb_list_erase (&self->nodes, &node->item);
            mb_free (node);
            break;
        }
    }

    it = mb_list_begin (&self->vnodes);
    while (it != mb_list_end (&self->vnodes)) {
        struct mb_ring_vnode *vn = mb_cont (it, struct mb_ring_vnode, item);
        it = mb_list_next (&self->vnodes, it);
        if (vn->node_id == node_id) {
            mb_list_erase (&self->vnodes, &vn->item);
            mb_free (vn);
        }
    }

    return 0;
}

uint32_t mb_ring_lookup (struct mb_ring *self, const void *key,
    size_t keylen)
{
    if (mb_list_empty (&self->vnodes))
        return 0;

    uint32_t h = mb_ring_hash (key, keylen);

    struct mb_list_item *it;
    for (it = mb_list_begin (&self->vnodes);
         it != mb_list_end (&self->vnodes);
         it = mb_list_next (&self->vnodes, it)) {
        struct mb_ring_vnode *vn = mb_cont (it, struct mb_ring_vnode, item);
        if (h <= vn->hash)
            return vn->node_id;
    }

    struct mb_ring_vnode *first =
        mb_cont (mb_list_begin (&self->vnodes), struct mb_ring_vnode, item);
    return first->node_id;
}

uint32_t mb_ring_lookup_n (struct mb_ring *self, const void *key,
    size_t keylen, uint32_t *node_ids, int max_count)
{
    uint32_t primary = mb_ring_lookup (self, key, keylen);
    if (primary == 0 || max_count <= 0)
        return 0;

    int count = 0;
    uint32_t prev = 0;

    struct mb_list_item *it;
    for (it = mb_list_begin (&self->vnodes);
         it != mb_list_end (&self->vnodes);
         it = mb_list_next (&self->vnodes, it)) {
        struct mb_ring_vnode *vn = mb_cont (it, struct mb_ring_vnode, item);
        if (vn->node_id != prev) {
            node_ids[count++] = vn->node_id;
            prev = vn->node_id;
            if (count >= max_count)
                break;
        }
    }

    return (uint32_t) count;
}

int mb_ring_count (struct mb_ring *self)
{
    int count = 0;
    struct mb_list_item *it;
    for (it = mb_list_begin (&self->nodes);
         it != mb_list_end (&self->nodes);
         it = mb_list_next (&self->nodes, it)) {
        count++;
    }
    return count;
}
