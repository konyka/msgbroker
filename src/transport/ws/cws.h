#ifndef MB_TRANSPORT_WS_CWS_H_INCLUDED
#define MB_TRANSPORT_WS_CWS_H_INCLUDED

#include "../../transport.h"
#include "../../pal/thread.h"
#include "../../pal/mutex.h"
#include "../../utils/net.h"
#include "sws.h"

#include <stdint.h>

struct mb_ep;

struct mb_cws {
    struct mb_ep *ep;
    struct mb_sws *sws;
    struct mb_sws *zombie;   /* stopped session awaiting free */
    volatile int running;
    int reconnecting;
    struct mb_thread reconnect_thread;
    struct mb_mutex lock;
    char host[256];
    uint16_t port;
    struct mb_net_epaddr resolved;
};

int mb_cws_create (struct mb_ep *ep);

#endif
