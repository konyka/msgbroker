#ifndef MB_TRANSPORT_INPROC_CINPROC_H_INCLUDED
#define MB_TRANSPORT_INPROC_CINPROC_H_INCLUDED

#include "ins.h"
#include "sinproc.h"
#include "../../transport.h"
#include "../../aio/fsm.h"
#include "../../utils/list.h"

struct mb_cinproc {
    struct mb_fsm fsm;
    int state;
    struct mb_ins_item item;
    struct mb_sinproc *sinproc;
};

int mb_cinproc_create (struct mb_ep *ep);

#endif
