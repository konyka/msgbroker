#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_survey.h>

#include <errno.h>

struct mb_xrespondent_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
};

struct mb_xrespondent {
    struct mb_sockbase base;
    struct mb_list pipes;
};

static void mb_xrespondent_destroy (struct mb_sockbase *self)
{
    struct mb_xrespondent *xp = (struct mb_xrespondent *) self;
    struct mb_list_item *it = mb_list_begin (&xp->pipes);
    while (it != mb_list_end (&xp->pipes)) {
        struct mb_xrespondent_pipe_data *data =
            (struct mb_xrespondent_pipe_data *) it;
        struct mb_list_item *next = mb_list_next (&xp->pipes, it);
        mb_list_erase (&xp->pipes, &data->item);
        mb_free (data);
        it = next;
    }
    mb_list_term (&xp->pipes);
    mb_free (xp);
}

static int mb_xrespondent_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xrespondent *xp = (struct mb_xrespondent *) self;
    struct mb_xrespondent_pipe_data *data =
        (struct mb_xrespondent_pipe_data *) mb_alloc (sizeof (*data));
    if (!data) return -ENOMEM;
    data->pipe = pipe;
    mb_list_item_init (&data->item);
    mb_list_insert (&xp->pipes, &data->item, mb_list_end (&xp->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xrespondent_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xrespondent *xp = (struct mb_xrespondent *) self;
    struct mb_xrespondent_pipe_data *data =
        (struct mb_xrespondent_pipe_data *) mb_pipe_getdata (pipe);
    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&xp->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }
}

static void mb_xrespondent_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static void mb_xrespondent_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{ (void) self; (void) pipe; }

static int mb_xrespondent_events (struct mb_sockbase *self)
{
    struct mb_xrespondent *xp = (struct mb_xrespondent *) self;
    int ev = 0;

    if (mb_list_begin (&xp->pipes) != mb_list_end (&xp->pipes))
        ev |= MB_SOCKBASE_EVENT_IN | MB_SOCKBASE_EVENT_OUT;
    return ev;
}

static int mb_xrespondent_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xrespondent *xp = (struct mb_xrespondent *) self;
    struct mb_list_item *it;

    /* No identity/last_pipe in this raw model: try pipes round-robin. */
    for (it = mb_list_begin (&xp->pipes); it != mb_list_end (&xp->pipes); ) {
        struct mb_xrespondent_pipe_data *data =
            (struct mb_xrespondent_pipe_data *) it;
        struct mb_list_item *next = mb_list_next (&xp->pipes, it);
        int rc = mb_pipe_send (data->pipe, msg);

        if (rc == 0) {
            mb_list_erase (&xp->pipes, &data->item);
            mb_list_insert (&xp->pipes, &data->item,
                mb_list_end (&xp->pipes));
            return 0;
        }
        if (rc != -EAGAIN)
            return rc;
        it = next;
    }
    return -EAGAIN;
}

static int mb_xrespondent_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xrespondent *xp = (struct mb_xrespondent *) self;
    struct mb_list_item *it;

    for (it = mb_list_begin (&xp->pipes); it != mb_list_end (&xp->pipes); ) {
        struct mb_xrespondent_pipe_data *data =
            (struct mb_xrespondent_pipe_data *) it;
        struct mb_list_item *next = mb_list_next (&xp->pipes, it);
        int rc = mb_pipe_recv (data->pipe, msg);

        if (rc == 0) {
            mb_list_erase (&xp->pipes, &data->item);
            mb_list_insert (&xp->pipes, &data->item,
                mb_list_end (&xp->pipes));
            return 0;
        }
        if (rc != -EAGAIN)
            return rc;
        it = next;
    }
    return -EAGAIN;
}

static int mb_xrespondent_setopt (struct mb_sockbase *self, int level,
    int option, const void *optval, size_t optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static int mb_xrespondent_getopt (struct mb_sockbase *self, int level,
    int option, void *optval, size_t *optvallen)
{ (void) self; (void) level; (void) option; (void) optval; (void) optvallen;
  return -ENOPROTOOPT; }

static const struct mb_sockbase_vfptr mb_xrespondent_vfptr = {
    NULL, mb_xrespondent_destroy, mb_xrespondent_add, mb_xrespondent_rm,
    mb_xrespondent_in, mb_xrespondent_out, mb_xrespondent_events,
    mb_xrespondent_send, mb_xrespondent_recv, mb_xrespondent_setopt,
    mb_xrespondent_getopt,
};

static int mb_xrespondent_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xrespondent *xp; (void) hint;
    xp = (struct mb_xrespondent *) mb_alloc (sizeof (*xp));
    if (!xp) return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xrespondent_vfptr, NULL);
    mb_list_init (&xp->pipes);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xrespondent_ispeer (int socktype)
{
    return socktype == MB_SURVEYOR || socktype == MB_XSURVEYOR;
}

const struct mb_socktype mb_xrespondent_socktype = {
    AF_MB, MB_XRESPONDENT, MB_SOCKTYPE_FLAG_RAW,
    mb_xrespondent_create, mb_xrespondent_ispeer,
};
