#include "../../src/memory/slab.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main (void)
{
    struct mb_slab slab;
    mb_slab_init (&slab, 64, 16);

    void *objs [16];
    int i;
    for (i = 0; i < 16; i++) {
        objs[i] = mb_slab_alloc (&slab);
        assert (objs[i] != NULL);
        memset (objs[i], i, 64);
    }
    void *overflow = mb_slab_alloc (&slab);
    assert (overflow == NULL);

    mb_slab_free (&slab, objs[5]);
    mb_slab_free (&slab, objs[10]);

    void *r1 = mb_slab_alloc (&slab);
    assert (r1 != NULL);
    void *r2 = mb_slab_alloc (&slab);
    assert (r2 != NULL);

    mb_slab_term (&slab);

    /* Huge freelist: alloc fails → empty slab, no crash. */
    mb_slab_init (&slab, 64, (size_t) -1 / 4);
    assert (mb_slab_alloc (&slab) == NULL);
    mb_slab_term (&slab);
    printf ("  slab_init_oom: OK\n");

    printf ("test_slab: PASSED\n");
    return 0;
}
