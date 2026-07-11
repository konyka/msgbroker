#ifndef MB_TRANSPORT_TLS_BTLS_H_INCLUDED
#define MB_TRANSPORT_TLS_BTLS_H_INCLUDED

#include "../../transport.h"
#include "../../utils/list.h"
#include "../../pal/mutex.h"
#include "../../pal/thread.h"

#include <openssl/ssl.h>

struct mb_ep;

struct mb_btls {
    struct mb_ep *ep;
    int listen_fd;
    SSL_CTX *ctx;
    struct mb_list stlss;
    struct mb_list zombies;
    struct mb_mutex lock;
    struct mb_thread accept_thread;
    volatile int running;
};

int mb_btls_create (struct mb_ep *ep);

#endif
