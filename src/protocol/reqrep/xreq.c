#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/lb.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_reqrep.h>

#include <errno.h>

struct mb_xreq {
    struct mb_sockbase base;
    struct mb_lb lb;
};

static void mb_xreq_destroy (struct mb_sockbase *self)
{
    struct mb_xreq *xp = (struct mb_xreq *) self;
    mb_lb_term (&xp->lb);
    mb_free (xp);
}

static int mb_xreq_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xreq *xp = (struct mb_xreq *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_alloc (sizeof (*data));
    if (!data) return -ENOMEM;
    mb_lb_add (&xp->lb, data, pipe);
    mb_lb_activate (&xp->lb, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xreq_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xreq *xp = (struct mb_xreq *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);
    if (data) { mb_lb_rm (&xp->lb, data); mb_free (data); }
}

static void mb_xreq_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xreq *xp = (struct mb_xreq *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);
    if (data) mb_lb_activate (&xp->lb, data);
}

static void mb_xreq_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static int mb_xreq_events (struct mb_sockbase *self)
{
    (void) self;
    return MB_SOCKBASE_EVENT_IN | MB_SOCKBASE_EVENT_OUT;
}

static int mb_xreq_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xreq *xp = (struct mb_xreq *) self;
    return mb_lb_send (&xp->lb, msg);
}

static int mb_xreq_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    (void) self;
    (void) msg;
    return -EAGAIN;
}

static int mb_xreq_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xreq_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xreq_vfptr = {
    NULL, mb_xreq_destroy, mb_xreq_add, mb_xreq_rm,
    mb_xreq_in, mb_xreq_out, mb_xreq_events,
    mb_xreq_send, mb_xreq_recv, mb_xreq_setopt, mb_xreq_getopt,
};

static int mb_xreq_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xreq *xp; (void) hint;
    xp = (struct mb_xreq *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xreq_vfptr, NULL);
    mb_lb_init (&xp->lb);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xreq_ispeer (int socktype)
{
    return socktype == MB_REP || socktype == MB_XREP;
}

const struct mb_socktype mb_xreq_socktype = {
    AF_MB, MB_XREQ, MB_SOCKTYPE_FLAG_RAW, mb_xreq_create, mb_xreq_ispeer,
};
