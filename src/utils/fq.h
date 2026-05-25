#ifndef MB_UTILS_FQ_H_INCLUDED
#define MB_UTILS_FQ_H_INCLUDED

#include "../utils/list.h"
#include "../memory/msg.h"

struct mb_pipe;

struct mb_fq_data {
    struct mb_list_item item;
    struct mb_pipe *pipe;
    int active;
};

struct mb_fq {
    struct mb_list pipes;
};

void mb_fq_init (struct mb_fq *self);
void mb_fq_term (struct mb_fq *self);
void mb_fq_add (struct mb_fq *self, struct mb_fq_data *data,
    struct mb_pipe *pipe);
void mb_fq_rm (struct mb_fq *self, struct mb_fq_data *data);
void mb_fq_activate (struct mb_fq *self, struct mb_fq_data *data);
void mb_fq_deactivate (struct mb_fq *self, struct mb_fq_data *data);
int mb_fq_can_recv (struct mb_fq *self);
int mb_fq_recv (struct mb_fq *self, struct mb_msg *msg);
int mb_fq_recv_pipe (struct mb_fq *self, struct mb_msg *msg,
    struct mb_pipe **pipe);

#endif
