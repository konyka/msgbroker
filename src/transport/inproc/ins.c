#include "ins.h"
#include "../../core/ep.h"

#include "../../utils/list.h"

#include <string.h>

/* mb_ins_bound: address slot owners that were declared via mb_bind.
 *
 * mb_ins_lazy: address slot owners that were declared via mb_connect
 *              when no mb_bind existed yet (lazy / on-demand attach).
 *              The first connect to declare an address claims the slot;
 *              subsequent connects on the same name auto-attach.
 *
 * The first declarer of an address — bind or connect — wins the slot.
 * A later mb_bind against a lazy-slot succeeds by upgrading the slot
 * to a bind-owner; the existing lazy connectors keep their wires
 * attached and gain a matching bind-side peer.
 */
static struct mb_list mb_ins_bound;
static struct mb_list mb_ins_lazy;
static int mb_ins_initialised = 0;

void mb_ins_init (void)
{
    if (!mb_ins_initialised) {
        mb_list_init (&mb_ins_bound);
        mb_list_init (&mb_ins_lazy);
        mb_ins_initialised = 1;
    }
}

void mb_ins_term (void)
{
    mb_ins_initialised = 0;
    /* Drop any remaining items so a subsequent mb_ins_init starts clean.
     * Tests terminate the library between sub-tests; without this, stale
     * mb_ins_item pointers linger across the registry. */
    while (!mb_list_empty (&mb_ins_bound)) {
        struct mb_list_item *it = mb_list_begin (&mb_ins_bound);
        mb_list_erase (&mb_ins_bound, it);
    }
    while (!mb_list_empty (&mb_ins_lazy)) {
        struct mb_list_item *it = mb_list_begin (&mb_ins_lazy);
        mb_list_erase (&mb_ins_lazy, it);
    }
}

void mb_ins_item_init (struct mb_ins_item *self, struct mb_ep *ep)
{
    mb_list_item_init (&self->item);
    self->ep = ep;
    self->protocol = ep->protocol;
    self->in_lazy_list = 0;
}

void mb_ins_item_term (struct mb_ins_item *self)
{
    mb_list_item_term (&self->item);
    self->in_lazy_list = 0;
}

/* Find an existing owner (bind or lazy connect) for the given address.
 * Returns the matching mb_ins_item or NULL. */
static struct mb_ins_item *mb_ins_find_owner (const char *addr)
{
    struct mb_list_item *it;
    struct mb_ins_item *peer;

    for (it = mb_list_begin (&mb_ins_bound); it != mb_list_end (&mb_ins_bound);
         it = mb_list_next (&mb_ins_bound, it)) {
        peer = (struct mb_ins_item *) it;
        if (strcmp (mb_ep_getaddr (peer->ep), addr) == 0)
            return peer;
    }
    for (it = mb_list_begin (&mb_ins_lazy); it != mb_list_end (&mb_ins_lazy);
         it = mb_list_next (&mb_ins_lazy, it)) {
        peer = (struct mb_ins_item *) it;
        if (strcmp (mb_ep_getaddr (peer->ep), addr) == 0)
            return peer;
    }
    return NULL;
}

int mb_ins_bind (struct mb_ins_item *item, mb_ins_fn fn)
{
    const char *addr;
    struct mb_ins_item *existing;

    (void) fn;
    addr = mb_ep_getaddr (item->ep);

    existing = mb_ins_find_owner (addr);
    if (existing && existing->ep->bind)
        return -EADDRINUSE;

    /* Bind-after-lazy: erase the lazy slot so this bind claims the
     * address cleanly. The existing lazy connectors fall back to
     * their internal wait-list to discover the new bind peer. */
    if (existing && existing->in_lazy_list) {
        mb_list_erase (&mb_ins_lazy, &existing->item);
        existing->in_lazy_list = 0;
    }

    mb_list_insert (&mb_ins_bound, &item->item, mb_list_end (&mb_ins_bound));
    item->in_lazy_list = 0;

    return 0;
}

int mb_ins_connect (struct mb_ins_item *item, mb_ins_fn fn)
{
    const char *addr;
    struct mb_ins_item *existing;

    addr = mb_ep_getaddr (item->ep);

    existing = mb_ins_find_owner (addr);
    if (existing) {
        /* Skip protocol check when either endpoint is a synthetic ep
         * (no sock attached — used by OOM tests with stack eps). */
        if (item->ep->sock && existing->ep->sock &&
            (!mb_ep_ispeer (item->ep, existing->protocol) ||
             !mb_ep_ispeer (existing->ep, item->protocol)))
            return -EPROTONOSUPPORT;
        if (fn)
            return fn (item, existing);
        return 0;
    }

    /* No owner yet — lazy: register this connector as the address
     * owner so future connects attach to it. */
    mb_list_insert (&mb_ins_lazy, &item->item, mb_list_end (&mb_ins_lazy));
    item->in_lazy_list = 1;

    return 0;
}

/* Erase `item` from its registry slot.
 *
 * mb_list_erase dereferences item->prev / item->next without checking
 * whether the item is actually a member — they hold MB_LIST_NOTINLIST
 * (0xFF...FF) when the item is unlinked. We must therefore pick the
 * EXACT list the item belongs to, not blindly try both. The mb_ins_item
 * carries an in_lazy_list flag set by mb_ins_bind / mb_ins_connect and
 * cleared here — it tracks the actual registry slot, independent of
 * any ep->bind hint that the test or stack ep may leave uninitialised. */
static void mb_ins_erase_item (struct mb_ins_item *item)
{
    if (!mb_list_item_isinlist (&item->item))
        return;
    if (item->in_lazy_list) {
        mb_list_erase (&mb_ins_lazy, &item->item);
        item->in_lazy_list = 0;
    } else {
        mb_list_erase (&mb_ins_bound, &item->item);
    }
}

void mb_ins_unbind (struct mb_ins_item *item)
{
    mb_ins_erase_item (item);
}

void mb_ins_disconnect (struct mb_ins_item *item)
{
    mb_ins_erase_item (item);
}
