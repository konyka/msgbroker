#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_survey.h>

#include <errno.h>

struct mb_surveyor_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
    int active;
};

struct mb_surveyor {
    struct mb_sockbase base;
    struct mb_list pipes;
    int surveying;
};

static void mb_surveyor_destroy (struct mb_sockbase *self)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    mb_list_term (&sv->pipes);
    mb_free (sv);
}

static int mb_surveyor_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    struct mb_surveyor_pipe_data *data;

    data = (struct mb_surveyor_pipe_data *) mb_alloc (
        sizeof (struct mb_surveyor_pipe_data));
    if (!data)
        return -ENOMEM;

    data->pipe = pipe;
    data->active = 0;
    mb_list_item_init (&data->item);
    mb_list_insert (&sv->pipes, &data->item, mb_list_end (&sv->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_surveyor_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    struct mb_surveyor_pipe_data *data = (struct mb_surveyor_pipe_data *)
        mb_pipe_getdata (pipe);

    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&sv->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }
}

static void mb_surveyor_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self;
    struct mb_surveyor_pipe_data *data = (struct mb_surveyor_pipe_data *)
        mb_pipe_getdata (pipe);
    if (data)
        data->active = 1;
}

static void mb_surveyor_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_surveyor_events (struct mb_sockbase *self)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    int ev = 0;
    struct mb_list_item *it;

    if (!sv->surveying)
        ev |= MB_SOCKBASE_EVENT_OUT;

    if (sv->surveying) {
        for (it = mb_list_begin (&sv->pipes);
             it != mb_list_end (&sv->pipes);
             it = mb_list_next (&sv->pipes, it)) {
            struct mb_surveyor_pipe_data *data =
                (struct mb_surveyor_pipe_data *) it;
            if (data->active) {
                ev |= MB_SOCKBASE_EVENT_IN;
                break;
            }
        }
    }
    return ev;
}

static int mb_surveyor_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    struct mb_list_item *it;

    if (sv->surveying)
        return -EFSM;

    for (it = mb_list_begin (&sv->pipes);
         it != mb_list_end (&sv->pipes);
         it = mb_list_next (&sv->pipes, it)) {
        struct mb_surveyor_pipe_data *data =
            (struct mb_surveyor_pipe_data *) it;
        struct mb_msg copy;
        mb_msg_init (&copy, 0);
        mb_msg_cp (&copy, msg);
        int rc = mb_pipe_send (data->pipe, &copy);
        if (rc < 0)
            mb_msg_term (&copy);
    }

    sv->surveying = 1;
    return 0;
}

static int mb_surveyor_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    struct mb_list_item *it;

    if (!sv->surveying)
        return -EFSM;

    for (it = mb_list_begin (&sv->pipes);
         it != mb_list_end (&sv->pipes);
         it = mb_list_next (&sv->pipes, it)) {
        struct mb_surveyor_pipe_data *data =
            (struct mb_surveyor_pipe_data *) it;
        int rc = mb_pipe_recv (data->pipe, msg);
        if (rc == 0)
            return 0;
        if (rc != -EAGAIN)
            return rc;
    }

    return -EAGAIN;
}

static int mb_surveyor_setopt (struct mb_sockbase *self, int level,
    int option, const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_surveyor_getopt (struct mb_sockbase *self, int level,
    int option, void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_surveyor_vfptr = {
    NULL,
    mb_surveyor_destroy,
    mb_surveyor_add,
    mb_surveyor_rm,
    mb_surveyor_in,
    mb_surveyor_out,
    mb_surveyor_events,
    mb_surveyor_send,
    mb_surveyor_recv,
    mb_surveyor_setopt,
    mb_surveyor_getopt,
};

static int mb_surveyor_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_surveyor *sv;
    (void) hint;

    sv = (struct mb_surveyor *) mb_alloc (sizeof (struct mb_surveyor));
    if (!sv)
        return -ENOMEM;

    mb_sockbase_init (&sv->base, &mb_surveyor_vfptr, NULL);
    mb_list_init (&sv->pipes);
    sv->surveying = 0;

    *sockbase = &sv->base;
    return 0;
}

static int mb_surveyor_ispeer (int socktype)
{
    return socktype == MB_RESPONDENT;
}

const struct mb_socktype mb_surveyor_socktype = {
    AF_MB,
    MB_SURVEYOR,
    0,
    mb_surveyor_create,
    mb_surveyor_ispeer,
};
