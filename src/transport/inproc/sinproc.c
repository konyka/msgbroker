#include "sinproc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../pal/efd.h"
#include "../../utils/alloc.h"
#include "../../utils/cont.h"
#include "../../utils/err.h"

#include <string.h>

static int mb_sinproc_send (struct mb_pipebase *self, struct mb_msg *msg);
static int mb_sinproc_recv (struct mb_pipebase *self, struct mb_msg *msg);

static const struct mb_pipebase_vfptr mb_sinproc_vfptr = {
    mb_sinproc_send,
    mb_sinproc_recv,
};

int mb_sinproc_create (struct mb_sinproc *self, struct mb_ep *ep)
{
    self->peer = NULL;
    mb_pipebase_init (&self->pipebase, &mb_sinproc_vfptr, ep);
    mb_msgqueue_init (&self->msgqueue, 0);
    mb_list_item_init (&self->item);
    return 0;
}

void mb_sinproc_term (struct mb_sinproc *self)
{
    mb_list_item_term (&self->item);
    mb_msgqueue_term (&self->msgqueue);
    mb_pipebase_term (&self->pipebase);
}

void mb_sinproc_connect (struct mb_sinproc *self, struct mb_sinproc *peer)
{
    self->peer = peer;
    peer->peer = self;
    mb_pipebase_start (&self->pipebase);
    mb_pipebase_start (&peer->pipebase);
}

void mb_sinproc_stop (struct mb_sinproc *self)
{
    if (self->pipebase.state == 2)
        mb_pipebase_stop (&self->pipebase);
    if (self->peer) {
        if (self->peer->pipebase.state == 2)
            mb_pipebase_stop (&self->peer->pipebase);
        self->peer->peer = NULL;
        self->peer = NULL;
    }
}

static int mb_sinproc_send (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    if (!self->peer)
        return -EAGAIN;

    int was_empty = mb_msgqueue_empty (&self->peer->msgqueue);
    int rc = mb_msgqueue_push (&self->peer->msgqueue, msg);
    if (rc >= 0 && was_empty)
        mb_efd_signal (&self->peer->pipebase.sock->rcvfd);
    return rc;
}

static int mb_sinproc_recv (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    if (mb_msgqueue_empty (&self->msgqueue))
        return -EAGAIN;

    mb_msgqueue_pop (&self->msgqueue, msg);
    return 0;
}
