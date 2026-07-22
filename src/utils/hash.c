#include "hash.h"
#include "alloc.h"
#include "fast.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

static uint32_t mb_hash_fn (uint32_t key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
    return key;
}

int mb_hash_init (struct mb_hash *self, size_t nbuckets)
{
    self->nbuckets = 0;
    self->count = 0;
    self->buckets = NULL;

    if (nbuckets == 0)
        return -EINVAL;

    self->buckets = (struct mb_hash_item **) mb_alloc (
        nbuckets * sizeof (struct mb_hash_item *));
    if (!self->buckets)
        return -ENOMEM;

    memset (self->buckets, 0, nbuckets * sizeof (struct mb_hash_item *));
    self->nbuckets = nbuckets;
    return 0;
}

void mb_hash_term (struct mb_hash *self)
{
    mb_free (self->buckets);
    self->buckets = NULL;
    self->nbuckets = 0;
    self->count = 0;
}

void mb_hash_insert (struct mb_hash *self, uint32_t key,
    struct mb_hash_item *item)
{
    size_t idx;

    if (!self->buckets || self->nbuckets == 0)
        return;

    idx = mb_hash_fn (key) % self->nbuckets;
    item->key = key;
    item->next = self->buckets[idx];
    self->buckets[idx] = item;
    self->count++;
}

void mb_hash_erase (struct mb_hash *self, uint32_t key)
{
    size_t idx;
    struct mb_hash_item **pp;

    if (!self->buckets || self->nbuckets == 0)
        return;

    idx = mb_hash_fn (key) % self->nbuckets;
    pp = &self->buckets[idx];
    while (*pp) {
        if ((*pp)->key == key) {
            *pp = (*pp)->next;
            self->count--;
            return;
        }
        pp = &(*pp)->next;
    }
}

struct mb_hash_item *mb_hash_find (struct mb_hash *self, uint32_t key)
{
    size_t idx;
    struct mb_hash_item *it;

    if (!self->buckets || self->nbuckets == 0)
        return NULL;

    idx = mb_hash_fn (key) % self->nbuckets;
    it = self->buckets[idx];
    while (it) {
        if (it->key == key)
            return it;
        it = it->next;
    }
    return NULL;
}

size_t mb_hash_count (struct mb_hash *self)
{
    return self->count;
}
