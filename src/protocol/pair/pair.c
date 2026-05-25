#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct mb_pair {
    struct mb_sockbase base;
    struct mb_pipe *pipe;
};

static void mb_pair_stop (struct mb_sockbase *self)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    (void) pair;
}

static void mb_pair_destroy (struct mb_sockbase *self)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    mb_free (pair);
}

static int mb_pair_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    if (pair->pipe)
        return -EISCONN;
    pair->pipe = pipe;
    return 0;
}

static void mb_pair_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    (void) pipe;
    pair->pipe = NULL;
}

static void mb_pair_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static void mb_pair_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_pair_events (struct mb_sockbase *self)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    int ev = 0;
    if (pair->pipe)
        ev |= MB_SOCKBASE_EVENT_OUT;
    ev |= MB_SOCKBASE_EVENT_IN;
    return ev;
}

static int mb_pair_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    if (!pair->pipe)
        return -EAGAIN;
    return mb_pipe_send (pair->pipe, msg);
}

static int mb_pair_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_pair *pair = (struct mb_pair *) self;
    if (!pair->pipe)
        return -EAGAIN;
    return mb_pipe_recv (pair->pipe, msg);
}

static int mb_pair_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_pair_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_pair_sockbase_vfptr = {
    mb_pair_stop,
    mb_pair_destroy,
    mb_pair_add,
    mb_pair_rm,
    mb_pair_in,
    mb_pair_out,
    mb_pair_events,
    mb_pair_send,
    mb_pair_recv,
    mb_pair_setopt,
    mb_pair_getopt,
};

static int mb_pair_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_pair *pair;
    (void) hint;

    pair = (struct mb_pair *) mb_alloc (sizeof (struct mb_pair));
    if (!pair)
        return -ENOMEM;

    mb_sockbase_init (&pair->base, &mb_pair_sockbase_vfptr, NULL);
    pair->pipe = NULL;

    *sockbase = &pair->base;
    return 0;
}

static int mb_pair_ispeer (int socktype)
{
    return socktype == MB_PAIR;
}

const struct mb_socktype mb_pair_socktype = {
    AF_MB,
    MB_PAIR,
    0,
    mb_pair_create,
    mb_pair_ispeer,
};
