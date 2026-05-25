#ifndef MB_TRANSPORT_INPROC_BINPROC_H_INCLUDED
#define MB_TRANSPORT_INPROC_BINPROC_H_INCLUDED

#include "ins.h"
#include "../../transport.h"
#include "../../aio/fsm.h"
#include "../../utils/list.h"

struct mb_binproc {
    struct mb_fsm fsm;
    int state;
    struct mb_ins_item item;
    struct mb_list sinprocs;
};

int mb_binproc_create (struct mb_ep *ep);

#endif
