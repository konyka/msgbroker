#ifndef MB_CORE_EP_H_INCLUDED
#define MB_CORE_EP_H_INCLUDED

#include "../transport.h"
#include "../aio/fsm.h"
#include "../utils/list.h"

#include <msgbroker/mb.h>

#define MB_EP_STATE_IDLE     1
#define MB_EP_STATE_ACTIVE   2
#define MB_EP_STATE_STOPPING 3

#define MB_EP_STOPPED 1

struct mb_ep {
    struct mb_fsm fsm;
    int state;
    struct mb_sock *sock;
    struct mb_ep_options options;
    int eid;
    struct mb_list_item item;
    char addr[MB_SOCKADDR_MAX + 1];
    int protocol;
    int last_errno;
    int bind; /* 1 if created via bind, 0 if via connect */

    void *tran;
    struct mb_ep_ops ops;
};

int mb_ep_init (struct mb_ep *self, int src, struct mb_sock *sock, int eid,
    const struct mb_transport *transport, int bind, const char *addr);
void mb_ep_term (struct mb_ep *self);
void mb_ep_start (struct mb_ep *self);
void mb_ep_stop (struct mb_ep *self);
void mb_ep_stopped_cb (struct mb_ep *self);
struct mb_ctx *mb_ep_getctx (struct mb_ep *self);
const char *mb_ep_getaddr (struct mb_ep *self);
void mb_ep_getopt (struct mb_ep *self, int level, int option,
    void *optval, size_t *optvallen);
int mb_ep_ispeer (struct mb_ep *self, int socktype);
void mb_ep_set_error (struct mb_ep *self, int errnum);
void mb_ep_clear_error (struct mb_ep *self);
void mb_ep_stat_increment (struct mb_ep *self, int name, int increment);
struct mb_sock *mb_ep_sock (struct mb_ep *self);

#endif
