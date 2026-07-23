#include "../transport.h"

#include "sock.h"
#include "ep.h"

#include <errno.h>
#include <string.h>

static void mb_ep_handler (struct mb_fsm *self, int src, int type,
    void *srcptr);
static void mb_ep_shutdown (struct mb_fsm *self, int src, int type,
    void *srcptr);

int mb_ep_init (struct mb_ep *self, int src, struct mb_sock *sock, int eid,
    const struct mb_transport *transport, int bind, const char *addr)
{
    int rc;

    if (!addr || strlen (addr) > MB_SOCKADDR_MAX)
        return -EINVAL;

    mb_fsm_init (&self->fsm, mb_ep_handler, mb_ep_shutdown,
        src, self, &sock->fsm);
    self->state = MB_EP_STATE_IDLE;

    self->sock = sock;
    self->eid = eid;
    self->last_errno = 0;
    mb_list_item_init (&self->item);
    memcpy (&self->options, &sock->ep_template, sizeof (struct mb_ep_options));

    self->protocol = sock->socktype->protocol;

    strcpy (self->addr, addr);

    self->tran = NULL;
    memset (&self->ops, 0, sizeof (self->ops));

    if (bind)
        rc = transport->bind (self);
    else
        rc = transport->connect (self);

    if (rc < 0) {
        mb_list_item_term (&self->item);
        mb_fsm_term (&self->fsm);
        return rc;
    }

    return 0;
}

void mb_ep_term (struct mb_ep *self)
{
    if (self->ops.destroy)
        self->ops.destroy (self->tran);
    mb_list_item_term (&self->item);
    mb_fsm_term (&self->fsm);
}

void mb_ep_start (struct mb_ep *self)
{
    self->state = MB_EP_STATE_ACTIVE;
    mb_fsm_start (&self->fsm);
}

void mb_ep_stop (struct mb_ep *self)
{
    /* Call ops.stop even when still IDLE: mb_ep_init may have already
     * started accept/reconnect threads before mb_ep_start marks ACTIVE. */
    if (self->ops.stop)
        self->ops.stop (self->tran);
}

void mb_ep_stopped_cb (struct mb_ep *self)
{
    self->state = MB_EP_STATE_IDLE;
    /* During synchronous sock shutdown the endpoint is freed immediately
     * after stop/term; do not queue MB_EP_STOPPED (would UAF the event). */
    if (__atomic_load_n (&self->sock->flags, __ATOMIC_ACQUIRE) &
        MB_SOCK_FLAG_STOPPING)
        return;
    if (self->fsm.owner)
        mb_fsm_raise (self->fsm.owner, &self->fsm.stopped, MB_EP_STOPPED);
}

struct mb_ctx *mb_ep_getctx (struct mb_ep *self)
{
    return mb_sock_getctx (self->sock);
}

const char *mb_ep_getaddr (struct mb_ep *self)
{
    return self->addr;
}

void mb_ep_getopt (struct mb_ep *self, int level, int option,
    void *optval, size_t *optvallen)
{
    mb_sock_getopt_inner (self->sock, level, option, optval, optvallen);
}

int mb_ep_ispeer (struct mb_ep *self, int socktype)
{
    return mb_sock_ispeer (self->sock, socktype);
}

void mb_ep_set_error (struct mb_ep *self, int errnum)
{
    if (self->last_errno == 0 && errnum != 0)
        self->sock->statistics.current_ep_errors++;
    else if (self->last_errno != 0 && errnum == 0)
        self->sock->statistics.current_ep_errors--;
    self->last_errno = errnum;
}

void mb_ep_clear_error (struct mb_ep *self)
{
    mb_ep_set_error (self, 0);
}

void mb_ep_stat_increment (struct mb_ep *self, int name, int increment)
{
    mb_sock_stat_increment (self->sock, name, increment);
}

struct mb_sock *mb_ep_sock (struct mb_ep *self)
{
    return self->sock;
}

void mb_ep_tran_setup (struct mb_ep *self, const struct mb_ep_ops *ops,
    void *transport)
{
    self->ops = *ops;
    self->tran = transport;
}

void mb_ep_stopped (struct mb_ep *self)
{
    mb_ep_stopped_cb (self);
}

static void mb_ep_handler (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_ep *self = (struct mb_ep *) fsm;
    (void) src;
    (void) type;
    (void) srcptr;

    switch (self->state) {
    case MB_EP_STATE_ACTIVE:
        break;
    case MB_EP_STATE_STOPPING:
        mb_ep_stopped_cb (self);
        break;
    }
}

static void mb_ep_shutdown (struct mb_fsm *fsm, int src, int type,
    void *srcptr)
{
    struct mb_ep *self = (struct mb_ep *) fsm;
    (void) src;
    (void) type;
    (void) srcptr;

    if (self->state == MB_EP_STATE_ACTIVE) {
        self->state = MB_EP_STATE_STOPPING;
        if (self->ops.stop)
            self->ops.stop (self->tran);
        else
            mb_ep_stopped_cb (self);
    }
}
