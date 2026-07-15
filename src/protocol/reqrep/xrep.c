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

struct mb_xrep {
    struct mb_sockbase base;
    struct mb_fq fq;
    struct mb_lb lb;
};

static void mb_xrep_destroy (struct mb_sockbase *self)
{
    struct mb_xrep *xp = (struct mb_xrep *) self;
    mb_lb_term (&xp->lb);
    mb_fq_term (&xp->fq);
    mb_free (xp);
}

static int mb_xrep_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xrep *xp = (struct mb_xrep *) self;
    struct mb_fq_data *fqd = (struct mb_fq_data *) mb_alloc (sizeof (*fqd));
    struct mb_lb_data *lbd;
    if (!fqd) return -ENOMEM;
    mb_fq_add (&xp->fq, fqd, pipe);
    mb_fq_activate (&xp->fq, fqd);
    lbd = (struct mb_lb_data *) mb_alloc (sizeof (*lbd));
    if (!lbd) { mb_fq_rm (&xp->fq, fqd); mb_free (fqd); return -ENOMEM; }
    mb_lb_add (&xp->lb, lbd, pipe);
    mb_lb_activate (&xp->lb, lbd);
    return 0;
}

static void mb_xrep_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xrep *xp = (struct mb_xrep *) self;
    struct mb_list_item *it;
    struct mb_list_item *next;

    for (it = mb_list_begin (&xp->fq.pipes); it != mb_list_end (&xp->fq.pipes);
        it = next) {
        struct mb_fq_data *data = (struct mb_fq_data *) it;
        next = mb_list_next (&xp->fq.pipes, it);
        if (data->pipe == pipe) {
            mb_fq_rm (&xp->fq, data);
            mb_free (data);
            break;
        }
    }

    for (it = mb_list_begin (&xp->lb.pipes); it != mb_list_end (&xp->lb.pipes);
        it = next) {
        struct mb_lb_data *data = (struct mb_lb_data *) it;
        next = mb_list_next (&xp->lb.pipes, it);
        if (data->pipe == pipe) {
            mb_lb_rm (&xp->lb, data);
            mb_free (data);
            break;
        }
    }
}

static void mb_xrep_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static void mb_xrep_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static int mb_xrep_events (struct mb_sockbase *self)
{
    (void) self;
    return MB_SOCKBASE_EVENT_IN | MB_SOCKBASE_EVENT_OUT;
}

static int mb_xrep_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    (void) self; (void) msg;
    return -EAGAIN;
}

static int mb_xrep_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xrep *xp = (struct mb_xrep *) self;
    return mb_fq_recv (&xp->fq, msg);
}

static int mb_xrep_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xrep_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xrep_vfptr = {
    NULL, mb_xrep_destroy, mb_xrep_add, mb_xrep_rm,
    mb_xrep_in, mb_xrep_out, mb_xrep_events,
    mb_xrep_send, mb_xrep_recv, mb_xrep_setopt, mb_xrep_getopt,
};

static int mb_xrep_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xrep *xp; (void) hint;
    xp = (struct mb_xrep *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xrep_vfptr, NULL);
    mb_fq_init (&xp->fq);
    mb_lb_init (&xp->lb);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xrep_ispeer (int socktype)
{
    return socktype == MB_REQ || socktype == MB_XREQ;
}

const struct mb_socktype mb_xrep_socktype = {
    AF_MB, MB_XREP, MB_SOCKTYPE_FLAG_RAW, mb_xrep_create, mb_xrep_ispeer,
};
