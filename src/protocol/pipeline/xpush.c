#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/lb.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>

#include <errno.h>

struct mb_xpush {
    struct mb_sockbase base;
    struct mb_lb lb;
};

static void mb_xpush_destroy (struct mb_sockbase *self)
{
    struct mb_xpush *xp = (struct mb_xpush *) self;
    mb_lb_term (&xp->lb);
    mb_free (xp);
}

static int mb_xpush_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpush *xp = (struct mb_xpush *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_alloc (sizeof (*data));
    if (!data) return -ENOMEM;
    mb_lb_add (&xp->lb, data, pipe);
    mb_lb_activate (&xp->lb, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xpush_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpush *xp = (struct mb_xpush *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);
    if (data) { mb_lb_rm (&xp->lb, data); mb_free (data); }
}

static void mb_xpush_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static void mb_xpush_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpush *xp = (struct mb_xpush *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);
    if (data) mb_lb_activate (&xp->lb, data);
}

static int mb_xpush_events (struct mb_sockbase *self)
{
    (void) self;
    return MB_SOCKBASE_EVENT_OUT;
}

static int mb_xpush_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xpush *xp = (struct mb_xpush *) self;
    return mb_lb_send (&xp->lb, msg);
}

static int mb_xpush_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xpush_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xpush_vfptr = {
    NULL, mb_xpush_destroy, mb_xpush_add, mb_xpush_rm,
    mb_xpush_in, mb_xpush_out, mb_xpush_events,
    mb_xpush_send, NULL, mb_xpush_setopt, mb_xpush_getopt,
};

static int mb_xpush_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xpush *xp; (void) hint;
    xp = (struct mb_xpush *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xpush_vfptr, NULL);
    mb_lb_init (&xp->lb);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xpush_ispeer (int socktype)
{
    return socktype == MB_PULL || socktype == MB_XPULL;
}

const struct mb_socktype mb_xpush_socktype = {
    AF_MB, MB_XPUSH, MB_SOCKTYPE_FLAG_RAW, mb_xpush_create, mb_xpush_ispeer,
};
