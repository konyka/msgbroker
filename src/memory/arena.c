#include "arena.h"
#include "../utils/alloc.h"

#include <string.h>
#include <stddef.h>

static struct mb_arena_block *mb_arena_block_new (size_t size)
{
    struct mb_arena_block *blk = (struct mb_arena_block *)
        mb_alloc (sizeof (struct mb_arena_block) + size);
    blk->used = 0;
    blk->size = size;
    blk->next = NULL;
    return blk;
}

void mb_arena_init (struct mb_arena *self, size_t block_size)
{
    self->block_size = block_size > 0 ? block_size : 4096;
    self->blocks = mb_arena_block_new (self->block_size);
}

void mb_arena_term (struct mb_arena *self)
{
    struct mb_arena_block *blk = self->blocks;
    while (blk) {
        struct mb_arena_block *next = blk->next;
        mb_free (blk);
        blk = next;
    }
    self->blocks = NULL;
}

void *mb_arena_alloc (struct mb_arena *self, size_t size)
{
    struct mb_arena_block *blk = self->blocks;
    while (blk) {
        if (blk->used + size <= blk->size) {
            void *ptr = blk->data + blk->used;
            blk->used += size;
            return ptr;
        }
        if (!blk->next)
            blk->next = mb_arena_block_new (
                size > self->block_size ? size : self->block_size);
        blk = blk->next;
    }
    return NULL;
}

void mb_arena_reset (struct mb_arena *self)
{
    struct mb_arena_block *blk = self->blocks;
    while (blk) {
        blk->used = 0;
        blk = blk->next;
    }
}
