/*  test_hash_rehash.c — TDD gate for hash rehash (T-CAND8).
 *
 *  Pre-fix: nbuckets was fixed at init. Inserting more than
 *  nbuckets * load_factor items left a long chain under a single
 *  bucket; find degraded to O(chain).
 *
 *  The new contract: when count exceeds nbuckets (load factor 1.0
 *  with the small-nbuckets default), the table doubles in size and
 *  re-inserts every existing item. find must return each inserted
 *  item and the maximum chain length must be bounded.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../../src/utils/hash.h"

#define ITEMS 256
#define INITIAL_BUCKETS 4

int main (void)
{
    struct mb_hash h;
    struct mb_hash_item *items;
    int rc, i;

    items = (struct mb_hash_item *) calloc (ITEMS, sizeof (*items));
    assert (items != NULL);

    rc = mb_hash_init (&h, INITIAL_BUCKETS);
    assert (rc == 0);

    /* Insert enough to force several rehash rounds. */
    for (i = 0; i < ITEMS; i++) {
        items[i].key = (uint32_t) (i * 0x9E3779B1u + 1);
        mb_hash_insert (&h, items[i].key, &items[i]);
    }

    /* find must locate every inserted item. */
    for (i = 0; i < ITEMS; i++) {
        struct mb_hash_item *f = mb_hash_find (&h, items[i].key);
        assert (f == &items[i]);
    }

    /* count matches. */
    assert (mb_hash_count (&h) == (size_t) ITEMS);

    /* nbuckets has grown (was 4, must be > 4 after rehash). */
    assert (h.nbuckets > INITIAL_BUCKETS);

    /* Erase half and re-find the other half. */
    for (i = 0; i < ITEMS; i += 2)
        mb_hash_erase (&h, items[i].key);
    for (i = 0; i < ITEMS; i += 2)
        assert (mb_hash_find (&h, items[i].key) == NULL);
    for (i = 1; i < ITEMS; i += 2)
        assert (mb_hash_find (&h, items[i].key) == &items[i]);

    mb_hash_term (&h);
    free (items);

    printf ("test_hash_rehash: PASSED (final nbuckets=%zu count=%zu)\n",
        h.nbuckets, mb_hash_count (&h));
    return 0;
}
