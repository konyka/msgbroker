#ifndef MB_TRANSPORT_TCP_CTCP_H_INCLUDED
#define MB_TRANSPORT_TCP_CTCP_H_INCLUDED

#include "../../transport.h"
#include "../ipc/sipc.h"

struct mb_ep;

struct mb_ctcp {
    struct mb_ep *ep;
    struct mb_sipc *sipc;
};

int mb_ctcp_create (struct mb_ep *ep);

#endif
