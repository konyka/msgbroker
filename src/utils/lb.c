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

    for (it = mb_list_begin (&self->pipes); it != mb_list_end (&self->pipes);
         it = mb_list_next (&self->pipes, it)) {
        struct mb_lb_data *data = (struct mb_lb_data *) it;
        if (data->active)
            return 1;
    }
    return 0;
}

int mb_lb_send (struct mb_lb *self, struct mb_msg *msg)
{
    struct mb_list_item *it;
    struct mb_list_item *first;

    first = mb_list_begin (&self->pipes);

    for (it = first; it != mb_list_end (&self->pipes);
         it = mb_list_next (&self->pipes, it)) {
        struct mb_lb_data *data = (struct mb_lb_data *) it;
        if (data->active) {
            int rc = mb_pipe_send (data->pipe, msg);
            if (rc == 0) {
                mb_list_erase (&self->pipes, &data->item);
                mb_list_insert (&self->pipes, &data->item,
                    mb_list_end (&self->pipes));
                return 0;
            }
            if (rc != -EAGAIN)
                return rc;
        }
    }

    return -EAGAIN;
}
