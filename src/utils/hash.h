#ifndef MB_HASH_H_INCLUDED
#define MB_HASH_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

struct mb_hash_item {
    uint32_t key;
    struct mb_hash_item *next;
};

struct mb_hash {
    struct mb_hash_item **buckets;
    size_t nbuckets;
    size_t count;
};

void mb_hash_init (struct mb_hash *self, size_t nbuckets);
void mb_hash_term (struct mb_hash *self);
void mb_hash_insert (struct mb_hash *self, uint32_t key,
    struct mb_hash_item *item);
void mb_hash_erase (struct mb_hash *self, uint32_t key);
struct mb_hash_item *mb_hash_find (struct mb_hash *self, uint32_t key);
size_t mb_hash_count (struct mb_hash *self);

#endif
