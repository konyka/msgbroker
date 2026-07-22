#include "pool.h"
#include "msg.h"

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
    /* Slab-only: heap fallback cannot be freed safely via mb_slab_free. */
    return mb_slab_alloc (&self->msg_slab);
}

void mb_mempool_free_msg (struct mb_mempool *self, void *msg)
{
    if (!msg)
        return;
    mb_slab_free (&self->msg_slab, msg);
}

void *mb_mempool_alloc (struct mb_mempool *self, size_t size)
{
    /* Arena-only: heap fallback would leak on mb_mempool_term. */
    return mb_arena_alloc (&self->arena, size);
}
