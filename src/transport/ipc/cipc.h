#ifndef MB_TRANSPORT_IPC_CIPC_H_INCLUDED
#define MB_TRANSPORT_IPC_CIPC_H_INCLUDED

#include "../../transport.h"
#include "../../pal/thread.h"
#include "../../pal/mutex.h"
#include "sipc.h"

struct mb_ep;

struct mb_cipc {
    struct mb_ep *ep;
    struct mb_sipc *sipc;
    int running;
    struct mb_thread reconnect_thread;
    struct mb_mutex lock;
    char path[108];
};

int mb_cipc_create (struct mb_ep *ep);

#endif
