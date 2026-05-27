#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/fq.h"
#include "../../utils/trie.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>

#include <errno.h>
#include <string.h>

struct mb_sub {
    struct mb_sockbase base;
    struct mb_fq fq;
    struct mb_trie subscriptions;
    int has_subscriptions;
};

static void mb_sub_destroy (struct mb_sockbase *self)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    mb_fq_term (&sub->fq);
    mb_trie_term (&sub->subscriptions);
    mb_free (sub);
}

static int mb_sub_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    struct mb_fq_data *data;

    data = (struct mb_fq_data *) mb_alloc (sizeof (struct mb_fq_data));
    if (!data)
        return -ENOMEM;

    mb_fq_add (&sub->fq, data, pipe);
    mb_fq_activate (&sub->fq, data);
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_sub_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);

    if (data) {
        mb_fq_rm (&sub->fq, data);
        mb_free (data);
    }
}

static void mb_sub_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    struct mb_fq_data *data = (struct mb_fq_data *) mb_pipe_getdata (pipe);
    if (data)
        mb_fq_activate (&sub->fq, data);
}

static void mb_sub_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_sub_events (struct mb_sockbase *self)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    int ev = 0;
    if (mb_fq_can_recv (&sub->fq))
        ev |= MB_SOCKBASE_EVENT_IN;
    return ev;
}

static int mb_sub_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    int rc;

    rc = mb_fq_recv (&sub->fq, msg);
    if (rc < 0)
        return rc;

    if (!sub->has_subscriptions)
        return 0;

    if (mb_trie_match (&sub->subscriptions,
            mb_chunkref_data (&msg->body),
            mb_chunkref_size (&msg->body)))
        return 0;

    mb_msg_term (msg);
    mb_msg_init (msg, 0);
    return -EAGAIN;
}

static int mb_sub_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    struct mb_sub *sub = (struct mb_sub *) self;
    int rc;

    if (level != MB_SUB_PROTO)
        return -ENOPROTOOPT;

    switch (option) {
    case MB_SUB_SUBSCRIBE:
        if (!optval || optvallen == 0)
            return -EINVAL;
        rc = mb_trie_add (&sub->subscriptions, optval, optvallen);
        if (rc == 0)
            sub->has_subscriptions = 1;
        return rc;
    case MB_SUB_UNSUBSCRIBE:
        if (!optval || optvallen == 0)
            return -EINVAL;
        return mb_trie_rm (&sub->subscriptions, optval, optvallen);
    default:
        return -ENOPROTOOPT;
    }
}

static int mb_sub_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_sub_vfptr = {
    NULL,
    mb_sub_destroy,
    mb_sub_add,
    mb_sub_rm,
    mb_sub_in,
    mb_sub_out,
    mb_sub_events,
    NULL,
    mb_sub_recv,
    mb_sub_setopt,
    mb_sub_getopt,
};

static int mb_sub_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_sub *sub;
    (void) hint;

    sub = (struct mb_sub *) mb_alloc (sizeof (struct mb_sub));
    if (!sub)
        return -ENOMEM;

    mb_sockbase_init (&sub->base, &mb_sub_vfptr, NULL);
    mb_fq_init (&sub->fq);
    mb_trie_init (&sub->subscriptions);
    sub->has_subscriptions = 0;

    *sockbase = &sub->base;
    return 0;
}

static int mb_sub_ispeer (int socktype)
{
    return socktype == MB_PUB;
}

const struct mb_socktype mb_sub_socktype = {
    AF_MB,
    MB_SUB,
    MB_SOCKTYPE_FLAG_NOSEND,
    mb_sub_create,
    mb_sub_ispeer,
};
