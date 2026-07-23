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
#include <errno.h>

static int mb_sinproc_send (struct mb_pipebase *self, struct mb_msg *msg);
static int mb_sinproc_recv (struct mb_pipebase *self, struct mb_msg *msg);
static int mb_sinproc_has_msg (struct mb_pipebase *self);
static int mb_sinproc_can_send (struct mb_pipebase *self);

static const struct mb_pipebase_vfptr mb_sinproc_vfptr = {
    mb_sinproc_send,
    mb_sinproc_recv,
    mb_sinproc_has_msg,
    mb_sinproc_can_send,
};

int mb_sinproc_create (struct mb_sinproc *self, struct mb_ep *ep)
{
    int rcvbuf;
    size_t maxmem;

    self->peer = NULL;
    mb_pipebase_init (&self->pipebase, &mb_sinproc_vfptr, ep);
    /* Queue holds messages for this end — cap by local MB_RCVBUF. */
    rcvbuf = mb_ep_sock (ep)->rcvbuf;
    maxmem = rcvbuf > 0 ? (size_t) rcvbuf : 0;
    mb_msgqueue_init (&self->msgqueue, maxmem);
    mb_list_item_init (&self->item);
    return 0;
}

void mb_sinproc_term (struct mb_sinproc *self)
{
    mb_list_item_term (&self->item);
    mb_msgqueue_term (&self->msgqueue);
    mb_pipebase_term (&self->pipebase);
}

int mb_sinproc_connect (struct mb_sinproc *self, struct mb_sinproc *peer)
{
    int rc;

    self->peer = peer;
    peer->peer = self;

    rc = mb_pipebase_start (&self->pipebase);
    if (rc < 0) {
        self->peer = NULL;
        peer->peer = NULL;
        return rc;
    }

    rc = mb_pipebase_start (&peer->pipebase);
    if (rc < 0) {
        self->peer = NULL;
        peer->peer = NULL;
        mb_pipebase_stop (&self->pipebase);
        return rc;
    }
    return 0;
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

    /* Match sipc/stls/sws: honor sender MB_RCVMAXSIZE. */
    if (mb_sock_msg_too_large (base->sock, mb_chunkref_size (&msg->body)))
        return -EMSGSIZE;

    /* push returns 1 when the queue was empty (wake peer); race-free under
     * msgqueue mutex so we do not lose the rcvfd signal. */
    int rc = mb_msgqueue_push (&self->peer->msgqueue, msg);
    if (rc > 0)
        mb_efd_signal (&self->peer->pipebase.sock->rcvfd);
    return rc < 0 ? rc : 0;
}

static int mb_sinproc_has_msg (struct mb_pipebase *base)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    return !mb_msgqueue_empty (&self->msgqueue);
}

static int mb_sinproc_can_send (struct mb_pipebase *base)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    return self->peer != NULL &&
        mb_msgqueue_can_push (&self->peer->msgqueue);
}

static int mb_sinproc_recv (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    if (mb_msgqueue_empty (&self->msgqueue))
        return -EAGAIN;

    mb_msgqueue_pop (&self->msgqueue, msg);
    /* Clear sticky POLLIN; re-arm if concurrent push left more messages.
     * Multi-pipe sockets re-arm via mb_sock_sync_rcvfd after send/recv. */
    mb_efd_unsignal (&self->pipebase.sock->rcvfd);
    if (!mb_msgqueue_empty (&self->msgqueue))
        mb_efd_signal (&self->pipebase.sock->rcvfd);

    /* Peer may have blocked on our RCVBUF — wake its POLLOUT. */
    if (self->peer && self->peer->pipebase.sock &&
        mb_msgqueue_can_push (&self->msgqueue))
        mb_efd_signal (&self->peer->pipebase.sock->sndfd);
    return 0;
}
