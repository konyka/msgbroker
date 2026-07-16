#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_survey.h>

#include <errno.h>

struct mb_respondent_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
    int active;
};

struct mb_respondent {
    struct mb_sockbase base;
    struct mb_list pipes;
    struct mb_pipe *last_pipe;
};

static void mb_respondent_destroy (struct mb_sockbase *self)
{
    struct mb_respondent *resp = (struct mb_respondent *) self;
    mb_list_term (&resp->pipes);
    mb_free (resp);
}

static int mb_respondent_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_respondent *resp = (struct mb_respondent *) self;
    struct mb_respondent_pipe_data *data;

    data = (struct mb_respondent_pipe_data *) mb_alloc (
        sizeof (struct mb_respondent_pipe_data));
    if (!data)
        return -ENOMEM;

    data->pipe = pipe;
    data->active = 0;
    mb_list_item_init (&data->item);
    mb_list_insert (&resp->pipes, &data->item, mb_list_end (&resp->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_respondent_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_respondent *resp = (struct mb_respondent *) self;
    struct mb_respondent_pipe_data *data = (struct mb_respondent_pipe_data *)
        mb_pipe_getdata (pipe);

    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&resp->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }

    if (resp->last_pipe == pipe)
        resp->last_pipe = NULL;
}

static void mb_respondent_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self;
    struct mb_respondent_pipe_data *data = (struct mb_respondent_pipe_data *)
        mb_pipe_getdata (pipe);
    if (data)
        data->active = 1;
}

static void mb_respondent_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_respondent_events (struct mb_sockbase *self)
{
    struct mb_respondent *resp = (struct mb_respondent *) self;
    int ev = 0;
    struct mb_list_item *it;

    if (resp->last_pipe)
        ev |= MB_SOCKBASE_EVENT_OUT;

    for (it = mb_list_begin (&resp->pipes);
         it != mb_list_end (&resp->pipes);
         it = mb_list_next (&resp->pipes, it)) {
        struct mb_respondent_pipe_data *data =
            (struct mb_respondent_pipe_data *) it;
        if (data->active) {
            ev |= MB_SOCKBASE_EVENT_IN;
            break;
        }
    }
    return ev;
}

static int mb_respondent_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_respondent *resp = (struct mb_respondent *) self;
    struct mb_list_item *it;

    for (it = mb_list_begin (&resp->pipes);
         it != mb_list_end (&resp->pipes);
         it = mb_list_next (&resp->pipes, it)) {
        struct mb_respondent_pipe_data *data =
            (struct mb_respondent_pipe_data *) it;
        int rc = mb_pipe_recv (data->pipe, msg);
        if (rc == 0) {
            resp->last_pipe = data->pipe;
            return 0;
        }
        if (rc != -EAGAIN)
            return rc;
    }

    return -EAGAIN;
}

static int mb_respondent_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_respondent *resp = (struct mb_respondent *) self;

    if (!resp->last_pipe)
        return -EAGAIN;

    int rc = mb_pipe_send (resp->last_pipe, msg);
    /* Keep last_pipe on -EAGAIN so mb_send can retry the same peer after
     * transport backpressure; clear on success or hard error. */
    if (rc != -EAGAIN)
        resp->last_pipe = NULL;
    return rc;
}

static int mb_respondent_setopt (struct mb_sockbase *self, int level,
    int option, const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_respondent_getopt (struct mb_sockbase *self, int level,
    int option, void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_respondent_vfptr = {
    NULL,
    mb_respondent_destroy,
    mb_respondent_add,
    mb_respondent_rm,
    mb_respondent_in,
    mb_respondent_out,
    mb_respondent_events,
    mb_respondent_send,
    mb_respondent_recv,
    mb_respondent_setopt,
    mb_respondent_getopt,
};

static int mb_respondent_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_respondent *resp;
    (void) hint;

    resp = (struct mb_respondent *) mb_alloc (sizeof (struct mb_respondent));
    if (!resp)
        return -ENOMEM;

    mb_sockbase_init (&resp->base, &mb_respondent_vfptr, NULL);
    mb_list_init (&resp->pipes);
    resp->last_pipe = NULL;

    *sockbase = &resp->base;
    return 0;
}

static int mb_respondent_ispeer (int socktype)
{
    return socktype == MB_SURVEYOR;
}

const struct mb_socktype mb_respondent_socktype = {
    AF_MB,
    MB_RESPONDENT,
    0,
    mb_respondent_create,
    mb_respondent_ispeer,
};
