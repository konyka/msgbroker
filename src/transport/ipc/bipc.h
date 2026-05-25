#ifndef MB_TRANSPORT_IPC_BIPC_H_INCLUDED
#define MB_TRANSPORT_IPC_BIPC_H_INCLUDED

#include "../../transport.h"
#include "../../utils/list.h"
#include "../../pal/mutex.h"
#include "../../pal/thread.h"

struct mb_ep;

struct mb_bipc {
    struct mb_ep *ep;
    int listen_fd;
    struct mb_list sipcs;
    struct mb_mutex lock;
    struct mb_thread accept_thread;
    int running;
    char path[108];
};

int mb_bipc_create (struct mb_ep *ep);

#endif
