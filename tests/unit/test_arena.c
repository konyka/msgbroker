#include "../../src/memory/arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int main (void)
{
    struct mb_arena arena;
    mb_arena_init (&arena, 256);

    void *p1 = mb_arena_alloc (&arena, 100);
    assert (p1 != NULL);
    memset (p1, 'A', 100);

    void *p2 = mb_arena_alloc (&arena, 100);
    assert (p2 != NULL);
    assert (p1 != p2);

    void *p3 = mb_arena_alloc (&arena, 200);
    assert (p3 != NULL);

    mb_arena_reset (&arena);
    void *p4 = mb_arena_alloc (&arena, 50);
    assert (p4 != NULL);

    mb_arena_term (&arena);

    /* Huge first block: alloc fails → empty arena, no crash. */
    {
        size_t huge = (size_t) -1 / 4;
        mb_arena_init (&arena, huge);
        assert (mb_arena_alloc (&arena, 1) == NULL);
        mb_arena_reset (&arena);
        mb_arena_term (&arena);
        printf ("  arena_init_oom: OK\n");
    }

    /* size_t wrap must not succeed with a tiny backing allocation. */
    mb_arena_init (&arena, SIZE_MAX);
    assert (mb_arena_alloc (&arena, 1) == NULL);
    mb_arena_term (&arena);

    mb_arena_init (&arena, 256);
    assert (mb_arena_alloc (&arena, SIZE_MAX) == NULL);
    assert (mb_arena_alloc (&arena, SIZE_MAX - 1) == NULL);
    mb_arena_term (&arena);
    printf ("  arena_size_overflow: OK\n");

    printf ("test_arena: PASSED\n");
    return 0;
}
