#ifndef MB_TRANSPORT_TCP_CTCP_H_INCLUDED
#define MB_TRANSPORT_TCP_CTCP_H_INCLUDED

#include "../../transport.h"
#include "../../pal/thread.h"
#include "../../pal/mutex.h"
#include "../ipc/sipc.h"

struct mb_ep;

struct mb_ctcp {
    struct mb_ep *ep;
    struct mb_sipc *sipc;
    struct mb_sipc *zombie;   /* stopped session awaiting free */
    volatile int running;
    int reconnecting;
    struct mb_thread reconnect_thread;
    struct mb_mutex lock;
    char host[256];
    uint16_t port;
};

int mb_ctcp_create (struct mb_ep *ep);

#endif
