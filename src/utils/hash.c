#include "hash.h"
#include "alloc.h"
#include "fast.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t mb_hash_fn (uint32_t key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
    return key;
}

static int mb_hash_grow (struct mb_hash *self);

int mb_hash_init (struct mb_hash *self, size_t nbuckets)
{
    self->nbuckets = 0;
    self->count = 0;
    self->buckets = NULL;

    if (nbuckets == 0)
        return -EINVAL;
    if (nbuckets > SIZE_MAX / sizeof (struct mb_hash_item *))
        return -ENOMEM;

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

    /*  Grow when load factor would exceed 1.0. nbuckets can be 0 only
     *  before init. */
    if (self->count >= self->nbuckets) {
        if (mb_hash_grow (self) < 0)
            return;
    }

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

static int mb_hash_grow (struct mb_hash *self)
{
    size_t new_nb;
    struct mb_hash_item **new_buckets;
    size_t i;

    /*  Double the table; guard against overflow. */
    if (self->nbuckets > SIZE_MAX / 2 / sizeof (struct mb_hash_item *))
        return -ENOMEM;
    new_nb = self->nbuckets * 2;
    if (new_nb < 16)
        new_nb = 16;

    new_buckets = (struct mb_hash_item **) mb_alloc (
        new_nb * sizeof (struct mb_hash_item *));
    if (!new_buckets)
        return -ENOMEM;
    memset (new_buckets, 0, new_nb * sizeof (struct mb_hash_item *));

    /*  Re-insert every existing item into the new bucket array. */
    for (i = 0; i < self->nbuckets; i++) {
        struct mb_hash_item *it = self->buckets[i];
        while (it) {
            struct mb_hash_item *next = it->next;
            size_t idx = mb_hash_fn (it->key) % new_nb;
            it->next = new_buckets[idx];
            new_buckets[idx] = it;
            it = next;
        }
    }

    mb_free (self->buckets);
    self->buckets = new_buckets;
    self->nbuckets = new_nb;
    return 0;
}
