#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/fq.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>

#include <errno.h>

struct mb_xsub {
    struct mb_sockbase base;
    struct mb_fq fq;
};

static void mb_xsub_destroy (struct mb_sockbase *self)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    mb_fq_term (&xp->fq);
    mb_free (xp);
}

static int mb_xsub_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_alloc (sizeof (*data));
    if (!data) return -ENOMEM;
    mb_fq_add (&xp->fq, data, pipe);
    mb_fq_activate (&xp->fq, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xsub_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);
    if (data) { mb_fq_rm (&xp->fq, data); mb_free (data); }
}

static void mb_xsub_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);
    if (data) mb_fq_activate (&xp->fq, data);
}

static void mb_xsub_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static int mb_xsub_events (struct mb_sockbase *self)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    int ev = MB_SOCKBASE_EVENT_OUT;

    if (mb_fq_can_recv (&xp->fq))
        ev |= MB_SOCKBASE_EVENT_IN;
    return ev;
}

static int mb_xsub_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    struct mb_list_item *it;

    /* Fan-out subscription/control messages to all upstream PUB/XPUB peers. */
    for (it = mb_list_begin (&xp->fq.pipes); it != mb_list_end (&xp->fq.pipes);
         it = mb_list_next (&xp->fq.pipes, it)) {
        struct mb_fq_data *data = (struct mb_fq_data *) it;
        struct mb_msg copy;
        int rc;

        mb_msg_init (&copy, 0);
        mb_msg_cp (&copy, msg);
        rc = mb_pipe_send (data->pipe, &copy);
        if (rc < 0)
            mb_msg_term (&copy);
    }
    return 0;
}

static int mb_xsub_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xsub *xp = (struct mb_xsub *) self;
    return mb_fq_recv (&xp->fq, msg);
}

static int mb_xsub_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xsub_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xsub_vfptr = {
    NULL, mb_xsub_destroy, mb_xsub_add, mb_xsub_rm,
    mb_xsub_in, mb_xsub_out, mb_xsub_events,
    mb_xsub_send, mb_xsub_recv, mb_xsub_setopt, mb_xsub_getopt,
};

static int mb_xsub_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xsub *xp; (void) hint;
    xp = (struct mb_xsub *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xsub_vfptr, NULL);
    mb_fq_init (&xp->fq);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xsub_ispeer (int socktype)
{
    return socktype == MB_PUB || socktype == MB_XPUB;
}

const struct mb_socktype mb_xsub_socktype = {
    AF_MB, MB_XSUB, MB_SOCKTYPE_FLAG_RAW, mb_xsub_create, mb_xsub_ispeer,
};
