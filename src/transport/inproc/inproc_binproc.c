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

#include <string.h>

#define MB_BINPROC_STATE_IDLE     1
#define MB_BINPROC_STATE_ACTIVE   2
#define MB_BINPROC_STATE_STOPPING 3

static void mb_binproc_handler (struct mb_fsm *self, int src, int type,
    void *srcptr);
static void mb_binproc_shutdown (struct mb_fsm *self, int src, int type,
    void *srcptr);

static void mb_binproc_stop (void *p);
static void mb_binproc_destroy (void *p);

static const struct mb_ep_ops mb_binproc_ops = {
    mb_binproc_stop,
    mb_binproc_destroy,
    NULL,
};

static void mb_binproc_connect_cb (struct mb_ins_item *self,
    struct mb_ins_item *peer_item)
{
    struct mb_binproc *binproc;
    struct mb_sinproc *peer_sinproc;
    struct mb_sinproc *sinproc;

    peer_sinproc = (struct mb_sinproc *) peer_item;
    binproc = mb_cont (self, struct mb_binproc, item);

    sinproc = (struct mb_sinproc *) mb_alloc (sizeof (struct mb_sinproc));
    if (!sinproc)
        return;

    mb_sinproc_create (sinproc, binproc->item.ep);
    mb_list_insert (&binproc->sinprocs, &sinproc->item,
        mb_list_end (&binproc->sinprocs));

    mb_sinproc_connect (sinproc, peer_sinproc);
}

int mb_binproc_create (struct mb_ep *ep)
{
    struct mb_binproc *self;
    int rc;

    self = (struct mb_binproc *) mb_alloc (sizeof (struct mb_binproc));
    if (!self)
        return -ENOMEM;

    mb_fsm_init (&self->fsm, mb_binproc_handler, mb_binproc_shutdown,
        0, self, &mb_ep_sock (ep)->fsm);
    self->state = MB_BINPROC_STATE_IDLE;
    mb_ins_item_init (&self->item, ep);
    mb_list_init (&self->sinprocs);

    rc = mb_ins_bind (&self->item, mb_binproc_connect_cb);
    if (rc < 0) {
        mb_list_term (&self->sinprocs);
        mb_ins_item_term (&self->item);
        mb_fsm_term (&self->fsm);
        mb_free (self);
        return rc;
    }

    mb_ep_tran_setup (ep, &mb_binproc_ops, self);

    mb_fsm_start (&self->fsm);
    self->state = MB_BINPROC_STATE_ACTIVE;

    return 0;
}

static void mb_binproc_stop (void *p)
{
    struct mb_binproc *self = (struct mb_binproc *) p;
    struct mb_list_item *it;
    struct mb_sinproc *sinproc;

    self->state = MB_BINPROC_STATE_STOPPING;

    mb_ins_unbind (&self->item);

    while (!mb_list_empty (&self->sinprocs)) {
        it = mb_list_begin (&self->sinprocs);
        sinproc = mb_cont (it, struct mb_sinproc, item);
        mb_list_erase (&self->sinprocs, it);
        mb_sinproc_stop (sinproc);
        mb_sinproc_term (sinproc);
        mb_free (sinproc);
    }

    self->state = MB_BINPROC_STATE_IDLE;
    mb_ep_stopped (self->item.ep);
}

static void mb_binproc_destroy (void *p)
{
    struct mb_binproc *self = (struct mb_binproc *) p;
    struct mb_list_item *it;
    struct mb_sinproc *sinproc;

    mb_ins_unbind (&self->item);

    while (!mb_list_empty (&self->sinprocs)) {
        it = mb_list_begin (&self->sinprocs);
        sinproc = mb_cont (it, struct mb_sinproc, item);
        mb_list_erase (&self->sinprocs, it);
        mb_sinproc_stop (sinproc);
        mb_sinproc_term (sinproc);
        mb_free (sinproc);
    }

    mb_list_term (&self->sinprocs);
    mb_ins_item_term (&self->item);
    mb_fsm_term (&self->fsm);
    mb_free (self);
}

static void mb_binproc_handler (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_binproc *self = (struct mb_binproc *) fsm;
    (void) src; (void) type; (void) srcptr;

    switch (self->state) {
    case MB_BINPROC_STATE_ACTIVE:
        break;
    default:
        break;
    }
}

static void mb_binproc_shutdown (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_binproc *self = (struct mb_binproc *) fsm;
    (void) src; (void) type; (void) srcptr;

    mb_binproc_stop (self);
}
