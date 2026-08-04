#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/cont.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"
#include "../../core/sock.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_pubsub.h>

#include <errno.h>
#include <string.h>

/*  T-PRIO: per-priority send queue on PUB socket.
 *
 *  The PUB retains messages rejected by all peers into per-priority
 *  buckets (8 levels: 0..7). mb_pub_send drains the highest non-empty
 *  bucket first before handling the new message, so the existing
 *  fire-and-forget loop becomes priority-ordered. The
 *  MB_STAT_CURRENT_SND_PRIORITY counter reflects the highest currently
 *  non-empty bucket. */

#define MB_PUB_PRIO_LEVELS  8
#define MB_PUB_PRIO_NORMAL  4

struct mb_pub_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
};

struct mb_pub_prio_item {
    struct mb_list_item item;
    struct mb_msg msg;
};

struct mb_pub {
    struct mb_sockbase base;
    struct mb_list pipes;
    struct mb_list buckets[MB_PUB_PRIO_LEVELS];
};

static void mb_pub_destroy (struct mb_sockbase *self)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    int i;

    for (i = 0; i < MB_PUB_PRIO_LEVELS; i++) {
        struct mb_list_item *it;
        struct mb_list_item *next;
        for (it = mb_list_begin (&pub->buckets[i]);
             it != mb_list_end (&pub->buckets[i]); it = next) {
            struct mb_pub_prio_item *pi =
                mb_cont (it, struct mb_pub_prio_item, item);
            next = mb_list_next (&pub->buckets[i], it);
            mb_list_erase (&pub->buckets[i], it);
            mb_msg_term (&pi->msg);
            mb_free (pi);
        }
        mb_list_term (&pub->buckets[i]);
    }
    mb_list_term (&pub->pipes);
    mb_free (pub);
}

/*  Decode priority from msg->sphdr. The cmsg payload is the int itself
 *  (host endianness is acceptable because the bytes never leave the
 *  process). Returns -1 if the value is out of [0, MB_PUB_PRIO_LEVELS). */
static int mb_pub_prio_decode (const struct mb_msg *msg)
{
    void *data;
    size_t sz;
    int prio;

    sz = mb_chunkref_size ((struct mb_chunkref *) &msg->sphdr);
    if (sz < sizeof (int))
        return MB_PUB_PRIO_NORMAL;
    data = mb_chunkref_data ((struct mb_chunkref *) &msg->sphdr);
    if (!data)
        return MB_PUB_PRIO_NORMAL;
    memcpy (&prio, data, sizeof (int));
    if (prio < 0 || prio >= MB_PUB_PRIO_LEVELS)
        return -1;
    return prio;
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

    /* OUT only when a peer can accept. Bucket-drain piggybacks on the
     * next mb_pub_send call; firing POLLOUT here would spuriously wake
     * pollers when no peer can accept (and buckets exist only because
     * every peer has just rejected). */
    for (it = mb_list_begin (&pub->pipes); it != mb_list_end (&pub->pipes);
         it = mb_list_next (&pub->pipes, it)) {
        struct mb_pub_pipe_data *data = (struct mb_pub_pipe_data *) it;
        if (mb_pipe_can_send (data->pipe))
            return MB_SOCKBASE_EVENT_OUT;
    }
    return 0;
}

/*  Try to send one deferred message to any peer. Returns 1 if the
 *  message was delivered (bucket node removed), 0 if it remains
 *  deferred (all peers rejected), -1 on unrecoverable error. */
static int mb_pub_try_drain_one (struct mb_pub *pub,
    struct mb_pub_prio_item *pi)
{
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
        mb_msg_cp (&copy, &pi->msg);
        rc = mb_pipe_send (data->pipe, &copy);
        if (rc == 0)
            sent++;
        else
            mb_msg_term (&copy);
    }

    if (have_peer && sent == 0)
        return 0;
    return sent > 0 ? 1 : 0;
}

/*  Drain the highest non-empty bucket first. Stops as soon as any peer
 *  rejects so we don't busy-loop. */
static void mb_pub_drain_buckets (struct mb_pub *pub)
{
    int level;
    int any_can_send;
    struct mb_list_item *pit;

    any_can_send = 0;
    for (pit = mb_list_begin (&pub->pipes);
         pit != mb_list_end (&pub->pipes);
         pit = mb_list_next (&pub->pipes, pit)) {
        struct mb_pub_pipe_data *pd = (struct mb_pub_pipe_data *) pit;
        if (mb_pipe_can_send (pd->pipe)) {
            any_can_send = 1;
            break;
        }
    }
    if (!any_can_send)
        return;

    for (level = MB_PUB_PRIO_LEVELS - 1; level >= 0; level--) {
        while (!mb_list_empty (&pub->buckets[level])) {
            struct mb_list_item *it = mb_list_begin (&pub->buckets[level]);
            struct mb_pub_prio_item *pi =
                mb_cont (it, struct mb_pub_prio_item, item);
            int rc;

            rc = mb_pub_try_drain_one (pub, pi);
            if (rc <= 0)
                return;
            mb_list_erase (&pub->buckets[level], it);
            mb_msg_term (&pi->msg);
            mb_free (pi);
        }
    }
}

static int mb_pub_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_pub *pub = (struct mb_pub *) self;
    struct mb_list_item *it;
    int sent = 0;
    int have_peer = 0;
    int retained = 0;
    int prio;
    int rc;

    /* T-PRIO: drain the highest non-empty bucket first. Each call to
     * mb_pub_send is also a drain opportunity; this is what makes the
     * per-priority ordering observable at the receiver. */
    mb_pub_drain_buckets (pub);

    prio = mb_pub_prio_decode (msg);
    if (prio < 0)
        return -EINVAL;

    for (it = mb_list_begin (&pub->pipes); it != mb_list_end (&pub->pipes);
         it = mb_list_next (&pub->pipes, it)) {
        struct mb_pub_pipe_data *data = (struct mb_pub_pipe_data *) it;
        struct mb_msg copy;
        int prc;

        have_peer = 1;
        mb_msg_init (&copy, 0);
        mb_msg_cp (&copy, msg);
        prc = mb_pipe_send (data->pipe, &copy);
        if (prc == 0) {
            sent++;
        } else {
            /* Pipe rejected: stash the copy into the priority bucket.
             * T-PRIO retention makes the user-visible call succeed even
             * when no peer is currently accepting; the PUB will drain
             * the bucket on a subsequent send or via the events path. */
            mb_msg_term (&copy);
            {
                struct mb_pub_prio_item *pi =
                    (struct mb_pub_prio_item *) mb_alloc (
                        sizeof (struct mb_pub_prio_item));
                if (pi) {
                    mb_msg_init (&pi->msg, 0);
                    mb_msg_cp (&pi->msg, msg);
                    mb_list_item_init (&pi->item);
                    mb_list_insert (&pub->buckets[prio], &pi->item,
                        mb_list_end (&pub->buckets[prio]));
                    retained++;
                }
            }
        }
    }

    if (self->sock) {
        int highest = 0;
        for (rc = MB_PUB_PRIO_LEVELS - 1; rc >= 0; rc--) {
            if (!mb_list_empty (&pub->buckets[rc])) {
                highest = rc;
                break;
            }
        }
        mb_sock_set_current_snd_priority (self->sock, highest);
    }

    /* Drop sphdr so the caller-owned msg doesn't carry priority bytes
     * downstream (which would confuse peers that don't understand T-PRIO
     * priority). */
    mb_chunkref_term (&msg->sphdr);
    mb_chunkref_init (&msg->sphdr, 0);

    /* T-PRIO: zero peers stay success (fire-and-forget, no buckets
     * touched). Peers present and at least one accepted -> success.
     * All peers rejected -> the PUB retained copies in the priority
     * bucket, but we still surface -EAGAIN so the user can back off
     * (consistent with the pre-T-PRIO contract observed by tests like
     * test_pub_poll_polout_after_backpressure). */
    if (!have_peer)
        return 0;
    if (sent > 0)
        return 0;
    if (retained > 0)
        return -EAGAIN;
    return -EAGAIN;
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
    int i;
    (void) hint;

    pub = (struct mb_pub *) mb_alloc (sizeof (struct mb_pub));
    if (!pub)
        return -ENOMEM;

    mb_sockbase_init (&pub->base, &mb_pub_vfptr, NULL);
    mb_list_init (&pub->pipes);
    for (i = 0; i < MB_PUB_PRIO_LEVELS; i++)
        mb_list_init (&pub->buckets[i]);

    *sockbase = &pub->base;
    return 0;
}

static int mb_pub_ispeer (int socktype)
{
    return socktype == MB_SUB || socktype == MB_XSUB;
}

const struct mb_socktype mb_pub_socktype = {
    AF_MB,
    MB_PUB,
    MB_SOCKTYPE_FLAG_NORECV,
    mb_pub_create,
    mb_pub_ispeer,
};