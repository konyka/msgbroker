#include "../../src/memory/chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

int main (void)
{
    void *c1;
    void *c2;
    void *p;
    int rc;

    rc = mb_chunk_alloc (100, &c1);
    assert (rc == 0);
    assert (mb_chunk_size (c1) == 100);

    memset (c1, 'A', 100);
    mb_chunk_addref (c1, 1);
    assert (mb_chunk_size (c1) == 100);

    mb_chunk_free (c1);
    mb_chunk_free (c1);

    rc = mb_chunk_alloc (200, &c2);
    assert (rc == 0);
    assert (mb_chunk_size (c2) == 200);

    rc = mb_chunk_realloc (500, &c2);
    assert (rc == 0);
    assert (mb_chunk_size (c2) == 500);

    /* size_t wrap must not succeed with a tiny backing allocation. */
    p = (void *) 0x1;
    assert (mb_chunk_alloc (SIZE_MAX, &p) == -ENOMEM);
    assert (mb_chunk_alloc (SIZE_MAX - 1, &p) == -ENOMEM);
    assert (mb_chunk_realloc (SIZE_MAX, &c2) == -ENOMEM);
    assert (mb_chunk_size (c2) == 500);
    mb_chunk_free (c2);
    printf ("  chunk_size_overflow: OK\n");

    printf ("test_chunk: PASSED\n");
    return 0;
}
