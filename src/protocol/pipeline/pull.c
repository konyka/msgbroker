#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/fq.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>

#include <errno.h>

struct mb_pull {
    struct mb_sockbase base;
    struct mb_fq fq;
};

static void mb_pull_destroy (struct mb_sockbase *self)
{
    struct mb_pull *pull = (struct mb_pull *) self;
    mb_fq_term (&pull->fq);
    mb_free (pull);
}

static int mb_pull_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pull *pull = (struct mb_pull *) self;
    struct mb_fq_data *data;

    data = (struct mb_fq_data *) mb_alloc (sizeof (struct mb_fq_data));
    if (!data)
        return -ENOMEM;

    mb_fq_add (&pull->fq, data, pipe);
    mb_fq_activate (&pull->fq, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_pull_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pull *pull = (struct mb_pull *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);

    if (data) {
        mb_fq_rm (&pull->fq, data);
        mb_free (data);
    }
}

static void mb_pull_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pull *pull = (struct mb_pull *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);
    if (data)
        mb_fq_activate (&pull->fq, data);
}

static void mb_pull_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_pull_events (struct mb_sockbase *self)
{
    struct mb_pull *pull = (struct mb_pull *) self;
    int ev = 0;
    if (mb_fq_can_recv (&pull->fq))
        ev |= MB_SOCKBASE_EVENT_IN;
    return ev;
}

static int mb_pull_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_pull *pull = (struct mb_pull *) self;
    return mb_fq_recv (&pull->fq, msg);
}

static int mb_pull_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_pull_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_pull_vfptr = {
    NULL,
    mb_pull_destroy,
    mb_pull_add,
    mb_pull_rm,
    mb_pull_in,
    mb_pull_out,
    mb_pull_events,
    NULL,
    mb_pull_recv,
    mb_pull_setopt,
    mb_pull_getopt,
};

static int mb_pull_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_pull *pull;
    (void) hint;

    pull = (struct mb_pull *) mb_alloc (sizeof (struct mb_pull));
    if (!pull)
        return -ENOMEM;

    mb_sockbase_init (&pull->base, &mb_pull_vfptr, NULL);
    mb_fq_init (&pull->fq);

    *sockbase = &pull->base;
    return 0;
}

static int mb_pull_ispeer (int socktype)
{
    return socktype == MB_PUSH;
}

const struct mb_socktype mb_pull_socktype = {
    AF_MB,
    MB_PULL,
    MB_SOCKTYPE_FLAG_NOSEND,
    mb_pull_create,
    mb_pull_ispeer,
};
