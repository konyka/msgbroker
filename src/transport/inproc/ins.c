#include "ins.h"
#include "../../core/ep.h"

#include "../../utils/alloc.h"
#include "../../utils/list.h"

#include <string.h>

static struct mb_list mb_ins_bound;
static int mb_ins_initialised = 0;

void mb_ins_init (void)
{
    if (!mb_ins_initialised) {
        mb_list_init (&mb_ins_bound);
        mb_ins_initialised = 1;
    }
}

void mb_ins_term (void)
{
    mb_ins_initialised = 0;
}

void mb_ins_item_init (struct mb_ins_item *self, struct mb_ep *ep)
{
    mb_list_item_init (&self->item);
    self->ep = ep;
    self->protocol = ep->protocol;
}

void mb_ins_item_term (struct mb_ins_item *self)
{
    mb_list_item_term (&self->item);
}

int mb_ins_bind (struct mb_ins_item *item, mb_ins_fn fn)
{
    struct mb_list_item *it;
    const char *addr;
    struct mb_ins_item *peer;

    (void) fn;
    addr = mb_ep_getaddr (item->ep);

    for (it = mb_list_begin (&mb_ins_bound); it != mb_list_end (&mb_ins_bound);
         it = mb_list_next (&mb_ins_bound, it)) {
        peer = (struct mb_ins_item *) it;
        if (strcmp (mb_ep_getaddr (peer->ep), addr) == 0) {
            mb_list_item_term (&item->item);
            return -EADDRINUSE;
        }
    }

    mb_list_insert (&mb_ins_bound, &item->item, mb_list_end (&mb_ins_bound));

    return 0;
}

void mb_ins_connect (struct mb_ins_item *item, mb_ins_fn fn)
{
    struct mb_list_item *it;
    const char *addr;

    addr = mb_ep_getaddr (item->ep);

    for (it = mb_list_begin (&mb_ins_bound); it != mb_list_end (&mb_ins_bound);
         it = mb_list_next (&mb_ins_bound, it)) {
        struct mb_ins_item *peer = (struct mb_ins_item *) it;
        if (strcmp (mb_ep_getaddr (peer->ep), addr) == 0) {
            if (fn)
                fn (item, peer);
            return;
        }
    }
}

void mb_ins_unbind (struct mb_ins_item *item)
{
    if (mb_list_item_isinlist (&item->item))
        mb_list_erase (&mb_ins_bound, &item->item);
}

void mb_ins_disconnect (struct mb_ins_item *item)
{
    (void) item;
}
