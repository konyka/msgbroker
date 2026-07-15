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

struct mb_req {
    struct mb_sockbase base;
    struct mb_lb lb;
    struct mb_pipe *pipe;
    int sending;
};

static void mb_req_destroy (struct mb_sockbase *self)
{
    struct mb_req *req = (struct mb_req *) self;
    mb_lb_term (&req->lb);
    mb_free (req);
}

static int mb_req_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_req *req = (struct mb_req *) self;
    struct mb_lb_data *data;

    data = (struct mb_lb_data *) mb_alloc (sizeof (struct mb_lb_data));
    if (!data)
        return -ENOMEM;

    mb_lb_add (&req->lb, data, pipe);
    mb_lb_activate (&req->lb, data);
    mb_pipe_setdata (pipe, data);

    if (!req->pipe)
        req->pipe = pipe;

    return 0;
}

static void mb_req_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_req *req = (struct mb_req *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);

    if (data) {
        mb_lb_rm (&req->lb, data);
        mb_free (data);
        mb_pipe_setdata (pipe, NULL);
    }

    if (req->pipe == pipe) {
        struct mb_list_item *it;

        /* Reply path died with the pipe; drop the half-open REQ FSM and
         * fall back to another live peer if present. */
        req->pipe = NULL;
        req->sending = 0;

        it = mb_list_begin (&req->lb.pipes);
        if (it != mb_list_end (&req->lb.pipes))
            req->pipe = ((struct mb_lb_data *) it)->pipe;
    }
}

static void mb_req_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_req *req = (struct mb_req *) self;
    struct mb_lb_data *data = (struct mb_lb_data *) mb_pipe_getdata (pipe);
    if (data)
        mb_lb_activate (&req->lb, data);
}

static void mb_req_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_req_events (struct mb_sockbase *self)
{
    struct mb_req *req = (struct mb_req *) self;
    int ev = 0;
    if (!req->sending && req->pipe)
        ev |= MB_SOCKBASE_EVENT_OUT;
    if (req->sending && req->pipe)
        ev |= MB_SOCKBASE_EVENT_IN;
    return ev;
}

static int mb_req_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_req *req = (struct mb_req *) self;

    if (req->sending)
        return -EFSM;
    if (!req->pipe)
        return -EAGAIN;

    int rc = mb_pipe_send (req->pipe, msg);
    if (rc == 0)
        req->sending = 1;
    return rc;
}

static int mb_req_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_req *req = (struct mb_req *) self;

    if (!req->sending)
        return -EFSM;
    if (!req->pipe)
        return -EAGAIN;

    int rc = mb_pipe_recv (req->pipe, msg);
    if (rc == 0)
        req->sending = 0;
    return rc;
}

static int mb_req_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_req_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_req_vfptr = {
    NULL,
    mb_req_destroy,
    mb_req_add,
    mb_req_rm,
    mb_req_in,
    mb_req_out,
    mb_req_events,
    mb_req_send,
    mb_req_recv,
    mb_req_setopt,
    mb_req_getopt,
};

static int mb_req_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_req *req;
    (void) hint;

    req = (struct mb_req *) mb_alloc (sizeof (struct mb_req));
    if (!req)
        return -ENOMEM;

    mb_sockbase_init (&req->base, &mb_req_vfptr, NULL);
    mb_lb_init (&req->lb);
    req->pipe = NULL;
    req->sending = 0;

    *sockbase = &req->base;
    return 0;
}

static int mb_req_ispeer (int socktype)
{
    return socktype == MB_REP;
}

const struct mb_socktype mb_req_socktype = {
    AF_MB,
    MB_REQ,
    0,
    mb_req_create,
    mb_req_ispeer,
};
