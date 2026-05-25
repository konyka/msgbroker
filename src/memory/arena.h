#ifndef MB_ARENA_H_INCLUDED
#define MB_ARENA_H_INCLUDED

#include <stddef.h>

struct mb_arena_block {
    size_t used;
    size_t size;
    struct mb_arena_block *next;
    char data [];
};

struct mb_arena {
    struct mb_arena_block *blocks;
    size_t block_size;
};

void mb_arena_init (struct mb_arena *self, size_t block_size);
void mb_arena_term (struct mb_arena *self);
void *mb_arena_alloc (struct mb_arena *self, size_t size);
void mb_arena_reset (struct mb_arena *self);

#endif
