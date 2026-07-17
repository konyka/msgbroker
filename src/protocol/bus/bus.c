#include "../../protocol.h"
#include "../../transport.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../memory/msg.h"

#include <msgbroker/mb.h>
#include <msgbroker/mb_bus.h>

#include <errno.h>

struct mb_bus_pipe_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
    int active;
};

struct mb_bus {
    struct mb_sockbase base;
    struct mb_list pipes;
};

static void mb_bus_destroy (struct mb_sockbase *self)
{
    struct mb_bus *bus = (struct mb_bus *) self;
    mb_list_term (&bus->pipes);
    mb_free (bus);
}

static int mb_bus_add (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_bus *bus = (struct mb_bus *) self;
    struct mb_bus_pipe_data *data;

    data = (struct mb_bus_pipe_data *) mb_alloc (
        sizeof (struct mb_bus_pipe_data));
    if (!data)
        return -ENOMEM;

    data->pipe = pipe;
    data->active = 0;
    mb_list_item_init (&data->item);
    mb_list_insert (&bus->pipes, &data->item, mb_list_end (&bus->pipes));
    mb_pipe_setdata (pipe, data);
    return 0;
}

static void mb_bus_rm (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    struct mb_bus *bus = (struct mb_bus *) self;
    struct mb_bus_pipe_data *data = (struct mb_bus_pipe_data *)
        mb_pipe_getdata (pipe);

    if (data) {
        if (mb_list_item_isinlist (&data->item))
            mb_list_erase (&bus->pipes, &data->item);
        mb_list_item_term (&data->item);
        mb_free (data);
    }
}

static void mb_bus_in (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self;
    struct mb_bus_pipe_data *data = (struct mb_bus_pipe_data *)
        mb_pipe_getdata (pipe);
    if (data)
        data->active = 1;
}

static void mb_bus_out (struct mb_sockbase *self, struct mb_pipe *pipe)
{
    (void) self; (void) pipe;
}

static int mb_bus_events (struct mb_sockbase *self)
{
    struct mb_bus *bus = (struct mb_bus *) self;
    int ev = 0;
    struct mb_list_item *it;

    if (mb_list_begin (&bus->pipes) != mb_list_end (&bus->pipes))
        ev |= MB_SOCKBASE_EVENT_OUT;

    for (it = mb_list_begin (&bus->pipes); it != mb_list_end (&bus->pipes);
         it = mb_list_next (&bus->pipes, it)) {
        struct mb_bus_pipe_data *data = (struct mb_bus_pipe_data *) it;
        if (data->active) {
            ev |= MB_SOCKBASE_EVENT_IN;
            break;
        }
    }
    return ev;
}

static int mb_bus_send (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_bus *bus = (struct mb_bus *) self;
    struct mb_list_item *it;
    int sent = 0;

    for (it = mb_list_begin (&bus->pipes); it != mb_list_end (&bus->pipes);
         it = mb_list_next (&bus->pipes, it)) {
        struct mb_bus_pipe_data *data = (struct mb_bus_pipe_data *) it;
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

    /* Match XBUS: zero delivery is not success. */
    if (sent == 0)
        return -EAGAIN;
    return 0;
}

static int mb_bus_recv (struct mb_sockbase *self, struct mb_msg *msg)
{
    struct mb_bus *bus = (struct mb_bus *) self;
    struct mb_list_item *it;

    /* Probe every pipe: IN callbacks are not always wired, so active is only
     * a poll hint. Clear sticky active on EAGAIN; rotate on success. */
    for (it = mb_list_begin (&bus->pipes); it != mb_list_end (&bus->pipes); ) {
        struct mb_bus_pipe_data *data = (struct mb_bus_pipe_data *) it;
        struct mb_list_item *next = mb_list_next (&bus->pipes, it);
        int rc = mb_pipe_recv (data->pipe, msg);

        if (rc == 0) {
            data->active = 1;
            mb_list_erase (&bus->pipes, &data->item);
            mb_list_insert (&bus->pipes, &data->item,
                mb_list_end (&bus->pipes));
            return 0;
        }
        if (rc == -EAGAIN)
            data->active = 0;
        else
            return rc;
        it = next;
    }

    return -EAGAIN;
}

static int mb_bus_setopt (struct mb_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static int mb_bus_getopt (struct mb_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    (void) self; (void) level; (void) option;
    (void) optval; (void) optvallen;
    return -ENOPROTOOPT;
}

static const struct mb_sockbase_vfptr mb_bus_vfptr = {
    NULL,
    mb_bus_destroy,
    mb_bus_add,
    mb_bus_rm,
    mb_bus_in,
    mb_bus_out,
    mb_bus_events,
    mb_bus_send,
    mb_bus_recv,
    mb_bus_setopt,
    mb_bus_getopt,
};

static int mb_bus_create (void *hint, struct mb_sockbase **sockbase)
{
    struct mb_bus *bus;
    (void) hint;

    bus = (struct mb_bus *) mb_alloc (sizeof (struct mb_bus));
    if (!bus)
        return -ENOMEM;

    mb_sockbase_init (&bus->base, &mb_bus_vfptr, NULL);
    mb_list_init (&bus->pipes);

    *sockbase = &bus->base;
    return 0;
}

static int mb_bus_ispeer (int socktype)
{
    return socktype == MB_BUS;
}

const struct mb_socktype mb_bus_socktype = {
    AF_MB,
    MB_BUS,
    0,
    mb_bus_create,
    mb_bus_ispeer,
};
