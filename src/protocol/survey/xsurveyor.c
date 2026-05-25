#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_survey.h>

#include <errno.h>

struct mb_xsurveyor_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
};

struct mb_xsurveyor {
    struct mb_sockbase base;
    struct mb_list pipes;
};

static void mb_xsurveyor_destroy (struct mb_sockbase *self)
{
    struct mb_xsurveyor *xp = (struct mb_xsurveyor *) self;
    struct mb_list_item *it = mb_list_begin (&xp->pipes);
    while (it != mb_list_end (&xp->pipes)) {
        struct mb_xsurveyor_pipe_data *data =
            (struct mb_xsurveyor_pipe_data *) it;
        struct mb_list_item *next = mb_list_next (&xp->pipes, it);
        mb_list_erase (&xp->pipes, &data->item);
        mb_free (data);
        it = next;
    }
    mb_list_term (&xp->pipes);
    mb_free (xp);
}

static int mb_xsurveyor_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xsurveyor *xp = (struct mb_xsurveyor *) self;
    struct mb_xsurveyor_pipe_data *data = (struct mb_xsurveyor_pipe_data *)
        mb_alloc (sizeof (*data));
    if (!data) return -ENOMEM;
    data->pipe = pipe;
    mb_list_item_init (&data->item);
    mb_list_insert (&xp->pipes, &data->item, mb_list_end (&xp->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xsurveyor_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xsurveyor *xp = (struct mb_xsurveyor *) self;
    struct mb_xsurveyor_pipe_data *data = (struct mb_xsurveyor_pipe_data *)
        mb_pipe_getdata (pipe);
    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&xp->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }
}

static void mb_xsurveyor_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static void mb_xsurveyor_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static int mb_xsurveyor_events (struct mb_sockbase *self)
{
    (void) self;
    return MB_SOCKBASE_EVENT_IN | MB_SOCKBASE_EVENT_OUT;
}

static int mb_xsurveyor_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xsurveyor *xp = (struct mb_xsurveyor *) self;
    struct mb_list_item *it;
    for (it = mb_list_begin (&xp->pipes); it != mb_list_end (&xp->pipes);
         it = mb_list_next (&xp->pipes, it)) {
        struct mb_xsurveyor_pipe_data *data =
            (struct mb_xsurveyor_pipe_data *) it;
        struct mb_msg copy;
        mb_msg_init (&copy, 0);
        mb_msg_cp (&copy, msg);
        int rc = mb_pipe_send (data->pipe, &copy);
        if (rc < 0) mb_msg_term (&copy);
    }
    return 0;
}

static int mb_xsurveyor_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xsurveyor *xp = (struct mb_xsurveyor *) self;
    struct mb_list_item *it;
    for (it = mb_list_begin (&xp->pipes); it != mb_list_end (&xp->pipes);
         it = mb_list_next (&xp->pipes, it)) {
        struct mb_xsurveyor_pipe_data *data =
            (struct mb_xsurveyor_pipe_data *) it;
        int rc = mb_pipe_recv (data->pipe, msg);
        if (rc == 0) return 0;
        if (rc != -EAGAIN) return rc;
    }
    return -EAGAIN;
}

static int mb_xsurveyor_setopt (struct mb_sockbase *self, int level,
    int option, const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xsurveyor_getopt (struct mb_sockbase *self, int level,
    int option, void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xsurveyor_vfptr = {
    NULL, mb_xsurveyor_destroy, mb_xsurveyor_add, mb_xsurveyor_rm,
    mb_xsurveyor_in, mb_xsurveyor_out, mb_xsurveyor_events,
    mb_xsurveyor_send, mb_xsurveyor_recv, mb_xsurveyor_setopt,
    mb_xsurveyor_getopt,
};

static int mb_xsurveyor_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xsurveyor *xp; (void) hint;
    xp = (struct mb_xsurveyor *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xsurveyor_vfptr, NULL);
    mb_list_init (&xp->pipes);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xsurveyor_ispeer (int socktype)
{
    return socktype == MB_RESPONDENT || socktype == MB_XRESPONDENT;
}

const struct mb_socktype mb_xsurveyor_socktype = {
    AF_MB, MB_XSURVEYOR, MB_SOCKTYPE_FLAG_RAW,
    mb_xsurveyor_create, mb_xsurveyor_ispeer,
};
