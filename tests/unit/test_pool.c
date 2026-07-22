#include "../../src/memory/pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main (void)
{
    struct mb_mempool pool;
    void *msgs [1025];
    int i;

    mb_mempool_init (&pool);

    for (i = 0; i < 20; i++) {
        msgs[i] = mb_mempool_alloc_msg (&pool);
        assert (msgs[i] != NULL);
    }
    for (i = 0; i < 20; i++)
        mb_mempool_free_msg (&pool, msgs[i]);

    void *p1 = mb_mempool_alloc (&pool, 128);
    assert (p1 != NULL);
    memset (p1, 'B', 128);

    /* Exhaust msg_slab (1024); overflow must be NULL, not an orphan heap ptr. */
    for (i = 0; i < 1024; i++) {
        msgs[i] = mb_mempool_alloc_msg (&pool);
        assert (msgs[i] != NULL);
    }
    msgs[1024] = mb_mempool_alloc_msg (&pool);
    assert (msgs[1024] == NULL);
    for (i = 0; i < 1024; i++)
        mb_mempool_free_msg (&pool, msgs[i]);
    mb_mempool_free_msg (&pool, NULL);

    mb_mempool_term (&pool);
    printf ("  pool_msg_exhaust: OK\n");
    printf ("test_pool: PASSED\n");
    return 0;
}
