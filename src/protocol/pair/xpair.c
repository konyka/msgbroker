#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include <errno.h>

struct mb_xpair_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
};

struct mb_xpair {
    struct mb_sockbase base;
    struct mb_list pipes;
};

static void mb_xpair_destroy (struct mb_sockbase *self)
{
    struct mb_xpair *xp = (struct mb_xpair *) self;
    struct mb_list_item *it = mb_list_begin (&xp->pipes);
    while (it != mb_list_end (&xp->pipes)) {
        struct mb_xpair_pipe_data *data = (struct mb_xpair_pipe_data *) it;
        struct mb_list_item *next = mb_list_next (&xp->pipes, it);
        mb_list_erase (&xp->pipes, &data->item);
        mb_free (data);
        it = next;
    }
    mb_list_term (&xp->pipes);
    mb_free (xp);
}

static int mb_xpair_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpair *xp = (struct mb_xpair *) self;
    struct mb_xpair_pipe_data *data = (struct mb_xpair_pipe_data *)
        mb_alloc (sizeof (struct mb_xpair_pipe_data));
    if (!data)
        return -ENOMEM;
    data->pipe = pipe;
    mb_list_item_init (&data->item);
    mb_list_insert (&xp->pipes, &data->item, mb_list_end (&xp->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_xpair_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_xpair *xp = (struct mb_xpair *) self;
    struct mb_xpair_pipe_data *data = (struct mb_xpair_pipe_data *)
        mb_pipe_getdata (pipe);
    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&xp->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }
}

static void mb_xpair_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static void mb_xpair_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_xpair_events (struct mb_sockbase *self)
{
    struct mb_xpair *xp = (struct mb_xpair *) self;
    int ev = 0;
    struct mb_list_item *it;

    if (mb_list_begin (&xp->pipes) != mb_list_end (&xp->pipes))
        ev |= MB_SOCKBASE_EVENT_OUT;
    for (it = mb_list_begin (&xp->pipes); it != mb_list_end (&xp->pipes);
         it = mb_list_next (&xp->pipes, it)) {
        struct mb_xpair_pipe_data *data = (struct mb_xpair_pipe_data *) it;
        if (mb_pipe_has_msg (data->pipe)) {
            ev |= MB_SOCKBASE_EVENT_IN;
            break;
        }
    }
    return ev;
}

static int mb_xpair_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xpair *xp = (struct mb_xpair *) self;
    struct mb_list_item *it;
    int sent = 0;

    for (it = mb_list_begin (&xp->pipes);
         it != mb_list_end (&xp->pipes);
         it = mb_list_next (&xp->pipes, it)) {
        struct mb_xpair_pipe_data *data = (struct mb_xpair_pipe_data *) it;
        struct mb_msg copy;
        int rc;

        mb_msg_init (&copy, 0);
        mb_msg_cp (&copy, msg);
        rc = mb_pipe_send (data->pipe, &copy);
        if (rc == 0)
            sent++;
        else
            mb_msg_term (&copy);
    }
    /* Match PAIR/XSURVEYOR: zero delivery is not success. */
    if (sent == 0)
        return -EAGAIN;
    return 0;
}

static int mb_xpair_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_xpair *xp = (struct mb_xpair *) self;
    struct mb_list_item *it;

    for (it = mb_list_begin (&xp->pipes); it != mb_list_end (&xp->pipes); ) {
        struct mb_xpair_pipe_data *data = (struct mb_xpair_pipe_data *) it;
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

static int mb_xpair_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_xpair_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_xpair_vfptr = {
    NULL,
    mb_xpair_destroy,
    mb_xpair_add,
    mb_xpair_rm,
    mb_xpair_in,
    mb_xpair_out,
    mb_xpair_events,
    mb_xpair_send,
    mb_xpair_recv,
    mb_xpair_setopt,
    mb_xpair_getopt,
};

static int mb_xpair_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_xpair *xp;
    (void) hint;
    xp = (struct mb_xpair *) mb_alloc (sizeof (struct mb_xpair));
    if (!xp)
        return -ENOMEM;
    mb_sockbase_init (&xp->base, &mb_xpair_vfptr, NULL);
    mb_list_init (&xp->pipes);
    *sockbase = &xp->base;
    return 0;
}

static int mb_xpair_ispeer (int socktype)
{
    return socktype == MB_PAIR || socktype == MB_XPAIR;
}

const struct mb_socktype mb_xpair_socktype = {
    AF_MB,
    MB_XPAIR,
    MB_SOCKTYPE_FLAG_RAW,
    mb_xpair_create,
    mb_xpair_ispeer,
};
