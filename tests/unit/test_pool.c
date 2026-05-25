#include "../../src/memory/pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main (void)
{
    struct mb_mempool pool;
    mb_mempool_init (&pool);

    void *msgs [20];
    int i;
    for (i = 0; i < 20; i++) {
        msgs[i] = mb_mempool_alloc_msg (&pool);
        assert (msgs[i] != NULL);
    }
    for (i = 0; i < 20; i++) {
        mb_mempool_free_msg (&pool, msgs[i]);
    }

    void *p1 = mb_mempool_alloc (&pool, 128);
    assert (p1 != NULL);
    memset (p1, 'B', 128);

    mb_mempool_term (&pool);
    printf ("test_pool: PASSED\n");
    return 0;
}
