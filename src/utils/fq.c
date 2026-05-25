#include "fq.h"
#include "alloc.h"
#include "cont.h"
#include "list.h"
#include "../protocol.h"
#include "../memory/msg.h"

#include <errno.h>

void mb_fq_init (struct mb_fq *self)
{
    mb_list_init (&self->pipes);
}

void mb_fq_term (struct mb_fq *self)
{
    mb_list_term (&self->pipes);
}

void mb_fq_add (struct mb_fq *self, struct mb_fq_data *data,
    struct mb_pipe *pipe)
{
    data->pipe = pipe;
    data->active = 0;
    mb_list_item_init (&data->item);
    mb_list_insert (&self->pipes, &data->item, mb_list_end (&self->pipes));
}

void mb_fq_rm (struct mb_fq *self, struct mb_fq_data *data)
{
    if (mb_list_item_isinlist (&data->item))
        mb_list_erase (&self->pipes, &data->item);
    mb_list_item_term (&data->item);
}

void mb_fq_activate (struct mb_fq *self, struct mb_fq_data *data)
{
    (void) self;
    data->active = 1;
}

void mb_fq_deactivate (struct mb_fq *self, struct mb_fq_data *data)
{
    (void) self;
    data->active = 0;
}

int mb_fq_can_recv (struct mb_fq *self)
{
    struct mb_list_item *it;

    for (it = mb_list_begin (&self->pipes); it != mb_list_end (&self->pipes);
         it = mb_list_next (&self->pipes, it)) {
        struct mb_fq_data *data = (struct mb_fq_data *) it;
        if (data->active)
            return 1;
    }
    return 0;
}

int mb_fq_recv (struct mb_fq *self, struct mb_msg *msg)
{
    return mb_fq_recv_pipe (self, msg, NULL);
}

int mb_fq_recv_pipe (struct mb_fq *self, struct mb_msg *msg,
    struct mb_pipe **pipe)
{
    struct mb_list_item *it;

    for (it = mb_list_begin (&self->pipes); it != mb_list_end (&self->pipes);
         it = mb_list_next (&self->pipes, it)) {
        struct mb_fq_data *data = (struct mb_fq_data *) it;
        if (data->active) {
            int rc = mb_pipe_recv (data->pipe, msg);
            if (rc == 0) {
                if (pipe)
                    *pipe = data->pipe;
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
