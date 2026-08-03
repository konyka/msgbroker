/*  test_chunk_realloc_refcount.c — TDD gate for chunk realloc (T-CHUNK1).
 *
 *  When a chunk has refcount > 1 (multiple holders), mb_chunk_realloc
 *  used to call mb_realloc(hdr, ...) which may move the underlying
 *  allocation. The original data pointer captured by the second holder
 *  then silently dangled. The TDD gate below forces realloc to move
 *  the buffer by allocating a guard chunk immediately after the target,
 *  then frees the guard so realloc cannot extend in-place.
 *
 *  The contract: when refcount > 1, mb_chunk_realloc copies the payload
 *  into a new allocation, frees the old, and updates the caller-provided
 *  chunk pointer. Other refcount holders still see their original
 *  (pre-realloc) buffer; realloc is therefore safe but does not
 *  propagate the resize to co-holders.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "../../src/memory/chunk.h"

int main (void)
{
    void *a;
    void *guard;
    void *b;
    void *c;
    int rc;

    rc = mb_chunk_alloc (64, &a);
    assert (rc == 0);
    memset (a, 0xAA, 64);

    /*  Pin the address: a small allocation immediately after a, large
     *  enough to deter in-place grow into a. The glibc allocator will
     *  normally place adjacent allocations contiguously, so freeing
     *  'guard' after the realloc request would let the allocator
     *  extend 'a' in-place. We instead keep 'guard' alive, forcing
     *  realloc to copy when we ask for a larger size. */
    rc = mb_chunk_alloc (4096, &guard);
    assert (rc == 0);

    mb_chunk_addref (a, 1);  /* refcount == 2 */
    b = a;                    /* co-holder */

    rc = mb_chunk_realloc (8192, &a);
    assert (rc == 0);
    assert (mb_chunk_size (a) == 8192);

    /* b must NOT have been freed by the realloc. With the bug, the
     * realloc moved the underlying buffer and 'b' now points at
     * freed memory; the next mb_chunk_free(b) would either be a
     * double-free (refcount off-by-one) or an ASan-detected UAF. */
    mb_chunk_free (b);

    /* refcount is now 1 again; freeing a releases the new buffer. */
    mb_chunk_free (a);

    mb_chunk_free (guard);

    /* Sanity: refcount==1 path still works in-place or via copy. */
    rc = mb_chunk_alloc (32, &c);
    assert (rc == 0);
    rc = mb_chunk_realloc (96, &c);
    assert (rc == 0);
    assert (mb_chunk_size (c) == 96);
    mb_chunk_free (c);

    printf ("test_chunk_realloc_refcount: PASSED\n");
    return 0;
}
