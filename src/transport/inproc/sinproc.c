#include "sinproc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../aio/ctx.h"
#include "../../pal/efd.h"
#include "../../utils/alloc.h"
#include "../../utils/cont.h"
#include "../../utils/err.h"

#include <string.h>
#include <stdint.h>

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
    struct mb_sinproc *peer = self->peer;
    struct mb_sock *sa;
    struct mb_sock *sb;

    if (!peer) {
        if (self->pipebase.state == 2)
            mb_pipebase_stop (&self->pipebase);
        return;
    }

    /* Lock both socks in address order to avoid A↔B deadlock when both
     * ends close concurrently. */
    sa = self->pipebase.sock;
    sb = peer->pipebase.sock;
    if ((uintptr_t) sa < (uintptr_t) sb) {
        mb_ctx_enter (&sa->ctx);
        mb_ctx_enter (&sb->ctx);
    } else if (sa != sb) {
        mb_ctx_enter (&sb->ctx);
        mb_ctx_enter (&sa->ctx);
    } else {
        mb_ctx_enter (&sa->ctx);
    }

    if (self->pipebase.state == 2)
        mb_pipebase_stop (&self->pipebase);
    if (peer->pipebase.state == 2)
        mb_pipebase_stop (&peer->pipebase);
    self->peer = NULL;
    peer->peer = NULL;

    if (sa != sb) {
        if ((uintptr_t) sa < (uintptr_t) sb) {
            mb_ctx_leave (&sb->ctx);
            mb_ctx_leave (&sa->ctx);
        } else {
            mb_ctx_leave (&sa->ctx);
            mb_ctx_leave (&sb->ctx);
        }
    } else {
        mb_ctx_leave (&sa->ctx);
    }
}

static int mb_sinproc_send (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    if (!self->peer)
        return -EAGAIN;

    /* push returns 1 when the queue was empty (wake peer); race-free under
     * msgqueue mutex so we do not lose the rcvfd signal. */
    int rc = mb_msgqueue_push (&self->peer->msgqueue, msg);
    if (rc > 0)
        mb_efd_signal (&self->peer->pipebase.sock->rcvfd);
    return rc < 0 ? rc : 0;
}

static int mb_sinproc_recv (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    if (mb_msgqueue_empty (&self->msgqueue))
        return -EAGAIN;

    mb_msgqueue_pop (&self->msgqueue, msg);
    /* Clear sticky POLLIN; re-arm if concurrent push left more messages. */
    mb_efd_unsignal (&self->pipebase.sock->rcvfd);
    if (!mb_msgqueue_empty (&self->msgqueue))
        mb_efd_signal (&self->pipebase.sock->rcvfd);
    return 0;
}
