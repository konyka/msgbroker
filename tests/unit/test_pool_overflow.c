#include "../../src/memory/pool.h"
#include "../../src/memory/msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>

/*
 * TDD gate for T-POOL-GROW.
 *
 * Verifies the arena-backed overflow path of mb_mempool_alloc_msg /
 * mb_mempool_free_msg:
 *
 *   1. The slab holds 1024 entries. After exhausting it, further
 *      mb_mempool_alloc_msg calls must still succeed (not return NULL),
 *      routing through the arena the pool already owns.
 *
 *   2. Freeing a message allocated past the slab boundary must NOT be
 *      double-freed (i.e. must NOT be routed back to mb_slab_free, which
 *      would either silently no-op or — with the slab-header guard — be
 *      ignored because the raw address is not on the slab freelist).
 *      Under ASan (allocator_may_return_null=1, detect_leaks=1), leaking
 *      or double-freeing any of the 1030 allocations surfaces here.
 *
 *   3. The slab portion (alloc/free within 1024) must continue to behave
 *      identically: pointers must remain valid, idempotent alloc/free
 *      pairs must round-trip.
 *
 *   4. mb_mempool_term must still tear everything down without leaks.
 */

#define MB_MSG_SLAB_CAP 1024
#define OVERFLOW_EXTRA 6              /* 1030 - 1024 = 6 arena msgs */
#define TOTAL_MSGS (MB_MSG_SLAB_CAP + OVERFLOW_EXTRA)

int main (void)
{
    struct mb_mempool pool;
    void *msgs [TOTAL_MSGS];
    size_t i;

    mb_mempool_init (&pool);

    /* Allocate 1030 messages: first 1024 from the slab, last 6 from
     * the arena overflow path. All must be non-NULL and writable. */
    for (i = 0; i < TOTAL_MSGS; i++) {
        msgs[i] = mb_mempool_alloc_msg (&pool);
        assert (msgs[i] != NULL);
        /* Touch every byte of the user-visible struct to ensure the
         * chunk is fully addressable, regardless of which allocator
         * served it. */
        memset (msgs[i], (int) (i & 0xff), sizeof (struct mb_msg));
    }

    /* Free all 1030 messages. The slab frees must round-trip cleanly;
     * the arena frees must not double-free the slab freelist (which
     * would otherwise leave dangling references for the slab-allocs
     * freed later). */
    for (i = 0; i < TOTAL_MSGS; i++)
        mb_mempool_free_msg (&pool, msgs[i]);

    /* NULL is a documented no-op for mb_mempool_free_msg. */
    mb_mempool_free_msg (&pool, NULL);

    /* Re-allocate across the slab/arena boundary to make sure both
     * paths are reusable, not just one-shot. */
    for (i = 0; i < TOTAL_MSGS; i++) {
        msgs[i] = mb_mempool_alloc_msg (&pool);
        assert (msgs[i] != NULL);
        memset (msgs[i], (int) ((i + 1) & 0xff), sizeof (struct mb_msg));
    }
    for (i = 0; i < TOTAL_MSGS; i++)
        mb_mempool_free_msg (&pool, msgs[i]);

    mb_mempool_term (&pool);
    /* ASan (set in tests/CMakeLists.txt ENVIRONMENT) reports leaks
     * here on failure. */
    printf ("  pool_overflow_1030: OK\n");
    printf ("test_pool_overflow: PASSED\n");
    return 0;
}