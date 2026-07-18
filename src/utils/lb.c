#include "lb.h"
#include "alloc.h"
#include "cont.h"
#include "list.h"
#include "../protocol.h"
#include "../memory/msg.h"

#include <errno.h>

void mb_lb_init (struct mb_lb *self)
{
    mb_list_init (&self->pipes);
}

void mb_lb_term (struct mb_lb *self)
{
    mb_list_term (&self->pipes);
}

void mb_lb_add (struct mb_lb *self, struct mb_lb_data *data,
    struct mb_pipe *pipe)
{
    data->pipe = pipe;
    data->active = 0;
    mb_list_item_init (&data->item);
    mb_list_insert (&self->pipes, &data->item, mb_list_end (&self->pipes));
}

void mb_lb_rm (struct mb_lb *self, struct mb_lb_data *data)
{
    if (mb_list_item_isinlist (&data->item))
        mb_list_erase (&self->pipes, &data->item);
    mb_list_item_term (&data->item);
}

void mb_lb_activate (struct mb_lb *self, struct mb_lb_data *data)
{
    (void) self;
    data->active = 1;
}

void mb_lb_deactivate (struct mb_lb *self, struct mb_lb_data *data)
{
    (void) self;
    data->active = 0;
}

int mb_lb_can_send (struct mb_lb *self)
{
    struct mb_list_item *it;

    /* Probe real writability — OUT callbacks are unwired, so sticky active
     * alone never recovers after backpressure clears all pipes. */
    for (it = mb_list_begin (&self->pipes); it != mb_list_end (&self->pipes);
         it = mb_list_next (&self->pipes, it)) {
        struct mb_lb_data *data = (struct mb_lb_data *) it;
        if (mb_pipe_can_send (data->pipe)) {
            data->active = 1;
            return 1;
        }
        data->active = 0;
    }
    return 0;
}

int mb_lb_send (struct mb_lb *self, struct mb_msg *msg)
{
    struct mb_list_item *it;

    /* Probe every pipe: sockbase OUT callbacks are not wired, so active is
     * only a poll hint. Clear sticky active on EAGAIN; rotate on success. */
    for (it = mb_list_begin (&self->pipes); it != mb_list_end (&self->pipes); ) {
        struct mb_lb_data *data = (struct mb_lb_data *) it;
        struct mb_list_item *next = mb_list_next (&self->pipes, it);
        int rc = mb_pipe_send (data->pipe, msg);

        if (rc == 0) {
            data->active = 1;
            mb_list_erase (&self->pipes, &data->item);
            mb_list_insert (&self->pipes, &data->item,
                mb_list_end (&self->pipes));
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
