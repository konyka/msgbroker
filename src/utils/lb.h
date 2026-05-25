#ifndef MB_UTILS_LB_H_INCLUDED
#define MB_UTILS_LB_H_INCLUDED

#include "../utils/list.h"
#include "../memory/msg.h"

struct mb_pipe;

struct mb_lb_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
    int active;
};

struct mb_lb {
    struct mb_list pipes;
};

void mb_lb_init (struct mb_lb *self);
void mb_lb_term (struct mb_lb *self);
void mb_lb_add (struct mb_lb *self, struct mb_lb_data *data,
    struct mb_pipe *pipe);
void mb_lb_rm (struct mb_lb *self, struct mb_lb_data *data);
void mb_lb_activate (struct mb_lb *self, struct mb_lb_data *data);
void mb_lb_deactivate (struct mb_lb *self, struct mb_lb_data *data);
int mb_lb_can_send (struct mb_lb *self);
int mb_lb_send (struct mb_lb *self, struct mb_msg *msg);

#endif
