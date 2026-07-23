#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/fq.h"
#include "../../utils/lb.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_reqrep.h>

#include <errno.h>
#include <string.h>

struct mb_rep {
    struct mb_sockbase base;
    struct mb_fq fq;
    struct mb_lb lb;
    struct mb_pipe *last_pipe;
};

static void mb_rep_destroy (struct mb_sockbase *self)
{
    struct mb_rep *rep = (struct mb_rep *) self;
    mb_lb_term (&rep->lb);
    mb_fq_term (&rep->fq);
    mb_free (rep);
}

static int mb_rep_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_rep *rep = (struct mb_rep *) self;
    struct mb_fq_data *fqd;
    struct mb_lb_data *lbd;

    fqd = (struct mb_fq_data *) mb_alloc (sizeof (struct mb_fq_data));
    if (!fqd)
        return -ENOMEM;
    mb_fq_add (&rep->fq, fqd, pipe);
    mb_fq_activate (&rep->fq, fqd);

    lbd = (struct mb_lb_data *) mb_alloc (sizeof (struct mb_lb_data));
    if (!lbd) {
        mb_fq_rm (&rep->fq, fqd);
        mb_free (fqd);
        return -ENOMEM;
    }
    mb_lb_add (&rep->lb, lbd, pipe);
    mb_lb_activate (&rep->lb, lbd);
    return 0;
}

static void mb_rep_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_rep *rep = (struct mb_rep *) self;
    struct mb_list_item *it;
    struct mb_list_item *next;

    if (rep->last_pipe == pipe)
        rep->last_pipe = NULL;

    for (it = mb_list_begin (&rep->fq.pipes); it != mb_list_end (&rep->fq.pipes);
        it = next) {
        struct mb_fq_data *data = (struct mb_fq_data *) it;
        next = mb_list_next (&rep->fq.pipes, it);
        if (data->pipe == pipe) {
            mb_fq_rm (&rep->fq, data);
            mb_free (data);
            break;
        }
    }

    for (it = mb_list_begin (&rep->lb.pipes); it != mb_list_end (&rep->lb.pipes);
        it = next) {
        struct mb_lb_data *data = (struct mb_lb_data *) it;
        next = mb_list_next (&rep->lb.pipes, it);
        if (data->pipe == pipe) {
            mb_lb_rm (&rep->lb, data);
            mb_free (data);
            break;
        }
    }
}

static void mb_rep_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static void mb_rep_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_rep_events (struct mb_sockbase *self)
{
    struct mb_rep *rep = (struct mb_rep *) self;
    int ev = 0;
    if (rep->last_pipe) {
        /* Match PAIR: OUT only when the reply pipe can accept a send. */
        if (mb_pipe_can_send (rep->last_pipe))
            ev |= MB_SOCKBASE_EVENT_OUT;
        return ev;
    }
    if (mb_fq_can_recv (&rep->fq))
        ev |= MB_SOCKBASE_EVENT_IN;
    return ev;
}

static int mb_rep_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_rep *rep = (struct mb_rep *) self;
    struct mb_pipe *pipe = NULL;
    int rc;

    /* Must reply before accepting another request (preserves reply route). */
    if (rep->last_pipe)
        return -EFSM;

    rc = mb_fq_recv_pipe (&rep->fq, msg, &pipe);
    if (rc == 0)
        rep->last_pipe = pipe;
    return rc;
}

static int mb_rep_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_rep *rep = (struct mb_rep *) self;
    if (!rep->last_pipe)
        return -EFSM;
    int rc = mb_pipe_send (rep->last_pipe, msg);
    /* Keep last_pipe on -EAGAIN so mb_send can retry after backpressure. */
    if (rc != -EAGAIN)
        rep->last_pipe = NULL;
    return rc;
}

static int mb_rep_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_rep_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_rep_vfptr = {
    NULL,
    mb_rep_destroy,
    mb_rep_add,
    mb_rep_rm,
    mb_rep_in,
    mb_rep_out,
    mb_rep_events,
    mb_rep_send,
    mb_rep_recv,
    mb_rep_setopt,
    mb_rep_getopt,
};

static int mb_rep_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_rep *rep;
    (void) hint;

    rep = (struct mb_rep *) mb_alloc (sizeof (struct mb_rep));
    if (!rep)
        return -ENOMEM;

    mb_sockbase_init (&rep->base, &mb_rep_vfptr, NULL);
    mb_fq_init (&rep->fq);
    mb_lb_init (&rep->lb);
    rep->last_pipe = NULL;

    *sockbase = &rep->base;
    return 0;
}

static int mb_rep_ispeer (int socktype)
{
    return socktype == MB_REQ || socktype == MB_XREQ;
}

const struct mb_socktype mb_rep_socktype = {
    AF_MB,
    MB_REP,
    0,
    mb_rep_create,
    mb_rep_ispeer,
};
