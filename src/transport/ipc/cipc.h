#ifndef MB_TRANSPORT_IPC_CIPC_H_INCLUDED
#define MB_TRANSPORT_IPC_CIPC_H_INCLUDED

#include "../../transport.h"

struct mb_ep;

struct mb_cipc {
    struct mb_ep *ep;
    struct mb_sipc *sipc;
};

int mb_cipc_create (struct mb_ep *ep);

#endif
