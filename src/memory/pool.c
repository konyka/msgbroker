#include "pool.h"
#include "msg.h"
#include "../utils/alloc.h"

#include <stddef.h>

void mb_mempool_init (struct mb_mempool *self)
{
    mb_arena_init (&self->arena, 65536);
    mb_slab_init (&self->msg_slab, sizeof (struct mb_msg), 1024);
    mb_slab_init (&self->chunk_slab, 256, 2048);
}

void mb_mempool_term (struct mb_mempool *self)
{
    mb_slab_term (&self->chunk_slab);
    mb_slab_term (&self->msg_slab);
    mb_arena_term (&self->arena);
}

void *mb_mempool_alloc_msg (struct mb_mempool *self)
{
    void *msg = mb_slab_alloc (&self->msg_slab);
    if (!msg)
        msg = mb_alloc (sizeof (struct mb_msg));
    return msg;
}

void mb_mempool_free_msg (struct mb_mempool *self, void *msg)
{
    mb_slab_free (&self->msg_slab, msg);
}

void *mb_mempool_alloc (struct mb_mempool *self, size_t size)
{
    void *ptr = mb_arena_alloc (&self->arena, size);
    if (!ptr)
        ptr = mb_alloc (size);
    return ptr;
}
