#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"
#include "../../pal/clock.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_survey.h>

#include <errno.h>

#define MB_SURVEYOR_DEFAULT_DEADLINE_MS 1000

struct mb_surveyor_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
    int active;
};

struct mb_surveyor {
    struct mb_sockbase base;
    struct mb_list pipes;
    int surveying;
    int deadline_ms;
    uint64_t survey_expire_ms;
};

static void mb_surveyor_check_deadline (struct mb_surveyor *sv)
{
    if (!sv->surveying)
        return;
    if (sv->deadline_ms <= 0)
        return;
    if (mb_clock_ms () >= sv->survey_expire_ms)
        sv->surveying = 0;
}

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
        mb_pipe_setdata (pipe, NULL);
    }

    /* No respondents left — abandon the open survey so send is not stuck. */
    if (mb_list_empty (&sv->pipes))
        sv->surveying = 0;
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

    mb_surveyor_check_deadline (sv);

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
    int sent = 0;

    mb_surveyor_check_deadline (sv);

    if (sv->surveying)
        return -EFSM;

    for (it = mb_list_begin (&sv->pipes);
         it != mb_list_end (&sv->pipes);
         it = mb_list_next (&sv->pipes, it)) {
        struct mb_surveyor_pipe_data *data =
            (struct mb_surveyor_pipe_data *) it;
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

    /* Zero delivery must not enter surveying (would block OUT until deadline). */
    if (sent == 0)
        return -EAGAIN;

    sv->surveying = 1;
    if (sv->deadline_ms > 0)
        sv->survey_expire_ms = mb_clock_ms () + (uint64_t) sv->deadline_ms;
    else
        sv->survey_expire_ms = 0;
    return 0;
}

static int mb_surveyor_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;
    struct mb_list_item *it;

    mb_surveyor_check_deadline (sv);

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
    struct mb_surveyor *sv = (struct mb_surveyor *) self;

    if (level != MB_SURVEYOR)
        return -ENOPROTOOPT;

    if (option == MB_SURVEYOR_DEADLINE) {
        int ms;

        if (!optval || optvallen != sizeof (int))
            return -EINVAL;
        ms = *((const int *) optval);
        if (ms < 0)
            return -EINVAL;
        sv->deadline_ms = ms;
        return 0;
    }

    return -ENOPROTOOPT;
}

static int mb_surveyor_getopt (struct mb_sockbase *self, int level,
    int option, void *optval, size_t *optvallen)
{
    struct mb_surveyor *sv = (struct mb_surveyor *) self;

    if (level != MB_SURVEYOR)
        return -ENOPROTOOPT;

    if (option == MB_SURVEYOR_DEADLINE) {
        if (!optval || !optvallen || *optvallen < sizeof (int))
            return -EINVAL;
        *((int *) optval) = sv->deadline_ms;
        *optvallen = sizeof (int);
        return 0;
    }

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
    sv->deadline_ms = MB_SURVEYOR_DEFAULT_DEADLINE_MS;
    sv->survey_expire_ms = 0;

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
