#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/fq.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>

#include <errno.h>

struct mb_xpull {
    struct mb_sockbase base;
    struct mb_fq fq;
};

static void mb_xpull_destroy (struct mb_sockbase *self)
{
    struct mb_xpull *xp = (struct mb_xpull *) self;
    mb_fq_term (&xp->fq);
    mb_free (xp);
}

static int mb_xpull_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpull *xp = (struct mb_xpull *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_alloc (sizeof (*data));
    if (!data) return -ENOMEM;
    mb_fq_add (&xp->fq, data, pipe);
    mb_fq_activate (&xp->fq, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xpull_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpull *xp = (struct mb_xpull *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);
    if (data) { mb_fq_rm (&xp->fq, data); mb_free (data); }
}

static void mb_xpull_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpull *xp = (struct mb_xpull *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);
    if (data) mb_fq_activate (&xp->fq, data);
}

static void mb_xpull_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static int mb_xpull_events (struct mb_sockbase *self)
{
    struct mb_xpull *xp = (struct mb_xpull *) self;
    return mb_fq_can_recv (&xp->fq) ? MB_SOCKBASE_EVENT_IN : 0;
}

static int mb_xpull_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xpull *xp = (struct mb_xpull *) self;
    return mb_fq_recv (&xp->fq, msg);
}

static int mb_xpull_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xpull_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xpull_vfptr = {
    NULL, mb_xpull_destroy, mb_xpull_add, mb_xpull_rm,
    mb_xpull_in, mb_xpull_out, mb_xpull_events,
    NULL, mb_xpull_recv, mb_xpull_setopt, mb_xpull_getopt,
};

static int mb_xpull_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xpull *xp; (void) hint;
    xp = (struct mb_xpull *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xpull_vfptr, NULL);
    mb_fq_init (&xp->fq);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xpull_ispeer (int socktype)
{
    return socktype == MB_PUSH || socktype == MB_XPUSH;
}

const struct mb_socktype mb_xpull_socktype = {
    AF_MB, MB_XPULL, MB_SOCKTYPE_FLAG_RAW, mb_xpull_create, mb_xpull_ispeer,
};
