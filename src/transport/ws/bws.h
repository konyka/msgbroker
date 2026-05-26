#ifndef MB_TRANSPORT_WS_BWS_H_INCLUDED
#define MB_TRANSPORT_WS_BWS_H_INCLUDED

#include "../../transport.h"
#include "../../utils/list.h"
#include "../../pal/mutex.h"
#include "../../pal/thread.h"

struct mb_ep;

struct mb_bws {
    struct mb_ep *ep;
    int listen_fd;
    struct mb_list sws_list;
    struct mb_mutex lock;
    struct mb_thread accept_thread;
    int running;
};

int mb_bws_create (struct mb_ep *ep);

#endif
