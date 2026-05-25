#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/lb.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>

#include <errno.h>

struct mb_push {
    struct mb_sockbase base;
    struct mb_lb lb;
};

static void mb_push_destroy (struct mb_sockbase *self)
{
    struct mb_push *push = (struct mb_push *) self;
    mb_lb_term (&push->lb);
    mb_free (push);
}

static int mb_push_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_push *push = (struct mb_push *) self;
    struct mb_lb_data *data;

    data = (struct mb_lb_data *) mb_alloc (sizeof (struct mb_lb_data));
    if (!data)
        return -ENOMEM;

    mb_lb_add (&push->lb, data, pipe);
    mb_lb_activate (&push->lb, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_push_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_push *push = (struct mb_push *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);

    if (data) {
        mb_lb_rm (&push->lb, data);
        mb_free (data);
    }
}

static void mb_push_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static void mb_push_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_push *push = (struct mb_push *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);
    if (data)
        mb_lb_activate (&push->lb, data);
}

static int mb_push_events (struct mb_sockbase *self)
{
    struct mb_push *push = (struct mb_push *) self;
    int ev = 0;
    if (mb_lb_can_send (&push->lb))
        ev |= MB_SOCKBASE_EVENT_OUT;
    return ev;
}

static int mb_push_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_push *push = (struct mb_push *) self;
    return mb_lb_send (&push->lb, msg);
}

static int mb_push_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_push_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_push_vfptr = {
    NULL,
    mb_push_destroy,
    mb_push_add,
    mb_push_rm,
    mb_push_in,
    mb_push_out,
    mb_push_events,
    mb_push_send,
    NULL,
    mb_push_setopt,
    mb_push_getopt,
};

static int mb_push_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_push *push;
    (void) hint;

    push = (struct mb_push *) mb_alloc (sizeof (struct mb_push));
    if (!push)
        return -ENOMEM;

    mb_sockbase_init (&push->base, &mb_push_vfptr, NULL);
    mb_lb_init (&push->lb);

    *sockbase = &push->base;
    return 0;
}

static int mb_push_ispeer (int socktype)
{
    return socktype == MB_PULL;
}

const struct mb_socktype mb_push_socktype = {
    AF_MB,
    MB_PUSH,
    MB_SOCKTYPE_FLAG_NORECV,
    mb_push_create,
    mb_push_ispeer,
};
