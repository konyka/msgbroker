#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>

#include <errno.h>

struct mb_pub_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
};

struct mb_pub {
    struct mb_sockbase base;
    struct mb_list pipes;
};

static void mb_pub_destroy (struct mb_sockbase *self)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    mb_list_term (&pub->pipes);
    mb_free (pub);
}

static int mb_pub_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    struct mb_pub_pipe_data *data;

    data = (struct mb_pub_pipe_data *) mb_alloc (
        sizeof (struct mb_pub_pipe_data));
    if (!data)
        return -ENOMEM;

    data->pipe = pipe;
    mb_list_item_init (&data->item);
    mb_list_insert (&pub->pipes, &data->item, mb_list_end (&pub->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_pub_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    struct mb_pub_pipe_data *data = (struct mb_pub_pipe_data *)
        mb_pipe_getdata (pipe);

    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&pub->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }
}

static void mb_pub_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static void mb_pub_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_pub_events (struct mb_sockbase *self)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    struct mb_list_item *it;

    /* OUT only when a peer can accept (avoid sticky POLLOUT under backpressure).
     * Send still fire-and-forgets with zero peers. */
    for (it = mb_list_begin (&pub->pipes); it != mb_list_end (&pub->pipes);
         it = mb_list_next (&pub->pipes, it)) {
        struct mb_pub_pipe_data *data = (struct mb_pub_pipe_data *) it;
        if (mb_pipe_can_send (data->pipe))
            return MB_SOCKBASE_EVENT_OUT;
    }
    return 0;
}

static int mb_pub_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    struct mb_list_item *it;
    int sent = 0;
    int have_peer = 0;

    for (it = mb_list_begin (&pub->pipes); it != mb_list_end (&pub->pipes);
         it = mb_list_next (&pub->pipes, it)) {
        struct mb_pub_pipe_data *data = (struct mb_pub_pipe_data *) it;
        struct mb_msg copy;
        int rc;

        have_peer = 1;
        mb_msg_init (&copy, 0);
        mb_msg_cp (&copy, msg);
        rc = mb_pipe_send (data->pipe, &copy);
        if (rc == 0)
            sent++;
        else
            mb_msg_term (&copy);
    }

    /* Peers exist but none accepted — backpressure. Zero peers stay success. */
    if (have_peer && sent == 0)
        return -EAGAIN;
    return 0;
}

static int mb_pub_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_pub_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_pub_vfptr = {
    NULL,
    mb_pub_destroy,
    mb_pub_add,
    mb_pub_rm,
    mb_pub_in,
    mb_pub_out,
    mb_pub_events,
    mb_pub_send,
    NULL,
    mb_pub_setopt,
    mb_pub_getopt,
};

static int mb_pub_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_pub *pub;
    (void) hint;

    pub = (struct mb_pub *) mb_alloc (sizeof (struct mb_pub));
    if (!pub)
        return -ENOMEM;

    mb_sockbase_init (&pub->base, &mb_pub_vfptr, NULL);
    mb_list_init (&pub->pipes);

    *sockbase = &pub->base;
    return 0;
}

static int mb_pub_ispeer (int socktype)
{
    return socktype == MB_SUB;
}

const struct mb_socktype mb_pub_socktype = {
    AF_MB,
    MB_PUB,
    MB_SOCKTYPE_FLAG_NORECV,
    mb_pub_create,
    mb_pub_ispeer,
};
