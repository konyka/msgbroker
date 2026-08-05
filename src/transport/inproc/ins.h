#ifndef MB_TRANSPORT_INPROC_INS_H_INCLUDED
#define MB_TRANSPORT_INPROC_INS_H_INCLUDED

#include "../../utils/list.h"

struct mb_ep;

struct mb_ins_item {
    struct mb_list_item item;
    struct mb_ep *ep;
    int protocol;
    /* Truthful registry hint: which list owns this item right now.
     * Updated by mb_ins_bind / mb_ins_connect and cleared on erase.
     * Used to safely pick the right mb_list_erase target. */
    int in_lazy_list;
};

void mb_ins_init (void);
void mb_ins_term (void);
void mb_ins_item_init (struct mb_ins_item *self, struct mb_ep *ep);
void mb_ins_item_term (struct mb_ins_item *self);

typedef int (*mb_ins_fn) (struct mb_ins_item *self, struct mb_ins_item *peer);

int mb_ins_bind (struct mb_ins_item *item, mb_ins_fn fn);
int mb_ins_connect (struct mb_ins_item *item, mb_ins_fn fn);
void mb_ins_unbind (struct mb_ins_item *item);
void mb_ins_disconnect (struct mb_ins_item *item);

#endif
