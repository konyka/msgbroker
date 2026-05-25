#ifndef MB_TRANSPORT_INPROC_SINPROC_H_INCLUDED
#define MB_TRANSPORT_INPROC_SINPROC_H_INCLUDED

#include "msgqueue.h"
#include "../../transport.h"
#include "../../utils/list.h"

struct mb_ep;

struct mb_sinproc {
    struct mb_sinproc *peer;
    struct mb_pipebase pipebase;
    struct mb_msgqueue msgqueue;
    struct mb_list_item item;
};

int mb_sinproc_create (struct mb_sinproc *self, struct mb_ep *ep);
void mb_sinproc_term (struct mb_sinproc *self);
void mb_sinproc_connect (struct mb_sinproc *self, struct mb_sinproc *peer);
void mb_sinproc_stop (struct mb_sinproc *self);

#endif
