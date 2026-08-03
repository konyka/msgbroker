#include "ring.h"
#include "../utils/alloc.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

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
    self->nodes = NULL;
    self->node_count = 0;
    self->node_cap = 0;
    self->vnodes = NULL;
    self->vnode_count = 0;
    self->vnode_cap = 0;
    self->virtual_count = virtual_count > 0 ? virtual_count :
        MB_RING_VIRTUAL_NODES;
}

void mb_ring_term (struct mb_ring *self)
{
    if (self->vnodes)
        mb_free (self->vnodes);
    if (self->nodes)
        mb_free (self->nodes);
    self->vnodes = NULL;
    self->vnode_count = 0;
    self->vnode_cap = 0;
    self->nodes = NULL;
    self->node_count = 0;
    self->node_cap = 0;
}

static int mb_ring_grow_nodes (struct mb_ring *self)
{
    if (self->node_count < self->node_cap)
        return 0;
    size_t new_cap = self->node_cap ? self->node_cap * 2 : 4;
    struct mb_ring_node *grown = (struct mb_ring_node *)
        mb_realloc (self->nodes, new_cap * sizeof (*grown));
    if (!grown)
        return -1;
    self->nodes = grown;
    self->node_cap = new_cap;
    return 0;
}

static int mb_ring_grow_vnodes (struct mb_ring *self)
{
    if (self->vnode_count < self->vnode_cap)
        return 0;
    size_t new_cap = self->vnode_cap ? self->vnode_cap * 2 : 16;
    struct mb_ring_vnode *grown = (struct mb_ring_vnode *)
        mb_realloc (self->vnodes, new_cap * sizeof (*grown));
    if (!grown)
        return -1;
    self->vnodes = grown;
    self->vnode_cap = new_cap;
    return 0;
}

/* Insert vn into vnodes[] keeping it sorted by hash (ascending). */
static void mb_ring_insert_vnode_sorted (struct mb_ring *self,
    struct mb_ring_vnode vn)
{
    size_t i = 0;
    while (i < self->vnode_count && self->vnodes[i].hash < vn.hash)
        i++;
    if (i < self->vnode_count)
        memmove (&self->vnodes[i + 1], &self->vnodes[i],
            (self->vnode_count - i) * sizeof (*self->vnodes));
    self->vnodes[i] = vn;
    self->vnode_count++;
}

int mb_ring_add (struct mb_ring *self, uint32_t node_id, const char *addr)
{
    /* Reject duplicates: a duplicate add would multiply virtual nodes and
       skew the consistent-hash distribution. Discovery can deliver the
       same node_id multiple times on retransmit; callers must therefore
       tolerate -EEXIST. */
    for (size_t i = 0; i < self->node_count; i++) {
        if (self->nodes[i].node_id == node_id)
            return -EEXIST;
    }

    if (mb_ring_grow_nodes (self) != 0)
        return -1;

    struct mb_ring_node *node = &self->nodes[self->node_count++];
    node->node_id = node_id;
    strncpy (node->addr, addr, sizeof (node->addr) - 1);
    node->addr[sizeof (node->addr) - 1] = '\0';

    for (int i = 0; i < self->virtual_count; i++) {
        if (mb_ring_grow_vnodes (self) != 0) {
            size_t write = 0;
            for (size_t j = 0; j < self->vnode_count; j++) {
                if (self->vnodes[j].node_id == node_id)
                    continue;
                if (write != j)
                    self->vnodes[write] = self->vnodes[j];
                write++;
            }
            self->vnode_count = write;

            self->node_count--;
            return -1;
        }

        struct mb_ring_vnode vn;
        char buf[128];
        int slen = snprintf (buf, sizeof (buf), "%u:%d", node_id, i);
        vn.hash = mb_ring_hash (buf, (size_t) slen);
        vn.node_id = node_id;
        mb_ring_insert_vnode_sorted (self, vn);
    }

    return 0;
}

int mb_ring_remove (struct mb_ring *self, uint32_t node_id)
{
    /* Drop the node entry if present. */
    for (size_t i = 0; i < self->node_count; i++) {
        if (self->nodes[i].node_id == node_id) {
            if (i + 1 < self->node_count)
                memmove (&self->nodes[i], &self->nodes[i + 1],
                    (self->node_count - i - 1) * sizeof (*self->nodes));
            self->node_count--;
            break;
        }
    }

    /* Compact out every vnode belonging to node_id; the array stays
       sorted because every removal only shrinks the run. */
    size_t write = 0;
    for (size_t j = 0; j < self->vnode_count; j++) {
        if (self->vnodes[j].node_id == node_id)
            continue;
        if (write != j)
            self->vnodes[write] = self->vnodes[j];
        write++;
    }
    self->vnode_count = write;

    return 0;
}

uint32_t mb_ring_lookup (struct mb_ring *self, const void *key,
    size_t keylen)
{
    if (self->vnode_count == 0)
        return 0;

    uint32_t h = mb_ring_hash (key, keylen);

    /* Lower-bound: first vnode whose hash >= h.  Standard binary
       search on a sorted array; we can't use libc bsearch() because
       we need the *insertion index*, not just an equality hit. */
    size_t lo = 0;
    size_t hi = self->vnode_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (self->vnodes[mid].hash < h)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == self->vnode_count)
        lo = 0;     /* wrap around */
    return self->vnodes[lo].node_id;
}

uint32_t mb_ring_lookup_n (struct mb_ring *self, const void *key,
    size_t keylen, uint32_t *node_ids, int max_count)
{
    if (max_count <= 0 || self->vnode_count == 0)
        return 0;

    uint32_t h = mb_ring_hash (key, keylen);

    /* Locate the primary's index via lower-bound binary search. */
    size_t lo = 0;
    size_t hi = self->vnode_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (self->vnodes[mid].hash < h)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == self->vnode_count)
        lo = 0;

    /* Walk the ring starting at the primary, collecting distinct
       node_ids.  We stop when we either have max_count or have
       completed one revolution (i.e. reached lo again), whichever
       comes first.  A node_id is only collected if it hasn't been
       collected yet on this walk — without that check, a sparsely
       populated ring would re-emit the primary once the walk
       crosses a node that wraps back to its hash range. */
    int count = 0;
    size_t idx = lo;
    for (size_t step = 0; step <= self->vnode_count; step++) {
        if (step == self->vnode_count)
            break;
        uint32_t nid = self->vnodes[idx].node_id;
        int already = 0;
        for (int k = 0; k < count; k++) {
            if (node_ids[k] == nid) { already = 1; break; }
        }
        if (!already) {
            node_ids[count++] = nid;
            if (count >= max_count)
                break;
        }
        idx++;
        if (idx == self->vnode_count)
            idx = 0;
    }

    return (uint32_t) count;
}

int mb_ring_count (struct mb_ring *self)
{
    return (int) self->node_count;
}