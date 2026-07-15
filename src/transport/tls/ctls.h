#ifndef MB_TRANSPORT_TLS_CTLS_H_INCLUDED
#define MB_TRANSPORT_TLS_CTLS_H_INCLUDED

#include "../../transport.h"
#include "../../pal/thread.h"
#include "../../pal/mutex.h"
#include "../../utils/net.h"
#include "stls.h"

#include <openssl/ssl.h>
#include <stdint.h>

struct mb_ep;

struct mb_ctls {
    struct mb_ep *ep;
    struct mb_stls *stls;
    struct mb_stls *zombie;   /* stopped session awaiting free */
    volatile int running;
    int reconnecting;
    struct mb_thread reconnect_thread;
    struct mb_mutex lock;
    char host[256];
    uint16_t port;
    struct mb_net_epaddr resolved;
};

int mb_ctls_create (struct mb_ep *ep);

#endif
