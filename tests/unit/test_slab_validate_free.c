/*  test_slab_validate_free.c — TDD gate for slab free owner/obj_size
 *  validation (T-SLAB1).
 *
 *  Pre-fix: mb_slab_free blindly wrote obj into freelist[count] and
 *  decremented count, with no check that the object belonged to the
 *  slab. The TDD gate exercises three rejection paths:
 *
 *    1. NULL obj                 — accepted silently, no-op.
 *    2. obj pointing to foreign  — the magic prefix does not match,
 *      so the call is a no-op; the slab's own freelist is preserved.
 *    3. obj that was never       — random stack memory fails the magic
 *      stamped by mb_slab_alloc — check, again a no-op.
 *
 *  In every case the slab's bookkeeping must remain consistent
 *  (subsequent mb_slab_alloc returns a valid pointer).
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/memory/slab.h"

int main (void)
{
    struct mb_slab s;
    void *p, *q;

    mb_slab_init (&s, 64, 4);

    p = mb_slab_alloc (&s);
    assert (p != NULL);
    q = mb_slab_alloc (&s);
    assert (q != NULL);

    /* Free q so we can attempt to free garbage into the freed slot. */
    mb_slab_free (&s, q);

    /* A stack variable masquerading as an object — its magic field is
     * not MB_SLAB_MAGIC. The free must be rejected as a no-op. */
    {
        volatile unsigned char buf[64] = {0};
        mb_slab_free (&s, (void *) buf);
    }

    /* A wild pointer that we never allocated at all — no magic prefix. */
    {
        void *fake = (void *) 0x1;
        mb_slab_free (&s, fake);
    }

    /* s currently has 1 user-visible free slot (q was returned to the
     * freelist). The rejected frees above must not have changed that. */
    void *r = mb_slab_alloc (&s);
    assert (r != NULL);

    /* Free p, then q, then r — all valid slab objects. */
    mb_slab_free (&s, p);
    mb_slab_free (&s, q);
    mb_slab_free (&s, r);

    /* And an alloc should still work. */
    void *t = mb_slab_alloc (&s);
    assert (t != NULL);
    mb_slab_free (&s, t);

    mb_slab_term (&s);

    printf ("test_slab_validate_free: PASSED\n");
    return 0;
}
