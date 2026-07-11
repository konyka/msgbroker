#ifndef MB_TRANSPORT_TCP_BTCP_H_INCLUDED
#define MB_TRANSPORT_TCP_BTCP_H_INCLUDED

#include "../../transport.h"
#include "../../utils/list.h"
#include "../../pal/mutex.h"
#include "../../pal/thread.h"

struct mb_ep;

struct mb_btcp {
    struct mb_ep *ep;
    int listen_fd;
    struct mb_list sipcs;
    struct mb_list zombies;
    struct mb_mutex lock;
    struct mb_thread accept_thread;
    volatile int running;
};

int mb_btcp_create (struct mb_ep *ep);

#endif
