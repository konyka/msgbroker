#ifndef MB_POOL_H_INCLUDED
#define MB_POOL_H_INCLUDED

#include "slab.h"
#include "arena.h"
#include <stddef.h>

struct mb_mempool {
    struct mb_arena arena;
    struct mb_slab msg_slab;
    struct mb_slab chunk_slab;
};

void mb_mempool_init (struct mb_mempool *self);
void mb_mempool_term (struct mb_mempool *self);
void *mb_mempool_alloc_msg (struct mb_mempool *self);
void mb_mempool_free_msg (struct mb_mempool *self, void *msg);
void *mb_mempool_alloc (struct mb_mempool *self, size_t size);

#endif
