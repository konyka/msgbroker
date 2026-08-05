#include "cinproc.h"
#include "binproc.h"
#include "sinproc.h"
#include "ins.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../core/pipe.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../utils/cont.h"

#define MB_CINPROC_STATE_IDLE     1
#define MB_CINPROC_STATE_ACTIVE   2
#define MB_CINPROC_STATE_STOPPING 3

static void mb_cinproc_handler (struct mb_fsm *self, int src, int type,
    void *srcptr);
static void mb_cinproc_shutdown (struct mb_fsm *self, int src, int type,
    void *srcptr);

static void mb_cinproc_stop (void *p);
static void mb_cinproc_destroy (void *p);

static const struct mb_ep_ops mb_cinproc_ops = {
    mb_cinproc_stop,
    mb_cinproc_destroy,
    NULL,
};

static int mb_cinproc_connect_cb (struct mb_ins_item *self,
    struct mb_ins_item *peer_item)
{
    struct mb_cinproc *cinproc;
    struct mb_cinproc *peer_cinproc;
    struct mb_binproc *binproc;
    struct mb_sinproc *bind_side;

    cinproc = mb_cont (self, struct mb_cinproc, item);

    cinproc->sinproc = (struct mb_sinproc *) mb_alloc (
        sizeof (struct mb_sinproc));
    if (!cinproc->sinproc)
        return -ENOMEM;

    mb_sinproc_create (cinproc->sinproc, cinproc->item.ep);

    bind_side = (struct mb_sinproc *) mb_alloc (sizeof (struct mb_sinproc));
    if (!bind_side) {
        mb_sinproc_term (cinproc->sinproc);
        mb_free (cinproc->sinproc);
        cinproc->sinproc = NULL;
        return -ENOMEM;
    }

    /* Peer is either a real bind-side binproc or a lazy-owner cinproc;
     * both expose a `sinprocs` list that mirrors the bind side. */
    if (peer_item->ep->bind) {
        binproc = mb_cont (peer_item, struct mb_binproc, item);
        mb_sinproc_create (bind_side, binproc->item.ep);
        mb_list_insert (&binproc->sinprocs, &bind_side->item,
            mb_list_end (&binproc->sinprocs));
    } else {
        peer_cinproc = mb_cont (peer_item, struct mb_cinproc, item);
        mb_sinproc_create (bind_side, peer_cinproc->item.ep);
        mb_list_insert (&peer_cinproc->sinprocs, &bind_side->item,
            mb_list_end (&peer_cinproc->sinprocs));
    }

    {
        struct mb_list *list;
        int rc = mb_sinproc_connect (cinproc->sinproc, bind_side);
        if (rc < 0) {
            list = peer_item->ep->bind
                ? &mb_cont (peer_item, struct mb_binproc, item)->sinprocs
                : &mb_cont (peer_item, struct mb_cinproc, item)->sinprocs;
            mb_list_erase (list, &bind_side->item);
            mb_sinproc_term (bind_side);
            mb_free (bind_side);
            mb_sinproc_term (cinproc->sinproc);
            mb_free (cinproc->sinproc);
            cinproc->sinproc = NULL;
            return rc;
        }
    }
    return 0;
}

int mb_cinproc_create (struct mb_ep *ep)
{
    struct mb_cinproc *self;
    int rc;

    self = (struct mb_cinproc *) mb_alloc (sizeof (struct mb_cinproc));
    if (!self)
        return -ENOMEM;

    mb_fsm_init (&self->fsm, mb_cinproc_handler, mb_cinproc_shutdown,
        0, self, &mb_ep_sock (ep)->fsm);
    self->state = MB_CINPROC_STATE_IDLE;
    mb_ins_item_init (&self->item, ep);
    self->sinproc = NULL;
    mb_list_init (&self->sinprocs);

    /* Connect before tran_setup so OOM can fail create without a half EP. */
    rc = mb_ins_connect (&self->item, mb_cinproc_connect_cb);
    if (rc < 0) {
        mb_list_term (&self->sinprocs);
        mb_ins_item_term (&self->item);
        mb_fsm_term (&self->fsm);
        mb_free (self);
        return rc;
    }

    mb_ep_tran_setup (ep, &mb_cinproc_ops, self);

    mb_fsm_start (&self->fsm);
    self->state = MB_CINPROC_STATE_ACTIVE;

    return 0;
}

/* Tear down every bind_side sinproc this cinproc hosted for lazy
 * connectors, plus our own outgoing sinproc. */
static void mb_cinproc_free_sinprocs (struct mb_cinproc *self)
{
    struct mb_list_item *it;

    if (self->sinproc) {
        mb_sinproc_stop (self->sinproc);
        mb_sinproc_term (self->sinproc);
        mb_free (self->sinproc);
        self->sinproc = NULL;
    }

    while (!mb_list_empty (&self->sinprocs)) {
        it = mb_list_begin (&self->sinprocs);
        struct mb_sinproc *sp =
            mb_cont (it, struct mb_sinproc, item);
        mb_sinproc_stop (sp);
        mb_list_erase (&self->sinprocs, &sp->item);
        mb_sinproc_term (sp);
        mb_free (sp);
    }
}

static void mb_cinproc_stop (void *p)
{
    struct mb_cinproc *self = (struct mb_cinproc *) p;

    self->state = MB_CINPROC_STATE_STOPPING;

    mb_cinproc_free_sinprocs (self);

    mb_ins_disconnect (&self->item);

    self->state = MB_CINPROC_STATE_IDLE;
    mb_ep_stopped (self->item.ep);
}

static void mb_cinproc_destroy (void *p)
{
    struct mb_cinproc *self = (struct mb_cinproc *) p;

    mb_cinproc_free_sinprocs (self);

    mb_ins_disconnect (&self->item);
    mb_list_term (&self->sinprocs);
    mb_ins_item_term (&self->item);
    mb_fsm_term (&self->fsm);
    mb_free (self);
}

static void mb_cinproc_handler (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_cinproc *self = (struct mb_cinproc *) fsm;
    (void) src; (void) type; (void) srcptr;

    switch (self->state) {
    case MB_CINPROC_STATE_ACTIVE:
        break;
    default:
        break;
    }
}

static void mb_cinproc_shutdown (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_cinproc *self = (struct mb_cinproc *) fsm;
    (void) src; (void) type; (void) srcptr;

    mb_cinproc_stop (self);
}
