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
static void mb_sinproc_on_rcvbuf (struct mb_pipebase *base);

static const struct mb_pipebase_vfptr mb_sinproc_vfptr = {
    mb_sinproc_send,
    mb_sinproc_recv,
    mb_sinproc_has_msg,
    mb_sinproc_can_send,
    mb_sinproc_on_rcvbuf,
};

/* Honor live MB_RCVBUF after connect (create only snapshots the initial value). */
static void mb_sinproc_sync_maxmem (struct mb_sinproc *self)
{
    int rcvbuf;

    if (!self->pipebase.sock)
        return;
    rcvbuf = self->pipebase.sock->rcvbuf;
    if (rcvbuf <= 0)
        rcvbuf = 1024 * 1024;
    mb_msgqueue_set_maxmem (&self->msgqueue, (size_t) rcvbuf);
}

static void mb_sinproc_on_rcvbuf (struct mb_pipebase *base)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    mb_sinproc_sync_maxmem (self);
    /* Enlarge may unblock peer POLLOUT; shrink must clear sticky SNDFD. */
    if (self->peer && self->peer->pipebase.sock)
        mb_sock_sync_sndfd (self->peer->pipebase.sock);
}

int mb_sinproc_create (struct mb_sinproc *self, struct mb_ep *ep)
{
    int rcvbuf;
    size_t maxmem;

    self->peer = NULL;
    mb_pipebase_init (&self->pipebase, &mb_sinproc_vfptr, ep);
    /* Queue holds messages for this end — cap by local MB_RCVBUF.
     * maxmem==0 means unlimited in msgqueue; never pass that through. */
    rcvbuf = mb_ep_sock (ep)->rcvbuf;
    if (rcvbuf <= 0)
        rcvbuf = 1024 * 1024;
    maxmem = (size_t) rcvbuf;
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
        /* Handshake aborted — do not count a broken/established connection. */
        mb_pipebase_cancel (&self->pipebase);
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

    {
        size_t body = mb_chunkref_size (&msg->body);

        /* Match stream send-side check, and peer recv-side (no wire decode). */
        if (mb_sock_msg_too_large (base->sock, body))
            return -EMSGSIZE;
        if (self->peer->pipebase.sock &&
            mb_sock_msg_too_large (self->peer->pipebase.sock, body))
            return -EMSGSIZE;
    }

    /* push returns 1 when the queue was empty (wake peer); race-free under
     * msgqueue mutex so we do not lose the rcvfd signal. */
    mb_sinproc_sync_maxmem (self->peer);
    {
        int rc = mb_msgqueue_push (&self->peer->msgqueue, msg);
        if (rc > 0)
            mb_efd_signal (&self->peer->pipebase.sock->rcvfd);
        return rc < 0 ? rc : 0;
    }
}

static int mb_sinproc_has_msg (struct mb_pipebase *base)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    return !mb_msgqueue_empty (&self->msgqueue);
}

static int mb_sinproc_can_send (struct mb_pipebase *base)
{
    struct mb_sinproc *self = mb_cont (base, struct mb_sinproc, pipebase);

    if (!self->peer)
        return 0;
    mb_sinproc_sync_maxmem (self->peer);
    return mb_msgqueue_can_push (&self->peer->msgqueue);
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
    mb_sinproc_sync_maxmem (self);
    if (self->peer && self->peer->pipebase.sock &&
        mb_msgqueue_can_push (&self->msgqueue))
        mb_efd_signal (&self->peer->pipebase.sock->sndfd);
    return 0;
}
